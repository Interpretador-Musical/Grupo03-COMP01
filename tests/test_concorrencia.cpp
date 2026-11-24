// Testes de concorrência da SCH-08 (#46).
//
// Estes casos sobem threads de verdade: a de áudio do miniaudio (pelo backend
// null, sem placa de som e sem tocar som) e uma thread comum que levanta um
// sinal. Num build normal eles conferem o comportamento. O que prova a
// ausência de corrida é o mesmo binário compilado com -fsanitize=thread: o job
// ThreadSanitizer do CI roda o `mus_tests` inteiro com halt_on_error=1, e uma
// leitura e uma escrita sem ordem entre si viram falha mesmo que, naquela
// execução, nada tenha dado errado. Ver docs/concorrencia.md.
//
// Ficam numa suíte própria (`concorrencia`), com entrada própria no ctest,
// porque tocam áudio em tempo real: somam cerca de um segundo à suíte e
// aparecem separados na saída do CI.

#include "doctest.h"

#include <csignal>
#include <cstdint>
#include <string>
#include <thread>

#include "audio/audio_engine.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"
#include "core/timeline_queue.h"
#include "engine/engine.h"
#include "espera_por.h"
#include "frontend/frontend.h"
#include "interpreter/interpreter.h"
#include "semantic/analisador_semantico.h"

using mus::testes::esperaPor;

namespace {

using Saida = mus::AudioEngine::Saida;

// Um período do dispositivo, em frames. Esperar alguns além do necessário
// garante que pelo menos um callback inteiro começou depois do ponto que o
// teste quer observar.
constexpr std::uint64_t kPeriodo = mus::AudioEngine::kPeriodSizeInFrames;

// Prazo das esperas. Com o ThreadSanitizer a thread de áudio fica várias vezes
// mais lenta, e o prazo só existe para um defeito virar falha em vez de travar.
constexpr std::chrono::milliseconds kPrazo{10000};

mus::SoundEvent notaLonga(double hz) {
    mus::SoundEvent evento;
    evento.startTime = 0.0;
    evento.duration = 2.0;
    evento.frequency = hz;
    evento.volume = 0.2f;
    return evento;
}

}  // namespace

TEST_SUITE("concorrencia") {

TEST_CASE("setMaxVoices com o áudio tocando é recusado e não encosta nas vozes") {
    // A fila vem antes do motor: o C++ destrói na ordem inversa, então a
    // thread de áudio para antes de a fila deixar de existir (como no main).
    mus::SpscRingBuffer<mus::SoundEvent> fila(4);
    for (const double hz : {220.0, 277.18, 329.63, 440.0}) {
        REQUIRE(fila.push(notaLonga(hz)));
    }

    mus::AudioEngine audio(Saida::Nula);
    audio.setRingBuffer(&fila);
    audio.setExpectedDuration(2.0);
    REQUIRE(audio.start());

    // Quatro vozes soando na thread de áudio enquanto a principal tenta
    // reduzir o limite. Antes da SCH-08 a troca zerava as vozes 2 a 31 daqui
    // de fora, no meio do callback: corrida de dados e corte sem fade.
    REQUIRE(esperaPor([&] { return audio.framesRenderizados() > kPeriodo; }, kPrazo));
    CHECK_FALSE(audio.setMaxVoices(2));
    CHECK(audio.maxVoices() == mus::AudioEngine::kMaxVoices);

    // Deixa o callback continuar mexendo nas vozes depois da tentativa.
    const std::uint64_t depois = audio.framesRenderizados();
    REQUIRE(esperaPor([&] { return audio.framesRenderizados() > depois + 4 * kPeriodo; },
                      kPrazo));
    audio.stop();

    // Parado, o mesmo pedido vale.
    CHECK(audio.setMaxVoices(2));
    CHECK(audio.maxVoices() == 2);
}

TEST_CASE("o pedido de parada feito por sinal em outra thread chega à principal") {
    // O doctest instala tratadores próprios para SIGINT e SIGTERM; eles voltam
    // no fim para não mudar o comportamento dos outros casos.
    using Tratador = void (*)(int);
    const Tratador anteriorInt = std::signal(SIGINT, SIG_DFL);
    const Tratador anteriorTerm = std::signal(SIGTERM, SIG_DFL);

    mus::installShutdownHandler();
    const bool estavaLimpo = !mus::shutdownRequested();

    // `raise` entrega o sinal à própria thread que o chama: o tratador escreve
    // a flag nesta thread secundária, e a principal a lê ao mesmo tempo — o
    // que acontece no `--loop` quando o Ctrl+C cai na thread de tempo ou na
    // de áudio.
    std::thread outra([] { std::raise(SIGINT); });
    const bool chegou = esperaPor([] { return mus::shutdownRequested(); });
    outra.join();

    std::signal(SIGINT, anteriorInt);
    std::signal(SIGTERM, anteriorTerm);
    mus::clearShutdownRequest();

    CHECK(estavaLimpo);
    CHECK(chegou);
    CHECK_FALSE(mus::shutdownRequested());
}

TEST_CASE("um programa compilado toca inteiro numa thread de áudio real") {
    // 0,75 s de música: três contra quatro duas vezes e um acorde, com
    // trilhas paralelas, como em exemplos/polirritmia.mus.
    const std::string fonte =
        "andamento 240\n"
        "repita 2 vezes {\n"
        "    repita 3 vezes { toca do4 por 1 / 3 },\n"
        "    repita 4 vezes { toca sol3 por 1 / 4 }\n"
        "}\n"
        "{ toca do4 por 1, toca mi4 por 1, toca sol4 por 1 }\n";

    std::string erro;
    auto programa = mus::parseToAst(fonte, &erro);
    REQUIRE_MESSAGE(programa != nullptr, erro);

    mus::semantica::AnalisadorSemantico analisador;
    REQUIRE(analisador.analisar(*programa));

    mus::Interpretador interpretador;
    REQUIRE_MESSAGE(interpretador.executar(*programa), interpretador.erro());
    const mus::Timeline& timeline = interpretador.timeline();
    REQUIRE(timeline.size() == 17);

    // Mesma montagem do main.cpp: a fila recebe a timeline inteira antes de o
    // áudio começar, e a thread de áudio é a única que a consome.
    mus::SpscRingBuffer<mus::SoundEvent> fila(timeline.size());
    REQUIRE(mus::carregarFila(timeline, fila));

    mus::AudioEngine audio(Saida::Nula);
    audio.setRingBuffer(&fila);
    audio.setExpectedDuration(timeline.duration());
    REQUIRE(audio.start());

    // Espera pelo progresso do callback, não pelo relógio: sob o TSan a
    // thread de áudio atrasa, e um prazo em segundos de música cortaria o fim.
    const auto framesDaMusica = static_cast<std::uint64_t>(
        timeline.duration() * mus::AudioEngine::kSampleRate);
    const std::uint64_t alvo = framesDaMusica + 4 * kPeriodo;
    REQUIRE(esperaPor([&] { return audio.framesRenderizados() >= alvo; }, kPrazo));
    audio.stop();

    // Depois do stop a thread de áudio não consome mais, e a principal pode
    // olhar a fila e as vozes: todo evento chegou ao callback e toda nota
    // terminou, inclusive o release.
    mus::SoundEvent sobra;
    CHECK_FALSE(fila.pop(&sobra));
    CHECK(audio.activeVoiceCount() == 0);
}

}  // TEST_SUITE("concorrencia")

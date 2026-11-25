// Consumo ocioso (SCH-12, #66).
//
// Duas partes. A primeira confere o motor de áudio quando ele não está
// produzindo som: a pausa e o volume 0 não ocupam voz, e o atalho que pula um
// trecho de silêncio de uma vez entrega exatamente as mesmas amostras que o
// laço frame a frame. A segunda, na suíte `ocioso`, sobe threads de verdade e
// confere que nenhum laço de espera gira em vazio. Medições em
// docs/consumo_ocioso.md.

#include "doctest.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <limits>
#include <thread>
#include <vector>

#include "audio/audio_engine.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"
#include "engine/engine.h"

using mus::AudioEngine;
using mus::SoundEvent;
using mus::SpscRingBuffer;

namespace {

constexpr double kTaxa = AudioEngine::kSampleRate;

// A pausa como o interpretador a emite: Synth de 0 Hz e volume 0.
SoundEvent pausa(double inicio, double duracao) {
    return SoundEvent{inicio, duracao, 0.0, 0.0f};
}

// Renderiza `eventos` em blocos de `bloco` frames até `frames` amostras, pelo
// mesmo caminho do callback, sem o master bus (que não guarda estado entre
// chamadas que interesse aqui).
std::vector<float> renderizar(const std::vector<SoundEvent>& eventos,
                              std::size_t frames,
                              std::uint32_t bloco,
                              double duracaoEsperada = 0.0,
                              int maxVozes = AudioEngine::kMaxVoices,
                              bool* houveUnderrun = nullptr,
                              int* vozesNoFim = nullptr) {
    SpscRingBuffer<SoundEvent> fila(eventos.size() + 1);
    for (const SoundEvent& evento : eventos) {
        REQUIRE(fila.push(evento));
    }
    AudioEngine engine;
    REQUIRE(engine.setMaxVoices(maxVozes));
    engine.setRingBuffer(&fila);
    engine.setExpectedDuration(duracaoEsperada);

    std::vector<float> saida;
    std::vector<float> buffer(bloco);
    bool underrun = false;
    while (saida.size() < frames) {
        engine.renderRealTime(buffer.data(), bloco);
        saida.insert(saida.end(), buffer.begin(), buffer.end());
        underrun = engine.checkAndClearUnderrun() || underrun;
    }
    saida.resize(frames);
    if (houveUnderrun != nullptr) {
        *houveUnderrun = underrun;
    }
    if (vozesNoFim != nullptr) {
        *vozesNoFim = engine.activeVoiceCount();
    }
    return saida;
}

}  // namespace

TEST_CASE("ocioso: a pausa não ocupa voz nem vira underrun no fim da música") {
    // Nota de 0,1 s e pausa final de 0,2 s: o silêncio da pausa é música.
    const std::vector<SoundEvent> eventos = {SoundEvent{0.0, 0.1, 440.0, 0.5f},
                                             pausa(0.1, 0.2)};
    bool underrun = true;
    int vozes = -1;

    // Só a pausa soando: nenhuma voz.
    renderizar({pausa(0.0, 0.2)}, static_cast<std::size_t>(0.1 * kTaxa), 256, 0.2,
               AudioEngine::kMaxVoices, &underrun, &vozes);
    CHECK(vozes == 0);
    CHECK_FALSE(underrun);

    // Até o fim esperado, sem falso positivo.
    const std::vector<float> saida =
        renderizar(eventos, static_cast<std::size_t>(0.3 * kTaxa), 256, 0.3,
                   AudioEngine::kMaxVoices, &underrun, &vozes);
    CHECK_FALSE(underrun);
    CHECK(vozes == 0);
    for (std::size_t i = static_cast<std::size_t>(0.11 * kTaxa); i < saida.size(); ++i) {
        REQUIRE(saida[i] == 0.0f);
    }
}

TEST_CASE("ocioso: com as vozes cheias, a pausa não rouba uma nota audível") {
    // Quatro vozes e quatro notas: antes da SCH-12, a pausa de uma quinta
    // trilha tomava a voz mais fraca, e uma nota do acorde sumia.
    const std::vector<SoundEvent> acorde = {SoundEvent{0.0, 0.5, 261.63, 0.2f},
                                            SoundEvent{0.0, 0.5, 329.63, 0.2f},
                                            SoundEvent{0.0, 0.5, 392.00, 0.2f},
                                            SoundEvent{0.0, 0.5, 523.25, 0.1f}};
    std::vector<SoundEvent> comPausa = acorde;
    comPausa.push_back(pausa(0.0, 0.25));
    comPausa.push_back(pausa(0.25, 0.25));

    const auto frames = static_cast<std::size_t>(0.5 * kTaxa);
    const std::vector<float> esperado = renderizar(acorde, frames, 256, 0.0, 4);
    const std::vector<float> obtido = renderizar(comPausa, frames, 256, 0.0, 4);

    CHECK(obtido == esperado);
}

TEST_CASE("ocioso: volume 0 e volume NaN não ocupam voz") {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    int vozes = -1;
    const std::vector<float> saida =
        renderizar({SoundEvent{0.0, 0.2, 440.0, 0.0f}, SoundEvent{0.0, 0.2, 660.0, nan}},
                   static_cast<std::size_t>(0.1 * kTaxa), 256, 0.0,
                   AudioEngine::kMaxVoices, nullptr, &vozes);
    CHECK(vozes == 0);
    for (float amostra : saida) {
        REQUIRE(amostra == 0.0f);
    }
}

TEST_CASE("ocioso: o silêncio pulado não depende do tamanho do bloco") {
    // O atalho do silêncio pula até o próximo evento ou até o fim do bloco, o
    // que vier antes. Se ele errasse um frame, a nota seguinte começaria
    // adiantada ou atrasada, e o erro mudaria com o tamanho do bloco.
    const std::vector<SoundEvent> eventos = {
        SoundEvent{0.010, 0.050, 440.0, 0.5f}, pausa(0.060, 0.100),
        SoundEvent{0.160, 0.020, 660.0, 0.5f}, SoundEvent{0.300, 0.003, 880.0, 0.5f},
        pausa(0.303, 0.200),                    SoundEvent{0.503, 0.100, 220.0, 0.5f}};
    const auto frames = static_cast<std::size_t>(0.7 * kTaxa);

    const std::vector<float> referencia = renderizar(eventos, frames, 1);
    for (std::uint32_t bloco : {2u, 7u, 256u, 4096u}) {
        CAPTURE(bloco);
        CHECK(renderizar(eventos, frames, bloco) == referencia);
    }
}

TEST_CASE("ocioso: depois de um silêncio longo, a nota começa no frame agendado") {
    // A mesma nota, sozinha em 0 s e depois de 10 000 frames de silêncio, tem
    // de sair igual, só deslocada: nenhum frame a mais nem a menos no pulo.
    constexpr std::size_t kAtraso = 10000;
    const double inicio = static_cast<double>(kAtraso) / kTaxa;
    const auto frames = static_cast<std::size_t>(0.05 * kTaxa);

    const std::vector<float> semAtraso =
        renderizar({SoundEvent{0.0, 0.03, 440.0, 0.5f}}, frames, 256);
    const std::vector<float> comAtraso =
        renderizar({SoundEvent{inicio, 0.03, 440.0, 0.5f}}, kAtraso + frames, 256);

    for (std::size_t i = 0; i < kAtraso; ++i) {
        REQUIRE(comAtraso[i] == 0.0f);
    }
    for (std::size_t i = 0; i < frames; ++i) {
        REQUIRE(comAtraso[kAtraso + i] == semAtraso[i]);
    }
}

// ---- Espera sem girar em vazio ---------------------------------------------
//
// Uma espera ativa (laço com yield ou sem pausa nenhuma) ocupa um núcleo
// inteiro: o processo gasta perto de 100% de CPU por segundo de relógio. As
// esperas do projeto bloqueiam, e o medido fica abaixo de 3% (ver
// docs/consumo_ocioso.md). O limite de 50% separa os dois casos com folga
// mesmo sob o ThreadSanitizer, que deixa cada volta várias vezes mais lenta.
//
// std::clock() mede o tempo de CPU do processo inteiro (todas as threads) no
// Linux e no macOS. No Windows ele mede tempo de relógio, e o teste não faria
// sentido.

TEST_SUITE("ocioso") {

#ifndef _WIN32

namespace {

constexpr double kLimiteDeCpu = 0.5;

// Fração de CPU do processo durante `tarefa`, em relação ao tempo de relógio.
template <typename F>
double fracaoDeCpu(F tarefa) {
    const std::clock_t cpuInicio = std::clock();
    const auto relogioInicio = std::chrono::steady_clock::now();
    tarefa();
    const double cpu =
        static_cast<double>(std::clock() - cpuInicio) / CLOCKS_PER_SEC;
    const double relogio = std::chrono::duration<double>(
                               std::chrono::steady_clock::now() - relogioInicio)
                               .count();
    return cpu / relogio;
}

}  // namespace

TEST_CASE("ocioso: a thread de tempo espera entre as voltas") {
    mus::Engine engine;
    const double fracao = fracaoDeCpu([&engine] {
        REQUIRE(engine.start());
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        engine.stop();
    });
    CAPTURE(fracao);
    CHECK(engine.tickCount() > 0);
    CHECK(fracao < kLimiteDeCpu);
}

TEST_CASE("ocioso: waitUntilFinished dorme enquanto a música toca") {
    // Uma pausa de 0,3 s pelo backend null: a thread de áudio existe e roda o
    // callback em tempo real, e a principal só espera.
    SpscRingBuffer<SoundEvent> fila(4);
    REQUIRE(fila.push(pausa(0.0, 0.3)));

    AudioEngine audio(AudioEngine::Saida::Nula);
    audio.setRingBuffer(&fila);
    audio.setExpectedDuration(0.3);

    const double fracao = fracaoDeCpu([&audio] {
        REQUIRE(audio.start());
        audio.waitUntilFinished();
        audio.stop();
    });
    CAPTURE(fracao);
    CHECK(audio.framesRenderizados() > 0);
    CHECK(fracao < kLimiteDeCpu);
}

#endif  // _WIN32

}  // TEST_SUITE("ocioso")

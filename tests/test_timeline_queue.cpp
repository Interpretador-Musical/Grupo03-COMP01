#include "doctest.h"

#include <cmath>
#include <cstdint>
#include <vector>

#include "audio/audio_engine.h"
#include "core/playhead.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"
#include "core/timeline_queue.h"

using namespace mus;

// Integridade da entrega da Timeline à thread de áudio — SCH-05 (#29): a
// política de carga (`carregarFila`) e o consumo pelo callback.

namespace {

SoundEvent evento(double inicio, double frequencia, double duracao = 0.25) {
    return SoundEvent{inicio, duracao, frequencia, 0.5f};
}

// Esvazia a fila do lado do consumidor, como a thread de áudio faria.
std::vector<SoundEvent> drenar(SpscRingBuffer<SoundEvent>& fila) {
    std::vector<SoundEvent> saida;
    SoundEvent item;
    while (fila.pop(&item)) {
        saida.push_back(item);
    }
    return saida;
}

}  // namespace

TEST_CASE("carregarFila entrega a timeline inteira em ordem de início") {
    Timeline timeline;
    timeline.add(evento(1.0, 330.0));
    timeline.add(evento(0.0, 220.0));
    timeline.add(evento(0.5, 275.0));

    SpscRingBuffer<SoundEvent> fila(timeline.size());
    REQUIRE(carregarFila(timeline, fila));

    const std::vector<SoundEvent> saida = drenar(fila);
    REQUIRE(saida.size() == 3);
    CHECK(saida[0].frequency == 220.0);
    CHECK(saida[1].frequency == 275.0);
    CHECK(saida[2].frequency == 330.0);
}

TEST_CASE("eventos simultâneos chegam ao consumidor na ordem em que foram agendados") {
    // Um acorde de 16 notas no mesmo instante: a ordenação estável não pode
    // embaralhá-las — é o que mantém a reprodução determinística.
    Timeline timeline;
    for (int i = 0; i < 16; ++i) {
        timeline.add(evento(0.0, 200.0 + i));
    }

    SpscRingBuffer<SoundEvent> fila(timeline.size());
    REQUIRE(carregarFila(timeline, fila));

    const std::vector<SoundEvent> saida = drenar(fila);
    REQUIRE(saida.size() == 16);
    for (int i = 0; i < 16; ++i) {
        CHECK(saida[static_cast<std::size_t>(i)].frequency == 200.0 + i);
    }
}

TEST_CASE("timeline maior que a fila é recusada sem empurrar nada") {
    Timeline timeline;
    for (int i = 0; i < 5; ++i) {
        timeline.add(evento(i * 0.25, 220.0));
    }

    SpscRingBuffer<SoundEvent> fila(4);
    CHECK_FALSE(carregarFila(timeline, fila));
    // Tudo ou nada: nenhum evento entrou, então não há meia música para tocar.
    CHECK(drenar(fila).empty());
}

TEST_CASE("fila já ocupada que enche no meio da carga é detectada") {
    // Viola a precondição (fila vazia) de propósito: o que importa é que a
    // falha aparece no retorno, em vez de um evento ficar para trás calado.
    SpscRingBuffer<SoundEvent> fila(2);
    REQUIRE(fila.push(evento(9.0, 999.0)));

    Timeline timeline;
    timeline.add(evento(0.0, 220.0));
    timeline.add(evento(0.5, 330.0));

    CHECK_FALSE(carregarFila(timeline, fila));
}

TEST_CASE("timeline vazia carrega numa fila de capacidade zero") {
    SpscRingBuffer<SoundEvent> fila(0);
    CHECK(carregarFila(Timeline{}, fila));
    CHECK(drenar(fila).empty());
}

TEST_CASE("o callback dispara no mesmo frame todos os eventos que já venceram") {
    // Rajada: 40 eventos em t = 0, mais do que as 16 vozes da engine, e um
    // evento em t = 1 s. Se o consumidor tirasse um evento por frame, a rajada
    // levaria 40 frames para sair da fila e as notas soariam desencontradas.
    // Ele tira todos os vencidos dentro do primeiro frame (e o próximo fica
    // como pendente), então depois de um frame a fila está vazia.
    Timeline timeline;
    for (int i = 0; i < 40; ++i) {
        timeline.add(evento(0.0, 200.0 + i, 0.1));
    }
    timeline.add(evento(1.0, 440.0, 0.1));

    SpscRingBuffer<SoundEvent> fila(timeline.size());
    REQUIRE(carregarFila(timeline, fila));

    AudioEngine engine;
    engine.setRingBuffer(&fila);
    engine.setExpectedDuration(timeline.duration());

    float amostra = 0.0f;
    engine.render(&amostra, 1);

    SoundEvent sobra;
    CHECK_FALSE(fila.pop(&sobra));
}

TEST_CASE("a reprodução inteira consome a fila e a saída fica limitada") {
    // Uma melodia densa de notas curtas, renderizada em blocos do tamanho que
    // o dispositivo pede, até o fim da música: a fila esvazia e nenhuma
    // amostra sai de [-1, 1] nem vira NaN.
    Timeline timeline;
    Playhead playhead(240.0);
    for (int i = 0; i < 64; ++i) {
        timeline.add(playhead.play(60.0 + (i % 12), 0.125));
    }

    SpscRingBuffer<SoundEvent> fila(timeline.size());
    REQUIRE(carregarFila(timeline, fila));

    AudioEngine engine;
    engine.setRingBuffer(&fila);
    engine.setExpectedDuration(timeline.duration());

    const auto totalFrames = static_cast<std::uint32_t>(
        std::ceil(timeline.duration() * AudioEngine::kSampleRate));
    std::vector<float> bloco(AudioEngine::kPeriodSizeInFrames);
    bool limitada = true;
    for (std::uint32_t feitos = 0; feitos < totalFrames;
         feitos += AudioEngine::kPeriodSizeInFrames) {
        engine.render(bloco.data(), AudioEngine::kPeriodSizeInFrames);
        for (const float amostra : bloco) {
            limitada = limitada && std::isfinite(amostra) && std::fabs(amostra) <= 1.0f;
        }
    }

    CHECK(limitada);
    SoundEvent sobra;
    CHECK_FALSE(fila.pop(&sobra));
}

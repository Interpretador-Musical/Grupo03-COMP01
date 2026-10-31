#include "doctest.h"

#include <cmath>
#include <limits>
#include <vector>

#include "audio/audio_engine.h"
#include "audio/limiter.h"
#include "core/playhead.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"

using namespace mus;

namespace {

float maiorModulo(const std::vector<float>& amostras) {
    float maior = 0.0f;
    for (float amostra : amostras) {
        maior = std::fmax(maior, std::fabs(amostra));
    }
    return maior;
}

}  // namespace

// ---- A curva -------------------------------------------------------------

TEST_CASE("abaixo do limiar a amostra passa intacta") {
    // Comparação exata de propósito: o limitador não pode colorir o sinal
    // normal nem com erro de arredondamento.
    for (float amostra : {0.0f, 0.1f, -0.25f, 0.5f, -0.79f, kSoftClipThreshold,
                          -kSoftClipThreshold}) {
        CHECK(softClip(amostra) == amostra);
    }
}

TEST_CASE("nenhuma entrada sai com módulo acima de 1") {
    // O NFR da issue: segurança acústica, sem exceção. Inclui o `volume 9999`
    // do texto da DSP-10 e os extremos do float.
    const float infinito = std::numeric_limits<float>::infinity();
    for (float amostra : {0.81f, 1.0f, 1.5f, 4.0f, 16.0f, 9999.0f, 1e30f,
                          std::numeric_limits<float>::max(), infinito}) {
        CHECK(softClip(amostra) <= 1.0f);
        CHECK(softClip(-amostra) >= -1.0f);
    }
}

TEST_CASE("a curva é ímpar: o negativo espelha o positivo") {
    for (float amostra : {0.3f, 0.9f, 1.2f, 5.0f, 9999.0f}) {
        CHECK(softClip(-amostra) == -softClip(amostra));
    }
}

TEST_CASE("a curva nunca desce: amplitude maior nunca sai menor") {
    float anterior = softClip(0.0f);
    for (float amostra = 0.0f; amostra <= 4.0f; amostra += 0.001f) {
        const float atual = softClip(amostra);
        CHECK(atual >= anterior);
        anterior = atual;
    }
}

TEST_CASE("sem quina no limiar: valor e inclinação contínuos") {
    // Um passo pequeno logo acima do limiar anda o mesmo que logo abaixo, onde
    // a inclinação é 1. Quina ali seria exatamente o estalo que o corte seco
    // produzia em ±1.0.
    const float passo = 1e-3f;
    const float abaixo = softClip(kSoftClipThreshold) - softClip(kSoftClipThreshold - passo);
    const float acima = softClip(kSoftClipThreshold + passo) - softClip(kSoftClipThreshold);
    CHECK(abaixo == doctest::Approx(passo).epsilon(0.01));
    CHECK(acima == doctest::Approx(passo).epsilon(0.01));
}

TEST_CASE("NaN vira silêncio em vez de chegar à placa") {
    CHECK(softClip(std::numeric_limits<float>::quiet_NaN()) == 0.0f);
}

// ---- O master bus --------------------------------------------------------

TEST_CASE("master bus: volume 9999 no programa compilado não passa de 1") {
    // Caminho do TimelinePlayer, que até a DSP-10 não tinha limite nenhum: a
    // amostra saía como seno * volume, ou seja, ±9999.
    Timeline timeline;
    timeline.add(SoundEvent{0.0, 0.1, 440.0, 9999.0f});

    AudioEngine engine;
    engine.loadTimeline(timeline);

    std::vector<float> saida(AudioEngine::kSampleRate / 20, 0.0f);
    engine.render(saida.data(), static_cast<std::uint32_t>(saida.size()));

    CHECK(maiorModulo(saida) <= 1.0f);
    CHECK(maiorModulo(saida) > kSoftClipThreshold);  // limitado, não mudo
}

TEST_CASE("master bus: acorde de 16 vozes no volume máximo não passa de 1") {
    // Caminho do ring buffer: a soma de 16 senoides chega perto de 16.
    SpscRingBuffer<SoundEvent> buffer(32);
    for (int i = 0; i < 16; ++i) {
        buffer.push(SoundEvent{0.0, 0.1, 220.0 + 55.0 * i, 1.0f});
    }

    AudioEngine engine;
    engine.setRingBuffer(&buffer);

    std::vector<float> saida(AudioEngine::kSampleRate / 20, 0.0f);
    engine.render(saida.data(), static_cast<std::uint32_t>(saida.size()));

    CHECK(maiorModulo(saida) <= 1.0f);
    CHECK(maiorModulo(saida) > kSoftClipThreshold);
}

TEST_CASE("master bus: ganho absurdo na senoide de teste não passa de 1") {
    AudioEngine engine;
    engine.setGain(9999.0f);

    std::vector<float> saida(AudioEngine::kSampleRate / 20, 0.0f);
    engine.render(saida.data(), static_cast<std::uint32_t>(saida.size()));

    CHECK(maiorModulo(saida) <= 1.0f);
    CHECK(maiorModulo(saida) > kSoftClipThreshold);
}

TEST_CASE("master bus: sinal dentro da faixa não é atenuado") {
    // Ganho 0.5 fica abaixo do limiar: o pico da senoide que sai de render()
    // tem de ser o próprio ganho, sem compressão.
    AudioEngine engine;
    engine.setGain(0.5f);

    std::vector<float> saida(AudioEngine::kSampleRate / 20, 0.0f);
    engine.render(saida.data(), static_cast<std::uint32_t>(saida.size()));

    CHECK(maiorModulo(saida) == doctest::Approx(0.5f).epsilon(0.001));
}

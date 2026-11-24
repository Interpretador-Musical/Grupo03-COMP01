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

TEST_CASE("master bus: acorde de 32 vozes no volume máximo não passa de 1") {
    // Caminho do ring buffer: a soma de 32 senoides chega perto de 32.
    SpscRingBuffer<SoundEvent> buffer(32);
    for (int i = 0; i < AudioEngine::kMaxVoices; ++i) {
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

// ---- O limitador de pico (BUG-AUDIO, #109) --------------------------------

namespace {

constexpr double kDoisPi = 6.283185307179586;

// Senoide de `amplitude` e `hz`, com `segundos` de duração, na taxa do motor.
std::vector<float> senoide(double hz, float amplitude, double segundos) {
    const auto frames =
        static_cast<std::size_t>(segundos * AudioEngine::kSampleRate);
    std::vector<float> sinal(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        sinal[i] = amplitude * static_cast<float>(std::sin(
                                   kDoisPi * hz * static_cast<double>(i) /
                                   AudioEngine::kSampleRate));
    }
    return sinal;
}

// Energia acima de 4 kHz sobre a energia total, em dB. As notas dos testes
// ficam abaixo de 1 kHz, então o que passa deste filtro é distorção: são os
// harmônicos que o ouvido escuta como chiado. O passa-altas tem 8ª ordem
// (quatro biquads RBJ iguais em cascata). Com 2ª ordem, o próprio sol4 vazaria
// a -40 dB e esconderia a medida.
double energiaAcimaDe4kHzEmDb(const std::vector<float>& sinal) {
    const double w0 = kDoisPi * 4000.0 / AudioEngine::kSampleRate;
    const double alfa = std::sin(w0) / (2.0 * 0.7071067811865476);
    const double c = std::cos(w0);
    const double a0 = 1.0 + alfa;
    const double b0 = (1.0 + c) / 2.0 / a0;
    const double b1 = -(1.0 + c) / a0;
    const double b2 = b0;
    const double a1 = -2.0 * c / a0;
    const double a2 = (1.0 - alfa) / a0;

    double estado[4][4] = {};
    double agudo = 0.0;
    double total = 0.0;
    for (float amostra : sinal) {
        double y = amostra;
        for (auto& e : estado) {
            const double saida = b0 * y + b1 * e[0] + b2 * e[1] - a1 * e[2] - a2 * e[3];
            e[1] = e[0];
            e[0] = y;
            e[3] = e[2];
            e[2] = saida;
            y = saida;
        }
        agudo += y * y;
        total += static_cast<double>(amostra) * amostra;
    }
    return 10.0 * std::log10(agudo / total);
}

// Toca os eventos pelo master bus, em blocos do tamanho que o dispositivo
// pede, e devolve exatamente o que iria para a placa.
std::vector<float> renderizarPeloMasterBus(const std::vector<SoundEvent>& eventos,
                                           double segundos) {
    SpscRingBuffer<SoundEvent> fila(eventos.size());
    for (const SoundEvent& evento : eventos) {
        REQUIRE(fila.push(evento));
    }

    AudioEngine engine;
    engine.setRingBuffer(&fila);

    const auto total =
        static_cast<std::size_t>(segundos * AudioEngine::kSampleRate);
    std::vector<float> saida;
    std::vector<float> bloco(AudioEngine::kPeriodSizeInFrames);
    while (saida.size() < total) {
        engine.render(bloco.data(), AudioEngine::kPeriodSizeInFrames);
        saida.insert(saida.end(), bloco.begin(), bloco.end());
    }
    saida.resize(total);
    return saida;
}

// O acorde de `exemplos/polirritmia.mus`: dó4, mi4 e sol4 por 2 tempos a 90 BPM.
std::vector<SoundEvent> acordeDoMaior(float volume) {
    constexpr double kDuracao = 2.0 * 60.0 / 90.0;
    return {SoundEvent{0.0, kDuracao, 261.6255653005986, volume},
            SoundEvent{0.0, kDuracao, 329.6275569128699, volume},
            SoundEvent{0.0, kDuracao, 391.99543598174927, volume}};
}

}  // namespace

TEST_CASE("limitador: abaixo do teto o sinal passa intacto, bit a bit") {
    LimitadorDePico limitador(AudioEngine::kSampleRate);
    const std::vector<float> entrada = senoide(440.0, kLimitadorTeto, 0.5);
    for (float amostra : entrada) {
        CHECK(limitador.processar(amostra) == amostra);
    }
    CHECK(limitador.ganho() == 1.0f);
}

TEST_CASE("limitador: tom sustentado acima do teto sai senoidal, só mais baixo") {
    // O ponto da #109: o softClip sozinho achatava a onda. Aqui, passado o
    // ataque, o ganho é constante, e a saída é a mesma senoide multiplicada
    // por um número.
    for (double hz : {32.70319566257483, 261.6255653005986, 1046.5022612023945}) {
        CAPTURE(hz);
        LimitadorDePico limitador(AudioEngine::kSampleRate);
        const std::vector<float> entrada = senoide(hz, 2.0f, 1.0);

        float menorGanho = 1.0f;
        float maiorGanho = 0.0f;
        float maiorSaida = 0.0f;
        for (std::size_t i = 0; i < entrada.size(); ++i) {
            const float saida = limitador.processar(entrada[i]);
            // A primeira volta inteira do grave é o ataque. Medir só depois.
            if (i >= AudioEngine::kSampleRate / 20) {
                menorGanho = std::fmin(menorGanho, limitador.ganho());
                maiorGanho = std::fmax(maiorGanho, limitador.ganho());
                maiorSaida = std::fmax(maiorSaida, std::fabs(saida));
            }
        }

        // Sem ondulação: a retenção cobre o período do dó1 (31 ms). Sem ela,
        // o ganho subia entre dois picos do grave.
        CHECK(maiorGanho - menorGanho < 0.002f);
        CHECK(maiorSaida == doctest::Approx(kLimitadorTeto).epsilon(0.01));
    }
}

TEST_CASE("limitador: o ganho volta a 1 depois do trecho alto") {
    LimitadorDePico limitador(AudioEngine::kSampleRate);
    for (float amostra : senoide(440.0, 3.0f, 0.5)) {
        limitador.processar(amostra);
    }
    REQUIRE(limitador.ganho() < 0.3f);

    // Retenção de 50 ms e recuperação de 150 ms: em 1 s de sinal baixo o
    // ganho já voltou, e a amostra sai idêntica de novo.
    const std::vector<float> baixo = senoide(440.0, 0.5f, 1.5);
    for (std::size_t i = 0; i < baixo.size(); ++i) {
        const float saida = limitador.processar(baixo[i]);
        if (i >= AudioEngine::kSampleRate) {
            CHECK(saida == baixo[i]);
        }
    }
    CHECK(limitador.ganho() == 1.0f);
}

TEST_CASE("limitador: NaN e infinito passam sem envenenar o estado") {
    // Os dois ficam com o softClip. Se o infinito entrasse no detector, o
    // ganho viraria teto / infinito = 0 e o resto da música sairia mudo.
    LimitadorDePico limitador(AudioEngine::kSampleRate);
    const float infinito = std::numeric_limits<float>::infinity();
    CHECK(limitador.processar(infinito) == infinito);
    CHECK(std::isnan(limitador.processar(std::numeric_limits<float>::quiet_NaN())));
    CHECK(limitador.processar(0.5f) == 0.5f);
    CHECK(limitador.ganho() == 1.0f);
}

TEST_CASE("limitador: reiniciar devolve o ganho a 1") {
    LimitadorDePico limitador(AudioEngine::kSampleRate);
    for (float amostra : senoide(440.0, 4.0f, 0.1)) {
        limitador.processar(amostra);
    }
    REQUIRE(limitador.ganho() < 1.0f);
    limitador.reiniciar();
    CHECK(limitador.ganho() == 1.0f);
    CHECK(limitador.processar(0.7f) == 0.7f);
}

// ---- Regressão da #109 no master bus ---------------------------------------
//
// Os limites ficam com folga de uns 5 dB sobre o que o motor corrigido mede
// (ver docs/chiados.md). Com o softClip sozinho, os três falham: -47.1 dB no
// acorde no volume 1, -81.6 dB no acorde no volume 0.33 e -64.0 dB na nota no
// volume 1.

TEST_CASE("master bus: acorde de três notas no volume 1 sai sem chiado") {
    const std::vector<float> saida = renderizarPeloMasterBus(acordeDoMaior(1.0f), 1.5);
    CHECK(maiorModulo(saida) <= 1.0f);
    CHECK(energiaAcimaDe4kHzEmDb(saida) < -70.0);
}

TEST_CASE("master bus: acorde que só passa do limiar nas cristas não piora") {
    // Volume 0.33: a soma só passa de 0.8 em 4% das amostras. Com o softClip
    // sozinho dava -81.6 dB. Com um limitador de degrau de ganho (sem o ataque
    // de 1 ms) ficava pior ainda, em -67.1 dB.
    const std::vector<float> saida = renderizarPeloMasterBus(acordeDoMaior(0.33f), 1.5);
    CHECK(energiaAcimaDe4kHzEmDb(saida) < -85.0);
}

TEST_CASE("master bus: uma nota no volume 1 sai sem chiado") {
    // O volume padrão da linguagem é 1, e a senoide inteira chega a 1.0.
    // Antes, 40% das amostras passavam pela tanh.
    const std::vector<float> saida =
        renderizarPeloMasterBus({SoundEvent{0.0, 1.0, 440.0, 1.0f}}, 1.2);
    CHECK(maiorModulo(saida) <= 1.0f);
    CHECK(energiaAcimaDe4kHzEmDb(saida) < -80.0);
}

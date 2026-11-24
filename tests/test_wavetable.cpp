#include "doctest.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include "audio/wavetable.h"

namespace {

constexpr double kTwoPi = 6.28318530717958647692;

}  // namespace

TEST_CASE("wavetable: precisao da interpolacao linear contra std::sin") {
    constexpr std::uint32_t kSamples = 16384;
    float erroMaximo = 0.0f;

    for (std::uint32_t i = 0; i < kSamples; ++i) {
        const std::uint32_t phase =
            static_cast<std::uint32_t>((std::uint64_t{i} << 32) / kSamples);
        const double angle = (kTwoPi * static_cast<double>(phase)) / 4294967296.0;
        const float esperado = static_cast<float>(std::sin(angle));
        const float obtido = mus::lookupSine(phase);

        const float erro = std::fabs(obtido - esperado);
        if (erro > erroMaximo) {
            erroMaximo = erro;
        }
    }

    // Erro teórico máximo para 2048 pontos com interpolação linear é ~1.18e-6.
    CHECK(erroMaximo < 2.0e-6f);
}

TEST_CASE("wavetable: pontos cardeais e continuidade no wrap-around de uint32_t") {
    CHECK(mus::lookupSine(0x00000000u) == doctest::Approx(0.0f).epsilon(1e-6));
    CHECK(mus::lookupSine(0x40000000u) == doctest::Approx(1.0f).epsilon(1e-6));
    CHECK(mus::lookupSine(0x80000000u) == doctest::Approx(0.0f).epsilon(1e-6));
    CHECK(mus::lookupSine(0xC0000000u) == doctest::Approx(-1.0f).epsilon(1e-6));

    // Último valor antes do overflow deve estar colado em 0.0 por baixo.
    const float antesDaVirada = mus::lookupSine(std::numeric_limits<std::uint32_t>::max());
    CHECK(std::isfinite(antesDaVirada));
    CHECK(std::fabs(antesDaVirada) < 1e-5f);
}

TEST_CASE("wavetable: frequencyToPhaseIncrement protege contra entradas extremas e invalidas") {
    constexpr std::uint32_t kRate = 44100;

    CHECK(mus::frequencyToPhaseIncrement(0.0, kRate) == 0u);
    CHECK(mus::frequencyToPhaseIncrement(-440.0, kRate) == 0u);
    CHECK(mus::frequencyToPhaseIncrement(440.0, 0u) == 0u);
    CHECK(mus::frequencyToPhaseIncrement(std::numeric_limits<double>::quiet_NaN(), kRate) == 0u);
    CHECK(mus::frequencyToPhaseIncrement(std::numeric_limits<double>::infinity(), kRate) == 0u);
    CHECK(mus::frequencyToPhaseIncrement(-std::numeric_limits<double>::infinity(), kRate) == 0u);

    // Frequência gigantesca (ex.: `toca 1000` -> ~1e24 Hz): não pode causar UB nem crash.
    const std::uint32_t incExtremo = mus::frequencyToPhaseIncrement(1e24, kRate);
    const float amostra = mus::lookupSine(incExtremo);
    CHECK(std::isfinite(amostra));
    CHECK(amostra >= -1.0f);
    CHECK(amostra <= 1.0f);
}

TEST_CASE("wavetable: acumulador de fase gera 440 Hz com precisao") {
    constexpr std::uint32_t kRate = 44100;
    const std::uint32_t inc = mus::frequencyToPhaseIncrement(440.0, kRate);
    REQUIRE(inc > 0u);

    std::uint32_t phase = 0u;
    double phaseRef = 0.0;
    const double incRef = (kTwoPi * 440.0) / static_cast<double>(kRate);

    float erroMaximo = 0.0f;
    for (int i = 0; i < 4410; ++i) {  // 100 ms
        const float obtido = mus::lookupSine(phase);
        const float esperado = static_cast<float>(std::sin(phaseRef));
        const float erro = std::fabs(obtido - esperado);
        if (erro > erroMaximo) {
            erroMaximo = erro;
        }

        phase += inc;
        phaseRef += incRef;
    }

    // 4410 amostras acumulando erro de quantização do incremento de 32 bits + LUT.
    CHECK(erroMaximo < 1.0e-4f);
}

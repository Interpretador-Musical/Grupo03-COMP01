#include "doctest.h"

#include <cmath>

#include "core/pitch.h"

using namespace mus;

TEST_CASE("a afinação de referência fecha exatamente") {
    CHECK(midiToFrequency(69.0) == doctest::Approx(440.0));
}

TEST_CASE("cada 12 semitons dobra ou divide a frequência") {
    // É a definição de oitava, e é o que garante que a conversão é exponencial
    // e não linear.
    CHECK(midiToFrequency(81.0) == doctest::Approx(880.0));
    CHECK(midiToFrequency(57.0) == doctest::Approx(220.0));
    CHECK(midiToFrequency(93.0) == doctest::Approx(1760.0));
}

TEST_CASE("Dó 4 é a nota MIDI 60") {
    CHECK(midiToFrequency(60.0) == doctest::Approx(261.6255653));
}

TEST_CASE("frequencyToMidi desfaz midiToFrequency") {
    CHECK(frequencyToMidi(440.0) == doctest::Approx(69.0));
    CHECK(frequencyToMidi(880.0) == doctest::Approx(81.0));

    for (double midi = 21.0; midi <= 108.0; midi += 1.0) {
        CHECK(frequencyToMidi(midiToFrequency(midi)) == doctest::Approx(midi));
    }
}

TEST_CASE("nota fracionária produz frequência entre as vizinhas") {
    // Microtons e pitch shifting contínuo (DSP-06, #36) dependem disso: a
    // conversão não pode arredondar para o semitom mais próximo.
    const double meioSemitom = midiToFrequency(69.5);

    CHECK(meioSemitom > midiToFrequency(69.0));
    CHECK(meioSemitom < midiToFrequency(70.0));
    CHECK(meioSemitom == doctest::Approx(440.0 * std::pow(2.0, 0.5 / 12.0)));
}

TEST_CASE("nearlyEqual absorve o erro de ponto flutuante") {
    CHECK(nearlyEqual(0.1 + 0.2, 0.3));
    CHECK(nearlyEqual(1.0, 1.0));
    CHECK(nearlyEqual(1.0, 1.0 + kEpsilon / 2.0));
}

TEST_CASE("nearlyEqual é simétrico e separa o que é diferente de verdade") {
    // A implementação usa `a > b ? a - b : b - a` justamente para não depender
    // da ordem dos argumentos.
    CHECK(nearlyEqual(1.0, 1.0 + 1e-6) == nearlyEqual(1.0 + 1e-6, 1.0));

    CHECK_FALSE(nearlyEqual(1.0, 1.0001));
    CHECK_FALSE(nearlyEqual(0.0, 1.0));
    CHECK_FALSE(nearlyEqual(-1.0, 1.0));
}

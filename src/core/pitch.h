#ifndef MUS_CORE_PITCH_H
#define MUS_CORE_PITCH_H

#include <cmath>

namespace mus {

// Tolerância padrão para comparar tempos e frequências. O MAT-04 (#22) vai
// construir em cima dela o tratamento de erro na junção de ciclos.
inline constexpr double kEpsilon = 1e-9;

constexpr bool nearlyEqual(double a, double b, double epsilon = kEpsilon) {
    return (a > b ? a - b : b - a) <= epsilon;
}

// Referência de afinação padrão: Lá 4 = nota MIDI 69 = 440 Hz.
inline constexpr int kMidiA4 = 69;
inline constexpr double kFrequencyA4 = 440.0;
inline constexpr int kSemitonesPerOctave = 12;

// Aceita nota fracionária de propósito: é o que vai permitir microtons e o
// pitch shifting contínuo do DSP-06 (#36).
inline double midiToFrequency(double midiNote) {
    return kFrequencyA4 *
           std::pow(2.0, (midiNote - kMidiA4) / kSemitonesPerOctave);
}

inline double frequencyToMidi(double hz) {
    return kMidiA4 + kSemitonesPerOctave * std::log2(hz / kFrequencyA4);
}

// Converte uma frequência (Hz) em taxa de reprodução (playback rate) relativa
// a uma frequência base (DSP-06, #36).
// Ex: 440 Hz com base de 440 Hz -> 1.0 (velocidade normal);
//     880 Hz com base de 440 Hz -> 2.0 (+1 oitava, dobro da velocidade);
//     220 Hz com base de 440 Hz -> 0.5 (-1 oitava, metade da velocidade).
inline double frequencyToPlaybackRate(double targetFrequency, double baseFrequency = kFrequencyA4) {
    if (baseFrequency <= 0.0 || targetFrequency <= 0.0) {
        return 0.0;
    }
    return targetFrequency / baseFrequency;
}

// Converte um deslocamento em semitons para taxa de reprodução fracionária (DSP-06, #36).
// Ex: +12 semitons -> 2.0; -12 semitons -> 0.5; +1 semitom -> 2^(1/12) ≈ 1.05946.
inline double semitonesToPlaybackRate(double semitones) {
    return std::pow(2.0, semitones / kSemitonesPerOctave);
}

}  // namespace mus

#endif  // MUS_CORE_PITCH_H

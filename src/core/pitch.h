/**
 * @file pitch.h
 * @brief Conversões entre altura MIDI, frequência (Hz) e taxa de reprodução.
 */

#ifndef MUS_CORE_PITCH_H
#define MUS_CORE_PITCH_H

#include <cmath>

namespace mus {

/** @brief Tolerância padrão para comparações numéricas de tempo e frequência. */
inline constexpr double kEpsilon = 1e-9;

/**
 * @brief Compara dois valores em ponto flutuante dentro de uma tolerância.
 * @param a Primeiro valor.
 * @param b Segundo valor.
 * @param epsilon Margem máxima de diferença aceita (padrão: `kEpsilon`).
 * @return `true` se `|a - b| <= epsilon`.
 */
constexpr bool nearlyEqual(double a, double b, double epsilon = kEpsilon) {
    return (a > b ? a - b : b - a) <= epsilon;
}

/** @brief Número MIDI do Lá central (A4). */
inline constexpr int kMidiA4 = 69;
/** @brief Frequência padrão de referência do Lá 4 em Hz. */
inline constexpr double kFrequencyA4 = 440.0;
/** @brief Quantidade de semitons em uma oitava do temperamento igual. */
inline constexpr int kSemitonesPerOctave = 12;

/**
 * @brief Converte uma nota MIDI (inclusive fracionária para microtons) em Hz.
 * @param midiNote Número MIDI (ex.: `69.0` = Lá 4, `60.0` = Dó 4).
 * @return Frequência em Hz, ou `0.0` se `midiNote` não for finito.
 */
inline double midiToFrequency(double midiNote) {
    if (!std::isfinite(midiNote)) {
        return 0.0;
    }
    return kFrequencyA4 *
           std::pow(2.0, (midiNote - kMidiA4) / kSemitonesPerOctave);
}

/**
 * @brief Converte uma frequência em Hz para o número MIDI correspondente.
 * @param hz Frequência positiva em Hz.
 * @return Número MIDI em `double`, ou `0.0` se `hz` não for finito e positivo.
 */
inline double frequencyToMidi(double hz) {
    if (!std::isfinite(hz) || hz <= 0.0) {
        return 0.0;
    }
    return kMidiA4 + kSemitonesPerOctave * std::log2(hz / kFrequencyA4);
}

/**
 * @brief Converte uma frequência alvo em taxa de reprodução relativa (DSP-06, #36).
 *
 * Exemplos com `baseFrequency = 440.0`: `440 Hz -> 1.0`, `880 Hz -> 2.0`, `220 Hz -> 0.5`.
 *
 * @param targetFrequency Frequência desejada em Hz.
 * @param baseFrequency Frequência de referência em Hz (padrão: `kFrequencyA4`).
 * @return Razão de playback, ou `0.0` se qualquer entrada for inválida.
 */
inline double frequencyToPlaybackRate(double targetFrequency, double baseFrequency = kFrequencyA4) {
    if (!std::isfinite(targetFrequency) || !std::isfinite(baseFrequency) ||
        baseFrequency <= 0.0 || targetFrequency <= 0.0) {
        return 0.0;
    }
    return targetFrequency / baseFrequency;
}

/**
 * @brief Converte um deslocamento em semitons para taxa de reprodução (DSP-06, #36).
 * @param semitones Deslocamento em semitons (ex.: `+12` = `2.0`, `-12` = `0.5`).
 * @return Fator multiplicativo de velocidade, ou `0.0` se não finito.
 */
inline double semitonesToPlaybackRate(double semitones) {
    if (!std::isfinite(semitones)) {
        return 0.0;
    }
    return std::pow(2.0, semitones / kSemitonesPerOctave);
}

}  // namespace mus

#endif  // MUS_CORE_PITCH_H

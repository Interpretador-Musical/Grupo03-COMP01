/**
 * @file frames.h
 * @brief Conversão determinística de segundos da `Timeline` para índices de frame.
 */

#ifndef MUS_CORE_FRAMES_H
#define MUS_CORE_FRAMES_H

#include <cmath>
#include <cstdint>

namespace mus {

/**
 * @brief Converte um instante absoluto em segundos para o índice de frame mais próximo.
 *
 * Regra única adotada na MAT-08 (#45) pelo `AudioEngine`, `TimelinePlayer` e
 * `gerador_wav`: arredonda cada instante absoluto (`std::llround`) em vez de
 * acumular durações convertidas, mantendo erro de quantização limitado a meio
 * frame (~11 µs a 44,1 kHz) sem drift e sem gaps/sobreposições entre notas.
 *
 * Valores negativos, `NaN` ou infinitos retornam `0`, evitando comportamento
 * indefinido na conversão para inteiro.
 *
 * @param segundos Instante em segundos na `Timeline`.
 * @param sampleRate Taxa de amostragem em Hz.
 * @return Índice absoluto do frame correspondente (ou `0` fora do domínio).
 */
inline std::uint64_t segundosParaFrame(double segundos, std::uint32_t sampleRate) {
    const double frames = segundos * static_cast<double>(sampleRate);
    if (!(frames > 0.0) || !std::isfinite(frames)) {
        return 0;
    }
    return static_cast<std::uint64_t>(std::llround(frames));
}

}  // namespace mus

#endif  // MUS_CORE_FRAMES_H

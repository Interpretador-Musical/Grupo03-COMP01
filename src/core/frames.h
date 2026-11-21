#ifndef MUS_CORE_FRAMES_H
#define MUS_CORE_FRAMES_H

#include <cmath>
#include <cstdint>

namespace mus {

// Regra única de conversão de um instante da Timeline (segundos) para um
// frame de áudio — MAT-08 (#45).
//
// Arredonda para o frame mais próximo: o erro de quantização fica limitado a
// meio frame (~11 µs a 44,1 kHz) e, como cada instante é convertido a partir
// do seu valor absoluto e não somando durações já convertidas, esse erro não
// se acumula ao longo da peça.
//
// Antes, o AudioEngine arredondava o início, truncava a duração e o
// TimelinePlayer disparava no primeiro frame >= início (equivale a ceil): a
// mesma Timeline caía em frames diferentes conforme o caminho, e o fim de uma
// nota podia ficar um frame antes do início da seguinte.
//
// Valores negativos, NaN ou infinitos viram o frame 0 — a Timeline já chega
// validada pelo interpretador (MAT-04), isto só impede comportamento
// indefinido na conversão para inteiro.
inline std::uint64_t segundosParaFrame(double segundos, std::uint32_t sampleRate) {
    const double frames = segundos * static_cast<double>(sampleRate);
    if (!(frames > 0.0) || !std::isfinite(frames)) {
        return 0;
    }
    return static_cast<std::uint64_t>(std::llround(frames));
}

}  // namespace mus

#endif  // MUS_CORE_FRAMES_H

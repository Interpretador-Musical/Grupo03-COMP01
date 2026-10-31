#ifndef MUS_AUDIO_LIMITER_H
#define MUS_AUDIO_LIMITER_H

#include <cmath>
#include <cstdint>

namespace mus {

// Limitador do master bus — DSP-10 (#57). Última etapa antes do dispositivo:
// nenhuma amostra sai com módulo acima de 1.0, venha de onde vier — acorde
// grande, `volume 9999`, ganho absurdo no --test-tone.
//
// Até aqui o único limite era um corte seco em ±1.0 no `renderRealTime`, e o
// caminho do `TimelinePlayer` não tinha nenhum. Corte seco transforma o pico
// em quina, e quina no sinal é harmônico agudo que o ouvido escuta como
// estalo. Aqui a curva entra suave:
//
// - Até `kSoftClipThreshold` a amostra passa intacta. O sinal normal não é
//   colorido pelo limitador.
// - Acima dele, a sobra é comprimida por tanh e se aproxima de 1.0 sem nunca
//   passar. A derivada vale 1 dos dois lados do limiar, então a curva não tem
//   quina nem ali.
//
// Header-only e sem estado: roda dentro do callback de áudio, onde não se pode
// alocar nem bloquear.
constexpr float kSoftClipThreshold = 0.8f;

inline float softClip(float amostra) {
    // NaN passaria por toda a conta e chegaria à placa como ruído. Infinito
    // não precisa de caso: tanh(inf) é 1.
    if (std::isnan(amostra)) {
        return 0.0f;
    }

    const float modulo = std::fabs(amostra);
    if (modulo <= kSoftClipThreshold) {
        return amostra;
    }

    const float folga = 1.0f - kSoftClipThreshold;
    const float comprimido =
        kSoftClipThreshold + folga * std::tanh((modulo - kSoftClipThreshold) / folga);
    return std::copysign(comprimido, amostra);
}

inline void softClipBuffer(float* amostras, std::uint32_t quantidade) {
    for (std::uint32_t i = 0; i < quantidade; ++i) {
        amostras[i] = softClip(amostras[i]);
    }
}

}  // namespace mus

#endif  // MUS_AUDIO_LIMITER_H

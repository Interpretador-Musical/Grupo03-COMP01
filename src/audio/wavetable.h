#ifndef MUS_AUDIO_WAVETABLE_H
#define MUS_AUDIO_WAVETABLE_H

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace mus {

// Look-Up Table de seno com interpolação linear e acumulador de fase em
// uint32_t (DSP-08, #47).
//
// - 2048 amostras + 1 ponto de guarda (8196 bytes): cabe inteira na cache L1d.
// - Erro máximo da interpolação linear: ~1,2e-6 (abaixo do degrau de 16 bits).
// - Fase em uint32_t: o overflow sem sinal faz o wrap-around automático em 2^32,
//   dispensando desvios condicionais (if) por amostra e garantindo que o índice
//   nunca saia do intervalo [0, kSineTableSize - 1].
// - Header-only e sem alocação dinâmica, seguindo o mesmo padrão de limiter.h.
constexpr std::uint32_t kSineTableBits = 11;
constexpr std::size_t kSineTableSize = std::size_t{1} << kSineTableBits;  // 2048
constexpr std::uint32_t kPhaseFracBits = 32u - kSineTableBits;            // 21
constexpr std::uint32_t kPhaseFracMask = (std::uint32_t{1} << kPhaseFracBits) - 1u;
constexpr float kPhaseFracScale = 1.0f / static_cast<float>(std::uint32_t{1} << kPhaseFracBits);

// SCH-11 (#61): tabela com armazenamento estático em escopo de namespace.
// Como variável `inline const`, é construída na inicialização estática do
// programa (antes de `main`), sem guarda `__cxa_guard_acquire` no callback.
inline const std::array<float, kSineTableSize + 1> kSineTable = []() {
    constexpr double kTwoPi = 6.28318530717958647692;

    std::array<float, kSineTableSize + 1> table{};

    for (std::size_t i = 0; i < kSineTableSize; ++i) {
        const double angle =
            (kTwoPi * static_cast<double>(i)) /
            static_cast<double>(kSineTableSize);

        table[i] = static_cast<float>(std::sin(angle));
    }

    // Ponto de guarda: sin(2π) == sin(0) == 0.0f. Permite ler table[idx + 1]
    // diretamente sem módulo nem branch.
    table[kSineTableSize] = 0.0f;

    return table;
}();

inline const std::array<float, kSineTableSize + 1>& sineTable() {
    return kSineTable;
}

// Consulta sin(2π * phase / 2^32) via interpolação linear.
inline float lookupSine(std::uint32_t phase) {
    const auto& table = sineTable();
    const std::size_t index = static_cast<std::size_t>(phase >> kPhaseFracBits);
    const float frac = static_cast<float>(phase & kPhaseFracMask) * kPhaseFracScale;
    const float s0 = table[index];
    const float s1 = table[index + 1];
    return s0 + frac * (s1 - s0);
}

// Converte uma frequência em Hz para o incremento inteiro de fase por amostra.
// Protegido contra entradas inválidas ou extremas (<= 0, NaN, Inf, 1e24) para
// nunca causar Undefined Behavior na conversão para uint32_t.
inline std::uint32_t frequencyToPhaseIncrement(double frequencyHz,
                                               std::uint32_t sampleRate) {
    if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0 || sampleRate == 0) {
        return 0u;
    }

    const double cyclesPerSample = frequencyHz / static_cast<double>(sampleRate);
    if (!std::isfinite(cyclesPerSample)) {
        return 0u;
    }

    // Retém apenas a parte fracionária do ciclo em [0.0, 1.0).
    double frac = cyclesPerSample - std::floor(cyclesPerSample);
    if (!(frac >= 0.0 && frac < 1.0)) {
        frac = 0.0;
    }

    constexpr double kPhaseScale = 4294967296.0;  // 2^32
    const double scaled = frac * kPhaseScale;
    if (scaled >= kPhaseScale) {
        return 0u;
    }

    return static_cast<std::uint32_t>(scaled);
}

}  // namespace mus

#endif  // MUS_AUDIO_WAVETABLE_H

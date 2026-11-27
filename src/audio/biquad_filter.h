/**
 * @file biquad_filter.h
 * @brief Filtro digital Passa-Baixa BiQuad de 2ª ordem (Audio EQ Cookbook RBJ, DSP-11 #62).
 */

#ifndef MUS_AUDIO_BIQUAD_FILTER_H
#define MUS_AUDIO_BIQUAD_FILTER_H

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mus {

/** @brief Fator de qualidade Butterworth maximamente plano (`1 / sqrt(2)`). */
constexpr float kBiquadDefaultQ = 0.70710678f;

/** @brief Limite inferior seguro para `Q` evitando degeneração numérica. */
constexpr float kBiquadMinQ = 0.1f;

/** @brief Limite superior seguro para `Q` mantendo polos bem dentro do círculo unitário. */
constexpr float kBiquadMaxQ = 10.0f;

/** @brief Menor frequência de corte ativa aceita pelo filtro passa-baixa (20 Hz). */
constexpr float kMinFilterCutoffHz = 20.0f;

/** @brief Maior frequência de corte ativa aceita pelo filtro passa-baixa (20 kHz). */
constexpr float kMaxFilterCutoffHz = 20000.0f;

/**
 * @brief Higieniza a frequência de corte em Hz (`0.0f` = desligado; ativo em `[20, min(20000, 0.49*fs)]`).
 */
inline float sanitizeFilterCutoff(float cutoffHz, std::uint32_t sampleRate) noexcept {
    if (!std::isfinite(cutoffHz) || cutoffHz <= 0.0f || sampleRate == 0u) {
        return 0.0f;
    }
    const float nyquistGuard = 0.49f * static_cast<float>(sampleRate);
    const float upperLimit = std::min(kMaxFilterCutoffHz, nyquistGuard);
    if (cutoffHz < kMinFilterCutoffHz) {
        return kMinFilterCutoffHz;
    }
    if (cutoffHz > upperLimit) {
        return upperLimit;
    }
    return cutoffHz;
}

/**
 * @brief Higieniza o fator de qualidade `Q` para `[kBiquadMinQ, kBiquadMaxQ]`.
 */
inline float sanitizeFilterQ(float q) noexcept {
    if (!std::isfinite(q) || q <= 0.0f) {
        return kBiquadDefaultQ;
    }
    return std::clamp(q, kBiquadMinQ, kBiquadMaxQ);
}

/**
 * @brief Filtro IIR BiQuad Passa-Baixa em Forma Direta II Transposta (DSP-11, #62).
 *
 * Implementa as equações biquadráticas clássicas do *Audio EQ Cookbook* de
 * Robert Bristow-Johnson (RBJ):
 *
 * \f[
 *   H(z) = \frac{b_0 + b_1 z^{-1} + b_2 z^{-2}}{1 + a_1 z^{-1} + a_2 z^{-2}}
 * \f]
 *
 * A recorrência em Forma Direta II Transposta utiliza apenas dois registradores
 * de estado (`z1_`, `z2_`), apresentando menor erro de arredondamento em ponto
 * flutuante de 32 bits em frequências graves e custo `O(1)` sem alocações.
 */
class BiquadFilter {
public:
    /** @brief Zera a memória interna de atraso (`z1_`, `z2_`) sem alterar coeficientes. */
    void resetState() noexcept {
        z1_ = 0.0f;
        z2_ = 0.0f;
    }

    /** @brief Desativa o filtro (modo bypass transparente) e limpa o estado. */
    void disable() noexcept {
        enabled_ = false;
        cutoffHz_ = 0.0f;
        b0_ = 1.0f;
        b1_ = 0.0f;
        b2_ = 0.0f;
        a1_ = 0.0f;
        a2_ = 0.0f;
        resetState();
    }

    /**
     * @brief Configura os coeficientes passa-baixa RBJ para `cutoffHz` e `q`.
     *
     * Se `cutoffHz <= 0.0f` (ou não finito), coloca o filtro em bypass. Se os
     * parâmetros higienizados coincidirem com a configuração já ativa, retorna
     * imediatamente sem recalcular seno/cosseno e preservando a continuidade de
     * fase em `z1_` e `z2_`.
     */
    void configureLowPass(std::uint32_t sampleRate,
                          float cutoffHz,
                          float q = kBiquadDefaultQ) noexcept {
        const float cleanCutoff = sanitizeFilterCutoff(cutoffHz, sampleRate);
        if (cleanCutoff == 0.0f) {
            if (enabled_) {
                disable();
            }
            return;
        }

        const float cleanQ = sanitizeFilterQ(q);
        if (enabled_ && cleanCutoff == cutoffHz_ && cleanQ == q_) {
            return;
        }

        constexpr double kTwoPi = 6.28318530717958647692;
        const double w0 =
            (kTwoPi * static_cast<double>(cleanCutoff)) / static_cast<double>(sampleRate);
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * static_cast<double>(cleanQ));

        const double a0 = 1.0 + alpha;
        const double invA0 = 1.0 / a0;
        const double oneMinusCos = 1.0 - cosW0;

        b0_ = static_cast<float>((0.5 * oneMinusCos) * invA0);
        b1_ = static_cast<float>(oneMinusCos * invA0);
        b2_ = b0_;
        a1_ = static_cast<float>((-2.0 * cosW0) * invA0);
        a2_ = static_cast<float>((1.0 - alpha) * invA0);

        cutoffHz_ = cleanCutoff;
        q_ = cleanQ;
        enabled_ = true;
    }

    /**
     * @brief Processa uma única amostra mono em Forma Direta II Transposta.
     */
    float processSample(float x) noexcept {
        if (!enabled_) {
            return x;
        }
        if (!std::isfinite(x)) {
            resetState();
            return 0.0f;
        }
        const float y = b0_ * x + z1_;
        z1_ = b1_ * x - a1_ * y + z2_;
        z2_ = b2_ * x - a2_ * y;
        return y;
    }

    /** @brief Zera resíduos subnormais na cauda de silêncio para evitar penalidade de CPU em x86. */
    void flushDenormals() noexcept {
        if (std::fabs(z1_) < 1e-20f) {
            z1_ = 0.0f;
        }
        if (std::fabs(z2_) < 1e-20f) {
            z2_ = 0.0f;
        }
    }

    /**
     * @brief Filtra um bloco contínuo de amostras mono (retorna imediatamente se em bypass).
     */
    void processBuffer(float* samples, std::uint32_t frameCount) noexcept {
        if (!enabled_) {
            return;
        }
        for (std::uint32_t i = 0; i < frameCount; ++i) {
            samples[i] = processSample(samples[i]);
        }
        flushDenormals();
    }

    bool isEnabled() const noexcept { return enabled_; }
    float cutoffHz() const noexcept { return cutoffHz_; }
    float q() const noexcept { return q_; }

private:
    float b0_ = 1.0f;
    float b1_ = 0.0f;
    float b2_ = 0.0f;
    float a1_ = 0.0f;
    float a2_ = 0.0f;

    float z1_ = 0.0f;
    float z2_ = 0.0f;

    float cutoffHz_ = 0.0f;
    float q_ = kBiquadDefaultQ;
    bool enabled_ = false;
};

}  // namespace mus

#endif  // MUS_AUDIO_BIQUAD_FILTER_H

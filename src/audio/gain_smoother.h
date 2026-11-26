/**
 * @file gain_smoother.h
 * @brief Suavização linear de ganho em tempo real para evitar cliques digitais (DSP-09, #52).
 */

#ifndef MUS_AUDIO_GAIN_SMOOTHER_H
#define MUS_AUDIO_GAIN_SMOOTHER_H

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mus {

/** @brief Duração padrão da rampa linear de transição de ganho (5 ms). */
constexpr double kGainSmoothingSeconds = 0.005;

/** @brief Teto seguro para o Volume Master final no master bus ([0.0, 1.0]). */
constexpr float kMaxMasterVolume = 1.0f;

/** @brief Teto seguro para ganhos de subgrupo Synth/Sample antes do limitador ([0.0, 4.0], ~+12 dB). */
constexpr float kMaxSubgroupGain = 4.0f;

/**
 * @brief Higieniza um multiplicador de ganho contra `NaN`, negativos e valores acima do teto.
 *
 * - `NaN` ou valores `<= 0` viram `0.0f` (evita inversão de fase ou contaminação do buffer).
 * - Valores acima de `maxGain` (incluindo `+inf`) são saturados em `maxGain`.
 *
 * @param gain Valor bruto solicitado.
 * @param maxGain Limite superior permitido para o estágio.
 * @return Multiplicador finito no intervalo `[0.0f, maxGain]`.
 */
inline float sanitizeGain(float gain, float maxGain) noexcept {
    if (std::isnan(gain) || gain <= 0.0f) {
        return 0.0f;
    }
    if (gain >= maxGain) {
        return maxGain;
    }
    return gain;
}

/**
 * @brief Interpolador linear amostra a amostra para mudanças suaves de volume (DSP-09, #52).
 *
 * Mudar um multiplicador de amplitude num degrau instantâneo multiplica a forma
 * de onda por uma descontinuidade vertical, gerando harmônicos agudos audíveis
 * como estalo (*click* / *zipper noise*). Este suavizador caminha linearmente do
 * ganho atual até o alvo ao longo de `rampFrames_` amostras (~5 ms a 44,1 kHz) e
 * crava exatamente o valor final ao término da rampa.
 *
 * Operação `O(1)` por amostra, sem alocação dinâmica nem bloqueios (real-time safe).
 */
class GainSmoother {
public:
    explicit GainSmoother(std::uint32_t sampleRate, float initialGain = 1.0f) noexcept
        : rampFrames_(std::max<std::uint32_t>(
              1u, static_cast<std::uint32_t>(
                      kGainSmoothingSeconds * static_cast<double>(sampleRate)))),
          current_(initialGain),
          target_(initialGain) {}

    /**
     * @brief Define o ganho imediatamente, sem rampa (usado na inicialização antes do áudio).
     */
    void snapTo(float value) noexcept {
        current_ = value;
        target_ = value;
        step_ = 0.0f;
        framesRemaining_ = 0u;
    }

    /**
     * @brief Atualiza o ganho alvo, iniciando uma rampa linear a partir do valor atual se mudou.
     */
    void setTarget(float newTarget) noexcept {
        if (newTarget == target_) {
            return;
        }
        target_ = newTarget;
        if (std::fabs(target_ - current_) <= 1e-6f) {
            current_ = target_;
            step_ = 0.0f;
            framesRemaining_ = 0u;
            return;
        }
        framesRemaining_ = rampFrames_;
        step_ = (target_ - current_) / static_cast<float>(rampFrames_);
    }

    /**
     * @brief Avança um frame na rampa e devolve o ganho instantâneo.
     */
    float next() noexcept {
        if (framesRemaining_ == 0u) {
            return current_;
        }
        --framesRemaining_;
        if (framesRemaining_ == 0u) {
            current_ = target_;
            step_ = 0.0f;
        } else {
            current_ += step_;
        }
        return current_;
    }

    /**
     * @brief Avança `frames` amostras de uma só vez (usado em atalhos de silêncio).
     */
    void advance(std::uint32_t frames) noexcept {
        if (framesRemaining_ == 0u || frames == 0u) {
            return;
        }
        if (frames >= framesRemaining_) {
            current_ = target_;
            step_ = 0.0f;
            framesRemaining_ = 0u;
        } else {
            current_ += step_ * static_cast<float>(frames);
            framesRemaining_ -= frames;
        }
    }

    /**
     * @brief Aplica o ganho suavizado sobre um bloco contínuo de amostras mono.
     *
     * Possui atalhos rápidos (*fast-path*) quando a rampa está concluída:
     * - `current_ == 1.0f`: não percorre o buffer (preserva saída bit a bit).
     * - `current_ == 0.0f`: zera o bloco de uma vez.
     * - constante qualquer: laço linear simples vetorizável pelo compilador.
     */
    void processBuffer(float* samples, std::uint32_t frameCount) noexcept {
        if (framesRemaining_ == 0u) {
            if (current_ == 1.0f) {
                return;
            }
            if (current_ == 0.0f) {
                std::fill(samples, samples + frameCount, 0.0f);
                return;
            }
            const float gain = current_;
            for (std::uint32_t i = 0; i < frameCount; ++i) {
                samples[i] *= gain;
            }
            return;
        }

        for (std::uint32_t i = 0; i < frameCount; ++i) {
            samples[i] *= next();
        }
    }

    float current() const noexcept { return current_; }
    float target() const noexcept { return target_; }
    bool isRamping() const noexcept { return framesRemaining_ > 0u; }
    std::uint32_t rampFrames() const noexcept { return rampFrames_; }

private:
    std::uint32_t rampFrames_;
    float current_ = 1.0f;
    float target_ = 1.0f;
    float step_ = 0.0f;
    std::uint32_t framesRemaining_ = 0u;
};

}  // namespace mus

#endif  // MUS_AUDIO_GAIN_SMOOTHER_H

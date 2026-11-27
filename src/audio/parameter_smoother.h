/**
 * @file parameter_smoother.h
 * @brief Suavizador exponencial de 1 polo (One-Pole Smoother) contra Zipper Noise (DSP-12, #67).
 */

#ifndef MUS_AUDIO_PARAMETER_SMOOTHER_H
#define MUS_AUDIO_PARAMETER_SMOOTHER_H

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mus {

/** @brief Constante de tempo padrão do suavizador de 1 polo (5 ms). */
constexpr double kDefaultOnePoleTauSeconds = 0.005;

/**
 * @brief Filtro IIR passa-baixa de 1ª ordem para suavização contínua de parâmetros (DSP-12, #67).
 *
 * Em sintetizadores digitais, alterar parâmetros contínuos (como frequência de corte
 * de um filtro LPF, ressonância `Q` ou ganho de oscilador) em degraus discretos a cada
 * bloco de callback produz descontinuidades audíveis como *zipper noise* ou estalos
 * (*clicks/pops*). Este suavizador aplica a recorrência clássica de 1 polo:
 *
 * \f[
 *   y[n] = y[n-1] + (1 - a)\,(x_{\text{alvo}} - y[n-1]), \qquad a = e^{-1 / (\tau \cdot f_s)}
 * \f]
 *
 * Propriedades garantidas:
 * - Resposta assintótica suave (`C^0`), sem *overshoot* mesmo quando o alvo muda no meio da transição;
 * - Convergência exata com *snap* quando `|alvo - atual| <= epsilon_`, eliminando números
 *   subnormais (*denormals*) e zerando o recálculo de coeficientes trigonométricos em regime permanente;
 * - Avanço fechado em `O(1)` para saltos de `k` frames (`a^k`), sem alocação dinâmica nem bloqueios.
 */
class OnePoleSmoother {
public:
    explicit OnePoleSmoother(std::uint32_t sampleRate,
                             double tauSeconds = kDefaultOnePoleTauSeconds,
                             float initialValue = 0.0f,
                             float snapEpsilon = 1e-4f) noexcept
        : coeff_((sampleRate > 0u && tauSeconds > 0.0)
                     ? static_cast<float>(
                           std::exp(-1.0 / (tauSeconds * static_cast<double>(sampleRate))))
                     : 0.0f),
          oneMinusCoeff_(1.0f - coeff_),
          epsilon_(std::max(1e-7f, snapEpsilon)),
          current_(initialValue),
          target_(initialValue) {}

    /**
     * @brief Crava imediatamente o valor atual e o alvo, sem transição (usado antes do 1º bloco).
     */
    void snapTo(float value) noexcept {
        if (!std::isfinite(value)) {
            return;
        }
        current_ = value;
        target_ = value;
        smoothing_ = false;
    }

    /**
     * @brief Define um novo valor alvo, iniciando suavização exponencial a partir do estado atual.
     */
    void setTarget(float newTarget) noexcept {
        if (!std::isfinite(newTarget) || newTarget == target_) {
            return;
        }
        target_ = newTarget;
        if (std::fabs(target_ - current_) <= epsilon_ || coeff_ == 0.0f) {
            current_ = target_;
            smoothing_ = false;
            return;
        }
        smoothing_ = true;
    }

    /**
     * @brief Avança uma amostra na curva exponencial de 1 polo e retorna o valor suavizado.
     */
    float next() noexcept {
        if (!smoothing_) {
            return current_;
        }
        current_ += oneMinusCoeff_ * (target_ - current_);
        if (std::fabs(target_ - current_) <= epsilon_) {
            current_ = target_;
            smoothing_ = false;
        }
        return current_;
    }

    /**
     * @brief Avança `frames` amostras em `O(1)` via solução fechada `alvo + a^k * (atual - alvo)`.
     */
    void advance(std::uint32_t frames) noexcept {
        if (!smoothing_ || frames == 0u) {
            return;
        }
        const float factor = std::pow(coeff_, static_cast<float>(frames));
        current_ = target_ + factor * (current_ - target_);
        if (std::fabs(target_ - current_) <= epsilon_) {
            current_ = target_;
            smoothing_ = false;
        }
    }

    float current() const noexcept { return current_; }
    float target() const noexcept { return target_; }
    bool isSmoothing() const noexcept { return smoothing_; }
    float poleCoefficient() const noexcept { return coeff_; }

private:
    float coeff_ = 0.0f;
    float oneMinusCoeff_ = 1.0f;
    float epsilon_ = 1e-4f;
    float current_ = 0.0f;
    float target_ = 0.0f;
    bool smoothing_ = false;
};

}  // namespace mus

#endif  // MUS_AUDIO_PARAMETER_SMOOTHER_H

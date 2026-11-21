#include "core/timeline_player.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/frames.h"

namespace mus {
namespace {

constexpr double kTwoPi = 6.283185307179586;

}  // namespace

void TimelinePlayer::load(std::vector<SoundEvent> eventos) {
    eventos_ = std::move(eventos);
    cursor_ = 0;
    framesRenderizados_ = 0;
    fase_ = 0.0;
}

double TimelinePlayer::totalDuration() const {
    double fim = 0.0;
    for (const SoundEvent& evento : eventos_) {
        fim = std::max(fim, evento.endTime());
    }
    return fim;
}

void TimelinePlayer::render(float* output, std::uint32_t frameCount) {
    for (std::uint32_t i = 0; i < frameCount; ++i) {
        // A comparação é feita em frames, com a mesma regra de arredondamento
        // do AudioEngine (core/frames.h, MAT-08 #45). Comparar em segundos
        // (frames / taxa >= startTime) disparava no primeiro frame >= início,
        // ou seja, arredondava para cima, e a mesma Timeline caía num frame
        // diferente conforme o caminho de reprodução.
        const std::uint64_t frame = framesRenderizados_;

        // O cursor só anda para frente: a Timeline é percorrida uma única vez
        // do início ao fim da reprodução. Cada evento é visitado no máximo
        // uma vez ao longo de toda a chamada, então isto é O(1) amortizado.
        while (cursor_ < eventos_.size() &&
               frame >= segundosParaFrame(eventos_[cursor_].endTime(), sampleRate_)) {
            ++cursor_;
        }

        float amostra = 0.0f;
        if (cursor_ < eventos_.size() &&
            frame >= segundosParaFrame(eventos_[cursor_].startTime, sampleRate_)) {
            const SoundEvent& evento = eventos_[cursor_];
            const double incremento = (kTwoPi * evento.frequency) / sampleRate_;
            amostra = static_cast<float>(std::sin(fase_)) * evento.volume;

            // Mantém a fase dentro de [0, 2π) pelo mesmo motivo de
            // AudioEngine::renderSine: não perder precisão com o tempo.
            fase_ += incremento;
            if (fase_ >= kTwoPi) {
                fase_ -= kTwoPi;
            }
        }
        // Se o frame ainda não chegou no startTime do evento do cursor (um
        // gap real, que não deveria ocorrer vindo do Interpretador) ou o
        // cursor já passou do último evento: silêncio, sem mexer na fase —
        // evita um salto de fase quando a próxima nota realmente começar.

        output[i] = amostra;
        ++framesRenderizados_;
    }
}

}  // namespace mus

#include "audio/audio_engine.h"

#include <cmath>
#include <thread>

#include "cli/logger.h"

// Esta é a única unidade de tradução que instancia o miniaudio.
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

namespace mus {
namespace {

constexpr double kTwoPi = 6.283185307179586;

}  // namespace

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() {
    stop();
    if (initialized_) {
        ma_device_uninit(device_);
        initialized_ = false;
    }
    delete device_;
    device_ = nullptr;
}

bool AudioEngine::init() {
    if (initialized_) {
        return true;
    }

    device_ = new ma_device();

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 1;  // mono
    config.sampleRate = kSampleRate;
    config.periodSizeInFrames = kPeriodSizeInFrames;
    config.dataCallback = &AudioEngine::dataCallback;
    config.pUserData = this;

    if (ma_device_init(nullptr, &config, device_) != MA_SUCCESS) {
        LOG_ERROR << "falha ao inicializar o dispositivo de áudio";
        delete device_;
        device_ = nullptr;
        return false;
    }

    initialized_ = true;
    LOG_DEBUG << "dispositivo de áudio pronto: " << kSampleRate
              << " Hz, mono, buffer de " << kPeriodSizeInFrames << " frames";
    return true;
}

bool AudioEngine::start() {
    if (!initialized_ && !init()) {
        return false;
    }
    if (running_) {
        return true;
    }

    currentFrame_ = 0;
    hasPendingEvent_ = false;
    sufferedUnderrun_.store(false, std::memory_order_relaxed);
    voices_.fill(Voice{});

    if (ma_device_start(device_) != MA_SUCCESS) {
        LOG_ERROR << "falha ao iniciar a reprodução de áudio";
        return false;
    }

    running_ = true;
    // Âncora de waitUntilFinished(): a partir daqui a duração do programa
    // carregado é medida contra o relógio, não contra voltas de sleep.
    playbackStart_ = std::chrono::steady_clock::now();
    LOG_DEBUG << "reprodução iniciada";
    return true;
}

void AudioEngine::stop() {
    if (!running_) {
        return;
    }
    ma_device_stop(device_);
    running_ = false;
    LOG_DEBUG << "reprodução interrompida";
}

void AudioEngine::setToneFrequency(double hz) {
    frequency_.store(hz, std::memory_order_relaxed);
}

void AudioEngine::setGain(float gain) {
    gain_.store(gain, std::memory_order_relaxed);
}

void AudioEngine::loadTimeline(const Timeline& timeline) {
    player_.load(std::vector<SoundEvent>(timeline.events().begin(),
                                         timeline.events().end()));
    usandoPrograma_ = true;
}

void AudioEngine::waitUntilFinished() {
    // Agora usa a variável expectedDuration_ injetada, pois o TimelinePlayer será bypassado
    const double duracao = (expectedDuration_ > 0.0) ? expectedDuration_ : player_.totalDuration();
    const auto alvo = playbackStart_ +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(duracao));

    // Dorme em pequenos ciclos para observar a bandeira do áudio em tempo real
    while (std::chrono::steady_clock::now() < alvo) {
        if (checkAndClearUnderrun()) {
            LOG_WARN << "buffer underrun: perca temporaria de sincronismo";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void AudioEngine::dataCallback(ma_device* device, void* output, const void* input, std::uint32_t frameCount) {
    (void)input;
    auto* engine = static_cast<AudioEngine*>(device->pUserData);
    if (engine == nullptr) {
        return;
    }

    // Roteamento de prioridade: se houver RingBuffer, usa ele (DSP-03)
    if (engine->ringBuffer_ != nullptr) {
        engine->renderRealTime(static_cast<float*>(output), frameCount);
    } else if (engine->usandoPrograma_) {
        engine->player_.render(static_cast<float*>(output), frameCount);
    } else {
        engine->renderSine(static_cast<float*>(output), frameCount);
    }
}

void AudioEngine::renderSine(float* output, std::uint32_t frameCount) {
    // Nada de alocação, lock ou I/O aqui dentro: esta função roda na thread de
    // áudio e um atraso vira estalo audível.
    const double increment =
        (kTwoPi * frequency_.load(std::memory_order_relaxed)) / kSampleRate;
    const float gain = gain_.load(std::memory_order_relaxed);

    for (std::uint32_t i = 0; i < frameCount; ++i) {
        output[i] = static_cast<float>(std::sin(phase_)) * gain;

        // Mantém a fase dentro de [0, 2π) para não perder precisão com o tempo.
        phase_ += increment;
        if (phase_ >= kTwoPi) {
            phase_ -= kTwoPi;
        }
    }
}

void AudioEngine::setRingBuffer(SpscRingBuffer<SoundEvent>* buffer) {
    ringBuffer_ = buffer;
}

void AudioEngine::setExpectedDuration(double durationSeconds) {
    expectedDuration_ = durationSeconds;
}

bool AudioEngine::checkAndClearUnderrun() {
    return sufferedUnderrun_.exchange(false, std::memory_order_relaxed);
}

AudioEngine::Voice* AudioEngine::allocateVoice() {
    Voice* weakest = &voices_[0];
    for (Voice& v : voices_) {
        if (v.envState == EnvState::Idle) {
            return &v;                        // achou uma livre
        }
        if (v.envLevel < weakest->envLevel) {
            weakest = &v;                     // guarda a mais fraca
        }
    }
    return weakest;                           // todas ocupadas: rouba a mais fraca
}

void AudioEngine::renderRealTime(float* output, std::uint32_t frameCount) {
    constexpr std::uint64_t kAttackFrames =
        static_cast<std::uint64_t>(0.005 * kSampleRate);
    constexpr std::uint64_t kReleaseFrames =
        static_cast<std::uint64_t>(0.005 * kSampleRate);
    const float attackRate  = 1.0f / static_cast<float>(kAttackFrames);
    const float releaseRate = 1.0f / static_cast<float>(kReleaseFrames);

    const std::uint64_t expectedFrames =
        static_cast<std::uint64_t>(std::llround(expectedDuration_ * kSampleRate));

    for (std::uint32_t i = 0; i < frameCount; ++i) {
        // ---- Bloco A: disparar eventos vencidos ----
        for (;;) {
            if (!hasPendingEvent_) {
                if (ringBuffer_ == nullptr || !ringBuffer_->pop(&pendingEvent_)) {
                    bool hasActiveVoices = false;
                    for (const Voice& v : voices_) {
                        if (v.envState != EnvState::Idle) {
                            hasActiveVoices = true;
                            break;
                        }
                    }

                    if (!hasActiveVoices) {
                        if (ringBuffer_ == nullptr ||
                            (expectedDuration_ > 0.0 && currentFrame_ < expectedFrames)) {
                            sufferedUnderrun_.store(true, std::memory_order_relaxed);
                        }

                        for (std::uint32_t j = i; j < frameCount; ++j) {
                            output[j] = 0.0f;
                            ++currentFrame_;
                        }
                        return;
                    }
                    break;
                }
                pendingStartFrame_ = static_cast<std::uint64_t>(
                    std::llround(pendingEvent_.startTime * kSampleRate));
                hasPendingEvent_ = true;
            }
            if (currentFrame_ < pendingStartFrame_) {
                break;                                // ainda não é a hora
            }

            const auto total = static_cast<std::uint64_t>(
                pendingEvent_.duration * kSampleRate);

            Voice* v = allocateVoice();
            v->frequency = pendingEvent_.frequency;
            v->volume = pendingEvent_.volume;
            v->framesRemaining = (total > kReleaseFrames) ? total - kReleaseFrames : 0;
            v->envState = EnvState::Attack;
            hasPendingEvent_ = false;         // volta ao topo do for(;;): próximo evento
        }

        // ---- Bloco B: envelope + oscilador de cada voz, somados ----
        float mix = 0.0f;
        for (Voice& v : voices_) {
            if (v.envState == EnvState::Idle) {
                continue;
            }

            if (v.envState != EnvState::Release) {
                if (v.framesRemaining == 0) {
                    v.envState = EnvState::Release;
                } else {
                    --v.framesRemaining;
                    if (v.envState == EnvState::Attack) {
                        v.envLevel += attackRate;
                        if (v.envLevel >= 1.0f) {
                            v.envLevel = 1.0f;
                            v.envState = EnvState::Sustain;
                        }
                    }
                }
            }

            if (v.envState == EnvState::Release) {
                v.envLevel -= releaseRate;
                if (v.envLevel <= 0.0f) {
                    v.envLevel = 0.0f;
                    v.envState = EnvState::Idle;
                    continue;
                }
            }

            mix += static_cast<float>(std::sin(v.phase)) * v.volume * v.envLevel;
            v.phase += (kTwoPi * v.frequency) / kSampleRate;
            if (v.phase >= kTwoPi) {
                v.phase -= kTwoPi;
            }
        }

        // ---- Bloco C: limitador simples ----
        output[i] = std::fmax(-1.0f, std::fmin(1.0f, mix));
        ++currentFrame_;
    }
}

}  // namespace mus

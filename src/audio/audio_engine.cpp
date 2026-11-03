#include "audio/audio_engine.h"

#include <cmath>
#include <thread>

#include "audio/limiter.h"
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

void AudioEngine::setMaxVoices(int limit) {
    if (limit < 1) {
        limit = 1;
    } else if (limit > kMaxVoices) {
        limit = kMaxVoices;
    }
    const int oldLimit = maxVoices_.exchange(limit, std::memory_order_relaxed);
    for (int i = limit; i < oldLimit; ++i) {
        voices_[i].envState = EnvState::Idle;
        voices_[i].envLevel = 0.0f;
        voices_[i].fadeOutFramesRemaining = 0;
        voices_[i].fadeOutLevel = 0.0f;
    }
}

int AudioEngine::maxVoices() const {
    return maxVoices_.load(std::memory_order_relaxed);
}

int AudioEngine::activeVoiceCount() const {
    int count = 0;
    const int limit = maxVoices_.load(std::memory_order_relaxed);
    for (int i = 0; i < limit; ++i) {
        if (voices_[i].envState != EnvState::Idle || voices_[i].fadeOutFramesRemaining > 0) {
            ++count;
        }
    }
    return count;
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
            LOG_WARN << "buffer underrun: perda temporária de sincronismo";
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
    engine->render(static_cast<float*>(output), frameCount);
}

void AudioEngine::render(float* output, std::uint32_t frameCount) {
    // Roteamento de prioridade: se houver RingBuffer, usa ele (DSP-03)
    if (ringBuffer_ != nullptr) {
        renderRealTime(output, frameCount);
    } else if (usandoPrograma_) {
        player_.render(output, frameCount);
    } else {
        renderSine(output, frameCount);
    }

    // Saída mono: uma amostra por frame.
    softClipBuffer(output, frameCount);
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

void AudioEngine::setExpectedDuration(double durationSeconds) {
    expectedDuration_ = durationSeconds;
}

void AudioEngine::setRingBuffer(SpscRingBuffer<SoundEvent>* buffer) {
    ringBuffer_ = buffer;
}

bool AudioEngine::checkAndClearUnderrun() {
    return sufferedUnderrun_.exchange(false, std::memory_order_relaxed);
}

bool AudioEngine::isBetterToSteal(const Voice& candidate, const Voice& currentBest) {
    const bool candInRelease = (candidate.envState == EnvState::Release);
    const bool bestInRelease = (currentBest.envState == EnvState::Release);

    // 1. Critério de Aceitação (DoD): preferir oscilador em fase de "release"
    if (candInRelease && !bestInRelease) {
        return true;
    }
    if (!candInRelease && bestInRelease) {
        return false;
    }

    // 2. Ambas em release OU ambas ativas:
    // Critério de Aceitação (DoD): preferir oscilador com menor amplitude efetiva (volume * envLevel)
    // Fora do release, vale o nível alvo da nota: no ataque o envLevel ainda
    // está subindo e faria a nota mais nova parecer a mais fraca.
    const float candAmp = candInRelease ? candidate.volume * candidate.envLevel : candidate.volume;
    const float bestAmp = bestInRelease ? currentBest.volume * currentBest.envLevel : currentBest.volume;
    constexpr float kEpsilon = 1e-4f;

    if (candAmp < bestAmp - kEpsilon) {
        return true;
    }
    if (candAmp > bestAmp + kEpsilon) {
        return false;
    }

    // 3. Especificação do Requisito: em empate de amplitude, rouba a voz mais antiga (menor startFrame)
    return candidate.startFrame < currentBest.startFrame;
}

AudioEngine::Voice* AudioEngine::allocateVoice() {
    const int limit = maxVoices_.load(std::memory_order_relaxed);

    // 1. Procura primeiro uma voz totalmente livre/ociosa (Idle e sem cauda residual)
    for (int i = 0; i < limit; ++i) {
        if (voices_[i].envState == EnvState::Idle && voices_[i].fadeOutFramesRemaining == 0) {
            return &voices_[i];
        }
    }

    // 1b. Se todas as ociosas têm fade-out residual, reaproveita a ociosa
    for (int i = 0; i < limit; ++i) {
        if (voices_[i].envState == EnvState::Idle) {
            return &voices_[i];
        }
    }

    // 2. Todas ocupadas: algoritmo de Voice Stealing seguro (DSP-07 #42)
    // Voz alocada neste mesmo frame (nota de acorde) não entra na disputa.
    Voice* best = nullptr;
    for (int i = 0; i < limit; ++i) {
        if (voices_[i].startFrame == currentFrame_ && voices_[i].envState == EnvState::Attack) {
            continue;
        }
        if (best == nullptr || isBetterToSteal(voices_[i], *best)) {
            best = &voices_[i];
        }
    }
    return best != nullptr ? best : &voices_[0];
}

void AudioEngine::renderRealTime(float* output, std::uint32_t frameCount) {
    constexpr std::uint64_t kAttackFrames =
        static_cast<std::uint64_t>(0.005 * kSampleRate);
    constexpr std::uint64_t kReleaseFrames =
        static_cast<std::uint64_t>(0.005 * kSampleRate);
    constexpr std::uint32_t kFadeOutFrames =
        static_cast<std::uint32_t>(0.002 * kSampleRate); // 2 ms de fade-out anti-clique (DSP-07)

    const float attackRate  = 1.0f / static_cast<float>(kAttackFrames);
    const float releaseRate = 1.0f / static_cast<float>(kReleaseFrames);

    const std::uint64_t expectedFrames =
        static_cast<std::uint64_t>(std::llround(expectedDuration_ * kSampleRate));

    const int voiceLimit = maxVoices_.load(std::memory_order_relaxed);

    for (std::uint32_t i = 0; i < frameCount; ++i) {
        // ---- Bloco A: disparar eventos vencidos ----
        for (;;) {
            if (!hasPendingEvent_) {
                if (!ringBuffer_->pop(&pendingEvent_)) {
                    bool hasActiveVoices = false;
                    for (int vi = 0; vi < voiceLimit; ++vi) {
                        const Voice& v = voices_[vi];
                        if (v.envState != EnvState::Idle || v.fadeOutFramesRemaining > 0) {
                            hasActiveVoices = true;
                            break;
                        }
                    }

                    if (!hasActiveVoices) {
                        if (expectedDuration_ > 0.0 && currentFrame_ < expectedFrames) {
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

            // Segurança acústica (NFR DSP-07): Se a voz já estava soando, aplica
            // um rápido fade-out de milissegundos para evitar cliques digitais.
            if (v->envState != EnvState::Idle && v->envLevel > 0.0001f) {
                const std::uint32_t fadeFrames = kFadeOutFrames;
                v->fadeOutPhase = v->phase;
                v->fadeOutFrequency = v->frequency;
                v->fadeOutVolume = v->volume;
                v->fadeOutLevel = v->envLevel;
                v->fadeOutFramesRemaining = fadeFrames;
                v->fadeOutStep = v->fadeOutLevel / static_cast<float>(fadeFrames);
            }

            v->frequency = pendingEvent_.frequency;
            v->volume = pendingEvent_.volume;
            v->framesRemaining = (total > kReleaseFrames) ? total - kReleaseFrames : 0;
            v->envLevel = 0.0f;
            v->envState = EnvState::Attack;
            v->startFrame = currentFrame_;
            v->phase = 0.0;
            hasPendingEvent_ = false;         // volta ao topo do for(;;): próximo evento
        }

        // ---- Bloco B: envelope + oscilador de cada voz, somados ----
        float mix = 0.0f;
        for (int vi = 0; vi < voiceLimit; ++vi) {
            Voice& v = voices_[vi];

            // Renderiza cauda de fade-out da voz anterior roubada (DSP-07)
            if (v.fadeOutFramesRemaining > 0) {
                mix += static_cast<float>(std::sin(v.fadeOutPhase)) * v.fadeOutVolume * v.fadeOutLevel;
                v.fadeOutPhase += (kTwoPi * v.fadeOutFrequency) / kSampleRate;
                if (v.fadeOutPhase >= kTwoPi) {
                    v.fadeOutPhase -= kTwoPi;
                }
                v.fadeOutLevel -= v.fadeOutStep;
                --v.fadeOutFramesRemaining;
                if (v.fadeOutFramesRemaining == 0 || v.fadeOutLevel <= 0.0f) {
                    v.fadeOutLevel = 0.0f;
                    v.fadeOutFramesRemaining = 0;
                }
            }

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

        // ---- Bloco C: saída da mixagem ----
        // Sem corte aqui: o limite fica no master bus, em render() (DSP-10,
        // #57). Cortar em ±1.0 antes dele achataria o pico em quina, e o
        // limitador suave receberia o sinal já estragado.
        output[i] = mix;
        ++currentFrame_;
    }
}

}  // namespace mus

#include "audio/audio_engine.h"
#include "audio/wavetable.h"

#include <cmath>
#include <thread>

#include "audio/limiter.h"
#include "cli/logger.h"
#include "audio/sample_bank.h"
#include "core/frames.h"
#include <algorithm>

// Esta é a única unidade de tradução que instancia o miniaudio.
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

namespace mus {

AudioEngine::AudioEngine(Saida saida) : saida_(saida) {
    // SCH-11: inicializa a wavetable antes de iniciar a thread de audio.
    // Evita que a primeira construcao ocorra dentro do callback.
    (void)sineTable();
}

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

    // Sem contexto explícito, o miniaudio tenta cada backend do sistema e cai
    // no null por último (ma_device_init_ex). Com Saida::Nula, vai direto a ele.
    ma_result resultado;
    if (saida_ == Saida::Nula) {
        const ma_backend nulo = ma_backend_null;
        resultado = ma_device_init_ex(&nulo, 1, nullptr, &config, device_);
    } else {
        resultado = ma_device_init(nullptr, &config, device_);
    }

    if (resultado != MA_SUCCESS) {
        LOG_ERROR << "falha ao inicializar o dispositivo de áudio";
        delete device_;
        device_ = nullptr;
        return false;
    }

    initialized_ = true;
    LOG_DEBUG << "dispositivo de áudio pronto: "
              << ma_get_backend_name(device_->pContext->backend) << ", " << kSampleRate
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
    framesRenderizados_.store(0, std::memory_order_relaxed);
    voices_.fill(Voice{});
    vozesAtivas_ = false;
    fimDoSilencio_ = 0;

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

bool AudioEngine::setMaxVoices(int limit) {
    // running_ só é escrito por start() e stop(), na thread que também chama
    // esta função: ler aqui não disputa com ninguém.
    if (running_) {
        return false;
    }
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
    return true;
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

    framesRenderizados_.fetch_add(frameCount, std::memory_order_relaxed);
}

void AudioEngine::renderSine(float* output, std::uint32_t frameCount) {
    // Nada de alocação, lock ou I/O aqui dentro: esta função roda na thread de
    // áudio e um atraso vira estalo audível.
    const std::uint32_t increment = frequencyToPhaseIncrement(
        frequency_.load(std::memory_order_relaxed), kSampleRate);
    const float gain = gain_.load(std::memory_order_relaxed);

    for (std::uint32_t i = 0; i < frameCount; ++i) {
        output[i] = lookupSine(phase_) * gain;
        phase_ += increment;
    }
}

void AudioEngine::setExpectedDuration(double durationSeconds) {
    expectedDuration_ = durationSeconds;
}

void AudioEngine::setRingBuffer(SpscRingBuffer<SoundEvent>* buffer) {
    ringBuffer_ = buffer;
}

void AudioEngine::setSampleBank(const SampleBank* bank) {
    sampleBank_ = bank;
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
    // Nome próprio: `kEpsilon` já é a tolerância de tempo e frequência de
    // core/pitch.h (1e-9), e esta é outra, de amplitude.
    constexpr float kEmpateDeAmplitude = 1e-4f;

    if (candAmp < bestAmp - kEmpateDeAmplitude) {
        return true;
    }
    if (candAmp > bestAmp + kEmpateDeAmplitude) {
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
    const std::uint64_t expectedFrames =
        static_cast<std::uint64_t>(std::llround(expectedDuration_ * kSampleRate));

    const int voiceLimit = maxVoices_.load(std::memory_order_relaxed);

    for (std::uint32_t i = 0; i < frameCount; ++i) {
        // ---- Bloco A: disparar eventos vencidos ----
        for (;;) {
            if (!hasPendingEvent_) {
                if (!ringBuffer_->pop(&pendingEvent_)) {
                    // Uma pausa descartada no fim da música não ocupa voz,
                    // mas o silêncio dela é intencional: não é underrun.
                    const bool hasActiveVoices =
                        vozesAtivas_ || currentFrame_ < fimDoSilencio_;

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
                pendingStartFrame_ =
                    segundosParaFrame(pendingEvent_.startTime, kSampleRate);
                hasPendingEvent_ = true;
            }
            if (currentFrame_ < pendingStartFrame_) {
                break;                                // ainda não é a hora
            }

            // O fim também sai do instante absoluto (MAT-08, #45), não de
            // duration * taxa truncado: assim o fim de uma nota cai no mesmo
            // frame em que começa a seguinte, sem gap nem sobreposição de um
            // frame. A conta é relativa ao frame agendado, não ao atual, para
            // que um disparo atrasado (underrun) não estique a nota.
            const std::uint64_t fimFrame =
                segundosParaFrame(pendingEvent_.endTime(), kSampleRate);
            const std::uint64_t total = fimFrame > pendingStartFrame_
                                            ? fimFrame - pendingStartFrame_
                                            : 0;

            // Eventos de sample precisam apontar para um buffer valido.
            // A consulta nao carrega arquivos e nao realiza alocacoes.
            const AudioBuffer* resolvedSample = nullptr;
            if (pendingEvent_.type == SoundEventType::Sample) {
                resolvedSample =
                    sampleBank_ != nullptr
                        ? sampleBank_->get(pendingEvent_.sampleId)
                        : nullptr;

                if (resolvedSample == nullptr || resolvedSample->frameCount == 0 ||
                    resolvedSample->sampleRate == 0) {
                    hasPendingEvent_ = false;
                    continue;
                }
            }

            // Evento silencioso (SCH-12, #66): a pausa chega como Synth de
            // 0 Hz e volume 0, e uma voz de volume 0 só somaria zero. Ocupar
            // uma voz com ele custava o envelope e o oscilador a cada amostra,
            // impedia o atalho de silêncio do Bloco B e, com as vozes cheias,
            // ainda roubava uma voz audível. `!(volume > 0)` também descarta
            // NaN, que contaminaria a mixagem inteira.
            const bool silencioso =
                !(pendingEvent_.volume > 0.0f) ||
                (pendingEvent_.type == SoundEventType::Synth &&
                 pendingEvent_.frequency == 0.0);
            if (silencioso) {
                fimDoSilencio_ = std::max(fimDoSilencio_, fimFrame);
                hasPendingEvent_ = false;
                continue;
            }

            Voice* v = allocateVoice();
            vozesAtivas_ = true;

            // Preserva o estado da voz anterior para o fade-out.
            if (v->envState != EnvState::Idle && v->envLevel > 0.0001f) {
                v->fadeOutType = v->type;
                v->fadeOutPhase = v->phase;
                v->fadeOutPhaseIncrement = v->phaseIncrement;
                v->fadeOutFrequency = v->frequency;
                v->fadeOutSampleId = v->sampleId;
                v->fadeOutSampleBuffer = v->sampleBuffer;
                v->fadeOutSamplePlaybackRate = v->samplePlaybackRate;
                v->fadeOutSampleCursor = v->sampleCursor;

                v->fadeOutVolume = v->volume;
                v->fadeOutLevel = v->envLevel;
                v->fadeOutFramesRemaining = kFadeOutFrames;
                v->fadeOutStep =
                    v->fadeOutLevel / static_cast<float>(kFadeOutFrames);
            }

            // Inicializa a nova fonte sem apagar o estado do fade-out.
            v->type = pendingEvent_.type;

            v->frequency = pendingEvent_.frequency;
            v->phaseIncrement =
                (pendingEvent_.type == SoundEventType::Synth)
                    ? frequencyToPhaseIncrement(pendingEvent_.frequency, kSampleRate)
                    : 0u;
            v->sampleId = pendingEvent_.sampleId;
            v->sampleBuffer = resolvedSample;
            v->samplePlaybackRate =
                resolvedSample != nullptr
                    ? static_cast<double>(resolvedSample->sampleRate) /
                          static_cast<double>(kSampleRate)
                    : 1.0;
            v->sampleCursor = 0.0;

            v->volume = pendingEvent_.volume;

            // Duracao da nova voz, descontando o tempo de release.
            // Adapta o envelope à duração da nota.
            // Para notas longas, preserva ataque e release de 5 ms.
            // Para notas curtas, divide os frames disponíveis entre eles.
            const std::uint64_t attackFrames =
                (total >= 2)
                    ? std::min(kAttackFrames, total / 2)
                    : total;

            const std::uint64_t releaseFrames =
                std::min(kReleaseFrames, total - attackFrames);

            v->framesRemaining = total - releaseFrames;

            v->attackRate = (attackFrames > 0)
                ? 1.0f / static_cast<float>(attackFrames)
                : 0.0f;

            v->releaseRate = (releaseFrames > 0)
                ? 1.0f / static_cast<float>(releaseFrames)
                : 0.0f;

            // Reinicia o envelope e a fase da nova voz.
            v->envLevel = 0.0f;
            v->envState = EnvState::Attack;
            v->startFrame = currentFrame_;
            
            // Em notas sintetizadas de 1 ou 2 frames, iniciar em fase
            // zero produziria silencio: a primeira amostra coincide
            // com sin(0), e a seguinte pode coincidir com o fim do release.
            // Em fase uint32_t (DSP-08), 0x40000000u equivale a 1/4 de ciclo (sin = 1.0).
            const bool notaSintetizadaMuitoCurta =
                v->type == SoundEventType::Synth &&
                total > 0 && total <= 2;

            v->phase = notaSintetizadaMuitoCurta
                ? 0x40000000u
                : 0u;

            // O evento foi agendado e pode ser removido da pendencia.
            hasPendingEvent_ = false;

        }

        // ---- Atalho do silêncio (SCH-12, #66) ----
        // Sem voz soando e com o próximo evento ainda no futuro, cada frame
        // até ele seria um zero: o Bloco A só repetiria "ainda não é a hora",
        // e o Bloco B visitaria todas as vozes paradas para somar nada. Aqui
        // o trecho sai de uma vez, com o mesmo resultado amostra a amostra.
        // Só vale com um evento pendente: com a fila vazia, um evento que
        // chegasse no meio do bloco tem de ser visto no frame seguinte.
        if (!vozesAtivas_ && hasPendingEvent_ && currentFrame_ < pendingStartFrame_) {
            const std::uint64_t ateOEvento = pendingStartFrame_ - currentFrame_;
            const std::uint32_t pulo = static_cast<std::uint32_t>(
                std::min<std::uint64_t>(ateOEvento, frameCount - i));
            std::fill(output + i, output + i + pulo, 0.0f);
            currentFrame_ += pulo;
            i += pulo - 1;  // o ++i do for completa o pulo
            continue;
        }

        // ---- Bloco B: envelope + oscilador de cada voz, somados ----
        float mix = 0.0f;
        bool algumaVozAtiva = false;
        for (int vi = 0; vi < voiceLimit; ++vi) {
            Voice& v = voices_[vi];

            // Renderiza a cauda da voz anterior roubada (DSP-07 + INT-05).
            if (v.fadeOutFramesRemaining > 0) {
                float fadeAmplitude = 0.0f;

                if (v.fadeOutType == SoundEventType::Sample) {
                    // Continua lendo o sample antigo durante o fade-out.
                    const AudioBuffer* sample = v.fadeOutSampleBuffer;

                    if (sample != nullptr &&
                        v.fadeOutSampleCursor <
                            static_cast<double>(sample->frameCount)) {
                        fadeAmplitude =
                            sample->sampleLinear(v.fadeOutSampleCursor);
                        v.fadeOutSampleCursor += v.fadeOutSamplePlaybackRate;
                    }
                } else {
                    // Mantem o fade-out senoidal do DSP-07 via wavetable (DSP-08).
                    fadeAmplitude = lookupSine(v.fadeOutPhase);
                    v.fadeOutPhase += v.fadeOutPhaseIncrement;
                }

                mix += fadeAmplitude * v.fadeOutVolume * v.fadeOutLevel;

                v.fadeOutLevel -= v.fadeOutStep;
                --v.fadeOutFramesRemaining;

                if (v.fadeOutFramesRemaining == 0 ||
                    v.fadeOutLevel <= 0.0f) {
                    v.fadeOutLevel = 0.0f;
                    v.fadeOutFramesRemaining = 0;
                } else {
                    algumaVozAtiva = true;
                }
            }

            if (v.envState == EnvState::Idle) {
                continue;
            }

            if (v.envState != EnvState::Release) {
                if (v.framesRemaining == 0) {
                    if (v.releaseRate == 0.0f) {
                        // Evento sem frames de release: encerra a voz
                        // imediatamente, sem prolongar sua duracao.
                        v.envLevel = 0.0f;
                        v.envState = EnvState::Idle;
                        continue;
                    }

                    v.envState = EnvState::Release;
                } else {
                    --v.framesRemaining;
                    if (v.envState == EnvState::Attack) {
                        v.envLevel += v.attackRate;
                        if (v.envLevel >= 1.0f) {
                            v.envLevel = 1.0f;
                            v.envState = EnvState::Sustain;
                        }
                    }
                }
            }

            if (v.envState == EnvState::Release) {
                v.envLevel -= v.releaseRate;
                if (v.envLevel <= 0.0f) {
                    v.envLevel = 0.0f;
                    v.envState = EnvState::Idle;
                    continue;
                }
            }

            float amplitude = 0.0f;

            if (v.type == SoundEventType::Sample) {
                const AudioBuffer* sample = v.sampleBuffer;

                // O banco deve permanecer vivo e imutavel durante a reproducao.
                if (sample == nullptr ||
                    v.sampleCursor >= static_cast<double>(sample->frameCount)) {
                    v.envLevel = 0.0f;
                    v.envState = EnvState::Idle;
                    continue;
                }

                // Os buffers sao decodificados em memoria antes da reproducao.
                // A interpolacao permite lidar com taxas de amostragem distintas.
                amplitude = sample->sampleLinear(v.sampleCursor);
                v.sampleCursor += v.samplePlaybackRate;

            } else {
                // Caminho de sintese senoidal otimizado com wavetable (DSP-08, #47).
                amplitude = lookupSine(v.phase);
                v.phase += v.phaseIncrement;
            }

            // Ambos os tipos utilizam o mesmo envelope e o mesmo volume.
            mix += amplitude * v.volume * v.envLevel;

            // Toda voz que chega até aqui segue soando no próximo frame: os
            // caminhos que a encerram saem antes, com `continue`.
            algumaVozAtiva = true;
        }
        vozesAtivas_ = algumaVozAtiva;

        // ---- Bloco C: saída da mixagem ----
        // Sem corte aqui: o limite fica no master bus, em render() (DSP-10,
        // #57). Cortar em ±1.0 antes dele achataria o pico em quina, e o
        // limitador suave receberia o sinal já estragado.
        output[i] = mix;
        ++currentFrame_;
    }
}

}  // namespace mus

#include "audio/audio_engine.h"

#include <cmath>

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

    if (ma_device_start(device_) != MA_SUCCESS) {
        LOG_ERROR << "falha ao iniciar a reprodução de áudio";
        return false;
    }

    running_ = true;
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

void AudioEngine::dataCallback(ma_device* device,
                               void* output,
                               const void* input,
                               std::uint32_t frameCount) {
    (void)input;  // saída apenas
    auto* engine = static_cast<AudioEngine*>(device->pUserData);
    if (engine == nullptr) {
        return;
    }
    engine->renderSine(static_cast<float*>(output), frameCount);
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

}  // namespace mus

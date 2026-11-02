#include "audio/audio_buffer.h"

#include <cmath>

#include "cli/logger.h"
#include "core/pitch.h"
#include "miniaudio.h"

namespace mus {

std::optional<AudioBuffer> AudioBuffer::loadFromFile(const std::string& filepath) {
    ma_decoder decoder;
    
    // Forçamos a decodificação para Mono (1 canal) e Float32, independente
    // de como o WAV original foi gravado. Isso garante que os dados casem 
    // perfeitamente com a configuração do nosso AudioEngine.
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 1, 44100);

    if (ma_decoder_init_file(filepath.c_str(), &config, &decoder) != MA_SUCCESS) {
        LOG_ERROR << "falha ao decodificar o arquivo de audio: " << filepath;
        return std::nullopt; // Retorna nulo graciosamente
    }

    ma_uint64 totalFrames;
    ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);

    AudioBuffer buffer;
    buffer.frameCount = static_cast<std::uint32_t>(totalFrames);
    buffer.channels = decoder.outputChannels;
    buffer.sampleRate = decoder.outputSampleRate;

    // NFR de Memória: resize() aloca a memória exata necessária de uma vez.
    // Quando a instância do AudioBuffer sair de escopo e for destruída, o 
    // vector liberará essa memória automaticamente, impedindo memory leaks.
    buffer.data.resize(buffer.frameCount);

    // Lê os blocos do disco diretamente para o nosso vetor na RAM
    ma_decoder_read_pcm_frames(&decoder, buffer.data.data(), buffer.frameCount, nullptr);
    
    // Libera os recursos do decodificador da miniaudio (fecha o arquivo)
    ma_decoder_uninit(&decoder);

    LOG_DEBUG << "wav carregado: " << filepath << " (" << buffer.frameCount << " frames)";
    return buffer;
}

float AudioBuffer::sampleLinear(double frameIndex, std::uint32_t channel) const {
    if (frameIndex < 0.0 || frameCount == 0 || data.empty()) {
        return 0.0f;
    }

    const auto i0 = static_cast<std::uint64_t>(frameIndex);
    if (i0 >= frameCount) {
        return 0.0f;
    }

    const double frac = frameIndex - static_cast<double>(i0);
    const std::uint32_t numChannels = (channels > 0) ? channels : 1;
    const std::uint32_t ch = (channel < numChannels) ? channel : 0;

    const std::size_t idx0 = static_cast<std::size_t>(i0) * numChannels + ch;
    if (idx0 >= data.size()) {
        return 0.0f;
    }
    const float s0 = data[idx0];

    // Se estiver no último frame, interpola suavemente para zero para evitar cliques na borda
    if (i0 + 1 >= frameCount) {
        return static_cast<float>(s0 * (1.0 - frac));
    }

    const std::size_t idx1 = static_cast<std::size_t>(i0 + 1) * numChannels + ch;
    if (idx1 >= data.size()) {
        return static_cast<float>(s0 * (1.0 - frac));
    }
    const float s1 = data[idx1];

    // Interpolação linear otimizada: s0 + frac * (s1 - s0)
    return static_cast<float>(s0 + frac * (s1 - s0));
}

std::uint32_t AudioBuffer::readFramesLinear(float* output,
                                            std::uint32_t count,
                                            double& cursor,
                                            double rate,
                                            float volume) const {
    if (output == nullptr || count == 0 || rate <= 0.0) {
        return 0;
    }

    std::uint32_t framesRead = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (cursor >= static_cast<double>(frameCount)) {
            output[i] = 0.0f;
            continue;
        }

        output[i] = sampleLinear(cursor) * volume;
        cursor += rate;
        ++framesRead;
    }
    return framesRead;
}

std::uint32_t AudioBuffer::renderEvent(const SoundEvent& event,
                                       float* output,
                                       std::uint32_t count,
                                       double& cursor,
                                       double baseFrequency) const {
    const double rate = (baseFrequency > 0.0 && event.frequency > 0.0)
                            ? frequencyToPlaybackRate(event.frequency, baseFrequency)
                            : 1.0;
    return readFramesLinear(output, count, cursor, rate, event.volume);
}

}  // namespace mus

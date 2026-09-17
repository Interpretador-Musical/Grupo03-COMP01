#include "audio/audio_buffer.h"

#include "cli/logger.h"
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

}  // namespace mus
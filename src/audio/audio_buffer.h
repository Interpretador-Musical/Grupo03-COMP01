#ifndef MUS_AUDIO_AUDIO_BUFFER_H
#define MUS_AUDIO_AUDIO_BUFFER_H

#include <vector>
#include <string>
#include <optional>
#include <cstdint>

namespace mus {

// Estrutura passiva que retém o áudio bruto na memória RAM.
// Cumpre o NFR de memória delegando o gerenciamento ao RAII do std::vector.
struct AudioBuffer {
    std::vector<float> data;
    std::uint32_t frameCount = 0;
    std::uint32_t channels = 0;
    std::uint32_t sampleRate = 0;

    // Tenta decodificar o arquivo. Retorna std::nullopt se falhar.
    static std::optional<AudioBuffer> loadFromFile(const std::string& filepath);
};

}  // namespace mus

#endif  // MUS_AUDIO_AUDIO_BUFFER_H
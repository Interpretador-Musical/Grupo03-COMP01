#ifndef MUS_AUDIO_AUDIO_BUFFER_H
#define MUS_AUDIO_AUDIO_BUFFER_H

#include <vector>
#include <string>
#include <optional>
#include <cstdint>

#include "core/sound_event.h"

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

    // DSP-06 (#36): Interpolação linear e pitch shifting para buffers PCM

    // Obtém uma amostra interpolada linearmente na posição fracionária `frameIndex`.
    // Retorna 0.0f fora dos limites [0, frameCount).
    float sampleLinear(double frameIndex, std::uint32_t channel = 0) const;

    // Lê `count` frames aplicando taxa de reprodução `rate` e ganho `volume`,
    // avançando o cursor fracionário `cursor` proporcionalmente a `rate`.
    // Retorna o número de frames válidos lidos antes de atingir o fim do buffer.
    std::uint32_t readFramesLinear(float* output,
                                   std::uint32_t count,
                                   double& cursor,
                                   double rate,
                                   float volume = 1.0f) const;

    // Lê `count` frames integrando diretamente com o SoundEvent (DSP-06, #36):
    // calcula o playback rate a partir de `event.frequency` relativo a `baseFrequency`.
    std::uint32_t renderEvent(const SoundEvent& event,
                              float* output,
                              std::uint32_t count,
                              double& cursor,
                              double baseFrequency = 440.0) const;
};

}  // namespace mus

#endif  // MUS_AUDIO_AUDIO_BUFFER_H

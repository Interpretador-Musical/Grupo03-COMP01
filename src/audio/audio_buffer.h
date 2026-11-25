/**
 * @file audio_buffer.h
 * @brief Buffer de áudio PCM em memória com interpolação linear e pitch shifting.
 */

#ifndef MUS_AUDIO_AUDIO_BUFFER_H
#define MUS_AUDIO_AUDIO_BUFFER_H

#include <vector>
#include <string>
#include <optional>
#include <cstdint>

#include "core/sound_event.h"

namespace mus {

/**
 * @brief Estrutura passiva que retém áudio PCM decodificado em memória RAM.
 *
 * Delega o ciclo de vida da memória ao RAII de `std::vector`, evitando vazamentos.
 */
struct AudioBuffer {
    std::vector<float> data;          ///< Amostras PCM normalizadas em `float`.
    std::uint32_t frameCount = 0;     ///< Quantidade total de frames decodificados.
    std::uint32_t channels = 0;       ///< Quantidade de canais (1 = mono após carga).
    std::uint32_t sampleRate = 0;     ///< Taxa de amostragem do buffer em Hz.

    /**
     * @brief Carrega e decodifica um arquivo WAV para mono `float32` a 44 100 Hz.
     * @param filepath Caminho do arquivo de áudio em disco.
     * @return `AudioBuffer` preenchido, ou `std::nullopt` se falhar ao decodificar.
     */
    static std::optional<AudioBuffer> loadFromFile(const std::string& filepath);

    /**
     * @brief Lê uma amostra interpolada linearmente na posição fracionária (DSP-06, #36).
     * @param frameIndex Índice fracionário no intervalo `[0, frameCount)`.
     * @param channel Canal desejado (padrão: `0`).
     * @return Valor interpolado em `[-1.0, 1.0]`, ou `0.0f` fora dos limites.
     */
    float sampleLinear(double frameIndex, std::uint32_t channel = 0) const;

    /**
     * @brief Lê `count` frames com taxa de reprodução `rate` e ganho `volume` (DSP-06, #36).
     *
     * Avança `cursor` proporcionalmente a `rate` e preenche o restante com `0.0f`
     * após o fim do buffer.
     *
     * @param[out] output Ponteiro para o buffer de saída com espaço para `count` amostras.
     * @param count Quantidade de frames solicitada.
     * @param[in,out] cursor Posição fracionária atual no buffer.
     * @param rate Taxa de avanço do cursor por frame de saída.
     * @param volume Multiplicador de amplitude (padrão: `1.0f`).
     * @return Número de frames válidos lidos antes do fim do buffer.
     */
    std::uint32_t readFramesLinear(float* output,
                                   std::uint32_t count,
                                   double& cursor,
                                   double rate,
                                   float volume = 1.0f) const;

    /**
     * @brief Renderiza `count` frames de um `SoundEvent` aplicando pitch shifting (DSP-06, #36).
     *
     * Calcula o playback rate a partir de `event.frequency` relativo a `baseFrequency`.
     *
     * @param event Evento sonoro com frequência e volume alvo.
     * @param[out] output Ponteiro para o buffer de saída.
     * @param count Quantidade de frames a sintetizar.
     * @param[in,out] cursor Cursor fracionário mantido entre blocos.
     * @param baseFrequency Frequência base de gravação do sample em Hz (padrão: `440.0`).
     * @return Número de frames válidos renderizados.
     */
    std::uint32_t renderEvent(const SoundEvent& event,
                              float* output,
                              std::uint32_t count,
                              double& cursor,
                              double baseFrequency = 440.0) const;
};

}  // namespace mus

#endif  // MUS_AUDIO_AUDIO_BUFFER_H

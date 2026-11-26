/**
 * @file gerador_wav.h
 * @brief Geração da saída-alvo musical offline em WAV PCM 16-bit (COMP-C, #108).
 */

#ifndef MUS_CODEGEN_GERADOR_WAV_H
#define MUS_CODEGEN_GERADOR_WAV_H

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>

#include "core/playhead.h"

namespace mus {

class SampleBank;

namespace codegen {

/** @brief Taxa de amostragem padrão da saída WAV em Hz (coincide com `AudioEngine`). */
constexpr std::uint32_t kWavSampleRate = 44100;
/** @brief Número de canais da saída WAV (1 = mono). */
constexpr std::uint16_t kWavChannels = 1;
/** @brief Profundidade de bits das amostras PCM lineares (16 bits). */
constexpr std::uint16_t kWavBitsPerSample = 16;
/** @brief Tamanho fixo em bytes do cabeçalho RIFF/WAVE canônico. */
constexpr std::size_t kWavHeaderSize = 44;

/**
 * @brief Metadados e status retornados pela etapa de geração WAV.
 */
struct ResultadoWav {
    bool ok = false;                   ///< `true` se o arquivo/fluxo foi gerado integralmente.
    std::uint64_t totalFrames = 0;     ///< Total de frames PCM gravados.
    std::uint64_t totalBytes = 0;      ///< Tamanho total da saída em bytes (`44 + 2 * totalFrames`).
    double duracaoSegundos = 0.0;      ///< Duração resultante (`totalFrames / kWavSampleRate`).
    std::string erro;                  ///< Mensagem descritiva quando `ok == false`.
};

/**
 * @brief Converte uma amostra normalizada `[-1.0, 1.0]` para PCM linear de 16 bits.
 *
 * Valores não finitos (`NaN`, infinito) viram silêncio (`0`) e amplitudes fora
 * da faixa são saturadas para `[-32768, 32767]`.
 *
 * @param amostra Amostra de ponto flutuante gerada pelo master bus.
 * @return Amostra quantizada assinada de 16 bits.
 */
std::int16_t floatParaPcm16(float amostra);

/**
 * @brief Gera um fluxo RIFF/WAVE PCM 16-bit a partir da `Timeline` (IR).
 *
 * Renderiza offline usando o próprio motor de síntese (`AudioEngine::render`),
 * sem inicializar dispositivo de áudio e sem repetir a análise do código `.mus`.
 *
 * @param timeline Representação intermediária compilada.
 * @param destino Fluxo de saída binário já aberto e em bom estado.
 * @param sampleBank Banco opcional de samples WAV para eventos `SoundEventType::Sample`.
 * @param masterVolume Multiplicador de ganho master final em `[0.0, 1.0]` (DSP-09, #52).
 * @return `ResultadoWav` indicando sucesso ou erro detalhado.
 */
ResultadoWav gerarWav(const Timeline& timeline,
                      std::ostream& destino,
                      const SampleBank* sampleBank = nullptr,
                      float masterVolume = 1.0f);

/**
 * @brief Grava a saída-alvo WAV diretamente em disco a partir da `Timeline`.
 *
 * Valida a timeline e o caminho antes de criar o arquivo e remove arquivos
 * parciais caso ocorra erro de escrita.
 *
 * @param timeline Representação intermediária compilada.
 * @param caminhoArquivo Caminho do arquivo `.wav` de destino.
 * @param sampleBank Banco opcional de samples WAV.
 * @param masterVolume Multiplicador de ganho master final em `[0.0, 1.0]` (DSP-09, #52).
 * @return `ResultadoWav` com estatísticas do arquivo gerado ou motivo da falha.
 */
ResultadoWav gerarWavArquivo(const Timeline& timeline,
                             const std::string& caminhoArquivo,
                             const SampleBank* sampleBank = nullptr,
                             float masterVolume = 1.0f);

}  // namespace codegen
}  // namespace mus

#endif  // MUS_CODEGEN_GERADOR_WAV_H

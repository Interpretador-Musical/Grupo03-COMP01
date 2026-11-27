/**
 * @file sample_bank.h
 * @brief Banco imutável de samples WAV decodificados antes da reprodução (INT-05).
 */

#ifndef MUS_AUDIO_SAMPLE_BANK_H
#define MUS_AUDIO_SAMPLE_BANK_H

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "audio/audio_buffer.h"

namespace mus {

/**
 * @brief Dicionário de samples carregados antes da execução sonora.
 *
 * Resolve nomes textuais para identificadores numéricos `SampleId` fora da
 * thread de áudio, permitindo consulta O(1) lock-free e sem alocações dentro
 * do callback real-time.
 *
 * @warning Não é seguro modificar o banco enquanto o callback de áudio estiver
 *          acessando seus buffers.
 */
class SampleBank {
public:
    /** @brief Identificador numérico compacto de um sample carregado. */
    using SampleId = std::uint32_t;

    /**
     * @brief Carrega um arquivo WAV do disco e o associa ao nome informado.
     * @param name Nome lógico usado na linguagem (ex.: `"PERC_09"`). Rejeita vazios e duplicados.
     * @param filepath Caminho completo para o arquivo `.wav`.
     * @return `SampleId` atribuído em sucesso, ou `std::nullopt` em erro/duplicata.
     */
    std::optional<SampleId> load(const std::string& name,
                                 const std::string& filepath);

    /**
     * @brief Resolve o nome de um sample para seu identificador numérico.
     * @param name Nome lógico procurado.
     * @return `SampleId` correspondente, ou `std::nullopt` se não encontrado.
     */
    std::optional<SampleId> findId(const std::string& name) const;

    /**
     * @brief Obtém o buffer PCM associado a um identificador numérico.
     * @param id Identificador retornado por `load()` ou `findId()`.
     * @return Ponteiro constante para o `AudioBuffer`, ou `nullptr` se inválido.
     */
    const AudioBuffer* get(SampleId id) const;

    /**
     * @brief Retorna a quantidade de samples carregados no banco.
     * @return Total de buffers armazenados.
     */
    std::size_t size() const { return samples_.size(); }

    /**
     * @brief Verifica se nenhum sample foi carregado ainda.
     * @return `true` se o banco está vazio; `false` caso contrário.
     */
    bool empty() const { return samples_.empty(); }

private:
    std::unordered_map<std::string, SampleId> ids_;
    std::vector<AudioBuffer> samples_;
};

}  // namespace mus

#endif  // MUS_AUDIO_SAMPLE_BANK_H

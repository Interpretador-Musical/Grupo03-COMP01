
#ifndef MUS_AUDIO_SAMPLE_BANK_H
#define MUS_AUDIO_SAMPLE_BANK_H

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "audio/audio_buffer.h"

namespace mus {

// Dicionario de samples carregados antes da reproducao.
//
// Os nomes sao resolvidos fora da thread de audio. Durante a
// reproducao, os eventos utilizam apenas identificadores numericos.
//
// Nao e seguro modificar o banco enquanto o callback de audio
// estiver acessando seus buffers.
class SampleBank {
public:
    using SampleId = std::uint32_t;

    // Carrega um WAV e o associa ao nome informado.
    // Retorna o ID em caso de sucesso ou nullopt em caso de falha.
    // Nomes vazios e duplicados nao sao aceitos.
    std::optional<SampleId> load(const std::string& name,
                                 const std::string& filepath);

    // Resolve um nome para seu identificador numerico.
    std::optional<SampleId> findId(const std::string& name) const;

    // Consulta um buffer previamente carregado.
    // Retorna nullptr para identificadores invalidos.
    const AudioBuffer* get(SampleId id) const;

    std::size_t size() const { return samples_.size(); }
    bool empty() const { return samples_.empty(); }

private:
    std::unordered_map<std::string, SampleId> ids_;
    std::vector<AudioBuffer> samples_;
};

}  // namespace mus

#endif  // MUS_AUDIO_SAMPLE_BANK_H

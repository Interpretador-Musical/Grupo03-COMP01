
#include "audio/sample_bank.h"

#include <limits>
#include <utility>

namespace mus {

std::optional<SampleBank::SampleId> SampleBank::load(
    const std::string& name,
    const std::string& filepath) {

    if (name.empty() || filepath.empty()) {
        return std::nullopt;
    }

    // Nao sobrescreve samples existentes.
    if (ids_.find(name) != ids_.end()) {
        return std::nullopt;
    }

    // Evita converter um indice fora da faixa de uint32_t.
    if (samples_.size() >=
        static_cast<std::size_t>(
            std::numeric_limits<SampleId>::max())) {
        return std::nullopt;
    }

    // Toda operacao de I/O ocorre aqui, antes da reproducao.
    auto buffer = AudioBuffer::loadFromFile(filepath);
    if (!buffer.has_value()) {
        return std::nullopt;
    }

    // Registra o nome e o buffer com o mesmo identificador.
    const SampleId id = static_cast<SampleId>(samples_.size());

    ids_.emplace(name, id);
    samples_.push_back(std::move(*buffer));

    return id;
}

std::optional<SampleBank::SampleId> SampleBank::findId(
    const std::string& name) const {

    const auto it = ids_.find(name);
    if (it == ids_.end()) {
        return std::nullopt;
    }

    return it->second;
}

const AudioBuffer* SampleBank::get(SampleId id) const {
    if (static_cast<std::size_t>(id) >= samples_.size()) {
        return nullptr;
    }

    return &samples_[id];
}

}  // namespace mus

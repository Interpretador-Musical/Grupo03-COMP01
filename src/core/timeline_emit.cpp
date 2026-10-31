#include "core/timeline_emit.h"

#include <cstdio>

namespace mus {

const char* const kCabecalhoTimeline =
    "indice,inicio_s,duracao_s,frequencia_hz,volume";

std::string emitirTimeline(const Timeline& timeline) {
    Timeline ordenada = timeline;
    ordenada.sortByStartTime();

    std::string saida = kCabecalhoTimeline;
    saida += '\n';

    // Quatro doubles com %.6f e um índice cabem folgados: mesmo uma frequência
    // absurda (1e20 Hz) dá menos de 40 caracteres por coluna.
    char linha[256];
    std::size_t indice = 1;
    for (const SoundEvent& evento : ordenada.events()) {
        std::snprintf(linha, sizeof(linha), "%zu,%.6f,%.6f,%.6f,%.6f\n", indice++,
                      evento.startTime, evento.duration, evento.frequency,
                      static_cast<double>(evento.volume));
        saida += linha;
    }
    return saida;
}

}  // namespace mus

#include "core/timeline_queue.h"

#include <algorithm>
#include <vector>

namespace mus {

namespace {

bool empurrarTodos(const std::vector<SoundEvent>& eventos,
                   SpscRingBuffer<SoundEvent>& fila) {
    for (const SoundEvent& evento : eventos) {
        if (!fila.push(evento)) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool carregarFila(const Timeline& timeline, SpscRingBuffer<SoundEvent>& fila) {
    if (timeline.size() > fila.capacity()) {
        return false;
    }

    // A Timeline que sai do `Interpretador::executar` já vem ordenada, e aí
    // copiar para ordenar de novo só dobraria a memória do caminho com áudio
    // (#125). Já em ordem não decrescente, a ordenação estável não mexeria em
    // nada, nem nos empates, então empurrar direto dá a mesma fila.
    const std::vector<SoundEvent>& eventos = timeline.events();
    const bool emOrdem = std::is_sorted(
        eventos.begin(), eventos.end(),
        [](const SoundEvent& a, const SoundEvent& b) { return a.startTime < b.startTime; });
    if (emOrdem) {
        return empurrarTodos(eventos, fila);
    }

    Timeline ordenada = timeline;
    ordenada.sortByStartTime();
    return empurrarTodos(ordenada.events(), fila);
}

}  // namespace mus

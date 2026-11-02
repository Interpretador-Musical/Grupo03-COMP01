#include "core/timeline_queue.h"

namespace mus {

bool carregarFila(const Timeline& timeline, SpscRingBuffer<SoundEvent>& fila) {
    if (timeline.size() > fila.capacity()) {
        return false;
    }

    Timeline ordenada = timeline;
    ordenada.sortByStartTime();

    for (const SoundEvent& evento : ordenada.events()) {
        if (!fila.push(evento)) {
            return false;
        }
    }
    return true;
}

}  // namespace mus

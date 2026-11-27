/**
 * @file timeline_queue.h
 * @brief Carga em lote da `Timeline` para o ring buffer lock-free (SCH-05, #29).
 */

#ifndef MUS_CORE_TIMELINE_QUEUE_H
#define MUS_CORE_TIMELINE_QUEUE_H

#include "core/playhead.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"

namespace mus {

/**
 * @brief Carrega integralmente a `Timeline` na fila SPSC antes de iniciar o áudio.
 *
 * Política batch: a fila deve ter `capacity() >= timeline.size()`. Ordena
 * estavelmente por `startTime` apenas se ainda não estiver em ordem não
 * decrescente (evitando cópia quando vem direto do `Interpretador`, #125). Se
 * a fila não comportar todos os eventos, devolve `false` sem tocar a música.
 *
 * @pre `fila` vazia e consumidor parado (antes de `AudioEngine::start()`).
 * @param timeline Timeline compilada.
 * @param fila Ring buffer SPSC compartilhado com a thread de áudio ou gerador offline.
 * @return `true` se todos os eventos entraram na fila; `false` se excedeu a capacidade.
 */
bool carregarFila(const Timeline& timeline, SpscRingBuffer<SoundEvent>& fila);

}  // namespace mus

#endif  // MUS_CORE_TIMELINE_QUEUE_H

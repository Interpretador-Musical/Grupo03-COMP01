#ifndef MUS_CORE_TIMELINE_QUEUE_H
#define MUS_CORE_TIMELINE_QUEUE_H

#include "core/playhead.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"

namespace mus {

// Entrega a Timeline compilada à fila da thread de áudio — SCH-05 (#29).
//
// Política do modelo batch: a fila é carregada INTEIRA antes do áudio começar,
// e nada mais é empurrado durante a reprodução. Por isso a fila nunca enche
// em uso normal: quem chama a cria com `capacity() >= timeline.size()`. Se,
// mesmo assim, algum evento não couber, a carga falha e devolve false — e o
// chamador não deve tocar a música, porque uma timeline incompleta tocaria
// sem as notas que ficaram de fora, sem nenhum aviso.
//
// - Ordem: por início, com ordenação estável (eventos simultâneos mantêm a
//   ordem de agendamento). É a mesma ordem do `--emit-timeline`, e é a que o
//   consumidor exige: ele só olha o evento da frente da fila.
// - Timeline maior que `capacity()`: devolve false sem empurrar nada.
// - Precondição: fila vazia e consumidor parado (antes de `AudioEngine::start`).
//   Se a fila já tiver itens e encher no meio da carga, devolve false, mas o
//   que já entrou fica lá — por isso a precondição.
//
// Roda na thread principal, nunca no callback: copia e ordena, então aloca.
bool carregarFila(const Timeline& timeline, SpscRingBuffer<SoundEvent>& fila);

}  // namespace mus

#endif  // MUS_CORE_TIMELINE_QUEUE_H

/**
 * @file timeline_emit.h
 * @brief Serialização determinística da `Timeline` em CSV (`--emit-timeline`, COMP-B).
 */

#ifndef MUS_CORE_TIMELINE_EMIT_H
#define MUS_CORE_TIMELINE_EMIT_H

#include <string>

#include "core/playhead.h"

namespace mus {

/**
 * @brief Linha de cabeçalho contratual da saída CSV de `--emit-timeline`.
 */
extern const char* const kCabecalhoTimeline;

/**
 * @brief Serializa a `Timeline` (IR) em texto CSV determinístico (COMP-B, #105).
 *
 * Contrato de saída:
 * - Ordem estável por `startTime` (preserva ordem de agendamento em acordes).
 * - Unidades: segundos (`inicio_s`, `duracao_s`), Hz (`frequencia_hz`) e `[0, 1]` (`volume`).
 * - Precisão fixa de 6 casas decimais com ponto separador (locale `"C"`).
 * - `indice` iniciado em `1`; toda linha termina em `\n`. Timeline vazia emite só o cabeçalho.
 *
 * Não modifica a `Timeline` recebida (ordena uma cópia).
 *
 * @param timeline Representação intermediária produzida pelo interpretador.
 * @return String contendo o CSV completo pronto para saída padrão.
 */
std::string emitirTimeline(const Timeline& timeline);

}  // namespace mus

#endif  // MUS_CORE_TIMELINE_EMIT_H

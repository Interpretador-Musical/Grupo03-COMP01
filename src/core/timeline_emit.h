#ifndef MUS_CORE_TIMELINE_EMIT_H
#define MUS_CORE_TIMELINE_EMIT_H

#include <string>

#include "core/playhead.h"

namespace mus {

// Primeira linha da saída do `--emit-timeline`. É parte do contrato: quem lê a
// saída (testes, planilha, script) pode contar com estes nomes de coluna.
extern const char* const kCabecalhoTimeline;

// Serializa a Timeline — a representação intermediária do compilador — em CSV,
// para o `--emit-timeline` da COMP-B (#105).
//
//   indice,inicio_s,duracao_s,frequencia_hz,volume
//   1,0.000000,0.500000,261.625565,0.800000
//
// O contrato que torna a saída determinística:
// - Ordem: por `startTime`, com ordenação estável, então eventos simultâneos
//   (as notas de um acorde) saem na ordem em que o programa os agendou. É a
//   mesma ordem em que o `main.cpp` entrega os eventos ao áudio.
// - Unidades: tempos em segundos, frequência em Hz, volume de 0 a 1.
// - Precisão: seis casas decimais fixas em todas as colunas (microssegundo nos
//   tempos), com ponto como separador decimal. O programa nunca chama
//   `setlocale`, então o printf fica no locale "C" em qualquer sistema.
// - `indice` começa em 1; quebra de linha `\n` no fim de toda linha, inclusive
//   a última. Timeline vazia devolve só o cabeçalho.
//
// Não modifica a Timeline recebida: ordena uma cópia.
std::string emitirTimeline(const Timeline& timeline);

}  // namespace mus

#endif  // MUS_CORE_TIMELINE_EMIT_H

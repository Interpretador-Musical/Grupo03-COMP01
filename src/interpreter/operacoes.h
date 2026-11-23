#ifndef MUS_INTERPRETER_OPERACOES_H
#define MUS_INTERPRETER_OPERACOES_H

#include <optional>

#include "interpreter/ambiente.h"
#include "interpreter/ast.h"

namespace mus {

// O significado de cada operador da linguagem, num lugar só — COMP-D (#110).
//
// São usadas pelo Interpretador, ao avaliar uma expressão durante a execução,
// e pela dobra de constantes (otimizacao/dobra_constantes.h), ao avaliar a
// mesma expressão antes dela. Por ser a mesma função, com as mesmas operações
// em double na mesma ordem, o valor dobrado é idêntico bit a bit ao que o
// interpretador calcularia: a otimização não tem como divergir da semântica.

double paraNumero(const Valor& valor);
bool paraBooleano(const Valor& valor);

// MIDI de uma nota com oitava conhecida: `do4` = 60.
double midiDaNota(int semitom, int oitava);

Valor aplicarUnario(ast::OperadorUnario operador, const Valor& operando);

// std::nullopt quando a operação não existe para esses valores (hoje, só
// aritmética ou ordem entre strings). O interpretador transforma isso em erro
// de execução; a dobra de constantes simplesmente não dobra, e o erro continua
// acontecendo na execução, com a mesma mensagem.
std::optional<Valor> aplicarBinario(ast::OperadorBinario operador,
                                    const Valor& esquerda, const Valor& direita);

}  // namespace mus

#endif  // MUS_INTERPRETER_OPERACOES_H

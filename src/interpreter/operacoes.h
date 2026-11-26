/**
 * @file operacoes.h
 * @brief Semântica única dos operadores da linguagem (COMP-D, #110).
 */

#ifndef MUS_INTERPRETER_OPERACOES_H
#define MUS_INTERPRETER_OPERACOES_H

#include <optional>

#include "interpreter/ambiente.h"
#include "interpreter/ast.h"

namespace mus {

/**
 * @brief Converte um `Valor` da execução para `double`.
 *
 * Compartilhada entre `Interpretador` e `dobrarConstantes` para garantir
 * equivalência bit a bit entre avaliação em execução e dobra de constantes.
 *
 * @param valor Valor em tempo de execução.
 * @return Número correspondente (`1.0`/`0.0` para `bool`, `0.0` para `std::string`).
 */
double paraNumero(const Valor& valor);

/**
 * @brief Converte um `Valor` da execução para booleano.
 * @param valor Valor em tempo de execução.
 * @return Valor booleano (`valor != 0.0` para números, `false` para `std::string`).
 */
bool paraBooleano(const Valor& valor);

/**
 * @brief Calcula o número MIDI de uma nota com oitava conhecida (`do4` = 60).
 * @param semitom Semitom dentro da oitava (`0` = Dó, ..., `11` = Si; acidentes podem sair dessa faixa).
 * @param oitava Número da oitava.
 * @return Número MIDI em `double`.
 */
double midiDaNota(int semitom, int oitava);

/**
 * @brief Aplica um operador unário (`-` ou `nao`) a um `Valor`.
 * @param operador Operador unário da AST.
 * @param operando Valor avaliado do operando.
 * @return Resultado da operação.
 */
Valor aplicarUnario(ast::OperadorUnario operador, const Valor& operando);

/**
 * @brief Aplica um operador binário a dois `Valor`es já avaliados.
 * @param operador Operador binário da AST.
 * @param esquerda Operando esquerdo.
 * @param direita Operando direito.
 * @return Resultado da operação, ou `std::nullopt` quando inválida para os tipos
 *         recebidos (ex.: aritmética ou ordem entre strings), caso em que o
 *         interpretador reporta erro e a dobra de constantes preserva o nó.
 */
std::optional<Valor> aplicarBinario(ast::OperadorBinario operador,
                                    const Valor& esquerda, const Valor& direita);

}  // namespace mus

#endif  // MUS_INTERPRETER_OPERACOES_H

/**
 * @file dobra_constantes.h
 * @brief Otimização de dobra de constantes (*constant folding*) sobre a AST (COMP-D, #110).
 */

#ifndef MUS_OTIMIZACAO_DOBRA_CONSTANTES_H
#define MUS_OTIMIZACAO_DOBRA_CONSTANTES_H

#include <vector>

#include "interpreter/ast.h"

namespace mus {
namespace otimizacao {

/**
 * @brief Estatísticas coletadas pela passada de dobra de constantes.
 */
struct ResultadoDobra {
    int expressoesDobradas = 0; ///< Nós unários/binários substituídos por literais.
    int nosAntes = 0;           ///< Total de nós de expressão na AST antes da dobra.
    int nosDepois = 0;          ///< Total de nós de expressão na AST após a dobra.
};

/**
 * @brief Dobra subexpressões constantes na AST preservando a semântica exata.
 *
 * Executa após a análise semântica e antes do interpretador, avaliando
 * expressões cujos operandos são todos constantes (literais numéricos,
 * booleanos, strings e notas com oitava explícita como `la4`) usando as mesmas
 * funções de `interpreter/operacoes.h`. Notas sem oitava e variáveis não são
 * dobradas, e não há simplificação algébrica para preservar `-0`, `NaN`,
 * infinitos e eventuais erros de execução bit a bit. A posição do nó é mantida
 * e a profundidade (CMP-08) é recalculada.
 *
 * O vetor é `const` porque a lista de comandos não muda — apenas os nós
 * apontados são modificados no lugar (Cppcheck `constParameterPointer`, CMP-12).
 *
 * @param programa Lista raiz de comandos da AST já validada semanticamente.
 * @return Estatísticas de nós dobrados e contagem antes/depois.
 */
ResultadoDobra dobrarConstantes(const std::vector<ast::Comando*>& programa);

/**
 * @brief Conta o número total de nós de expressão em um programa da AST.
 * @param programa Lista raiz de comandos da AST.
 * @return Quantidade total de nós `ast::Expressao` alcançáveis.
 */
int contarNosDeExpressao(const std::vector<ast::Comando*>& programa);

}  // namespace otimizacao
}  // namespace mus

#endif  // MUS_OTIMIZACAO_DOBRA_CONSTANTES_H

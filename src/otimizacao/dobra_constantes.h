#ifndef MUS_OTIMIZACAO_DOBRA_CONSTANTES_H
#define MUS_OTIMIZACAO_DOBRA_CONSTANTES_H

#include <vector>

#include "interpreter/ast.h"

namespace mus {
namespace otimizacao {

// Dobra de constantes (constant folding) sobre a AST — COMP-D (#110).
//
// Roda depois da análise semântica e antes do interpretador. Toda expressão
// unária ou binária cujos operandos são todos constantes é avaliada uma vez,
// aqui, e substituída por um literal com o resultado:
//
//   toca la4 - 2 por 1 / 4 * 2      →   toca 67 por 0.5
//   repita 4 * 4 vezes { ... }      →   repita 16 vezes { ... }
//   x * (2 + 3)                     →   x * 5
//
// O benefício aparece dentro de `repita`: o interpretador avalia a expressão
// a cada volta, e a dobra paga o custo uma vez só. Ver docs/otimizacao.md.
//
// O que conta como constante: literal numérico, booleano ou string, e nota com
// oitava escrita (`la4`). Ficam de fora variáveis (não há propagação de
// constantes) e nota sem oitava, que depende da `oitava` corrente na
// execução.
//
// Garantias, todas cobertas por teste:
//   - O valor dobrado é calculado pelas mesmas funções do interpretador
//     (interpreter/operacoes.h), então é idêntico bit a bit, incluindo -0, NaN
//     e infinito vindos de divisão por zero.
//   - Só dobra quando todos os operandos são constantes. Nenhuma
//     simplificação algébrica (x * 1, x + 0, falso e x): elas mudariam o
//     resultado para -0 ou NaN, ou pulariam um erro que x causaria.
//   - Uma operação inválida (aritmética entre strings) não é dobrada: o erro
//     acontece na execução, com a mesma mensagem de antes.
//   - A posição do nó é mantida, e a profundidade (CMP-08) é recalculada.
struct ResultadoDobra {
    int expressoesDobradas = 0;  // nós unários/binários trocados por literal
    int nosAntes = 0;            // nós de expressão na AST antes da dobra
    int nosDepois = 0;           // e depois
};

ResultadoDobra dobrarConstantes(std::vector<ast::Comando*>& programa);

// Quantos nós de expressão a AST tem. Exposto para os testes e o log.
int contarNosDeExpressao(const std::vector<ast::Comando*>& programa);

}  // namespace otimizacao
}  // namespace mus

#endif  // MUS_OTIMIZACAO_DOBRA_CONSTANTES_H

/**
 * @file analisador_semantico.h
 * @brief Análise semântica, checagem estática de tipos e isolamento de trilhas.
 */

#ifndef MUS_SEMANTIC_ANALISADOR_SEMANTICO_H
#define MUS_SEMANTIC_ANALISADOR_SEMANTICO_H

#include <string>
#include <vector>

#include "interpreter/ast.h"
#include "semantic/tabela_simbolos.h"

namespace mus {
namespace semantica {

/**
 * @brief Diagnóstico semântico com posição de origem no código-fonte.
 */
struct ErroSemantico {
    ast::Posicao posicao; ///< Linha e coluna onde o erro foi detectado.
    std::string mensagem; ///< Descrição legível da violação semântica.

    /**
     * @brief Formata o erro no padrão `"Linha X, Coluna Y: mensagem"`, igual ao parser.
     * @return Mensagem formatada pronta para exibição ao usuário.
     */
    std::string formatado() const;
};

/**
 * @brief Percorre a AST validando escopos, tipos e regras de trilhas (CMP-13, #95).
 *
 * Verifica:
 * - Declaração e uso: primeira atribuição infere o tipo no escopo atual;
 *   reatribuir com tipo distinto falha; blocos de `repita` abrem escopo local.
 * - Tipos: valida operandos de comandos e operadores conforme `docs/especificacao.md`.
 * - Trilhas paralelas (CMP-07, #39): cada trilha abre escopo próprio, não pode
 *   reatribuir variável externa ao grupo e não pode conter `andamento`.
 *
 * Coleta todos os erros sem interromper no primeiro, evitando cascata com
 * `Tipo::Erro`.
 */
class AnalisadorSemantico {
public:
    /**
     * @brief Analisa todos os comandos da AST de um programa.
     * @param programa Lista de comandos raiz retornada por `parseToAst()`.
     * @return `true` se o programa não possui erros semânticos; `false` caso contrário.
     */
    bool analisar(const std::vector<ast::Comando*>& programa);

    /**
     * @brief Retorna a lista de erros semânticos acumulados na análise.
     * @return Vetor constante de `ErroSemantico` em ordem de detecção.
     */
    const std::vector<ErroSemantico>& erros() const { return erros_; }

    /**
     * @brief Retorna a tabela de símbolos montada durante a análise.
     * @return Referência constante para `TabelaSimbolos` (usada por `--simbolos`).
     */
    const TabelaSimbolos& tabela() const { return tabela_; }

private:
    void analisarBloco(const std::vector<ast::Comando*>& bloco);
    void analisarComando(const ast::Comando& comando);
    void analisarAtribuicao(const ast::ComandoAtribuicao& atribuicao);
    void analisarGrupo(const ast::ComandoGrupo& grupo);

    Tipo tipoDe(const ast::Expressao& expressao);
    Tipo tipoBinaria(const ast::ExpressaoBinaria& binaria, ast::Posicao posicao);
    Tipo tipoUnaria(const ast::ExpressaoUnaria& unaria, ast::Posicao posicao);

    // Reporta se a expressão não for numérica. `contexto` completa a frase:
    // "andamento espera numero, recebeu nota".
    void exigirNumero(const ast::Expressao& expressao, const char* contexto);

    void reportar(ast::Posicao posicao, const std::string& mensagem);

    TabelaSimbolos tabela_;
    std::vector<ErroSemantico> erros_;

    // Nível de escopo em que o grupo mais interno começou, ou -1 fora de
    // grupo. Uma variável declarada nesse nível ou abaixo é "de fora do grupo"
    // para as trilhas dele.
    int nivelDoGrupo_ = -1;
};

}  // namespace semantica
}  // namespace mus

#endif  // MUS_SEMANTIC_ANALISADOR_SEMANTICO_H

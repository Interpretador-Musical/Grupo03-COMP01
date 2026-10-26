#ifndef MUS_SEMANTIC_ANALISADOR_SEMANTICO_H
#define MUS_SEMANTIC_ANALISADOR_SEMANTICO_H

#include <string>
#include <vector>

#include "interpreter/ast.h"
#include "semantic/tabela_simbolos.h"

namespace mus {
namespace semantica {

struct ErroSemantico {
    ast::Posicao posicao;
    std::string mensagem;

    // "Linha X, Coluna Y: mensagem" — o mesmo formato do `yyerror()` da
    // CMP-03 (#15), para erro de sintaxe e erro semântico lerem igual.
    std::string formatado() const;
};

// Fase entre o parser e o interpretador (CMP-13, #95). Percorre a AST uma vez,
// preenchendo a tabela de símbolos e verificando:
//
// - Declaração: a primeira atribuição a um nome o declara no escopo atual,
//   com o tipo do valor. Reatribuir com outro tipo é redeclaração inválida.
// - Uso: todo identificador lido precisa estar declarado num escopo visível.
//   O bloco `{}` do `repita` abre escopo, então uma variável criada lá dentro
//   não existe depois dele.
// - Tipos: operadores e comandos recebem os tipos que fazem sentido (ver a
//   tabela em `docs/especificacao.md`).
//
// Diferente do interpretador, não para no primeiro erro: junta todos, cada um
// com posição, para o usuário corrigir o arquivo numa passada só. O que evita
// a cascata é o `Tipo::Erro` (ver `tabela_simbolos.h`).
//
// Como esta fase recusa todo uso fora de escopo, o mapa global de variáveis
// do `Interpretador` dá o mesmo resultado que uma pilha de escopos daria — por
// isso o interpretador não precisou mudar.
class AnalisadorSemantico {
public:
    // true se o programa não tem erro semântico.
    bool analisar(const std::vector<ast::Comando*>& programa);

    const std::vector<ErroSemantico>& erros() const { return erros_; }
    const TabelaSimbolos& tabela() const { return tabela_; }

private:
    void analisarBloco(const std::vector<ast::Comando*>& bloco);
    void analisarComando(const ast::Comando& comando);
    void analisarAtribuicao(const ast::ComandoAtribuicao& atribuicao);

    Tipo tipoDe(const ast::Expressao& expressao);
    Tipo tipoBinaria(const ast::ExpressaoBinaria& binaria, ast::Posicao posicao);
    Tipo tipoUnaria(const ast::ExpressaoUnaria& unaria, ast::Posicao posicao);

    // Reporta se a expressão não for numérica. `contexto` completa a frase:
    // "andamento espera numero, recebeu nota".
    void exigirNumero(const ast::Expressao& expressao, const char* contexto);

    void reportar(ast::Posicao posicao, const std::string& mensagem);

    TabelaSimbolos tabela_;
    std::vector<ErroSemantico> erros_;
};

}  // namespace semantica
}  // namespace mus

#endif  // MUS_SEMANTIC_ANALISADOR_SEMANTICO_H

#ifndef MUS_SEMANTIC_TABELA_SIMBOLOS_H
#define MUS_SEMANTIC_TABELA_SIMBOLOS_H

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "interpreter/ast.h"

namespace mus {
namespace semantica {

// Tipos que a análise semântica (CMP-13, #95) enxerga. Inteiro e real chegam
// separados do lexer, mas viram o mesmo `Numero` aqui — mesma decisão da AST e
// do interpretador, que não tratam um diferente do outro.
//
// `Erro` não é um tipo da linguagem: é o que uma expressão já reportada como
// errada devolve, para que quem está acima dela não reporte o mesmo problema de
// novo (`x + 1 + 2` com `x` indefinido dá um erro, não três).
enum class Tipo { Numero, Nota, Booleano, Erro };

const char* nomeTipo(Tipo tipo);

struct Simbolo {
    std::string nome;
    Tipo tipo = Tipo::Erro;

    // 0 é o escopo global; cada bloco `{}` aberto soma 1.
    int nivelEscopo = 0;

    // Slot sequencial, único no programa inteiro e nunca reaproveitado — o
    // "endereço" que um gerador de código usaria para guardar o valor. Hoje o
    // interpretador ainda indexa variáveis por nome, então isto é informativo
    // (aparece no `--simbolos`).
    std::size_t endereco = 0;

    ast::Posicao declaracao;
    std::vector<ast::Posicao> usos;  // leituras e reatribuições, em ordem
};

// Tabela de símbolos com escopos aninhados: uma pilha de escopos, cada um
// mapeando nome -> índice em `simbolos_`. Buscar desce do escopo mais interno
// para o mais externo, que é o que faz um bloco enxergar as variáveis de fora e
// não o contrário.
//
// Os símbolos em si nunca são apagados: fechar um escopo só os tira da busca.
// Assim `simbolos()` continua devolvendo a tabela completa depois da análise,
// que é o que o `--simbolos` imprime. Pelo mesmo motivo `buscar` e `declarar`
// devolvem índice, e não ponteiro — `simbolos_` cresce e realocaria.
class TabelaSimbolos {
public:
    static constexpr std::size_t kNaoEncontrado = static_cast<std::size_t>(-1);

    TabelaSimbolos();  // já começa com o escopo global aberto

    void abrirEscopo();

    // Não fecha o escopo global: chamar com só ele aberto não faz nada.
    void fecharEscopo();

    int nivelAtual() const { return static_cast<int>(escopos_.size()) - 1; }

    // Declara no escopo atual e devolve o índice do símbolo novo. Devolve
    // kNaoEncontrado se o nome já existe NESTE escopo — sombrear um nome de
    // escopo de fora é permitido pela tabela; quem decide se a linguagem
    // aceita é o analisador.
    std::size_t declarar(const std::string& nome, Tipo tipo, ast::Posicao posicao);

    // Índice do símbolo visível com esse nome, ou kNaoEncontrado.
    std::size_t buscar(const std::string& nome) const;

    void registrarUso(std::size_t indice, ast::Posicao posicao);

    // Troca o tipo de um símbolo já declarado. Existe por um caso só: um nome
    // cuja primeira atribuição falhou entra na tabela com `Tipo::Erro`, e uma
    // atribuição seguinte com tipo válido precisa poder corrigi-lo — senão o
    // `Erro` fica grudado no símbolo e silencia todo erro posterior que envolva
    // o nome (ver `analisarAtribuicao`).
    void definirTipo(std::size_t indice, Tipo tipo);

    const Simbolo& simbolo(std::size_t indice) const { return simbolos_[indice]; }
    const std::vector<Simbolo>& simbolos() const { return simbolos_; }

private:
    std::vector<std::unordered_map<std::string, std::size_t>> escopos_;
    std::vector<Simbolo> simbolos_;
};

}  // namespace semantica
}  // namespace mus

#endif  // MUS_SEMANTIC_TABELA_SIMBOLOS_H

#ifndef MUS_INTERPRETER_AMBIENTE_H
#define MUS_INTERPRETER_AMBIENTE_H

#include <cstddef>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "interpreter/ast.h"

namespace mus {

// Resultado de avaliar uma Expressao. Notas já resolvidas (MIDI) e literais
// numéricos compartilham o mesmo alternative `double`.
using Valor = std::variant<double, bool>;

class Interpretador;

// Ambiente de execução em pilha de escopos (MAT-06, #34).
//
// Substitui o mapa plano antigo do Interpretador por uma pilha de tabelas hash,
// alinhada à TabelaSimbolos da análise semântica (CMP-13, #95):
// - Buscar um identificador percorre do escopo mais interno (topo) até o
//   global (base), com custo O(N) onde N é a profundidade do escopo.
// - Atribuir atualiza a variável no escopo mais próximo onde ela já existe;
//   se não existir em nenhum escopo visível, declara no escopo atual (topo).
// - DefinirLocal força a criação/sombreamento (shadowing) no escopo atual,
//   usado para parâmetros e variáveis capturadas por uma Closure.
class Ambiente {
public:
    using MapaVariaveis = std::unordered_map<std::string, Valor>;

    Ambiente();  // inicia com o escopo global (nível 0) aberto

    void abrirEscopo();

    // Fecha o escopo no topo da pilha. Nunca fecha o escopo global (nível 0).
    void fecharEscopo();

    int nivelAtual() const { return static_cast<int>(escopos_.size()) - 1; }
    std::size_t profundidade() const { return escopos_.size(); }

    // Cria ou sobrescreve a variável estritamente no escopo atual (topo),
    // permitindo sombrear (shadowing) variáveis de escopos externos.
    void definirLocal(const std::string& nome, Valor valor);

    // Se a variável já existe em algum escopo visível (do topo até a base),
    // atualiza o valor lá. Caso contrário, declara no escopo atual (topo).
    void atribuir(const std::string& nome, Valor valor);

    // Busca do escopo mais interno para o mais externo em O(N), onde N é a
    // profundidade de escopos abertos. Devolve nullptr se não existir.
    const Valor* buscar(const std::string& nome) const;

    // Captura uma fotografia (snapshot) de todas as variáveis visíveis no
    // momento da chamada, respeitando o sombreamento (escopos internos têm
    // prioridade sobre os externos). Base do escopo léxico para Closures.
    MapaVariaveis capturarEstado() const;

private:
    std::vector<MapaVariaveis> escopos_;
};

// Guardião RAII para garantir que todo escopo aberto seja fechado mesmo em
// caso de retorno antecipado por erro de execução.
class EscopoGuard {
public:
    explicit EscopoGuard(Ambiente& ambiente) : ambiente_(ambiente) {
        ambiente_.abrirEscopo();
    }
    ~EscopoGuard() { ambiente_.fecharEscopo(); }

    EscopoGuard(const EscopoGuard&) = delete;
    EscopoGuard& operator=(const EscopoGuard&) = delete;

private:
    Ambiente& ambiente_;
};

// Estrutura que emula Closures e Functores via ponteiros de função e blocos da
// AST (MAT-06, #34). Retém o estado léxico das variáveis visíveis no momento
// da declaração para uso interno isolado quando a função for invocada.
struct Closure {
    using PonteiroFuncao = Valor (*)(Ambiente& ambiente,
                                     const std::vector<Valor>& argumentos);

    std::vector<std::string> parametros;
    Ambiente::MapaVariaveis estadoCapturado;
    PonteiroFuncao funcao = nullptr;
    const std::vector<ast::Comando*>* corpoAst = nullptr;

    // Cria uma Closure capturando imediatamente o estado atual do ambiente.
    static Closure capturar(const Ambiente& ambiente,
                            std::vector<std::string> parametros,
                            PonteiroFuncao funcao);

    static Closure capturarBloco(const Ambiente& ambiente,
                                 std::vector<std::string> parametros,
                                 const std::vector<ast::Comando*>* corpoAst);

    // Operador Functor: abre um escopo isolado no ambiente, injeta o estado
    // léxico capturado na declaração, vincula os argumentos aos parâmetros e
    // executa o ponteiro de função, restaurando o escopo ao sair.
    Valor operator()(Ambiente& ambiente,
                     const std::vector<Valor>& argumentos = {}) const;
};

}  // namespace mus

#endif  // MUS_INTERPRETER_AMBIENTE_H

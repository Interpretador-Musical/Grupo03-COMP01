/**
 * @file ambiente.h
 * @brief Pilha de escopos léxicos, guarda RAII e infraestrutura de closures (MAT-06, #34).
 */

#ifndef MUS_INTERPRETER_AMBIENTE_H
#define MUS_INTERPRETER_AMBIENTE_H

#include <cstddef>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "interpreter/ast.h"

namespace mus {

/**
 * @brief Resultado da avaliação em tempo de execução de uma `ast::Expressao`.
 *
 * Notas resolvidas (MIDI) e números utilizam `double`; booleanos utilizam
 * `bool`; nomes de samples WAV utilizam `std::string`.
 */
using Valor = std::variant<double, bool, std::string>;

class Interpretador;

/**
 * @brief Ambiente de execução baseado em pilha de escopos léxicos (MAT-06, #34).
 *
 * Alinhado à `TabelaSimbolos` da análise semântica (CMP-13, #95):
 * - Buscar percorre do escopo mais interno (topo) até o global (base) em O(N).
 * - Atribuir atualiza a variável no escopo mais próximo onde já existe, ou a
 *   declara no topo se ainda não existir em nenhum escopo visível.
 * - `definirLocal` cria/sombreia estritamente no topo (parâmetros e capturas).
 */
class Ambiente {
public:
    /** @brief Tabela hash de variáveis pertencentes a um único nível de escopo. */
    using MapaVariaveis = std::unordered_map<std::string, Valor>;

    /** @brief Inicializa o ambiente com o escopo global (nível 0) aberto. */
    Ambiente();

    /** @brief Abre um novo escopo léxico no topo da pilha. */
    void abrirEscopo();

    /**
     * @brief Fecha o escopo no topo da pilha.
     *
     * Nunca remove o escopo global (nível 0).
     */
    void fecharEscopo();

    /**
     * @brief Retorna o índice do escopo ativo no topo (`0` = global).
     * @return Nível de aninhamento atual.
     */
    int nivelAtual() const { return static_cast<int>(escopos_.size()) - 1; }

    /**
     * @brief Retorna o número total de escopos empilhados (mínimo 1).
     * @return Quantidade de níveis abertos.
     */
    std::size_t profundidade() const { return escopos_.size(); }

    /**
     * @brief Cria ou sobrescreve a variável estritamente no escopo atual (topo).
     *
     * Permite sombrear (*shadowing*) variáveis de escopos externos.
     *
     * @param nome Identificador da variável.
     * @param valor Valor atribuído.
     */
    void definirLocal(const std::string& nome, Valor valor);

    /**
     * @brief Atualiza a variável visível mais próxima ou declara no topo.
     * @param nome Identificador da variável.
     * @param valor Novo valor associado.
     */
    void atribuir(const std::string& nome, Valor valor);

    /**
     * @brief Busca uma variável do escopo mais interno ao mais externo.
     * @param nome Identificador procurado.
     * @return Ponteiro constante para o valor armazenado, ou `nullptr` se ausente.
     */
    const Valor* buscar(const std::string& nome) const;

    /**
     * @brief Captura um snapshot de todas as variáveis visíveis no momento atual.
     *
     * Respeita o sombreamento (escopos internos prevalecem sobre os externos),
     * servindo de base para a captura léxica de `Closure`.
     *
     * @return Mapa consolidado com o estado léxico capturado.
     */
    MapaVariaveis capturarEstado() const;

private:
    std::vector<MapaVariaveis> escopos_;
};

/**
 * @brief Guardião RAII que abre um escopo na construção e o fecha na destruição.
 *
 * Garante fechamento seguro de escopos mesmo em retornos antecipados por erro.
 */
class EscopoGuard {
public:
    /**
     * @brief Abre um novo escopo no ambiente informado.
     * @param ambiente Referência para o ambiente gerenciado.
     */
    explicit EscopoGuard(Ambiente& ambiente) : ambiente_(ambiente) {
        ambiente_.abrirEscopo();
    }

    /** @brief Fecha o escopo aberto pelo guardião. */
    ~EscopoGuard() { ambiente_.fecharEscopo(); }

    EscopoGuard(const EscopoGuard&) = delete;
    EscopoGuard& operator=(const EscopoGuard&) = delete;

private:
    Ambiente& ambiente_;
};

/**
 * @brief Estrutura de Closure para funções musicais e functores (MAT-06, #34).
 *
 * Retém o snapshot léxico das variáveis visíveis no momento da declaração, os
 * nomes dos parâmetros formais e o ponteiro de função C++ ou bloco da AST.
 */
struct Closure {
    /** @brief Assinatura de função C++ invocável por uma closure. */
    using PonteiroFuncao = Valor (*)(Ambiente& ambiente,
                                     const std::vector<Valor>& argumentos);

    std::vector<std::string> parametros;           ///< Nomes dos parâmetros formais.
    Ambiente::MapaVariaveis estadoCapturado;       ///< Variáveis capturadas na declaração.
    PonteiroFuncao funcao = nullptr;               ///< Callback C++ (quando aplicável).
    const std::vector<ast::Comando*>* corpoAst = nullptr; ///< Corpo em AST (quando aplicável).

    /**
     * @brief Cria uma closure vinculada a um ponteiro de função C++.
     * @param ambiente Ambiente cujo estado visível será capturado.
     * @param parametros Lista ordenada de nomes dos parâmetros.
     * @param funcao Ponteiro para a função executável.
     * @return Instância de `Closure` pronta para invocação.
     */
    static Closure capturar(const Ambiente& ambiente,
                            std::vector<std::string> parametros,
                            PonteiroFuncao funcao);

    /**
     * @brief Cria uma closure vinculada a um bloco de comandos da AST.
     * @param ambiente Ambiente cujo estado visível será capturado.
     * @param parametros Lista ordenada de nomes dos parâmetros.
     * @param corpoAst Lista de comandos da AST executada na invocação.
     * @return Instância de `Closure` com o contexto capturado.
     */
    static Closure capturarBloco(const Ambiente& ambiente,
                                 std::vector<std::string> parametros,
                                 const std::vector<ast::Comando*>* corpoAst);

    /**
     * @brief Executa a closure em escopo isolado com passagem posicional de argumentos.
     *
     * Abre escopo no ambiente, injeta o estado léxico capturado, vincula cada
     * argumento ao parâmetro correspondente, invoca `funcao` e fecha os escopos.
     *
     * @param ambiente Ambiente onde os escopos temporários serão abertos.
     * @param argumentos Valores posicionais passados na chamada.
     * @return Valor retornado pela execução da função.
     */
    Valor operator()(Ambiente& ambiente,
                     const std::vector<Valor>& argumentos = {}) const;
};

}  // namespace mus

#endif  // MUS_INTERPRETER_AMBIENTE_H

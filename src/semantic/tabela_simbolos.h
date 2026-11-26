/**
 * @file tabela_simbolos.h
 * @brief Tipos estáticos e tabela de símbolos com escopos aninhados (CMP-13, #95).
 */

#ifndef MUS_SEMANTIC_TABELA_SIMBOLOS_H
#define MUS_SEMANTIC_TABELA_SIMBOLOS_H

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "interpreter/ast.h"

namespace mus {
namespace semantica {

/**
 * @brief Tipos estáticos reconhecidos pela análise semântica.
 *
 * `Inteiro` e `Real` chegam separados do lexer, mas convergem para `Numero`
 * (mesma regra da AST e do interpretador). `Erro` é um tipo sentinela interno
 * que evita erros em cascata em expressões compostas (ex.: `x + 1 + 2` com
 * `x` indefinido reporta apenas uma falha).
 */
enum class Tipo {
    Numero,   ///< Valores numéricos inteiros e reais.
    Nota,     ///< Alturas musicais (`do4`, `la4 - 2`, etc.).
    Booleano, ///< Valores lógicos (`verdadeiro`, `falso`, comparações).
    String,   ///< Nomes textuais de samples WAV (INT-05).
    Erro      ///< Sentinela interna para propagação silenciosa de erro já reportado.
};

/**
 * @brief Devolve a representação textual de um `Tipo` para mensagens e `--simbolos`.
 * @param tipo Tipo semântico consultado.
 * @return Nome legível do tipo (`"numero"`, `"nota"`, `"booleano"`, `"string"`, `"erro"`).
 */
const char* nomeTipo(Tipo tipo);

/**
 * @brief Registro histórico de um símbolo declarado no programa.
 */
struct Simbolo {
    std::string nome;                     ///< Identificador da variável.
    Tipo tipo = Tipo::Erro;               ///< Tipo inferido na primeira atribuição.
    int nivelEscopo = 0;                  ///< Profundidade do escopo (`0` = global; cada `{}` soma 1).
    std::size_t endereco = 0;             ///< Slot sequencial único exibido em `--simbolos`.
    ast::Posicao declaracao;              ///< Linha e coluna da declaração no código `.mus`.
    std::vector<ast::Posicao> usos;       ///< Posições de leituras e reatribuições, em ordem.
};

/**
 * @brief Tabela de símbolos com pilha de escopos léxicos aninhados.
 *
 * Mantém uma pilha de mapas `nome -> índice` para busca do escopo mais interno
 * ao global. Símbolos nunca são removidos de `simbolos_` quando um escopo fecha,
 * preservando o histórico completo para exibição via `--simbolos` e mantendo
 * índices estáveis mesmo quando o vetor cresce.
 */
class TabelaSimbolos {
public:
    /** @brief Valor sentinela devolvido quando um símbolo não é encontrado. */
    static constexpr std::size_t kNaoEncontrado = static_cast<std::size_t>(-1);

    /** @brief Constrói a tabela já com o escopo global (nível 0) aberto. */
    TabelaSimbolos();

    /** @brief Empilha um novo escopo léxico local. */
    void abrirEscopo();

    /**
     * @brief Fecha o escopo local no topo da pilha.
     *
     * Nunca fecha o escopo global: se apenas o nível 0 estiver aberto, não faz nada.
     */
    void fecharEscopo();

    /**
     * @brief Retorna o nível de aninhamento do escopo ativo no topo (`0` = global).
     * @return Profundidade atual (base 0).
     */
    int nivelAtual() const { return static_cast<int>(escopos_.size()) - 1; }

    /**
     * @brief Declara um novo símbolo estritamente no escopo atual.
     *
     * Sombrear nomes de escopos externos é permitido pela tabela; políticas de
     * linguagem ficam a cargo de `AnalisadorSemantico`.
     *
     * @param nome Nome do identificador.
     * @param tipo Tipo inferido para o símbolo.
     * @param posicao Posição da declaração no arquivo-fonte.
     * @return Índice em `simbolos()`, ou `kNaoEncontrado` se já existir no escopo atual.
     */
    std::size_t declarar(const std::string& nome, Tipo tipo, ast::Posicao posicao);

    /**
     * @brief Procura um símbolo visível do escopo atual até o global.
     * @param nome Nome do identificador procurado.
     * @return Índice em `simbolos()`, ou `kNaoEncontrado` se não estiver visível.
     */
    std::size_t buscar(const std::string& nome) const;

    /**
     * @brief Acrescenta uma posição de uso (leitura ou reatribuição) ao símbolo.
     * @param indice Índice válido em `simbolos()`.
     * @param posicao Posição do uso no arquivo-fonte.
     */
    void registrarUso(std::size_t indice, ast::Posicao posicao);

    /**
     * @brief Atualiza o tipo de um símbolo já registrado.
     *
     * Permite recuperar um símbolo marcado inicialmente com `Tipo::Erro` após
     * uma atribuição posterior com tipo válido (ver `analisarAtribuicao`).
     *
     * @param indice Índice válido em `simbolos()`.
     * @param tipo Novo tipo associado ao símbolo.
     */
    void definirTipo(std::size_t indice, Tipo tipo);

    /**
     * @brief Acessa o registro completo de um símbolo pelo índice.
     * @param indice Índice válido obtido por `declarar()` ou `buscar()`.
     * @return Referência constante para o `Simbolo`.
     */
    const Simbolo& simbolo(std::size_t indice) const { return simbolos_[indice]; }

    /**
     * @brief Retorna todo o histórico de símbolos declarados no programa.
     * @return Vetor constante com todos os símbolos registrados, em ordem de criação.
     */
    const std::vector<Simbolo>& simbolos() const { return simbolos_; }

private:
    std::vector<std::unordered_map<std::string, std::size_t>> escopos_;
    std::vector<Simbolo> simbolos_;
};

}  // namespace semantica
}  // namespace mus

#endif  // MUS_SEMANTIC_TABELA_SIMBOLOS_H

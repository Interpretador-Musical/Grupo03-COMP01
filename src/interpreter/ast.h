/**
 * @file ast.h
 * @brief Definição dos nós, posições e do dono da AST (`Arvore`).
 *
 * Memória (MAT-10, #55): todos os nós de uma árvore moram numa `Arena`, e o
 * dono da árvore é a `Arvore` no fim deste arquivo, que guarda a arena e a
 * raiz. Nenhum nó é liberado sozinho. Os ponteiros entre nós são crus e não
 * dizem quem é dono de quem: valem enquanto a `Arvore` viver. Destruir a
 * árvore não visita nenhum nó, porque os nós são trivialmente destrutíveis
 * (ver os `static_assert` no fim do arquivo) e os textos ficam copiados na
 * própria arena, como `std::string_view`. Só as listas de comandos
 * (`std::vector`) têm destrutor, e a arena guarda um registro delas.
 *
 * Antes da MAT-10, cada nó era alocado com `new` e liberado por uma função
 * `liberar()` que descia a árvore inteira, com `%destructor` no Bison para o
 * que sobrava num erro de sintaxe (MAT-11, #60).
 */

#ifndef MUS_INTERPRETER_AST_H
#define MUS_INTERPRETER_AST_H

#include <cstddef>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "interpreter/arena.h"

namespace mus {
namespace ast {

/**
 * @brief Operadores binários suportados pelas expressões da linguagem.
 *
 * AST mínima da INT-03 (#19): cobre atribuição, toca, pausa, andamento,
 * oitava, volume, repita+bloco, grupo de trilhas e expressões aritméticas e
 * lógicas sobre nós `struct` simples em `std::variant` (C++17).
 *
 * @note `ELogico`/`OuLogico` (e não `E`/`Ou`) existem porque o Bison gera
 * `#define E <código>` para o token `E` da gramática (ver `TipoToken::OperadorE`
 * em `frontend.h`). Em versões como o Bison 2.3 do macOS, um enumerador chamado
 * `E` seria substituído pela macro no meio do `enum`, quebrando a compilação.
 */
enum class OperadorBinario {
    Soma,          ///< Soma aritmética ou transposição `nota + numero`.
    Subtracao,     ///< Subtração aritmética, transposição ou intervalo.
    Multiplicacao, ///< Multiplicação aritmética.
    Divisao,       ///< Divisão aritmética.
    Igual,         ///< Igualdade (`==`).
    Diferente,     ///< Diferença (`!=`).
    Menor,         ///< Menor que (`<`).
    Maior,         ///< Maior que (`>`).
    MenorOuIgual,  ///< Menor ou igual (`<=`).
    MaiorOuIgual,  ///< Maior ou igual (`>=`).
    ELogico,       ///< Conjunção lógica (`e`).
    OuLogico       ///< Disjunção lógica (`ou`).
};

/**
 * @brief Operadores unários de prefixo da linguagem.
 */
enum class OperadorUnario {
    Negacao, ///< Menos unário (`-x`).
    Nao      ///< Negação lógica (`nao` / `não`).
};

/**
 * @brief Posição de origem de um nó no código-fonte `.mus`.
 *
 * Linha e coluna começam em 1, seguindo a convenção de `Token`, para que a
 * análise semântica (CMP-13, #95) reporte `"Linha X, Coluna Y"`. Zero indica
 * nó sintético sem posição (ex.: montado manualmente em testes).
 */
struct Posicao {
    int linha = 0;  ///< Linha base 1 no arquivo-fonte (0 se sintético).
    int coluna = 0; ///< Coluna base 1 no arquivo-fonte (0 se sintético).
};

/**
 * @brief Limite máximo de profundidade permitido na AST (CMP-08, #44).
 *
 * Semântica, interpretador e dobra de constantes descem a árvore
 * recursivamente; uma árvore muito funda estouraria a pilha do processo (ex.:
 * `x = 1+1+...+1` com 10 000 termos, já que a recursão à esquerda mantém a
 * pilha do Bison rasa). O parser recusa qualquer nó que exceda este teto,
 * mantendo margem segura mesmo com AddressSanitizer ou threads de pilha
 * reduzida. A liberação recursiva, que também descia a árvore, saiu com a
 * arena da MAT-10 (#55).
 */
constexpr int kProfundidadeMaxima = 256;

struct Expressao;

/**
 * @brief Literal numérico (inteiro ou real armazenado em `double`).
 */
struct ExpressaoLiteralNumero {
    double valor = 0.0; ///< Valor numérico do literal.
};

/**
 * @brief Literal booleano (`verdadeiro` ou `falso`).
 */
struct ExpressaoLiteralBooleano {
    bool valor = false; ///< Valor lógico do literal.
};

/**
 * @brief Literal textual usado para identificar samples WAV pelo nome (INT-05).
 */
struct ExpressaoLiteralString {
    std::string_view valor; ///< Conteúdo sem as aspas, copiado para a arena da árvore (MAT-10).
};

/**
 * @brief Referência a uma nota musical com oitava opcional.
 *
 * Não resolve o valor MIDI no parser: `oitava == -1` indica uso da oitava
 * corrente do interpretador em tempo de execução, pois uma mesma nota dentro
 * de um `repita` pode ser revisitada após um comando `oitava`.
 */
struct ExpressaoNota {
    int semitom = 0; ///< Deslocamento em semitons dentro da oitava.
    int oitava = -1; ///< Oitava explícita (0..9) ou -1 para oitava corrente.
};

/**
 * @brief Referência a uma variável pelo identificador.
 */
struct ExpressaoIdentificador {
    std::string_view nome; ///< Nome da variável, copiado para a arena da árvore (MAT-10).
};

/**
 * @brief Expressão unária aplicada sobre um operando.
 */
struct ExpressaoUnaria {
    OperadorUnario operador = OperadorUnario::Negacao; ///< Operador unário aplicado.
    Expressao* operando = nullptr;                    ///< Subexpressão operando.
};

/**
 * @brief Expressão binária entre duas subexpressões.
 */
struct ExpressaoBinaria {
    OperadorBinario operador = OperadorBinario::Soma; ///< Operador binário aplicado.
    Expressao* esquerda = nullptr;                    ///< Operando esquerdo.
    Expressao* direita = nullptr;                     ///< Operando direito.
};

/**
 * @brief Nó variante que representa qualquer expressão da AST.
 *
 * Folhas carregam a posição do próprio token; nós binários usam a posição do
 * operando à esquerda; nós unários usam a posição do operador (`-` ou `nao`).
 */
struct Expressao : std::variant<ExpressaoLiteralNumero, ExpressaoLiteralBooleano,
                                ExpressaoLiteralString, ExpressaoNota,
                                ExpressaoIdentificador, ExpressaoUnaria,
                                ExpressaoBinaria> {
    using variant::variant;

    Posicao posicao;      ///< Localização de origem no código-fonte `.mus`.
    int profundidade = 1; ///< 1 em folhas; 1 + maior profundidade dos filhos nos demais.
};

struct Comando;

/**
 * @brief Comando de atribuição (`nome = valor`), que declara ou atualiza variável.
 */
struct ComandoAtribuicao {
    std::string_view nome;        ///< Nome à esquerda do `=`, copiado para a arena (MAT-10).
    Expressao* valor = nullptr;   ///< Expressão atribuída.
    Posicao posicao;              ///< Posição do identificador à esquerda do `=`.
};

/**
 * @brief Comando `toca <altura> por <duracao>`.
 */
struct ComandoToca {
    Expressao* altura = nullptr;  ///< Expressão de altura (nota, MIDI ou nome de sample).
    Expressao* duracao = nullptr; ///< Expressão numérica de duração em tempos.
};

/**
 * @brief Comando `pausa por <duracao>`.
 */
struct ComandoPausa {
    Expressao* duracao = nullptr; ///< Expressão numérica de duração da pausa em tempos.
};

/**
 * @brief Comando `andamento <valor>` que altera o BPM a partir do cursor atual.
 */
struct ComandoAndamento {
    Expressao* valor = nullptr; ///< Expressão numérica do novo andamento em BPM.
};

/**
 * @brief Comando `oitava <valor>` que altera a oitava padrão de notas sem oitava.
 */
struct ComandoOitava {
    Expressao* valor = nullptr; ///< Expressão numérica da nova oitava (`-1` a `9`).
};

/**
 * @brief Comando `volume <valor>` que altera o ganho corrente (`0.0` a `1.0`).
 */
struct ComandoVolume {
    Expressao* valor = nullptr; ///< Expressão numérica do novo volume.
};

/**
 * @brief Comando `filtro <valor>` que define a frequência de corte do passa-baixa BiQuad (DSP-11, #62).
 *
 * `0` desliga o filtro (bypass); valores entre `20` e `20000` ativam o corte em Hz.
 */
struct ComandoFiltro {
    Expressao* valor = nullptr; ///< Expressão numérica da frequência de corte em Hz (`0` = desligado).
};

/**
 * @brief Comando de repetição `repita <vezes> vezes { <corpo> }`.
 */
struct ComandoRepita {
    Expressao* vezes = nullptr;              ///< Quantidade de iterações (truncada para inteiro).
    std::vector<Comando*>* corpo = nullptr;  ///< Sequência de comandos do bloco.
};

/**
 * @brief Grupo de trilhas paralelas `{ a, b, ... }` (CMP-07, #39).
 *
 * Cada trilha é uma sequência de comandos iniciada no mesmo instante de tempo.
 * A gramática exige ao menos duas trilhas separadas por vírgula; qualquer trilha
 * individual pode ser vazia.
 */
struct ComandoGrupo {
    std::vector<std::vector<Comando*>*>* trilhas = nullptr; ///< Lista de trilhas paralelas.
};

/**
 * @brief Nó variante que representa qualquer comando executável da AST.
 */
struct Comando : std::variant<ComandoAtribuicao, ComandoToca, ComandoPausa,
                               ComandoAndamento, ComandoOitava, ComandoVolume,
                               ComandoFiltro, ComandoRepita, ComandoGrupo> {
    using variant::variant;

    int profundidade = 1; ///< 1 + maior profundidade entre expressões, corpo ou trilhas.
};

// A destruição em O(blocos) da MAT-10 depende disto: um campo novo com
// destrutor (`std::string`, `std::vector` por valor, `unique_ptr`) faria cada
// nó precisar de um registro na arena. Texto vai para a arena com
// `Arena::copiarTexto`, e listas entram por ponteiro, criadas pela arena.
static_assert(std::is_trivially_destructible_v<Expressao>,
              "nó de expressão com destrutor quebra a destruição da arena (MAT-10)");
static_assert(std::is_trivially_destructible_v<Comando>,
              "nó de comando com destrutor quebra a destruição da arena (MAT-10)");

/**
 * @brief A AST de um programa inteiro, com dono: a arena dos nós e a raiz.
 *
 * Devolvida por `parseToAst()`. Quando ela sai de escopo, a arena vai junto e
 * leva todos os nós (MAT-10, #55). Só pode ser movida: mover transfere a
 * arena, e a origem fica vazia (`== nullptr`).
 *
 * A interface imita a do `std::unique_ptr` que ela substituiu (`*`, `->`,
 * `get()`, comparação com `nullptr`), para quem só lê a árvore não mudar.
 */
class Arvore {
public:
    /// Árvore vazia: `== nullptr`, sem arena.
    Arvore() = default;

    /**
     * @brief Toma posse de uma arena e da raiz que foi montada nela.
     * @param arena Arena onde estão todos os nós da árvore.
     * @param raiz Lista de comandos do programa, criada pela própria arena.
     */
    Arvore(std::unique_ptr<Arena> arena, std::vector<Comando*>* raiz)
        : arena_(std::move(arena)), raiz_(raiz) {}

    Arvore(Arvore&& outra) noexcept
        : arena_(std::move(outra.arena_)), raiz_(std::exchange(outra.raiz_, nullptr)) {}

    Arvore& operator=(Arvore&& outra) noexcept {
        arena_ = std::move(outra.arena_);
        raiz_ = std::exchange(outra.raiz_, nullptr);
        return *this;
    }

    Arvore(const Arvore&) = delete;
    Arvore& operator=(const Arvore&) = delete;

    /**
     * @brief Árvore com arena própria e raiz vazia, para montar nós à mão.
     *
     * Os nós acrescentados à raiz precisam vir de `arena()`.
     */
    static Arvore nova() {
        auto arena = std::make_unique<Arena>();
        std::vector<Comando*>* raiz = arena->criar<std::vector<Comando*>>();
        return Arvore(std::move(arena), raiz);
    }

    std::vector<Comando*>& operator*() const { return *raiz_; }
    std::vector<Comando*>* operator->() const { return raiz_; }
    std::vector<Comando*>* get() const { return raiz_; }
    explicit operator bool() const { return raiz_ != nullptr; }

    /// A arena que guarda os nós. Só existe numa árvore não vazia.
    Arena& arena() const { return *arena_; }

    /// Solta a árvore inteira de uma vez e fica vazia.
    void reset() {
        raiz_ = nullptr;
        arena_.reset();
    }

    friend bool operator==(const Arvore& arvore, std::nullptr_t) { return !arvore; }
    friend bool operator==(std::nullptr_t, const Arvore& arvore) { return !arvore; }
    friend bool operator!=(const Arvore& arvore, std::nullptr_t) { return !!arvore; }
    friend bool operator!=(std::nullptr_t, const Arvore& arvore) { return !!arvore; }

private:
    std::unique_ptr<Arena> arena_;
    std::vector<Comando*>* raiz_ = nullptr;
};

}  // namespace ast
}  // namespace mus

#endif  // MUS_INTERPRETER_AST_H

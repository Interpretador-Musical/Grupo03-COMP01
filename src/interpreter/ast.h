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

// AST mínima da INT-03 (#19): só o suficiente para o subconjunto de gramática
// que o parser já reconhece hoje (atribuição, toca, pausa, andamento, oitava,
// volume, repita+bloco, grupo de trilhas, expressões aritméticas/lógicas). Sem
// polimorfismo nem `unique_ptr` — isso é trabalho da CMP-04 (#21), quando
// `se`/`senão`/`defina` precisarem de nós que se aninhem de verdade. Os nós
// aqui são `struct`s simples dentro de `std::variant` (C++17).
//
// Memória (MAT-10, #55): todos os nós de uma árvore moram numa `Arena`, e quem
// é dono da árvore é a `Arvore` no fim deste arquivo, que guarda a arena e a
// raiz. Nenhum nó é liberado sozinho. Os ponteiros entre nós são crus e não
// dizem quem é dono de quem: valem enquanto a `Arvore` viver. Destruir a
// árvore não visita nenhum nó, porque os nós são trivialmente destrutíveis
// (ver os `static_assert` no fim do arquivo) e os textos ficam copiados na
// própria arena, como `std::string_view`. Só as listas de comandos
// (`std::vector`) têm destrutor, e a arena guarda um registro delas.
//
// Antes da MAT-10, cada nó era alocado com `new` e liberado por uma função
// `liberar()` que descia a árvore inteira, com `%destructor` no Bison para o
// que sobrava num erro de sintaxe (MAT-11, #60).

// `ELogico`/`OuLogico`, e não `E`/`Ou`: o Bison gera `#define E <código>` para
// o token `E` da gramática (ver TipoToken::OperadorE em frontend.h, que já
// documentava esse mesmo cuidado). Num Bison que emite os códigos de token
// como macro de pré-processador em vez de enum escopado — como o 2.3 que o
// macOS ainda traz de fábrica —, um enumerador chamado `E` vira `277` no meio
// da própria declaração do enum, quebrando a build só nessa plataforma.
enum class OperadorBinario {
    Soma,
    Subtracao,
    Multiplicacao,
    Divisao,
    Igual,
    Diferente,
    Menor,
    Maior,
    MenorOuIgual,
    MaiorOuIgual,
    ELogico,
    OuLogico
};

enum class OperadorUnario { Negacao, Nao };

// Onde o nó começa no `.mus`, ambas começando em 1 — a mesma convenção do
// `Token`. Existe para a análise semântica da CMP-13 (#95) apontar
// "Linha X, Coluna Y" num erro que só aparece depois do parse. Zero quer dizer
// "sem posição" (nó montado à mão, como nos testes).
struct Posicao {
    int linha = 0;
    int coluna = 0;
};

// Quantos nós cabem no caminho mais longo da raiz até uma folha — CMP-08
// (#44). Semântica, interpretador e dobra de constantes descem a árvore por
// recursão, e numa árvore funda o bastante isso estoura a pilha do processo
// (a liberação recursiva, que também descia, saiu com a MAT-10): medido em
// Debug no macOS, `x = 1+1+...+1` com 10 000 termos derrubava o interpretador.
// O limite da pilha do próprio Bison não protege, porque a soma é recursiva à
// esquerda e mantém a pilha dele rasa enquanto a árvore cresce.
//
// O parser recusa o nó que passaria disto, então uma árvore funda demais nunca
// chega a existir. 256 fica muito acima do que um programa escrito à mão
// alcança e muito abaixo do que derruba a pilha, mesmo com AddressSanitizer
// ou numa thread de pilha pequena.
constexpr int kProfundidadeMaxima = 256;

struct Expressao;

struct ExpressaoLiteralNumero {
    double valor = 0.0;
};

struct ExpressaoLiteralBooleano {
    bool valor = false;
};

// Literal textual utilizado para identificar samples pelo nome (INT-05). O
// texto, sem as aspas, é copiado para a arena da árvore (MAT-10).
struct ExpressaoLiteralString {
    std::string_view valor;
};

// Não resolve o MIDI aqui: oitava == -1 significa "usa a oitava corrente do
// interpretador" em tempo de execução, e não em tempo de parse — a mesma nota
// dentro de um `repita` pode ser revisitada depois que um comando `oitava`
// mudou o estado, então resolver cedo demais congelaria a oitava errada.
struct ExpressaoNota {
    int semitom = 0;
    int oitava = -1;
};

// O nome também mora na arena da árvore, como o texto da string.
struct ExpressaoIdentificador {
    std::string_view nome;
};

struct ExpressaoUnaria {
    OperadorUnario operador = OperadorUnario::Negacao;
    Expressao* operando = nullptr;
};

struct ExpressaoBinaria {
    OperadorBinario operador = OperadorBinario::Soma;
    Expressao* esquerda = nullptr;
    Expressao* direita = nullptr;
};

struct Expressao : std::variant<ExpressaoLiteralNumero, ExpressaoLiteralBooleano,
                                ExpressaoLiteralString, ExpressaoNota,
                                ExpressaoIdentificador, ExpressaoUnaria,
                                ExpressaoBinaria> {
    using variant::variant;

    // Folhas: posição do próprio token, carimbada pelo lexer. Binária:
    // posição do operando da esquerda. Unária: posição do operador — em
    // `-do4` e `nao x`, quem começa a expressão é o `-` e o `nao`.
    Posicao posicao;

    // 1 numa folha; 1 + a do filho mais fundo nos demais. Ver
    // `kProfundidadeMaxima`.
    int profundidade = 1;
};

struct Comando;

struct ComandoAtribuicao {
    std::string_view nome;  // na arena da árvore, como o do identificador
    Expressao* valor = nullptr;
    Posicao posicao;  // do nome à esquerda do `=`
};

struct ComandoToca {
    Expressao* altura = nullptr;
    Expressao* duracao = nullptr;
};

struct ComandoPausa {
    Expressao* duracao = nullptr;
};

struct ComandoAndamento {
    Expressao* valor = nullptr;
};

struct ComandoOitava {
    Expressao* valor = nullptr;
};

struct ComandoVolume {
    Expressao* valor = nullptr;
};

struct ComandoRepita {
    Expressao* vezes = nullptr;
    std::vector<Comando*>* corpo = nullptr;
};

// Trilhas paralelas `{ a, b }` — CMP-07 (#39). Cada trilha é uma sequência de
// comandos, e todas começam no mesmo instante. Sempre há pelo menos duas (a
// gramática exige a vírgula); uma trilha pode ser vazia.
struct ComandoGrupo {
    std::vector<std::vector<Comando*>*>* trilhas = nullptr;
};

struct Comando : std::variant<ComandoAtribuicao, ComandoToca, ComandoPausa,
                               ComandoAndamento, ComandoOitava, ComandoVolume,
                               ComandoRepita, ComandoGrupo> {
    using variant::variant;

    // 1 + a do filho mais fundo — expressão ou, no `repita` e no grupo,
    // comando do corpo ou das trilhas.
    int profundidade = 1;
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

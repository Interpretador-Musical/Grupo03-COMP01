#ifndef MUS_INTERPRETER_AST_H
#define MUS_INTERPRETER_AST_H

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace mus {
namespace ast {

// AST mínima da INT-03 (#19): só o suficiente para o subconjunto de gramática
// que o parser já reconhece hoje (atribuição, toca, pausa, andamento, oitava,
// volume, repita+bloco, expressões aritméticas/lógicas). Sem polimorfismo nem
// `unique_ptr` — isso é trabalho da CMP-04 (#21), quando `se`/`senão`/`defina`
// precisarem de nós que se aninhem de verdade. Os nós aqui são `struct`s
// simples dentro de `std::variant` (C++17), alocados com `new`.
//
// Quem libera é a raiz: um pai é dono dos filhos, e `liberar()` desce a árvore
// inteira. Até a MAT-11 (#60) os nós nunca eram liberados — o processo vive só
// uma execução —, mas a suíte parseia centenas de programas no mesmo processo,
// e o AddressSanitizer da CI não aceita vazamento. Quando a CMP-04 trocar os
// ponteiros crus por `unique_ptr`, `liberar()` e `LiberaArvore` deixam de ser
// necessários.

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
// (#44). Semântica, interpretador e `liberar()` descem a árvore por recursão,
// e numa árvore funda o bastante isso estoura a pilha do processo: medido em
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

// Não resolve o MIDI aqui: oitava == -1 significa "usa a oitava corrente do
// interpretador" em tempo de execução, e não em tempo de parse — a mesma nota
// dentro de um `repita` pode ser revisitada depois que um comando `oitava`
// mudou o estado, então resolver cedo demais congelaria a oitava errada.
struct ExpressaoNota {
    int semitom = 0;
    int oitava = -1;
};

struct ExpressaoIdentificador {
    std::string nome;
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
                                 ExpressaoNota, ExpressaoIdentificador,
                                 ExpressaoUnaria, ExpressaoBinaria> {
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
    std::string nome;
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

struct Comando : std::variant<ComandoAtribuicao, ComandoToca, ComandoPausa,
                               ComandoAndamento, ComandoOitava, ComandoVolume,
                               ComandoRepita> {
    using variant::variant;

    // 1 + a do filho mais fundo — expressão ou, no `repita`, comando do corpo.
    int profundidade = 1;
};

// Liberam o nó e tudo o que pende dele. Aceitam nullptr, como `delete`.
void liberar(Expressao* expressao);
void liberar(Comando* comando);
void liberar(std::vector<Comando*>* comandos);

struct LiberaArvore {
    void operator()(std::vector<Comando*>* comandos) const { liberar(comandos); }
};

// A AST de um programa inteiro, com dono: some no fim do escopo de quem a
// recebeu de `parseToAst()`, levando os nós junto.
using Arvore = std::unique_ptr<std::vector<Comando*>, LiberaArvore>;

}  // namespace ast
}  // namespace mus

#endif  // MUS_INTERPRETER_AST_H

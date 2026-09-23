#ifndef MUS_INTERPRETER_AST_H
#define MUS_INTERPRETER_AST_H

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
// simples dentro de `std::variant` (C++17), alocados com `new` e nunca
// liberados: o processo do `compilador` vive só o tempo de uma execução, então
// não há dono/tempo de vida para gerenciar ainda.

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
};

struct Comando;

struct ComandoAtribuicao {
    std::string nome;
    Expressao* valor = nullptr;
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
};

}  // namespace ast
}  // namespace mus

#endif  // MUS_INTERPRETER_AST_H

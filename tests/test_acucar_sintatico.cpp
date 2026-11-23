#include "doctest.h"

#include <string>
#include <vector>

#include "core/timeline_emit.h"
#include "frontend/frontend.h"
#include "interpreter/interpreter.h"
#include "semantic/analisador_semantico.h"

// Açúcar sintático — CMP-11 (#59). Figuras rítmicas como macros de duração e
// `vez` como sinônimo de `vezes`. Tudo é resolvido no lexer: o parser recebe os
// mesmos tokens que receberia da forma por extenso, então a gramática não
// muda (o `%expect 0` do parser.y continua valendo). Ver docs/gramatica_ebnf.md.

namespace {

using mus::TipoToken;
using mus::Token;

Token unico(const std::string& fonte) {
    const std::vector<Token> tokens = mus::tokenize(fonte);
    REQUIRE(tokens.size() == 1);
    return tokens[0];
}

// O pipeline inteiro até o CSV do --emit-timeline, ou "erro: ..." na primeira
// fase que recusar.
std::string compilar(const std::string& fonte) {
    std::string erro;
    mus::ast::Arvore programa = mus::parseToAst(fonte, &erro);
    if (programa == nullptr) {
        return "erro: " + erro;
    }
    mus::semantica::AnalisadorSemantico analisador;
    if (!analisador.analisar(*programa)) {
        return "erro: semantico";
    }
    mus::Interpretador interpretador;
    if (!interpretador.executar(*programa)) {
        return "erro: " + interpretador.erro();
    }
    return mus::emitirTimeline(interpretador.timeline());
}

struct Figura {
    const char* nome;
    double tempos;
};

// A semínima é o tempo, e cada figura vale metade da anterior.
const Figura kFiguras[] = {
    {"semibreve", 4.0}, {"minima", 2.0},       {"mínima", 2.0},
    {"seminima", 1.0},  {"semínima", 1.0},     {"colcheia", 0.5},
    {"semicolcheia", 0.25}, {"fusa", 0.125},   {"semifusa", 0.0625},
};

}  // namespace

// ------------------------------------------------------------------ léxico

TEST_CASE("açúcar: cada figura rítmica vira um REAL com a duração em tempos") {
    for (const Figura& figura : kFiguras) {
        CAPTURE(figura.nome);
        const Token token = unico(figura.nome);
        CHECK(token.tipo == TipoToken::Real);
        CHECK(token.lexema == figura.nome);
        CHECK(token.valor == figura.tempos);
    }
}

TEST_CASE("açúcar: palavra maior que a figura continua identificador") {
    // Maior casamento: `colcheias` tem 9 caracteres como IDENT e só 8 como
    // figura. `minima` começa com a nota `mi`, mas casa mais como figura.
    CHECK(unico("colcheias").tipo == TipoToken::Identificador);
    CHECK(unico("fusa2").tipo == TipoToken::Identificador);
    CHECK(unico("semibreves").tipo == TipoToken::Identificador);
    CHECK(unico("minima").tipo == TipoToken::Real);
    CHECK(unico("mi").tipo == TipoToken::Nota);
}

TEST_CASE("açúcar: `vez` é o mesmo token de `vezes`") {
    CHECK(unico("vez").tipo == TipoToken::Vezes);
    CHECK(unico("vezes").tipo == TipoToken::Vezes);
    CHECK(unico("vezinha").tipo == TipoToken::Identificador);
}

// ------------------------------------------------- equivalência com a forma longa

TEST_CASE("açúcar: figura e número geram a mesma Timeline") {
    for (const Figura& figura : kFiguras) {
        CAPTURE(figura.nome);
        const std::string numero = std::to_string(figura.tempos);
        CHECK(compilar(std::string("toca do4 por ") + figura.nome) ==
              compilar("toca do4 por " + numero));
        CHECK(compilar(std::string("pausa por ") + figura.nome) ==
              compilar("pausa por " + numero));
    }
}

TEST_CASE("açúcar: figura entra em expressão como qualquer número") {
    // Semínima pontuada, tercina de colcheia e figura em variável.
    CHECK(compilar("toca do4 por semínima + colcheia") == compilar("toca do4 por 1.5"));
    CHECK(compilar("repita 3 vezes { toca do4 por seminima / 3 }") ==
          compilar("repita 3 vezes { toca do4 por 1 / 3 }"));
    CHECK(compilar("d = colcheia * 3\ntoca do4 por d") == compilar("toca do4 por 1.5"));
    CHECK(compilar("{ toca do4 por minima, toca mi4 por semibreve }") ==
          compilar("{ toca do4 por 2, toca mi4 por 4 }"));
}

TEST_CASE("açúcar: `repita N vez` é `repita N vezes`") {
    CHECK(compilar("repita 1 vez { toca do4 por 1 }") ==
          compilar("repita 1 vezes { toca do4 por 1 }"));
    CHECK(compilar("repita 4 vez { toca do4 por colcheia }") ==
          compilar("repita 4 vezes { toca do4 por 0.5 }"));
}

// --------------------------------------------------- o que passa a ser recusado

TEST_CASE("açúcar: figura é reservada e não pode ser nome de variável") {
    // O preço de resolver no lexer: `colcheia = 2` vira `0.5 = 2`, que não é
    // um comando. Nenhum programa do repositório usava esses nomes.
    for (const char* fonte : {"colcheia = 2", "semínima = 1", "fusa = 0.1"}) {
        CAPTURE(fonte);
        std::string erro;
        CHECK(mus::parseToAst(fonte, &erro) == nullptr);
        CHECK(erro.find("Linha 1, Coluna") != std::string::npos);
    }
}

TEST_CASE("açúcar: a semântica vê a figura como Numero") {
    // Somar figura com nota é número + nota, como `0.5 + do4`; comparar com
    // booleano é recusado como seria com o literal.
    CHECK(compilar("toca do4 + 0 por colcheia") != "erro: semantico");
    CHECK(compilar("toca do4 por colcheia e verdadeiro") == "erro: semantico");
}

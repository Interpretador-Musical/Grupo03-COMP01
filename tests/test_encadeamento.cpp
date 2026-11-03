#include "doctest.h"

#include <string>
#include <vector>

#include "frontend/frontend.h"

// CMP-06 (#33): o encadeamento de métodos (`expr.metodo()`) está adiado — ver
// docs/decisoes_escopo_final.md. Estes testes não implementam nada:
// garantem que a sintaxe de ponto e as listas `[ ]` hoje são recusadas com
// erro limpo (sem cair, sem vazar sob o ASAN) e que os reais continuam
// aceitos. Quando o dot-chaining for retomado, estes casos passam a ser
// reescritos para aceitar a sintaxe.

namespace {

std::string erroDe(const std::string& fonte) {
    std::string erro;
    auto programa = mus::parseToAst(fonte, &erro);
    REQUIRE_MESSAGE(programa == nullptr, "devia ter sido recusado: " << fonte.substr(0, 60));
    return erro;
}

bool comecaCom(const std::string& texto, const std::string& prefixo) {
    return texto.rfind(prefixo, 0) == 0;
}

std::string repetido(int vezes, const std::string& trecho) {
    std::string texto;
    for (int i = 0; i < vezes; ++i) {
        texto += trecho;
    }
    return texto;
}

}  // namespace

TEST_CASE("CMP-06: o ponto de encadeamento é recusado apontando o ponto") {
    // "x = do4.reverse()": o `.` fica na coluna 8.
    CHECK(comecaCom(erroDe("x = do4.reverse()"), "Linha 1, Coluna 8:"));
    // "toca " ocupa 5 colunas, `do4` vai de 6 a 8 e o `.` é a 9.
    CHECK(comecaCom(erroDe("toca do4.reverse() por 1"), "Linha 1, Coluna 9:"));
}

TEST_CASE("CMP-06: a lista [ ] e o exemplo da issue são recusados") {
    // O `[` é o primeiro caractere que a gramática não conhece: coluna 5.
    CHECK(comecaCom(erroDe("x = [do re mi]"), "Linha 1, Coluna 5:"));
    CHECK(comecaCom(erroDe("x = [do re mi].reverse().speed(2)"), "Linha 1, Coluna 5:"));
}

TEST_CASE("CMP-06: erro com expressão já montada na pilha não vaza") {
    // `(1 + 2)` já virou nó quando o `.` aparece; o `%destructor` e o
    // `ast::Arvore` de `parseToAst` precisam recolher tudo. Quem prova é o
    // job do AddressSanitizer.
    CHECK(comecaCom(erroDe("x = (1 + 2).foo()"), "Linha 1,"));
    CHECK(comecaCom(erroDe("repita 2 vezes { toca do4 por 1 + 1.dobra() }"), "Linha 1,"));
}

TEST_CASE("CMP-06: uma cadeia enorme é recusada no primeiro ponto, sem recursão") {
    // A recusa acontece no primeiro `.`; nada é montado nem percorrido, então
    // o tamanho da cadeia não pesa.
    const std::string erro = erroDe("x = do4" + repetido(10000, ".a()"));
    CHECK(comecaCom(erro, "Linha 1, Coluna 8:"));
}

TEST_CASE("CMP-06: o ponto continua sendo só parte dos números reais") {
    CHECK(mus::parseToAst("x = 1.5 + 0.25\ntoca do4 por x") != nullptr);
    CHECK(mus::parseToAst("volume 0.8") != nullptr);
}

TEST_CASE("CMP-06: o lexer entrega o ponto como caractere desconhecido") {
    // Ponto de partida para quando o `.` ganhar token próprio.
    const auto tokens = mus::tokenize("do4.reverse()");
    REQUIRE(tokens.size() == 5);
    CHECK(tokens[1].tipo == mus::TipoToken::Desconhecido);
    CHECK(tokens[1].lexema == ".");
}

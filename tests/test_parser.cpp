#include "doctest.h"

#include <string>

#include "frontend/frontend.h"

namespace {

bool aceita(const std::string& fonte) {
    return mus::parseString(fonte);
}

}  // namespace

// ---------------------------------------------------------------------------
// O que a DoD da CMP-02 (#9) pede explicitamente
// ---------------------------------------------------------------------------

TEST_CASE("atribuição de valor é reconhecida") {
    CHECK(aceita("tempo = 0.5"));
    CHECK(aceita("n = 4"));
    CHECK(aceita("dobro = tempo * 2"));
    CHECK(aceita("ligado = verdadeiro"));
}

TEST_CASE("comando de tocar é reconhecido") {
    CHECK(aceita("toca do4 por 0.5"));
    CHECK(aceita("toca do por tempo"));
    CHECK(aceita("toca sib3 por 1.0"));
}

TEST_CASE("programa vazio é válido") {
    CHECK(aceita(""));
    CHECK(aceita("// só um comentário\n"));
}

// ---------------------------------------------------------------------------
// Demais comandos
// ---------------------------------------------------------------------------

TEST_CASE("comandos de contexto musical são reconhecidos") {
    CHECK(aceita("andamento 120"));
    CHECK(aceita("oitava 4"));
    CHECK(aceita("volume 0.8"));
    CHECK(aceita("pausa por 1.0"));
}

TEST_CASE("repetição com bloco é reconhecida") {
    CHECK(aceita("repita 4 vezes { toca do4 por 0.5 }"));
    CHECK(aceita("repita 4 vezes { }"));
    CHECK(aceita("repita n vezes { repita 2 vezes { pausa por 0.25 } }"));
}

// ---------------------------------------------------------------------------
// Expressões — o NFR pede que a gramática lide com os tipos numéricos, e a
// aritmética é o que justifica isso (transposição de nota, cálculo de duração).
// ---------------------------------------------------------------------------

TEST_CASE("altura e duração aceitam expressões dos dois lados") {
    CHECK(aceita("toca la4 - 2 por tempo * 2"));
    CHECK(aceita("toca (do4 + 12) / 1 por (tempo + 0.25)"));
}

TEST_CASE("menos unário é aceito") {
    // Só é possível porque o `por` separa os dois operandos do `toca`. Sem ele,
    // `toca la4 - 2` seria ambíguo e a gramática teria conflito.
    CHECK(aceita("x = -1"));
    CHECK(aceita("toca do4 por -tempo"));
}

TEST_CASE("comparações e operadores lógicos são aceitos em expressão") {
    CHECK(aceita("x = 1 < 2"));
    CHECK(aceita("x = verdadeiro e falso"));
    CHECK(aceita("x = nao verdadeiro ou 1 >= 2"));
}

// ---------------------------------------------------------------------------
// Programas completos
// ---------------------------------------------------------------------------

TEST_CASE("o programa de referência do site é aceito inteiro") {
    const std::string programa =
        "// Arpejo de Dó maior, quatro compassos.\n"
        "andamento 120\n"
        "tempo = 0.5\n"
        "\n"
        "repita 4 vezes {\n"
        "    toca do4  por tempo\n"
        "    toca mi4  por tempo\n"
        "    toca sol4 por tempo\n"
        "}\n"
        "\n"
        "toca la4 - 2 por tempo * 2   /* transposição e duração calculadas */\n"
        "pausa por 1.0\n";

    CHECK(aceita(programa));
}

// ---------------------------------------------------------------------------
// O que tem de ser recusado
// ---------------------------------------------------------------------------

TEST_CASE("comando de tocar incompleto é recusado") {
    CHECK_FALSE(aceita("toca"));
    CHECK_FALSE(aceita("toca do4"));
    CHECK_FALSE(aceita("toca do4 por"));
}

TEST_CASE("tocar sem o 'por' é recusado") {
    // É justamente o separador que tira a ambiguidade entre altura e duração.
    CHECK_FALSE(aceita("toca do4 tempo"));
    CHECK_FALSE(aceita("toca do4 0.5"));
}

TEST_CASE("repetição malformada é recusada") {
    CHECK_FALSE(aceita("repita 4 { }"));            // falta o `vezes`
    CHECK_FALSE(aceita("repita vezes { }"));        // falta a quantidade
    CHECK_FALSE(aceita("repita 4 vezes"));          // falta o bloco
}

TEST_CASE("bloco não fechado é recusado") {
    CHECK_FALSE(aceita("repita 2 vezes { toca do4 por 0.5"));
    CHECK_FALSE(aceita("}"));
}

TEST_CASE("lixo na entrada é recusado sem derrubar o processo") {
    // O NFR da CMP-03 (#15) pede exatamente isto: erro limpo, não segfault.
    CHECK_FALSE(aceita("toca 4 4 4"));
    CHECK_FALSE(aceita("@#$"));
    CHECK_FALSE(aceita("tempo = = 0.5"));
}

TEST_CASE("a mensagem de erro chega a quem chamou") {
    std::string erro;
    CHECK_FALSE(mus::parseString("toca do4", &erro));
    CHECK_FALSE(erro.empty());

    // Em programa válido a mensagem fica vazia.
    std::string semErro;
    CHECK(mus::parseString("pausa por 1.0", &semErro));
    CHECK(semErro.empty());
}

// ---------------------------------------------------------------------------
// Fronteira declarada deste PR
// ---------------------------------------------------------------------------

TEST_CASE("condicional e função já são tokens, mas ainda não são gramática") {
    // O lexer reconhece `se`, `entao`, `senao`, `defina` e `retorna`; as regras
    // correspondentes entram na CMP-04 (#21), junto com a AST. Este caso existe
    // para que a fronteira seja explícita, e para falhar no dia em que alguém
    // adicionar as regras sem atualizar os testes.
    CHECK_FALSE(aceita("se x > 1 entao { pausa por 1.0 }"));
    CHECK_FALSE(aceita("defina motivo() { pausa por 1.0 }"));
    CHECK_FALSE(aceita("retorna 1"));
}

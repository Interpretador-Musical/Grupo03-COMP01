#include "doctest.h"

#include <string>
#include <vector>

#include "frontend/frontend.h"
#include "interpreter/ast.h"

namespace {

// `parseToAst` devolve `nullptr` num programa que não parseia; os testes de
// aceitação/rejeição da gramática já vivem em `test_parser.cpp` — aqui só
// interessa a forma da árvore de um programa que já se sabe válido.
mus::ast::Arvore arvore(const std::string& fonte) {
    std::string erro;
    auto programa = mus::parseToAst(fonte, &erro);
    REQUIRE_MESSAGE(programa != nullptr, "erro de sintaxe inesperado: " << erro);
    return programa;
}

}  // namespace

TEST_CASE("atribuição simples vira ComandoAtribuicao com nome e literal certos") {
    const auto programa = arvore("tempo = 0.5");
    const auto& comandos = *programa;
    REQUIRE(comandos.size() == 1);

    const auto& atribuicao = std::get<mus::ast::ComandoAtribuicao>(*comandos[0]);
    CHECK(atribuicao.nome == "tempo");

    const auto& literal = std::get<mus::ast::ExpressaoLiteralNumero>(*atribuicao.valor);
    CHECK(literal.valor == doctest::Approx(0.5));
}

TEST_CASE("toca com nota e identificador vira ComandoToca com os operandos certos") {
    const auto programa = arvore("toca do4 por tempo");
    const auto& comandos = *programa;
    REQUIRE(comandos.size() == 1);

    const auto& toca = std::get<mus::ast::ComandoToca>(*comandos[0]);

    const auto& nota = std::get<mus::ast::ExpressaoNota>(*toca.altura);
    CHECK(nota.semitom == 0);
    CHECK(nota.oitava == 4);

    const auto& duracao = std::get<mus::ast::ExpressaoIdentificador>(*toca.duracao);
    CHECK(duracao.nome == "tempo");
}

TEST_CASE("transposição e duração calculada viram nós binários") {
    const auto programa = arvore("toca la4 - 2 por tempo * 2");
    const auto& comandos = *programa;
    REQUIRE(comandos.size() == 1);
    const auto& toca = std::get<mus::ast::ComandoToca>(*comandos[0]);

    const auto& transposicao = std::get<mus::ast::ExpressaoBinaria>(*toca.altura);
    CHECK(transposicao.operador == mus::ast::OperadorBinario::Subtracao);
    const auto& la4 = std::get<mus::ast::ExpressaoNota>(*transposicao.esquerda);
    CHECK(la4.semitom == 9);
    CHECK(la4.oitava == 4);
    const auto& dois = std::get<mus::ast::ExpressaoLiteralNumero>(*transposicao.direita);
    CHECK(dois.valor == doctest::Approx(2.0));

    const auto& duracaoCalculada = std::get<mus::ast::ExpressaoBinaria>(*toca.duracao);
    CHECK(duracaoCalculada.operador == mus::ast::OperadorBinario::Multiplicacao);
}

TEST_CASE("nota sem oitava preserva oitava=-1 no nó") {
    const auto programa = arvore("toca do por tempo");
    const auto& comandos = *programa;
    const auto& toca = std::get<mus::ast::ComandoToca>(*comandos[0]);
    const auto& nota = std::get<mus::ast::ExpressaoNota>(*toca.altura);

    // A AST não resolve a oitava corrente aqui — isso é trabalho do
    // interpretador em tempo de execução (INT-03, #19), porque a mesma nota
    // pode ser revisitada num `repita` depois de um `oitava` mudar o estado.
    CHECK(nota.oitava == -1);
}

TEST_CASE("repita guarda o corpo em ordem") {
    const auto programa = arvore("repita 4 vezes { toca do4 por 0.5 \n toca mi4 por 0.5 }");
    const auto& comandos = *programa;
    REQUIRE(comandos.size() == 1);

    const auto& repita = std::get<mus::ast::ComandoRepita>(*comandos[0]);
    REQUIRE(repita.corpo != nullptr);
    REQUIRE(repita.corpo->size() == 2);

    const auto& primeira = std::get<mus::ast::ComandoToca>(*(*repita.corpo)[0]);
    const auto& segunda = std::get<mus::ast::ComandoToca>(*(*repita.corpo)[1]);
    CHECK(std::get<mus::ast::ExpressaoNota>(*primeira.altura).semitom == 0);
    CHECK(std::get<mus::ast::ExpressaoNota>(*segunda.altura).semitom == 4);
}

TEST_CASE("erro de sintaxe devolve nullptr e preenche erro") {
    std::string erro;
    CHECK(mus::parseToAst("toca do4", &erro) == nullptr);
    CHECK_FALSE(erro.empty());
}

TEST_CASE("programa vazio devolve uma AST vazia, não nullptr") {
    const auto programa = arvore("");
    const auto& comandos = *programa;
    CHECK(comandos.empty());
}

TEST_CASE("os nós carregam a posição de onde começam no texto (CMP-13, #95)") {
    const auto programa = arvore("andamento 120\n  tempo = la4 - 2");
    const auto& comandos = *programa;
    REQUIRE(comandos.size() == 2);

    const auto& atribuicao = std::get<mus::ast::ComandoAtribuicao>(*comandos[1]);
    CHECK(atribuicao.posicao.linha == 2);
    CHECK(atribuicao.posicao.coluna == 3);

    // A binária herda a posição do operando da esquerda; a direita tem a sua.
    CHECK(atribuicao.valor->posicao.linha == 2);
    CHECK(atribuicao.valor->posicao.coluna == 11);
    const auto& binaria = std::get<mus::ast::ExpressaoBinaria>(*atribuicao.valor);
    CHECK(binaria.direita->posicao.coluna == 17);
}

TEST_CASE("a unária começa no operador, não no operando") {
    // Quem começa `-do4` é o `-` (coluna 5), e `nao verdadeiro` é o `nao`
    // (coluna 5) — os dois chegam do `@1` do Bison, não do operando.
    const auto programa = arvore("x = -do4\ny = nao verdadeiro");
    const auto& comandos = *programa;
    REQUIRE(comandos.size() == 2);

    const auto& negacao = std::get<mus::ast::ComandoAtribuicao>(*comandos[0]);
    CHECK(std::get<mus::ast::ExpressaoUnaria>(*negacao.valor).operador ==
          mus::ast::OperadorUnario::Negacao);
    CHECK(negacao.valor->posicao.linha == 1);
    CHECK(negacao.valor->posicao.coluna == 5);
    // O operando continua com a posição dele.
    CHECK(std::get<mus::ast::ExpressaoUnaria>(*negacao.valor).operando->posicao.coluna == 6);

    const auto& nao = std::get<mus::ast::ComandoAtribuicao>(*comandos[1]);
    CHECK(std::get<mus::ast::ExpressaoUnaria>(*nao.valor).operador ==
          mus::ast::OperadorUnario::Nao);
    CHECK(nao.valor->posicao.linha == 2);
    CHECK(nao.valor->posicao.coluna == 5);
    CHECK(std::get<mus::ast::ExpressaoUnaria>(*nao.valor).operando->posicao.coluna == 9);
}

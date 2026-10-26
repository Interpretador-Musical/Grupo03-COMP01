#include "doctest.h"

#include "semantic/tabela_simbolos.h"

using mus::semantica::TabelaSimbolos;
using mus::semantica::Tipo;

TEST_CASE("declarar e buscar no escopo global") {
    TabelaSimbolos tabela;
    CHECK(tabela.nivelAtual() == 0);

    const auto indice = tabela.declarar("tempo", Tipo::Numero, {3, 1});
    REQUIRE(indice != TabelaSimbolos::kNaoEncontrado);
    CHECK(tabela.buscar("tempo") == indice);

    const auto& simbolo = tabela.simbolo(indice);
    CHECK(simbolo.nome == "tempo");
    CHECK(simbolo.tipo == Tipo::Numero);
    CHECK(simbolo.nivelEscopo == 0);
    CHECK(simbolo.declaracao.linha == 3);
    CHECK(simbolo.declaracao.coluna == 1);

    CHECK(tabela.buscar("outro") == TabelaSimbolos::kNaoEncontrado);
}

TEST_CASE("declarar o mesmo nome duas vezes no mesmo escopo é recusado") {
    TabelaSimbolos tabela;
    tabela.declarar("x", Tipo::Numero, {1, 1});
    CHECK(tabela.declarar("x", Tipo::Nota, {2, 1}) == TabelaSimbolos::kNaoEncontrado);
    CHECK(tabela.simbolos().size() == 1);
}

TEST_CASE("escopo interno enxerga o externo, e o nome interno some ao fechar") {
    TabelaSimbolos tabela;
    const auto externo = tabela.declarar("tempo", Tipo::Numero, {1, 1});

    tabela.abrirEscopo();
    CHECK(tabela.nivelAtual() == 1);
    CHECK(tabela.buscar("tempo") == externo);

    const auto interno = tabela.declarar("nota", Tipo::Nota, {3, 5});
    CHECK(tabela.simbolo(interno).nivelEscopo == 1);
    CHECK(tabela.buscar("nota") == interno);

    tabela.fecharEscopo();
    CHECK(tabela.nivelAtual() == 0);
    CHECK(tabela.buscar("nota") == TabelaSimbolos::kNaoEncontrado);
    CHECK(tabela.buscar("tempo") == externo);
}

TEST_CASE("nome do escopo interno sombreia o externo enquanto o bloco está aberto") {
    TabelaSimbolos tabela;
    const auto externo = tabela.declarar("x", Tipo::Numero, {1, 1});

    tabela.abrirEscopo();
    const auto interno = tabela.declarar("x", Tipo::Nota, {2, 1});
    REQUIRE(interno != TabelaSimbolos::kNaoEncontrado);
    CHECK(tabela.buscar("x") == interno);

    tabela.fecharEscopo();
    CHECK(tabela.buscar("x") == externo);
}

TEST_CASE("endereços são sequenciais e não se repetem entre escopos") {
    TabelaSimbolos tabela;
    tabela.declarar("a", Tipo::Numero, {1, 1});
    tabela.abrirEscopo();
    tabela.declarar("b", Tipo::Nota, {2, 1});
    tabela.fecharEscopo();
    tabela.declarar("c", Tipo::Booleano, {3, 1});

    const auto& simbolos = tabela.simbolos();
    REQUIRE(simbolos.size() == 3);
    CHECK(simbolos[0].endereco == 0);
    CHECK(simbolos[1].endereco == 1);
    CHECK(simbolos[2].endereco == 2);
}

TEST_CASE("o histórico sobrevive ao fechamento do escopo") {
    TabelaSimbolos tabela;
    tabela.abrirEscopo();
    tabela.declarar("local", Tipo::Numero, {2, 5});
    tabela.fecharEscopo();

    REQUIRE(tabela.simbolos().size() == 1);
    CHECK(tabela.simbolos()[0].nome == "local");
    CHECK(tabela.simbolos()[0].nivelEscopo == 1);
}

TEST_CASE("fechar com só o escopo global aberto não faz nada") {
    TabelaSimbolos tabela;
    tabela.declarar("x", Tipo::Numero, {1, 1});
    tabela.fecharEscopo();
    CHECK(tabela.nivelAtual() == 0);
    CHECK(tabela.buscar("x") != TabelaSimbolos::kNaoEncontrado);
}

TEST_CASE("registrarUso acumula as posições em ordem") {
    TabelaSimbolos tabela;
    const auto indice = tabela.declarar("tempo", Tipo::Numero, {1, 1});
    tabela.registrarUso(indice, {4, 14});
    tabela.registrarUso(indice, {7, 20});

    const auto& usos = tabela.simbolo(indice).usos;
    REQUIRE(usos.size() == 2);
    CHECK(usos[0].linha == 4);
    CHECK(usos[1].linha == 7);
    CHECK(usos[1].coluna == 20);
}

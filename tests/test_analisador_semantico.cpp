#include "doctest.h"

#include <string>

#include "frontend/frontend.h"
#include "semantic/analisador_semantico.h"

using mus::semantica::AnalisadorSemantico;
using mus::semantica::Tipo;

namespace {

// Parseia (o programa tem de ser sintaticamente válido — isso é testado em
// `test_parser.cpp`) e roda só a análise semântica.
AnalisadorSemantico analisar(const std::string& fonte) {
    std::string erroSintaxe;
    auto programa = mus::parseToAst(fonte, &erroSintaxe);
    REQUIRE_MESSAGE(programa != nullptr, "erro de sintaxe inesperado: " << erroSintaxe);

    AnalisadorSemantico analisador;
    analisador.analisar(*programa);
    return analisador;
}

// Programa que se espera aceito: falha mostrando o primeiro erro, se houver.
void exigeValido(const std::string& fonte) {
    const auto analisador = analisar(fonte);
    REQUIRE_MESSAGE(analisador.erros().empty(),
                    "erro semântico inesperado: "
                        << analisador.erros().front().formatado());
}

// Programa que se espera recusado com exatamente um erro; devolve a mensagem
// já formatada com linha e coluna.
std::string unicoErro(const std::string& fonte) {
    const auto analisador = analisar(fonte);
    REQUIRE(analisador.erros().size() == 1);
    return analisador.erros().front().formatado();
}

}  // namespace

TEST_CASE("os exemplos do repositório passam limpos") {
    // Mesmo conteúdo de exemplos/arpejo.mus e exemplos/escala.mus.
    exigeValido(
        "andamento 120\nvolume 0.8\ntempo = 0.5\n"
        "repita 4 vezes {\n"
        "    toca do4  por tempo\n    toca mi4  por tempo\n    toca sol4 por tempo\n"
        "}\n"
        "toca la4 - 2 por tempo * 2\npausa por 1.0\n");
    exigeValido(
        "andamento 100\nduracao = 1.0\n"
        "toca do4 por duracao\ntoca re4 por duracao\ntoca do5 por duracao\n"
        "toca fa#4 por 0.5\ntoca sib3 por 0.5\n");
}

TEST_CASE("uso antes de declarar é erro, com linha e coluna do identificador") {
    CHECK(unicoErro("andamento 120\ntoca do4 por tempo") ==
          "Linha 2, Coluna 14: 'tempo' usado antes de ser declarado ou fora de escopo");
}

TEST_CASE("x = x + 1 com x novo acusa o x da direita") {
    CHECK(unicoErro("x = x + 1") ==
          "Linha 1, Coluna 5: 'x' usado antes de ser declarado ou fora de escopo");
}

TEST_CASE("o bloco do repita abre escopo") {
    SUBCASE("variável criada dentro não existe depois do bloco") {
        CHECK(unicoErro("repita 2 vezes {\n  d = 0.5\n  toca do4 por d\n}\n"
                        "toca re4 por d") ==
              "Linha 5, Coluna 14: 'd' usado antes de ser declarado ou fora de escopo");
    }
    SUBCASE("variável de fora é visível e atualizável dentro") {
        const auto analisador =
            analisar("d = 0.5\nrepita 2 vezes {\n  d = d * 2\n  toca do4 por d\n}\n"
                     "toca re4 por d");
        CHECK(analisador.erros().empty());

        // Uma variável só: a atribuição dentro do bloco atualizou a de fora em
        // vez de declarar outra.
        const auto& simbolos = analisador.tabela().simbolos();
        REQUIRE(simbolos.size() == 1);
        CHECK(simbolos[0].nivelEscopo == 0);
    }
    SUBCASE("variável local fica registrada com o nível do bloco") {
        const auto analisador = analisar("repita 2 vezes {\n  d = 0.5\n  toca do4 por d\n}");
        REQUIRE(analisador.tabela().simbolos().size() == 1);
        CHECK(analisador.tabela().simbolos()[0].nivelEscopo == 1);
    }
}

TEST_CASE("reatribuir com outro tipo é redeclaração inválida") {
    CHECK(unicoErro("tempo = 0.5\ntempo = verdadeiro") ==
          "Linha 2, Coluna 1: redeclaração inválida: 'tempo' é numero, recebeu booleano");
}

TEST_CASE("reatribuir com o mesmo tipo é permitido e conta como uso") {
    const auto analisador = analisar("tempo = 0.5\ntempo = 1");
    CHECK(analisador.erros().empty());
    REQUIRE(analisador.tabela().simbolos().size() == 1);
    CHECK(analisador.tabela().simbolos()[0].usos.size() == 1);
}

TEST_CASE("tipos nos comandos") {
    CHECK(unicoErro("toca verdadeiro por 1") ==
          "Linha 1, Coluna 6: toca espera nota ou numero como altura, recebeu booleano");
    CHECK(unicoErro("toca do4 por mi4") ==
          "Linha 1, Coluna 14: a duração do toca espera numero, recebeu nota");
    CHECK(unicoErro("repita do4 vezes { }") ==
          "Linha 1, Coluna 8: repita espera numero, recebeu nota");
    CHECK(unicoErro("andamento falso") ==
          "Linha 1, Coluna 11: andamento espera numero, recebeu booleano");
    CHECK(unicoErro("pausa por re4") ==
          "Linha 1, Coluna 11: a duração da pausa espera numero, recebeu nota");

    // MIDI cru como altura é aceito: o interpretador toca.
    exigeValido("toca 60 por 1");
}

TEST_CASE("regras de tipo das expressões") {
    SUBCASE("nota ± numero transpõe e continua nota") {
        const auto analisador = analisar("a = do4 + 7\nb = la4 - 2\nc = 2 + mi4");
        CHECK(analisador.erros().empty());
        for (const auto& simbolo : analisador.tabela().simbolos()) {
            CHECK(simbolo.tipo == Tipo::Nota);
        }
    }
    SUBCASE("nota - nota é o intervalo, um numero") {
        const auto analisador = analisar("i = sol4 - do4");
        CHECK(analisador.erros().empty());
        CHECK(analisador.tabela().simbolos()[0].tipo == Tipo::Numero);
    }
    SUBCASE("comparação dá booleano") {
        const auto analisador = analisar("b = do4 < re4 e 1 == 1");
        CHECK(analisador.erros().empty());
        CHECK(analisador.tabela().simbolos()[0].tipo == Tipo::Booleano);
    }
    SUBCASE("combinações sem sentido musical são recusadas") {
        CHECK(unicoErro("x = do4 + re4") == "Linha 1, Coluna 5: operação inválida: nota + nota");
        CHECK(unicoErro("x = 2 - la4") == "Linha 1, Coluna 5: operação inválida: numero - nota");
        CHECK(unicoErro("x = do4 * 2") == "Linha 1, Coluna 5: operação inválida: nota * numero");
        CHECK(unicoErro("x = 1 e verdadeiro") ==
              "Linha 1, Coluna 5: operação inválida: numero e booleano");
        CHECK(unicoErro("x = do4 == 60") == "Linha 1, Coluna 5: operação inválida: nota == numero");
        // A unária aponta o operador, não o operando: a coluna 5 é a do `-` e
        // a do `nao`.
        CHECK(unicoErro("x = -do4") == "Linha 1, Coluna 5: operação inválida: -nota");
        CHECK(unicoErro("x = nao 1") == "Linha 1, Coluna 5: operação inválida: nao numero");
    }
}

TEST_CASE("um erro não vira cascata") {
    // `x` indefinido: um erro só, e não um a mais para cada `+` acima dele.
    CHECK(unicoErro("y = x + 1 + 2") ==
          "Linha 1, Coluna 5: 'x' usado antes de ser declarado ou fora de escopo");

    // `y` fica declarado (com tipo erro) mesmo assim: usá-lo depois não é um
    // segundo "não declarado".
    CHECK(unicoErro("y = x + 1\ntoca do4 por y") ==
          "Linha 1, Coluna 5: 'x' usado antes de ser declarado ou fora de escopo");
}

TEST_CASE("o tipo erro não fica grudado no símbolo e some na primeira atribuição válida") {
    SUBCASE("a atribuição seguinte com tipo válido redefine o tipo") {
        const auto analisador = analisar("x = y\nx = do4");
        REQUIRE(analisador.erros().size() == 1);
        REQUIRE(analisador.tabela().simbolos().size() == 1);
        CHECK(analisador.tabela().simbolos()[0].tipo == Tipo::Nota);
    }
    SUBCASE("e os erros seguintes que envolvem o nome continuam aparecendo") {
        // Sem isso, o `x` ficaria com tipo erro para sempre e a nota como
        // duração passaria calada — o usuário só a veria na segunda rodada.
        const auto analisador = analisar("x = y\nx = do4\ntoca do4 por x");
        REQUIRE(analisador.erros().size() == 2);
        CHECK(analisador.erros()[0].formatado() ==
              "Linha 1, Coluna 5: 'y' usado antes de ser declarado ou fora de escopo");
        CHECK(analisador.erros()[1].formatado() ==
              "Linha 3, Coluna 14: a duração do toca espera numero, recebeu nota");
    }
    SUBCASE("depois de redefinido, o tipo volta a valer na redeclaração") {
        const auto analisador = analisar("x = y\nx = do4\nx = 1");
        REQUIRE(analisador.erros().size() == 2);
        CHECK(analisador.erros()[1].formatado() ==
              "Linha 3, Coluna 1: redeclaração inválida: 'x' é nota, recebeu numero");
    }
}

TEST_CASE("todos os erros independentes são reportados, em ordem") {
    const auto analisador = analisar("toca do4 por a\ntoca re4 por b\nandamento verdadeiro");
    REQUIRE(analisador.erros().size() == 3);
    CHECK(analisador.erros()[0].posicao.linha == 1);
    CHECK(analisador.erros()[1].posicao.linha == 2);
    CHECK(analisador.erros()[2].posicao.linha == 3);
}

TEST_CASE("a tabela registra tipo, endereço, declaração e usos") {
    const auto analisador =
        analisar("tempo = 0.5\nraiz = do4\ntoca raiz por tempo\ntoca raiz + 4 por tempo");
    CHECK(analisador.erros().empty());

    const auto& simbolos = analisador.tabela().simbolos();
    REQUIRE(simbolos.size() == 2);

    CHECK(simbolos[0].nome == "tempo");
    CHECK(simbolos[0].tipo == Tipo::Numero);
    CHECK(simbolos[0].endereco == 0);
    CHECK(simbolos[0].declaracao.linha == 1);
    CHECK(simbolos[0].declaracao.coluna == 1);
    CHECK(simbolos[0].usos.size() == 2);

    CHECK(simbolos[1].nome == "raiz");
    CHECK(simbolos[1].tipo == Tipo::Nota);
    CHECK(simbolos[1].endereco == 1);
    REQUIRE(simbolos[1].usos.size() == 2);
    CHECK(simbolos[1].usos[0].linha == 3);
    CHECK(simbolos[1].usos[0].coluna == 6);
    CHECK(simbolos[1].usos[1].linha == 4);
}

TEST_CASE("erro dentro de bloco aninhado não deixa escopo aberto") {
    // CMP-08 (#44): a análise continua depois de um erro, então um bloco que
    // falha no meio ainda precisa fechar o escopo que abriu. Se ficasse
    // aberto, `y` continuaria visível depois do `}` e o último `toca` seria
    // aceito.
    const auto analisador = analisar(
        "repita 2 vezes {\n"
        "    repita 3 vezes {\n"
        "        y = indefinido\n"
        "        toca do4 por verdadeiro\n"
        "    }\n"
        "}\n"
        "toca do4 por y\n");

    CHECK(analisador.tabela().nivelAtual() == 0);
    REQUIRE(analisador.erros().size() == 3);
    CHECK(analisador.erros().back().formatado() ==
          "Linha 7, Coluna 14: 'y' usado antes de ser declarado ou fora de escopo");
}

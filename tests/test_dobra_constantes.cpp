#include "doctest.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "core/timeline_emit.h"
#include "frontend/frontend.h"
#include "interpreter/ast.h"
#include "interpreter/interpreter.h"
#include "otimizacao/dobra_constantes.h"
#include "semantic/analisador_semantico.h"

using namespace mus;
using otimizacao::dobrarConstantes;
using otimizacao::ResultadoDobra;

// Dobra de constantes — COMP-D (#110). A regra de ouro: com ou sem a dobra, o
// programa produz a mesma Timeline (byte a byte no CSV do --emit-timeline) e
// os mesmos erros. Detalhes em docs/otimizacao.md.

namespace {

ast::Arvore parsear(const std::string& fonte) {
    std::string erro;
    ast::Arvore programa = parseToAst(fonte, &erro);
    REQUIRE_MESSAGE(programa != nullptr, "erro de sintaxe: " << erro);
    return programa;
}

// O pipeline do main.cpp, com a dobra ligada ou desligada. Devolve o CSV da
// Timeline, ou "erro: <mensagem>" se a semântica ou a execução recusar.
std::string compilar(const std::string& fonte, bool otimizar) {
    ast::Arvore programa = parsear(fonte);

    semantica::AnalisadorSemantico analisador;
    if (!analisador.analisar(*programa)) {
        return "erro: semantico";
    }
    if (otimizar) {
        dobrarConstantes(*programa);
    }

    Interpretador interpretador;
    if (!interpretador.executar(*programa)) {
        return "erro: " + interpretador.erro();
    }
    return emitirTimeline(interpretador.timeline());
}

std::string lerArquivo(const std::string& caminho) {
    std::ifstream arquivo(caminho);
    REQUIRE_MESSAGE(arquivo.good(), "não abriu " << caminho);
    std::ostringstream conteudo;
    conteudo << arquivo.rdbuf();
    return conteudo.str();
}

const ast::ComandoToca& primeiroToca(const ast::Arvore& programa) {
    for (const ast::Comando* comando : *programa) {
        if (const auto* toca = std::get_if<ast::ComandoToca>(comando)) {
            return *toca;
        }
    }
    FAIL("programa sem toca");
    return std::get<ast::ComandoToca>(*programa->front());  // inatingível
}

double literalNumero(const ast::Expressao* e) {
    REQUIRE(e != nullptr);
    const auto* literal = std::get_if<ast::ExpressaoLiteralNumero>(e);
    REQUIRE_MESSAGE(literal != nullptr, "esperava literal numérico, veio alternative "
                                            << e->index());
    return literal->valor;
}

// Igualdade bit a bit: distingue 0 de -0 e trata NaN == NaN.
bool mesmoValor(const Valor& a, const Valor& b) {
    if (a.index() != b.index()) {
        return false;
    }
    if (const auto* x = std::get_if<double>(&a)) {
        const double y = std::get<double>(b);
        return std::memcmp(x, &y, sizeof y) == 0;
    }
    return a == b;
}

Valor executarEObter(const std::string& fonte, bool otimizar, const std::string& nome) {
    // Sem análise semântica de propósito: as expressões aleatórias misturam
    // número e booleano, que a semântica recusaria, mas o interpretador aceita
    // (paraNumero/paraBooleano). A dobra precisa concordar até nesses casos.
    ast::Arvore programa = parsear(fonte);
    if (otimizar) {
        dobrarConstantes(*programa);
    }
    Interpretador interpretador;
    REQUIRE_MESSAGE(interpretador.executar(*programa), interpretador.erro());
    const Valor* valor = interpretador.ambiente().buscar(nome);
    REQUIRE(valor != nullptr);
    return *valor;
}

// Gerador determinístico (LCG) de expressões totalmente parentizadas.
class GeradorExpressoes {
public:
    explicit GeradorExpressoes(std::uint32_t semente) : estado_(semente) {}

    std::string gerar(int profundidade) {
        if (profundidade == 0 || sortear(4) == 0) {
            static const char* const folhas[] = {
                "0", "1", "2", "3", "0.1", "0.5", "1000000", "verdadeiro",
                "falso", "la4", "do", "y"};
            return folhas[sortear(12)];
        }
        if (sortear(5) == 0) {
            return std::string(sortear(2) == 0 ? "-(" : "nao (") +
                   gerar(profundidade - 1) + ")";
        }
        static const char* const operadores[] = {
            "+", "-", "*", "/", "==", "!=", "<", ">", "<=", ">=", "e", "ou"};
        return "(" + gerar(profundidade - 1) + " " + operadores[sortear(12)] + " " +
               gerar(profundidade - 1) + ")";
    }

private:
    std::uint32_t sortear(std::uint32_t n) {
        estado_ = estado_ * 1664525u + 1013904223u;
        return (estado_ >> 16) % n;
    }

    std::uint32_t estado_;
};

}  // namespace

// ------------------------------------------------------------ a transformação

TEST_CASE("dobra: altura e duração constantes viram literais") {
    ast::Arvore programa = parsear("toca la4 - 2 por 1 / 4 * 2");
    const ResultadoDobra r = dobrarConstantes(*programa);

    const ast::ComandoToca& toca = primeiroToca(programa);
    CHECK(literalNumero(toca.altura) == 67.0);
    CHECK(literalNumero(toca.duracao) == 0.5);
    CHECK(r.expressoesDobradas == 3);
    CHECK(r.nosAntes == 8);  // (la4 - 2): 3 nós; (1 / 4) * 2: 5 nós
    CHECK(r.nosDepois == 2);
}

TEST_CASE("dobra: só a parte constante de uma expressão com variável") {
    ast::Arvore programa = parsear("x = 2\ntoca x * (2 + 3) por 1");
    dobrarConstantes(*programa);

    const ast::ComandoToca& toca = primeiroToca(programa);
    const auto* mult = std::get_if<ast::ExpressaoBinaria>(toca.altura);
    REQUIRE(mult != nullptr);
    CHECK(std::holds_alternative<ast::ExpressaoIdentificador>(*mult->esquerda));
    CHECK(literalNumero(mult->direita) == 5.0);
}

TEST_CASE("dobra: nota sem oitava e variável não são constantes") {
    // `do` depende da `oitava` corrente na execução; dobrar aqui congelaria a
    // oitava errada depois de um `oitava 5`.
    ast::Arvore programa = parsear("oitava 5\ntoca do + 2 por 1");
    const ResultadoDobra r = dobrarConstantes(*programa);
    CHECK(r.expressoesDobradas == 0);
    CHECK(std::holds_alternative<ast::ExpressaoBinaria>(*primeiroToca(programa).altura));

    CHECK(compilar("oitava 5\ntoca do + 2 por 1", true) ==
          compilar("oitava 5\ntoca do + 2 por 1", false));
}

TEST_CASE("dobra: booleanos, unários e o número de repetições") {
    ast::Arvore programa = parsear(
        "b = nao (1 < 2) ou falso\n"
        "n = -(3 * 2)\n"
        "repita 2 * 2 vezes { toca 60 por 1 }");
    dobrarConstantes(*programa);

    const auto& b = std::get<ast::ComandoAtribuicao>(*(*programa)[0]);
    const auto* booleano = std::get_if<ast::ExpressaoLiteralBooleano>(b.valor);
    REQUIRE(booleano != nullptr);
    CHECK(booleano->valor == false);

    const auto& n = std::get<ast::ComandoAtribuicao>(*(*programa)[1]);
    CHECK(literalNumero(n.valor) == -6.0);

    const auto& repita = std::get<ast::ComandoRepita>(*(*programa)[2]);
    CHECK(literalNumero(repita.vezes) == 4.0);
}

TEST_CASE("dobra: dentro de repita e de trilhas paralelas") {
    ast::Arvore programa = parsear(
        "repita 3 vezes { { toca la4 + 0 por 1 / 2, toca mi4 por 2 * 0.25 } }");
    const ResultadoDobra r = dobrarConstantes(*programa);
    CHECK(r.expressoesDobradas == 3);
}

TEST_CASE("dobra: a posição é mantida e a profundidade, recalculada") {
    // 200 termos: perto do limite da CMP-08, mas abaixo dele.
    std::string soma = "x = 1";
    for (int i = 1; i < 200; ++i) {
        soma += " + 1";
    }
    ast::Arvore programa = parsear("andamento 120\n" + soma);
    auto& atribuicao = std::get<ast::ComandoAtribuicao>(*(*programa)[1]);
    const ast::Posicao antes = atribuicao.valor->posicao;
    REQUIRE(atribuicao.valor->profundidade == 200);

    dobrarConstantes(*programa);

    CHECK(literalNumero(atribuicao.valor) == 200.0);
    CHECK(atribuicao.valor->profundidade == 1);
    CHECK((*programa)[1]->profundidade == 2);
    CHECK(atribuicao.valor->posicao.linha == antes.linha);
    CHECK(atribuicao.valor->posicao.coluna == antes.coluna);
}

TEST_CASE("dobra: aplicar duas vezes não muda mais nada") {
    ast::Arvore programa = parsear("x = 2\ntoca x * (2 + 3) por 1 / 4");
    dobrarConstantes(*programa);
    const ResultadoDobra segunda = dobrarConstantes(*programa);
    CHECK(segunda.expressoesDobradas == 0);
    CHECK(segunda.nosAntes == segunda.nosDepois);
}

// --------------------------------------------------- preservação semântica

TEST_CASE("dobra: -0, infinito e NaN saem iguais bit a bit") {
    // Sem simplificação algébrica: `x * 1` com x = -0 e `x - x` com x = inf
    // só dão o mesmo resultado se a conta for feita de verdade.
    const char* const casos[] = {
        "v = 0 * -1",        // -0
        "v = -(0)",          // -0
        "v = 1 / 0",         // inf
        "v = -1 / 0",        // -inf
        "v = 0 / 0",         // NaN
        "v = (1 / 0) - (1 / 0)",  // NaN
        "v = 0.1 + 0.2",     // 0.30000000000000004
        "v = nao 0",         // verdadeiro
    };
    for (const char* fonte : casos) {
        CAPTURE(fonte);
        CHECK(mesmoValor(executarEObter(fonte, true, "v"),
                         executarEObter(fonte, false, "v")));
    }
}

TEST_CASE("dobra: erro de execução continua igual, na mesma mensagem") {
    // A divisão por zero é dobrada para infinito, e o erro continua vindo do
    // `toca`, como sem a dobra.
    const std::string fonte = "toca 60 por 1\ntoca 60 por 1 / 0";
    const std::string com = compilar(fonte, true);
    CHECK(com.rfind("erro: ", 0) == 0);
    CHECK(com == compilar(fonte, false));
}

TEST_CASE("dobra: operação inválida entre strings não é dobrada") {
    // A semântica recusaria `"a" + "b"`; a árvore é montada à mão para testar
    // a dobra sozinha. O erro tem de acontecer na execução, como antes.
    auto montar = [] {
        auto* soma = new ast::Expressao(ast::ExpressaoBinaria{
            ast::OperadorBinario::Soma,
            new ast::Expressao(ast::ExpressaoLiteralString{"a"}),
            new ast::Expressao(ast::ExpressaoLiteralString{"b"})});
        ast::Arvore programa(new std::vector<ast::Comando*>);
        programa->push_back(new ast::Comando(ast::ComandoAtribuicao{"v", soma, {}}));
        return programa;
    };

    ast::Arvore dobrado = montar();
    CHECK(dobrarConstantes(*dobrado).expressoesDobradas == 0);
    Interpretador com;
    CHECK_FALSE(com.executar(*dobrado));

    ast::Arvore original = montar();
    Interpretador sem;
    CHECK_FALSE(sem.executar(*original));
    CHECK(com.erro() == sem.erro());

    // Igualdade entre strings é válida e é dobrada.
    ast::Arvore igualdade(new std::vector<ast::Comando*>);
    igualdade->push_back(new ast::Comando(ast::ComandoAtribuicao{
        "v",
        new ast::Expressao(ast::ExpressaoBinaria{
            ast::OperadorBinario::Igual,
            new ast::Expressao(ast::ExpressaoLiteralString{"a"}),
            new ast::Expressao(ast::ExpressaoLiteralString{"a"})}),
        {}}));
    CHECK(dobrarConstantes(*igualdade).expressoesDobradas == 1);
}

TEST_CASE("dobra: 2000 expressões aleatórias dão o mesmo valor com e sem dobra") {
    GeradorExpressoes gerador(20261120u);
    for (int i = 0; i < 2000; ++i) {
        const std::string expressao = gerador.gerar(5);
        const std::string fonte = "y = 3\nv = " + expressao;
        CAPTURE(fonte);
        REQUIRE(mesmoValor(executarEObter(fonte, true, "v"),
                           executarEObter(fonte, false, "v")));
    }
}

TEST_CASE("dobra: todos os .mus do repositório geram a mesma Timeline") {
    const char* const arquivos[] = {
        "exemplos/arpejo.mus",       "exemplos/escala.mus",
        "exemplos/polirritmia.mus",  "exemplos/erros_semanticos.mus",
        "tests/ir/andamento.mus",    "tests/ir/expressoes.mus",
        "tests/ir/notas_e_pausas.mus", "tests/ir/repeticao.mus",
        "tests/ir/trilhas.mus",      "tests/ir/volume_invalido.mus",
        "tests/estresse/mista.mus",
    };
    for (const char* arquivo : arquivos) {
        CAPTURE(arquivo);
        const std::string fonte = lerArquivo(std::string(MUS_SOURCE_DIR) + "/" + arquivo);
        CHECK(compilar(fonte, true) == compilar(fonte, false));
    }
}

// --------------------------------------------------------------- medição

// Tempo do interpretador com e sem a dobra num laço com expressões constantes.
// Pulado por padrão; números em docs/otimizacao.md. Para rodar (em Release):
//
//   ./build/tests/mus_tests --no-skip --test-case="dobra: medição*"
TEST_CASE("dobra: medição de tempo de interpretação" * doctest::skip()) {
    const std::string fonte =
        "andamento 120\n"
        "repita 200000 vezes {\n"
        "    toca la4 - 12 + 7 por 1 / 8 * 2\n"
        "    volume 0.5 * 2 - 0.25\n"
        "}\n";

    auto medir = [&](bool otimizar, int* nos) {
        ast::Arvore programa = parsear(fonte);
        if (otimizar) {
            dobrarConstantes(*programa);
        }
        *nos = otimizacao::contarNosDeExpressao(*programa);
        Interpretador interpretador;
        const auto inicio = std::chrono::steady_clock::now();
        REQUIRE(interpretador.executar(*programa));
        const auto fim = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(fim - inicio).count();
    };

    // Melhor de 5, para tirar ruído do agendador.
    double sem = 1e300;
    double com = 1e300;
    int nosSem = 0;
    int nosCom = 0;
    for (int i = 0; i < 5; ++i) {
        sem = std::min(sem, medir(false, &nosSem));
        com = std::min(com, medir(true, &nosCom));
    }
    MESSAGE("nós de expressão: " << nosSem << " -> " << nosCom);
    MESSAGE("interpretação, melhor de 5: sem dobra " << sem << " ms, com dobra "
                                                     << com << " ms");
}

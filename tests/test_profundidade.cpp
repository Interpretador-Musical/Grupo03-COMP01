#include "doctest.h"

#include <string>

#include "frontend/frontend.h"
#include "interpreter/ast.h"
#include "interpreter/interpreter.h"
#include "semantic/analisador_semantico.h"

// CMP-08 (#44): o parser recusa a árvore que passaria de
// `kProfundidadeMaxima`, porque semântica, interpretador e `liberar()` descem
// a árvore por recursão. As contas de profundidade abaixo seguem `ast.h`:
// folha = 1, cada nó composto = 1 + o filho mais fundo, e o comando conta.

namespace {

constexpr int kLimite = mus::ast::kProfundidadeMaxima;

std::string repetido(int vezes, const std::string& trecho) {
    std::string texto;
    for (int i = 0; i < vezes; ++i) {
        texto += trecho;
    }
    return texto;
}

// `x = 1+1+...+1` com `termos` termos: soma recursiva à esquerda, então a
// pilha do Bison fica rasa e só a árvore cresce — profundidade = `termos`.
std::string soma(int termos) {
    std::string texto = "x = 1";
    for (int i = 1; i < termos; ++i) {
        texto += "+1";
    }
    return texto + "\n";
}

// `x = --...-1`: profundidade da expressão = `unarias` + 1.
std::string unarias(int unarias) {
    return "x = " + repetido(unarias, "-") + "1\n";
}

// `repita` aninhados, um por linha, em volta de `toca do4 por 1` (que tem
// profundidade 2): profundidade = `niveis` + 2.
std::string blocos(int niveis) {
    return repetido(niveis, "repita 1 vezes {\n") + "toca do4 por 1\n" +
           repetido(niveis, "}\n");
}

std::string erroDe(const std::string& fonte) {
    std::string erro;
    auto programa = mus::parseToAst(fonte, &erro);
    REQUIRE_MESSAGE(programa == nullptr, "devia ter sido recusado: " << fonte.substr(0, 60));
    return erro;
}

}  // namespace

TEST_CASE("árvore exatamente no limite passa por todas as fases") {
    // É o que justifica o limite: na profundidade máxima aceita, semântica e
    // interpretador terminam sem estourar a pilha.
    for (const std::string& fonte : {soma(kLimite - 1), unarias(kLimite - 2),
                                     blocos(kLimite - 2)}) {
        auto programa = mus::parseToAst(fonte);
        REQUIRE(programa != nullptr);

        mus::semantica::AnalisadorSemantico analisador;
        CHECK(analisador.analisar(*programa));

        mus::Interpretador interpretador;
        CHECK(interpretador.executar(*programa));
    }
}

TEST_CASE("um nível acima do limite é recusado com linha, coluna e o limite") {
    // A expressão ainda cabe; é o comando em volta dela que passa, então o
    // erro aponta o começo do comando.
    CHECK(erroDe(soma(kLimite)) ==
          "Linha 1, Coluna 1: aninhamento passa do limite de 256 níveis");
    CHECK(erroDe(unarias(kLimite - 1)) ==
          "Linha 1, Coluna 1: aninhamento passa do limite de 256 níveis");
    CHECK(erroDe(blocos(kLimite - 1)) ==
          "Linha 1, Coluna 1: aninhamento passa do limite de 256 níveis");
}

TEST_CASE("o erro aponta onde o limite foi cruzado, não o começo da expressão") {
    // Na soma, a binária número k tem profundidade k + 1; a 256ª é a primeira
    // a passar. "x = " ocupa 4 colunas e cada "1+" mais 2, então o k-ésimo `+`
    // fica na coluna 4 + 2k.
    CHECK(erroDe(soma(kLimite + 1)) ==
          "Linha 1, Coluna 516: aninhamento passa do limite de 256 níveis");

    // Com 300 `repita`, o que tem profundidade 257 é o 255º contando de
    // dentro, ou seja, o 46º contando de fora — e cada um está na sua linha.
    CHECK(erroDe(blocos(300)) ==
          "Linha 46, Coluna 1: aninhamento passa do limite de 256 níveis");
}

TEST_CASE("a soma longa que derrubava o interpretador vira erro de compilação") {
    // Antes da CMP-08, 10 000 termos já davam segmentation fault no
    // `compilador` (Debug, macOS): a semântica e o interpretador recursavam
    // 10 000 níveis. O limite do Bison não ajudava, porque ele nunca empilha
    // mais que três símbolos numa soma recursiva à esquerda.
    const std::string erro = erroDe(soma(100000));
    CHECK(erro.find("aninhamento passa do limite de 256 níveis") != std::string::npos);
}

TEST_CASE("aninhamento à direita além da pilha do Bison tem mensagem em português") {
    // Em `((((`, `----` e `repita` aninhados, o Bison empilha todos os tokens
    // antes de reduzir o primeiro nó, então a pilha dele (10 000) esgota antes
    // de qualquer `LIMITA_PROFUNDIDADE` rodar.
    const std::string mensagem = "aninhamento passa do limite do analisador sintático";
    CHECK(erroDe("x = " + repetido(20000, "(") + "1" + repetido(20000, ")")).find(mensagem) !=
          std::string::npos);
    CHECK(erroDe(unarias(20000)).find(mensagem) != std::string::npos);
    CHECK(erroDe(blocos(20000)).find(mensagem) != std::string::npos);
}

TEST_CASE("parênteses não contam como nível") {
    // `(` não cria nó, e é o nó que pesa na recursão. Mil pares em volta de um
    // literal dão uma árvore de profundidade 2.
    auto programa = mus::parseToAst("x = " + repetido(1000, "(") + "1" +
                                    repetido(1000, ")"));
    REQUIRE(programa != nullptr);
    CHECK((*programa)[0]->profundidade == 2);
}

TEST_CASE("profundidade de cada nó segue a regra folha = 1, pai = 1 + filho mais fundo") {
    auto programa = mus::parseToAst("toca do4 + 2 * 3 por -1\nrepita 2 vezes { pausa por 1 }");
    REQUIRE(programa != nullptr);

    // do4 + (2 * 3): folha 1, produto 2, soma 3; a duração -1 tem 2; toca = 4.
    const auto& toca = std::get<mus::ast::ComandoToca>(*(*programa)[0]);
    CHECK(toca.altura->profundidade == 3);
    CHECK(toca.duracao->profundidade == 2);
    CHECK((*programa)[0]->profundidade == 4);

    // pausa por 1 = 2; repita = 1 + max(vezes 1, corpo 2) = 3.
    CHECK((*programa)[1]->profundidade == 3);
}

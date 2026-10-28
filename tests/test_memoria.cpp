#include "doctest.h"

#include <string>

#include "frontend/frontend.h"
#include "interpreter/ast.h"
#include "interpreter/interpreter.h"
#include "semantic/analisador_semantico.h"

// Bateria da MAT-11 (#60). Fora do sanitizer, estes casos só conferem que nada
// cai; o que eles provam de verdade aparece no job `address-sanitizer` da CI,
// que roda a suíte com LeakSanitizer ligado e falha no primeiro byte perdido.
// Por isso a carga é grande: um vazamento por parse vira centenas aqui.

namespace {

// Usa tudo o que a gramática tem hoje — atribuição, os seis comandos, blocos
// aninhados, unária, binária, comparação e lógica — para que os prefixos dele
// passem por todos os estados da pilha do Bison.
const char* const kProgramaCompleto =
    "andamento 120\n"
    "oitava 4\n"
    "volume 0.8\n"
    "tempo = 0.5\n"
    "ligado = verdadeiro e nao falso\n"
    "repita 2 vezes {\n"
    "    repita 3 vezes {\n"
    "        toca do4 + 2 * (1 - 1) por tempo / 2\n"
    "        toca mi4 + -1 por tempo\n"
    "    }\n"
    "    pausa por 0.25\n"
    "}\n"
    "toca sol por tempo * 2\n";

std::string aninhado(int profundidade, const std::string& abre,
                     const std::string& miolo, const std::string& fecha) {
    std::string fonte;
    for (int i = 0; i < profundidade; ++i) {
        fonte += abre;
    }
    fonte += miolo;
    for (int i = 0; i < profundidade; ++i) {
        fonte += fecha;
    }
    return fonte;
}

}  // namespace

TEST_CASE("parse, semântica e interpretação repetidos não acumulam memória") {
    for (int i = 0; i < 300; ++i) {
        auto programa = mus::parseToAst(kProgramaCompleto);
        REQUIRE(programa != nullptr);

        mus::semantica::AnalisadorSemantico analisador;
        REQUIRE(analisador.analisar(*programa));

        mus::Interpretador interpretador;
        REQUIRE(interpretador.executar(*programa));
        // 2 x (3 x 2 notas + 1 pausa) + 1 nota: a pausa também vira evento.
        CHECK(interpretador.timeline().size() == 15);
    }
}

TEST_CASE("todo prefixo do programa é parseado sem vazar o que já foi montado") {
    // Cortar o texto em cada caractere faz o erro de sintaxe estourar com a
    // pilha em todos os formatos possíveis — é o que exercita cada
    // `%destructor` de `parser/parser.y`. Alguns prefixos são programas
    // válidos (os que terminam num fim de comando); o resto tem de ser
    // recusado sem cair.
    const std::string fonte = kProgramaCompleto;
    int aceitos = 0;
    for (std::size_t tamanho = 0; tamanho <= fonte.size(); ++tamanho) {
        auto programa = mus::parseToAst(fonte.substr(0, tamanho));
        if (programa != nullptr) {
            ++aceitos;
        }
    }
    CHECK(aceitos > 0);
    CHECK(aceitos < static_cast<int>(fonte.size()));
}

TEST_CASE("lixo no meio do programa é recusado sem vazar") {
    const std::string fonte = kProgramaCompleto;
    for (std::size_t posicao = 0; posicao < fonte.size(); posicao += 7) {
        std::string corrompido = fonte;
        corrompido.insert(posicao, " @ ");
        CHECK(mus::parseToAst(corrompido) == nullptr);
    }
}

TEST_CASE("tokenize e parseString repetidos liberam os nós dos literais") {
    // O lexer aloca um nó de AST para cada nota, número, identificador e
    // booleano. Nenhum dos dois caminhos entrega a árvore a ninguém.
    for (int i = 0; i < 300; ++i) {
        CHECK(mus::tokenize(kProgramaCompleto).size() > 50);
        CHECK(mus::parseString(kProgramaCompleto));
    }
}

TEST_CASE("aninhamento fundo é montado e liberado inteiro") {
    // No limite exato da CMP-08 (#44): a árvore mais funda que o parser
    // aceita. Atribuição (1) + unárias + literal (1); repita aninhados + toca
    // (1) + literal (1). Parênteses não criam nó, então cabem bem mais.
    const int limite = mus::ast::kProfundidadeMaxima;
    const std::string parenteses =
        "x = " + aninhado(1000, "(", "1", ")") + "\ntoca do4 por x";
    const std::string unarias = "x = " + aninhado(limite - 2, "-", "1", "") + "\n";
    const std::string blocos =
        aninhado(limite - 2, "repita 1 vezes {\n", "toca do4 por 1\n", "}\n");

    for (const std::string& fonte : {parenteses, unarias, blocos}) {
        auto programa = mus::parseToAst(fonte);
        REQUIRE(programa != nullptr);

        mus::Interpretador interpretador;
        CHECK(interpretador.executar(*programa));
    }
}

TEST_CASE("estourar a pilha do Bison é erro de sintaxe, e a pilha é liberada") {
    // O Bison para sozinho em YYMAXDEPTH (10 000), desempilhando tudo o que
    // já tinha montado. A mensagem é conferida em `test_profundidade.cpp`;
    // aqui interessa que nada caia nem vaze.
    std::string erro;
    CHECK(mus::parseToAst("x = " + aninhado(20000, "(", "1", ")"), &erro) == nullptr);
    CHECK_FALSE(erro.empty());
}

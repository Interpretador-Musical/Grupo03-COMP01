#include "doctest.h"

#include <string>
#include <vector>

#include "core/pitch.h"
#include "frontend/frontend.h"

namespace {

using mus::TipoToken;
using mus::Token;

std::vector<TipoToken> tipos(const std::string& fonte) {
    std::vector<TipoToken> resultado;
    for (const Token& token : mus::tokenize(fonte)) {
        resultado.push_back(token.tipo);
    }
    return resultado;
}

// Atalho para os casos que examinam um único token.
Token unico(const std::string& fonte) {
    const std::vector<Token> tokens = mus::tokenize(fonte);
    REQUIRE(tokens.size() == 1);
    return tokens[0];
}

}  // namespace

TEST_CASE("entrada vazia não produz token") {
    CHECK(mus::tokenize("").empty());
    CHECK(mus::tokenize("   \n\t  \n").empty());
}

// ---------------------------------------------------------------------------
// Notas
// ---------------------------------------------------------------------------

TEST_CASE("nota sem oitava deixa a altura para o interpretador resolver") {
    const Token token = unico("do");

    CHECK(token.tipo == TipoToken::Nota);
    CHECK(token.lexema == "do");
    CHECK(token.semitom == 0);
    CHECK(token.oitava == -1);
    CHECK(token.midi == -1);
}

TEST_CASE("dó 4 é o dó central, a nota MIDI 60") {
    const Token token = unico("do4");

    CHECK(token.tipo == TipoToken::Nota);
    CHECK(token.oitava == 4);
    CHECK(token.midi == 60);
}

// Amarra o lexer à afinação que o core já usa: se alguém mexer na conta de
// oitava, este caso quebra antes de o erro chegar ao áudio.
TEST_CASE("lá 4 cai exatamente no kMidiA4 do core, ou seja 440 Hz") {
    const Token token = unico("la4");

    CHECK(token.midi == mus::kMidiA4);
    // Erro zero exigido: comparação direta, não doctest::Approx.
    CHECK(mus::midiToFrequency(token.midi) == mus::kFrequencyA4);
}

TEST_CASE("as sete notas têm os semitons do sistema temperado") {
    CHECK(unico("do").semitom == 0);
    CHECK(unico("re").semitom == 2);
    CHECK(unico("mi").semitom == 4);
    CHECK(unico("fa").semitom == 5);
    CHECK(unico("sol").semitom == 7);
    CHECK(unico("la").semitom == 9);
    CHECK(unico("si").semitom == 11);
}

TEST_CASE("sustenido sobe e bemol desce um semitom") {
    CHECK(unico("re#").semitom == 3);
    CHECK(unico("mib").semitom == 3);   // mesma tecla, nome diferente
    CHECK(unico("fa#4").midi == 66);
    CHECK(unico("sib3").midi == 58);
}

TEST_CASE("dó bemol e si sustenido atravessam a fronteira da oitava") {
    // Dó bemol 4 é, na prática, o si 3 — e a conta do MIDI precisa dar isso
    // sem tratamento especial.
    CHECK(unico("dob4").midi == 59);
    CHECK(unico("si#4").midi == 72);
}

// ---------------------------------------------------------------------------
// Números — é o NFR da CMP-02 (#9): a gramática tem de diferenciar os tipos
// primitivos numéricos, e a distinção nasce aqui.
// ---------------------------------------------------------------------------

TEST_CASE("inteiro e real são tokens distintos") {
    const Token inteiro = unico("4");
    CHECK(inteiro.tipo == TipoToken::Inteiro);
    CHECK(inteiro.valor == 4.0);

    const Token real = unico("0.5");
    CHECK(real.tipo == TipoToken::Real);
    CHECK(real.valor == 0.5);
}

TEST_CASE("um inteiro seguido de ponto não vira real por acidente") {
    // `4.` não casa a regra do real, que exige dígito depois do ponto.
    const std::vector<TipoToken> t = tipos("4.");
    REQUIRE(t.size() == 2);
    CHECK(t[0] == TipoToken::Inteiro);
    CHECK(t[1] == TipoToken::Desconhecido);
}

// ---------------------------------------------------------------------------
// Palavras-chave
// ---------------------------------------------------------------------------

TEST_CASE("cada palavra-chave devolve seu próprio token") {
    CHECK(unico("toca").tipo == TipoToken::Toca);
    CHECK(unico("pausa").tipo == TipoToken::Pausa);
    CHECK(unico("por").tipo == TipoToken::Por);
    CHECK(unico("andamento").tipo == TipoToken::Andamento);
    CHECK(unico("oitava").tipo == TipoToken::Oitava);
    CHECK(unico("volume").tipo == TipoToken::Volume);
    CHECK(unico("repita").tipo == TipoToken::Repita);
    CHECK(unico("vezes").tipo == TipoToken::Vezes);
    CHECK(unico("se").tipo == TipoToken::Se);
    CHECK(unico("defina").tipo == TipoToken::Defina);
    CHECK(unico("retorna").tipo == TipoToken::Retorna);
    CHECK(unico("verdadeiro").tipo == TipoToken::Verdadeiro);
    CHECK(unico("falso").tipo == TipoToken::Falso);
    CHECK(unico("e").tipo == TipoToken::OperadorE);
    CHECK(unico("ou").tipo == TipoToken::OperadorOu);
}

TEST_CASE("acento é opcional nas palavras-chave que levam") {
    CHECK(unico("entao").tipo == TipoToken::Entao);
    CHECK(unico("então").tipo == TipoToken::Entao);
    CHECK(unico("senao").tipo == TipoToken::Senao);
    CHECK(unico("senão").tipo == TipoToken::Senao);
    CHECK(unico("nao").tipo == TipoToken::OperadorNao);
    CHECK(unico("não").tipo == TipoToken::OperadorNao);
}

// ---------------------------------------------------------------------------
// Casamento mais longo — a armadilha de ter notas de duas letras convivendo
// com identificadores livres.
// ---------------------------------------------------------------------------

TEST_CASE("identificador que começa com nome de nota continua identificador") {
    CHECK(unico("dose").tipo == TipoToken::Identificador);
    CHECK(unico("solo").tipo == TipoToken::Identificador);
    CHECK(unico("mineral").tipo == TipoToken::Identificador);
    CHECK(unico("lado").tipo == TipoToken::Identificador);
}

TEST_CASE("identificador que começa com palavra-chave continua identificador") {
    CHECK(unico("repitax").tipo == TipoToken::Identificador);
    CHECK(unico("tocata").tipo == TipoToken::Identificador);
    CHECK(unico("sentido").tipo == TipoToken::Identificador);
}

TEST_CASE("tempo é identificador comum, não palavra reservada") {
    // A variável do exemplo do site se chama `tempo`; se ela virasse token
    // próprio, o programa publicado pararia de compilar.
    CHECK(unico("tempo").tipo == TipoToken::Identificador);
}

// ---------------------------------------------------------------------------
// Comentários
// ---------------------------------------------------------------------------

TEST_CASE("comentário de linha vai até o fim da linha") {
    const std::vector<TipoToken> t = tipos("toca // isto some\npausa");
    REQUIRE(t.size() == 2);
    CHECK(t[0] == TipoToken::Toca);
    CHECK(t[1] == TipoToken::Pausa);
}

TEST_CASE("comentário de bloco pode atravessar linhas") {
    const std::vector<TipoToken> t = tipos("toca /* uma\nduas\ntrês */ pausa");
    REQUIRE(t.size() == 2);
    CHECK(t[0] == TipoToken::Toca);
    CHECK(t[1] == TipoToken::Pausa);
}

TEST_CASE("comentário de bloco não engole o código seguinte") {
    const std::vector<TipoToken> t = tipos("/* a */ toca /* b */ pausa /* c */");
    REQUIRE(t.size() == 2);
    CHECK(t[0] == TipoToken::Toca);
    CHECK(t[1] == TipoToken::Pausa);
}

// ---------------------------------------------------------------------------
// Linha e coluna — metade da CMP-03 (#15) nasce aqui.
// ---------------------------------------------------------------------------

TEST_CASE("linha e coluna apontam o primeiro caractere do token") {
    const std::vector<Token> tokens = mus::tokenize("toca do4");
    REQUIRE(tokens.size() == 2);

    CHECK(tokens[0].linha == 1);
    CHECK(tokens[0].coluna == 1);
    CHECK(tokens[1].linha == 1);
    CHECK(tokens[1].coluna == 6);
}

TEST_CASE("a quebra de linha reinicia a coluna") {
    const std::vector<Token> tokens = mus::tokenize("toca\n  pausa");
    REQUIRE(tokens.size() == 2);

    CHECK(tokens[1].linha == 2);
    CHECK(tokens[1].coluna == 3);
}

TEST_CASE("comentário de bloco multilinha conta as linhas que consome") {
    const std::vector<Token> tokens = mus::tokenize("/* uma\nduas\n */ toca");
    REQUIRE(tokens.size() == 1);

    CHECK(tokens[0].linha == 3);
    CHECK(tokens[0].coluna == 5);
}

// ---------------------------------------------------------------------------
// Robustez
// ---------------------------------------------------------------------------

TEST_CASE("caractere inválido é registrado sem interromper a varredura") {
    const std::vector<Token> tokens = mus::tokenize("toca @ pausa");
    REQUIRE(tokens.size() == 3);

    CHECK(tokens[0].tipo == TipoToken::Toca);
    CHECK(tokens[1].tipo == TipoToken::Desconhecido);
    CHECK(tokens[1].lexema == "@");
    CHECK(tokens[2].tipo == TipoToken::Pausa);
}

TEST_CASE("varreduras seguidas não vazam estado uma para a outra") {
    const std::vector<Token> primeira = mus::tokenize("toca\ntoca");
    const std::vector<Token> segunda = mus::tokenize("pausa");

    REQUIRE(segunda.size() == 1);
    CHECK(segunda[0].linha == 1);
    CHECK(segunda[0].coluna == 1);
    CHECK(primeira.size() == 2);
}

// ---------------------------------------------------------------------------
// Operadores
// ---------------------------------------------------------------------------

TEST_CASE("operadores de dois caracteres não são lidos como dois de um") {
    const std::vector<TipoToken> t = tipos("== != <= >= < > =");
    REQUIRE(t.size() == 7);
    CHECK(t[0] == TipoToken::Igual);
    CHECK(t[1] == TipoToken::Diferente);
    CHECK(t[2] == TipoToken::MenorOuIgual);
    CHECK(t[3] == TipoToken::MaiorOuIgual);
    CHECK(t[4] == TipoToken::Menor);
    CHECK(t[5] == TipoToken::Maior);
    CHECK(t[6] == TipoToken::Atribuicao);
}

TEST_CASE("aritmética e pontuação viram tokens próprios") {
    const std::vector<TipoToken> t = tipos("+ - * / ( ) { } ,");
    REQUIRE(t.size() == 9);
    CHECK(t[0] == TipoToken::Soma);
    CHECK(t[3] == TipoToken::Divisao);
    CHECK(t[6] == TipoToken::AbreChave);
    CHECK(t[8] == TipoToken::Virgula);
}

TEST_CASE("nomeToken devolve rótulo para todo tipo produzido") {
    for (const Token& token : mus::tokenize("toca do4 por 0.5 // fim")) {
        CHECK(std::string(mus::nomeToken(token.tipo)).empty() == false);
    }
}

%{
// Analisador léxico da linguagem musical — CMP-02 (#9) e INT-03 (#19).
//
// Vocabulário em português, notas em `do re mi`, oitava colada na nota
// (`do4` = dó central = MIDI 60) e duração em tempos.
//
// A ordem das regras importa em dois pontos, e os dois estão marcados abaixo:
// palavras-chave antes de identificador, e NOTA antes de identificador. O Flex
// escolhe o casamento mais longo e, no empate, a regra declarada primeiro.
//
// Desde a INT-03 (#19), os terminais com valor semântico (NOTA, INTEIRO,
// REAL, IDENT, VERDADEIRO, FALSO) também preenchem `yylval` — o campo do
// `%union` de `parser/parser.y` — com o nó de AST correspondente, além de
// continuar registrando `tokenAtual` para `tokenize()`/`--tokens`.

#include "frontend/frontend.h"
#include "frontend/scanner.h"
#include "interpreter/ast.h"
#include "parser.tab.h"

using mus::TipoToken;

// Roda antes de toda ação: fixa onde este lexema começa e avança linha/coluna
// por cima do texto consumido.
//
// Copia a mesma posição para o `yylloc` do Bison, que é o que dá `@1` às ações
// de `parser/parser.y` (habilitado lá pelo `%locations`). É assim que a unária
// descobre onde está o `-` ou o `nao`: eles não têm valor semântico no
// `%union`, então não podem carregar a posição como o IDENT carrega. Rodar
// também para espaço e comentário é inofensivo — essas regras não devolvem
// token, e o próximo lexema sobrescreve o campo antes de o parser olhar.
//
// Também zera `yylval.expressao`: assim, depois de `yylex()`, ele só não é
// nulo se este token alocou um nó — é o que `tokenize()` usa para saber o que
// liberar (ver `valorDoUltimoToken()` no fim do arquivo).
#define YY_USER_ACTION                                        \
    do {                                                      \
        yylval.expressao = nullptr;                           \
        mus::scanner::avancarPosicao(yytext, yyleng);         \
        yylloc.first_line = mus::scanner::tokenAtual.linha;   \
        yylloc.first_column = mus::scanner::tokenAtual.coluna; \
        yylloc.last_line = yylloc.first_line;                 \
        yylloc.last_column = yylloc.first_column;             \
    } while (0);

// Aloca o nó de AST do token que acabou de casar já com a posição dele — a
// análise semântica (CMP-13, #95) precisa disso para apontar onde está o erro.
template <typename No>
static mus::ast::Expressao* novaExpressao(No no) {
    mus::ast::Expressao* expressao = new mus::ast::Expressao(no);
    expressao->posicao = mus::ast::Posicao{mus::scanner::tokenAtual.linha,
                                           mus::scanner::tokenAtual.coluna};
    return expressao;
}

#define TOKEN(tipo, codigo)                                        \
    do {                                                           \
        mus::scanner::registrarToken(tipo, yytext, yyleng);        \
        return codigo;                                             \
    } while (0)
%}

%option noyywrap
%option nounput noinput

NOME_NOTA   (do|re|mi|fa|sol|la|si)
ACIDENTE    ("#"|b)
LETRA       [a-zA-Z_]
ALFANUM     [a-zA-Z0-9_]

%%

 /* ---- Ignorados ------------------------------------------------------- */

"//"[^\n]*                      { /* comentário de linha */ }
"/*"([^*]|\*+[^*/])*\*+"/"      { /* comentário de bloco; YY_USER_ACTION conta os \n */ }
[ \t\r\n]+                      { /* espaço em branco */ }

 /* ---- Palavras-chave -------------------------------------------------- */
 /* Precisam vir ANTES de IDENTIFICADOR: `repita` casa 6 caracteres nas duas
    regras, e no empate vence a primeira declarada. As formas acentuadas são
    aceitas junto das sem acento — o teclado do usuário não deve decidir se o
    programa compila. */

"toca"                          { TOKEN(TipoToken::Toca, TOCA); }
"pausa"                         { TOKEN(TipoToken::Pausa, PAUSA); }
"por"                           { TOKEN(TipoToken::Por, POR); }
"andamento"                     { TOKEN(TipoToken::Andamento, ANDAMENTO); }
"oitava"                        { TOKEN(TipoToken::Oitava, OITAVA); }
"volume"                        { TOKEN(TipoToken::Volume, VOLUME); }

"repita"                        { TOKEN(TipoToken::Repita, REPITA); }
"vezes"                         { TOKEN(TipoToken::Vezes, VEZES); }
"se"                            { TOKEN(TipoToken::Se, SE); }
"entao"|"então"                 { TOKEN(TipoToken::Entao, ENTAO); }
"senao"|"senão"                 { TOKEN(TipoToken::Senao, SENAO); }
"defina"                        { TOKEN(TipoToken::Defina, DEFINA); }
"retorna"                       { TOKEN(TipoToken::Retorna, RETORNA); }

"verdadeiro"                    {
    mus::scanner::registrarToken(TipoToken::Verdadeiro, yytext, yyleng);
    yylval.expressao = novaExpressao(mus::ast::ExpressaoLiteralBooleano{true});
    return VERDADEIRO;
}
"falso"                         {
    mus::scanner::registrarToken(TipoToken::Falso, yytext, yyleng);
    yylval.expressao = novaExpressao(mus::ast::ExpressaoLiteralBooleano{false});
    return FALSO;
}
"e"                             { TOKEN(TipoToken::OperadorE, E); }
"ou"                            { TOKEN(TipoToken::OperadorOu, OU); }
"nao"|"não"                     { TOKEN(TipoToken::OperadorNao, NAO); }

 /* ---- Notas ----------------------------------------------------------- */
 /* Também precisa vir ANTES de IDENTIFICADOR: `do4` casa 3 caracteres nas duas
    regras. Já `dose` e `solo` casam mais como identificador e continuam sendo
    identificadores, que é o comportamento desejado. */

{NOME_NOTA}{ACIDENTE}?[0-9]?    {
    mus::scanner::registrarNota(yytext, yyleng);
    yylval.expressao = novaExpressao(mus::ast::ExpressaoNota{
        mus::scanner::tokenAtual.semitom, mus::scanner::tokenAtual.oitava});
    return NOTA;
}

 /* ---- Números --------------------------------------------------------- */
 /* Inteiro e real são tokens distintos porque o NFR da CMP-02 (#9) exige que a
    gramática consiga diferenciar os tipos primitivos numéricos. A AST não
    repete essa distinção — os dois viram o mesmo ExpressaoLiteralNumero, já
    que a aritmética do interpretador (INT-03, #19) não trata int e float
    diferente. */

[0-9]+"."[0-9]+                 {
    mus::scanner::registrarNumero(TipoToken::Real, yytext, yyleng);
    yylval.expressao = novaExpressao(
        mus::ast::ExpressaoLiteralNumero{mus::scanner::tokenAtual.valor});
    return REAL;
}
[0-9]+                          {
    mus::scanner::registrarNumero(TipoToken::Inteiro, yytext, yyleng);
    yylval.expressao = novaExpressao(
        mus::ast::ExpressaoLiteralNumero{mus::scanner::tokenAtual.valor});
    return INTEIRO;
}

 /* ---- Identificadores ------------------------------------------------- */

{LETRA}{ALFANUM}*               {
    mus::scanner::registrarToken(TipoToken::Identificador, yytext, yyleng);
    // Vai como Expressao, e não como string solta, para levar a posição junto:
    // a regra de atribuição tira dela o nome e a posição de declaração.
    yylval.expressao = novaExpressao(
        mus::ast::ExpressaoIdentificador{mus::scanner::tokenAtual.lexema});
    return IDENT;
}

 /* ---- Operadores e pontuação ------------------------------------------ */
 /* Os de dois caracteres vêm antes dos de um para que `<=` não seja lido como
    `<` seguido de `=`. */

"=="                            { TOKEN(TipoToken::Igual, IGUAL); }
"!="                            { TOKEN(TipoToken::Diferente, DIFERENTE); }
"<="                            { TOKEN(TipoToken::MenorOuIgual, MENOR_IGUAL); }
">="                            { TOKEN(TipoToken::MaiorOuIgual, MAIOR_IGUAL); }
"<"                             { TOKEN(TipoToken::Menor, '<'); }
">"                             { TOKEN(TipoToken::Maior, '>'); }

"+"                             { TOKEN(TipoToken::Soma, '+'); }
"-"                             { TOKEN(TipoToken::Subtracao, '-'); }
"*"                             { TOKEN(TipoToken::Multiplicacao, '*'); }
"/"                             { TOKEN(TipoToken::Divisao, '/'); }
"="                             { TOKEN(TipoToken::Atribuicao, '='); }

"("                             { TOKEN(TipoToken::AbreParen, '('); }
")"                             { TOKEN(TipoToken::FechaParen, ')'); }
"{"                             { TOKEN(TipoToken::AbreChave, '{'); }
"}"                             { TOKEN(TipoToken::FechaChave, '}'); }
","                             { TOKEN(TipoToken::Virgula, ','); }

 /* ---- Qualquer outra coisa -------------------------------------------- */
 /* Registra e segue em frente, sem abortar. O NFR da CMP-03 (#15) pede que
    lixo na entrada não derrube o compilador; devolver o caractere cru como
    código faz a gramática recusá-lo de forma limpa. */

.                               { TOKEN(TipoToken::Desconhecido, static_cast<unsigned char>(yytext[0])); }

%%

// Fica aqui porque só este arquivo e o `parser.tab.cpp` enxergam `yylval`.
mus::ast::Expressao* mus::scanner::valorDoUltimoToken() {
    return yylval.expressao;
}

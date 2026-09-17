%{
// Analisador léxico da linguagem musical — CMP-02 (#9).
//
// Vocabulário em português, notas em `do re mi`, oitava colada na nota
// (`do4` = dó central = MIDI 60) e duração em tempos.
//
// A ordem das regras importa em dois pontos, e os dois estão marcados abaixo:
// palavras-chave antes de identificador, e NOTA antes de identificador. O Flex
// escolhe o casamento mais longo e, no empate, a regra declarada primeiro.

#include "frontend/frontend.h"
#include "frontend/scanner.h"
#include "parser.tab.h"

using mus::TipoToken;

// Roda antes de toda ação: fixa onde este lexema começa e avança linha/coluna
// por cima do texto consumido.
#define YY_USER_ACTION mus::scanner::avancarPosicao(yytext, yyleng);

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

"verdadeiro"                    { TOKEN(TipoToken::Verdadeiro, VERDADEIRO); }
"falso"                         { TOKEN(TipoToken::Falso, FALSO); }
"e"                             { TOKEN(TipoToken::OperadorE, E); }
"ou"                            { TOKEN(TipoToken::OperadorOu, OU); }
"nao"|"não"                     { TOKEN(TipoToken::OperadorNao, NAO); }

 /* ---- Notas ----------------------------------------------------------- */
 /* Também precisa vir ANTES de IDENTIFICADOR: `do4` casa 3 caracteres nas duas
    regras. Já `dose` e `solo` casam mais como identificador e continuam sendo
    identificadores, que é o comportamento desejado. */

{NOME_NOTA}{ACIDENTE}?[0-9]?    { mus::scanner::registrarNota(yytext, yyleng); return NOTA; }

 /* ---- Números --------------------------------------------------------- */
 /* Inteiro e real são tokens distintos porque o NFR da CMP-02 (#9) exige que a
    gramática consiga diferenciar os tipos primitivos numéricos. */

[0-9]+"."[0-9]+                 { mus::scanner::registrarNumero(TipoToken::Real, yytext, yyleng); return REAL; }
[0-9]+                          { mus::scanner::registrarNumero(TipoToken::Inteiro, yytext, yyleng); return INTEIRO; }

 /* ---- Identificadores ------------------------------------------------- */

{LETRA}{ALFANUM}*               { TOKEN(TipoToken::Identificador, IDENT); }

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

.                               { TOKEN(TipoToken::Desconhecido, yytext[0]); }

%%

%{
// Analisador sintático da linguagem musical — CMP-02 (#9).
//
// Hoje é um RECONHECEDOR: responde se o programa pertence à linguagem, sem
// construir árvore. A AST é a CMP-04 (#21), e é lá que entra o valor semântico.
//
// Por isso não há `%union` nem `%type`. Não é economia: o Bison disponível no
// macOS é o 2.3 (Apple, de 2006), e escrever agora um valor semântico na API
// antiga só para reescrevê-lo depois seria dívida pura. Um reconhecedor não
// precisa de nenhum, então a gramática fica no subconjunto que funciona igual
// no Bison 2.3 e no 3.x — sem `%code`, sem `%define`, sem referências nomeadas.
//
// Escopo da gramática: o que a DoD da issue pede (atribuição e comando de
// tocar) mais `repita` e blocos, o suficiente para aceitar o programa de
// exemplo do site. `se`, `senao`, `defina` e `retorna` já são reconhecidos pelo
// lexer, mas ainda não têm regra aqui — entram na CMP-04 junto com a AST.

#include <cstdio>
#include <string>

#include "frontend/frontend.h"
#include "frontend/scanner.h"

int yylex(void);
void yyerror(const char* mensagem);
%}

%token NOTA INTEIRO REAL IDENT
%token TOCA PAUSA POR ANDAMENTO OITAVA VOLUME
%token REPITA VEZES SE ENTAO SENAO DEFINA RETORNA
%token VERDADEIRO FALSO E OU NAO
%token IGUAL DIFERENTE MENOR_IGUAL MAIOR_IGUAL

/* Da menor para a maior precedência. O menos unário fica no topo, com um token
   fictício (NEG) usado só pelo %prec da regra correspondente. */
%left OU
%left E
%right NAO
%nonassoc IGUAL DIFERENTE '<' '>' MENOR_IGUAL MAIOR_IGUAL
%left '+' '-'
%left '*' '/'
%right NEG

%start programa

%%

programa
    : lista_comandos
    ;

lista_comandos
    : /* vazio — um programa sem comandos é válido */
    | lista_comandos comando
    ;

comando
    : IDENT '=' expressao                   /* tempo = 0.5     */
    | TOCA expressao POR expressao          /* toca do4 por tempo */
    | PAUSA POR expressao                   /* pausa por 1.0   */
    | ANDAMENTO expressao                   /* andamento 120   */
    | OITAVA expressao                      /* oitava 4        */
    | VOLUME expressao                      /* volume 0.8      */
    | REPITA expressao VEZES bloco          /* repita 4 vezes { ... } */
    ;

bloco
    : '{' lista_comandos '}'
    ;

expressao
    : expressao '+' expressao
    | expressao '-' expressao
    | expressao '*' expressao
    | expressao '/' expressao
    | expressao IGUAL expressao
    | expressao DIFERENTE expressao
    | expressao '<' expressao
    | expressao '>' expressao
    | expressao MENOR_IGUAL expressao
    | expressao MAIOR_IGUAL expressao
    | expressao E expressao
    | expressao OU expressao
    | NAO expressao
    | '-' expressao %prec NEG
    | '(' expressao ')'
    | NOTA
    | INTEIRO
    | REAL
    | IDENT
    | VERDADEIRO
    | FALSO
    ;

%%

void yyerror(const char* mensagem) {
    mus::scanner::erroSintatico = (mensagem != 0) ? mensagem : "erro sintático";
}

%{
// Analisador sintático da linguagem musical — CMP-02 (#9).
//
// Hoje é um RECONHECEDOR: responde se o programa pertence à linguagem, sem
// construir árvore. A AST é a CMP-04 (#21), e é lá que entra o valor semântico.
//
// Por isso não há `%union` nem `%type`. Não é economia: um reconhecedor não
// precisa de valor semântico nenhum, e escrever um agora só para reescrevê-lo
// quando a AST chegar seria dívida pura.
//
// Também é portabilidade. A versão de Bison instalada varia entre as máquinas
// do grupo e os runners da CI, e as mais antigas em circulação ainda são da
// série 2.x. Sem `%union` — e sem `%code`, sem `%define` e sem referências
// nomeadas — a gramática fica no subconjunto que compila igual da 2.3 à 3.x,
// então ninguém precisa instalar ou atualizar nada para buildar o projeto. A
// versão mínima passa a ser decisão explícita do grupo na CMP-04.
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

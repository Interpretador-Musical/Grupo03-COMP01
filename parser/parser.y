%{
// Analisador sintático da linguagem musical — CMP-02 (#9) e INT-03 (#19).
//
// Até a INT-03, isto era um RECONHECEDOR: respondia só se o programa
// pertencia à linguagem, sem construir árvore. A INT-03 (#19) precisa
// percorrer uma AST para calcular SoundEvents, então este arquivo passou a
// ter `%union`/`%type` e ações que constroem os nós de `interpreter/ast.h`.
//
// A AST construída aqui é deliberadamente mínima e sem polimorfismo (nós
// simples dentro de `std::variant`, sem `unique_ptr`, sem aninhamento de
// blocos além de `repita`) — o valor semântico do zero, mas a versão
// polimórfica de verdade, com nós que suportem `se`/`senão`/`defina`
// aninhados via `unique_ptr`, é trabalho da CMP-04 (#21). Esta AST existe só
// para o subconjunto de gramática que já está implementado abaixo.
//
// Portabilidade: a versão de Bison instalada varia entre as máquinas do
// grupo e os runners da CI, e as mais antigas em circulação ainda são da
// série 2.x. Por isso o valor semântico usa `%union` clássico — só ponteiros
// como membros, sem `%code requires`, sem `%define api.value.type variant`
// e sem o skeleton C++ (`lalr1.cc`) — que compila igual da 2.3 à 3.x. A
// alternativa moderna (`%define api.value.type variant`) exige Bison 3+ e
// quebraria o Bison 2.3 que o macOS ainda traz de fábrica.
//
// Escopo da gramática: o mesmo da CMP-02 — atribuição, comando de tocar,
// `repita` e blocos. `se`, `senao`, `defina` e `retorna` já são reconhecidos
// pelo lexer, mas ainda não têm regra aqui — entram na CMP-04 junto com a
// AST polimórfica.

#include <cstdio>
#include <string>
#include <vector>

#include "frontend/frontend.h"
#include "frontend/scanner.h"
#include "interpreter/ast.h"

int yylex(void);
void yyerror(const char* mensagem);
%}

%union {
    mus::ast::Expressao* expressao;
    mus::ast::Comando* comando;
    std::vector<mus::ast::Comando*>* comandos;
    std::string* nome;
}

%token <expressao> NOTA INTEIRO REAL VERDADEIRO FALSO
%token <nome> IDENT
%token TOCA PAUSA POR ANDAMENTO OITAVA VOLUME
%token REPITA VEZES SE ENTAO SENAO DEFINA RETORNA
%token E OU NAO
%token IGUAL DIFERENTE MENOR_IGUAL MAIOR_IGUAL

%type <expressao> expressao
%type <comando> comando
%type <comandos> lista_comandos bloco

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
    : lista_comandos { mus::scanner::arvoreAtual = $1; }
    ;

lista_comandos
    : /* vazio — um programa sem comandos é válido */
        { $$ = new std::vector<mus::ast::Comando*>(); }
    | lista_comandos comando
        { $1->push_back($2); $$ = $1; }
    ;

comando
    : IDENT '=' expressao
        { $$ = new mus::ast::Comando(mus::ast::ComandoAtribuicao{*$1, $3}); }
    | TOCA expressao POR expressao          /* toca do4 por tempo */
        { $$ = new mus::ast::Comando(mus::ast::ComandoToca{$2, $4}); }
    | PAUSA POR expressao                   /* pausa por 1.0   */
        { $$ = new mus::ast::Comando(mus::ast::ComandoPausa{$3}); }
    | ANDAMENTO expressao                   /* andamento 120   */
        { $$ = new mus::ast::Comando(mus::ast::ComandoAndamento{$2}); }
    | OITAVA expressao                      /* oitava 4        */
        { $$ = new mus::ast::Comando(mus::ast::ComandoOitava{$2}); }
    | VOLUME expressao                      /* volume 0.8      */
        { $$ = new mus::ast::Comando(mus::ast::ComandoVolume{$2}); }
    | REPITA expressao VEZES bloco          /* repita 4 vezes { ... } */
        { $$ = new mus::ast::Comando(mus::ast::ComandoRepita{$2, $4}); }
    ;

bloco
    : '{' lista_comandos '}' { $$ = $2; }
    ;

expressao
    : expressao '+' expressao
        { $$ = new mus::ast::Expressao(mus::ast::ExpressaoBinaria{mus::ast::OperadorBinario::Soma, $1, $3}); }
    | expressao '-' expressao
        { $$ = new mus::ast::Expressao(mus::ast::ExpressaoBinaria{mus::ast::OperadorBinario::Subtracao, $1, $3}); }
    | expressao '*' expressao
        { $$ = new mus::ast::Expressao(mus::ast::ExpressaoBinaria{mus::ast::OperadorBinario::Multiplicacao, $1, $3}); }
    | expressao '/' expressao
        { $$ = new mus::ast::Expressao(mus::ast::ExpressaoBinaria{mus::ast::OperadorBinario::Divisao, $1, $3}); }
    | expressao IGUAL expressao
        { $$ = new mus::ast::Expressao(mus::ast::ExpressaoBinaria{mus::ast::OperadorBinario::Igual, $1, $3}); }
    | expressao DIFERENTE expressao
        { $$ = new mus::ast::Expressao(mus::ast::ExpressaoBinaria{mus::ast::OperadorBinario::Diferente, $1, $3}); }
    | expressao '<' expressao
        { $$ = new mus::ast::Expressao(mus::ast::ExpressaoBinaria{mus::ast::OperadorBinario::Menor, $1, $3}); }
    | expressao '>' expressao
        { $$ = new mus::ast::Expressao(mus::ast::ExpressaoBinaria{mus::ast::OperadorBinario::Maior, $1, $3}); }
    | expressao MENOR_IGUAL expressao
        { $$ = new mus::ast::Expressao(mus::ast::ExpressaoBinaria{mus::ast::OperadorBinario::MenorOuIgual, $1, $3}); }
    | expressao MAIOR_IGUAL expressao
        { $$ = new mus::ast::Expressao(mus::ast::ExpressaoBinaria{mus::ast::OperadorBinario::MaiorOuIgual, $1, $3}); }
    | expressao E expressao
        { $$ = new mus::ast::Expressao(mus::ast::ExpressaoBinaria{mus::ast::OperadorBinario::ELogico, $1, $3}); }
    | expressao OU expressao
        { $$ = new mus::ast::Expressao(mus::ast::ExpressaoBinaria{mus::ast::OperadorBinario::OuLogico, $1, $3}); }
    | NAO expressao
        { $$ = new mus::ast::Expressao(mus::ast::ExpressaoUnaria{mus::ast::OperadorUnario::Nao, $2}); }
    | '-' expressao %prec NEG
        { $$ = new mus::ast::Expressao(mus::ast::ExpressaoUnaria{mus::ast::OperadorUnario::Negacao, $2}); }
    | '(' expressao ')'
        { $$ = $2; }
    | NOTA
        { $$ = $1; }
    | INTEIRO
        { $$ = $1; }
    | REAL
        { $$ = $1; }
    | IDENT
        { $$ = new mus::ast::Expressao(mus::ast::ExpressaoIdentificador{*$1}); }
    | VERDADEIRO
        { $$ = $1; }
    | FALSO
        { $$ = $1; }
    ;

%%

void yyerror(const char* mensagem) {
    mus::scanner::erroSintatico = (mensagem != 0) ? mensagem : "erro sintático";
}

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

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "frontend/frontend.h"
#include "frontend/scanner.h"
#include "interpreter/ast.h"

int yylex(void);
void yyerror(const char* mensagem);

// O nó carrega onde a expressão começa no texto, que é o que a análise
// semântica (CMP-13, #95) aponta. Na binária isso é o operando da esquerda
// (`la4 - 2` começa no `la4`)...
static mus::ast::Expressao* binaria(mus::ast::OperadorBinario operador,
                                    mus::ast::Expressao* esquerda,
                                    mus::ast::Expressao* direita) {
    mus::ast::Expressao* no = new mus::ast::Expressao(
        mus::ast::ExpressaoBinaria{operador, esquerda, direita});
    no->posicao = esquerda->posicao;
    no->profundidade = 1 + std::max(esquerda->profundidade, direita->profundidade);
    return no;
}

// ...e na unária é o próprio operador: `-do4` começa no `-`, não no `do4`. A
// posição vem do `@1` da regra, e não do operando, porque `-` e `nao` não têm
// valor semântico no `%union` para carregá-la (ver `%locations` abaixo).
static mus::ast::Expressao* unaria(mus::ast::OperadorUnario operador,
                                   mus::ast::Expressao* operando,
                                   mus::ast::Posicao posicao) {
    mus::ast::Expressao* no = new mus::ast::Expressao(
        mus::ast::ExpressaoUnaria{operador, operando});
    no->posicao = posicao;
    no->profundidade = 1 + operando->profundidade;
    return no;
}

// `filhoMaisFundo` é a profundidade do filho mais fundo do comando — ver
// `kProfundidadeMaxima` em `interpreter/ast.h`.
static mus::ast::Comando* novoComando(mus::ast::Comando no, int filhoMaisFundo) {
    mus::ast::Comando* novo = new mus::ast::Comando(std::move(no));
    novo->profundidade = 1 + filhoMaisFundo;
    return novo;
}

static int maisFundo(const std::vector<mus::ast::Comando*>* comandos) {
    int profundidade = 0;
    for (const mus::ast::Comando* c : *comandos) {
        profundidade = std::max(profundidade, c->profundidade);
    }
    return profundidade;
}

// Recusa o nó que acabou de ser montado se ele passa de `kProfundidadeMaxima`
// (CMP-08, #44). A mensagem aponta onde o limite foi cruzado — o operador, o
// `-`, o `repita` —, e não o começo da expressão, que numa soma longa fica
// lá atrás.
//
// O nó é liberado aqui porque, num YYABORT disparado por uma ação, o Bison não
// passa os símbolos da própria regra ao `%destructor`: só o resto da pilha e o
// lookahead. O nó novo já é dono desses símbolos, então liberá-lo cobre tudo.
#define LIMITA_PROFUNDIDADE(no, local)                                        \
    do {                                                                      \
        if ((no)->profundidade > mus::ast::kProfundidadeMaxima) {             \
            recusarAninhamento((local).first_line, (local).first_column);     \
            mus::ast::liberar(no);                                            \
            YYABORT;                                                          \
        }                                                                     \
    } while (0)

static void recusarAninhamento(int linha, int coluna) {
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer),
                  "Linha %d, Coluna %d: aninhamento passa do limite de %d níveis",
                  linha, coluna, mus::ast::kProfundidadeMaxima);
    mus::scanner::erroSintatico = buffer;
}
%}

%union {
    mus::ast::Expressao* expressao;
    mus::ast::Comando* comando;
    std::vector<mus::ast::Comando*>* comandos;
}

/* Rastreamento de posição do Bison, preenchido pelo YY_USER_ACTION do
   `lexer/lexer.l`. Dá `@N` às ações, o único jeito de uma regra saber onde
   está um token sem valor semântico — aqui, o `-` e o `nao` das unárias.
   Também compila no Bison 2.3: `%locations` é tão antigo quanto o `%union`. */
%locations

/* IDENT também chega como Expressao (um ExpressaoIdentificador) para trazer a
   posição do lexer junto do nome — ver `lexer/lexer.l`. */
%token <expressao> NOTA INTEIRO REAL VERDADEIRO FALSO IDENT
%token TOCA PAUSA POR ANDAMENTO OITAVA VOLUME
%token REPITA VEZES SE ENTAO SENAO DEFINA RETORNA
%token E OU NAO
%token IGUAL DIFERENTE MENOR_IGUAL MAIOR_IGUAL

%type <expressao> expressao
%type <comando> comando
%type <comandos> lista_comandos bloco

/* Num erro de sintaxe o Bison desempilha o que já tinha montado e descarta o
   token de lookahead. Sem isto, esses nós vazam — e o AddressSanitizer da
   MAT-11 (#60) acusa cada um. Escrito com a lista de símbolos, e não com
   `<tipo>`, porque a forma por tipo não existe no Bison 2.3. */
%destructor { mus::ast::liberar($$); } NOTA INTEIRO REAL VERDADEIRO FALSO IDENT expressao
%destructor { mus::ast::liberar($$); } comando
%destructor { mus::ast::liberar($$); } lista_comandos bloco

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
        { $$ = novoComando(mus::ast::ComandoAtribuicao{
              std::get<mus::ast::ExpressaoIdentificador>(*$1).nome, $3, $1->posicao},
              $3->profundidade);
          delete $1;
          LIMITA_PROFUNDIDADE($$, @1); }
    | TOCA expressao POR expressao          /* toca do4 por tempo */
        { $$ = novoComando(mus::ast::ComandoToca{$2, $4},
                           std::max($2->profundidade, $4->profundidade));
          LIMITA_PROFUNDIDADE($$, @1); }
    | PAUSA POR expressao                   /* pausa por 1.0   */
        { $$ = novoComando(mus::ast::ComandoPausa{$3}, $3->profundidade);
          LIMITA_PROFUNDIDADE($$, @1); }
    | ANDAMENTO expressao                   /* andamento 120   */
        { $$ = novoComando(mus::ast::ComandoAndamento{$2}, $2->profundidade);
          LIMITA_PROFUNDIDADE($$, @1); }
    | OITAVA expressao                      /* oitava 4        */
        { $$ = novoComando(mus::ast::ComandoOitava{$2}, $2->profundidade);
          LIMITA_PROFUNDIDADE($$, @1); }
    | VOLUME expressao                      /* volume 0.8      */
        { $$ = novoComando(mus::ast::ComandoVolume{$2}, $2->profundidade);
          LIMITA_PROFUNDIDADE($$, @1); }
    | REPITA expressao VEZES bloco          /* repita 4 vezes { ... } */
        { $$ = novoComando(mus::ast::ComandoRepita{$2, $4},
                           std::max($2->profundidade, maisFundo($4)));
          LIMITA_PROFUNDIDADE($$, @1); }
    ;

bloco
    : '{' lista_comandos '}' { $$ = $2; }
    ;

expressao
    : expressao '+' expressao
        { $$ = binaria(mus::ast::OperadorBinario::Soma, $1, $3);
          LIMITA_PROFUNDIDADE($$, @2); }
    | expressao '-' expressao
        { $$ = binaria(mus::ast::OperadorBinario::Subtracao, $1, $3);
          LIMITA_PROFUNDIDADE($$, @2); }
    | expressao '*' expressao
        { $$ = binaria(mus::ast::OperadorBinario::Multiplicacao, $1, $3);
          LIMITA_PROFUNDIDADE($$, @2); }
    | expressao '/' expressao
        { $$ = binaria(mus::ast::OperadorBinario::Divisao, $1, $3);
          LIMITA_PROFUNDIDADE($$, @2); }
    | expressao IGUAL expressao
        { $$ = binaria(mus::ast::OperadorBinario::Igual, $1, $3);
          LIMITA_PROFUNDIDADE($$, @2); }
    | expressao DIFERENTE expressao
        { $$ = binaria(mus::ast::OperadorBinario::Diferente, $1, $3);
          LIMITA_PROFUNDIDADE($$, @2); }
    | expressao '<' expressao
        { $$ = binaria(mus::ast::OperadorBinario::Menor, $1, $3);
          LIMITA_PROFUNDIDADE($$, @2); }
    | expressao '>' expressao
        { $$ = binaria(mus::ast::OperadorBinario::Maior, $1, $3);
          LIMITA_PROFUNDIDADE($$, @2); }
    | expressao MENOR_IGUAL expressao
        { $$ = binaria(mus::ast::OperadorBinario::MenorOuIgual, $1, $3);
          LIMITA_PROFUNDIDADE($$, @2); }
    | expressao MAIOR_IGUAL expressao
        { $$ = binaria(mus::ast::OperadorBinario::MaiorOuIgual, $1, $3);
          LIMITA_PROFUNDIDADE($$, @2); }
    | expressao E expressao
        { $$ = binaria(mus::ast::OperadorBinario::ELogico, $1, $3);
          LIMITA_PROFUNDIDADE($$, @2); }
    | expressao OU expressao
        { $$ = binaria(mus::ast::OperadorBinario::OuLogico, $1, $3);
          LIMITA_PROFUNDIDADE($$, @2); }
    | NAO expressao
        { $$ = unaria(mus::ast::OperadorUnario::Nao, $2,
                      mus::ast::Posicao{@1.first_line, @1.first_column});
          LIMITA_PROFUNDIDADE($$, @1); }
    | '-' expressao %prec NEG
        { $$ = unaria(mus::ast::OperadorUnario::Negacao, $2,
                      mus::ast::Posicao{@1.first_line, @1.first_column});
          LIMITA_PROFUNDIDADE($$, @1); }
    | '(' expressao ')'
        { $$ = $2; }
    | NOTA
        { $$ = $1; }
    | INTEIRO
        { $$ = $1; }
    | REAL
        { $$ = $1; }
    | IDENT
        { $$ = $1; }
    | VERDADEIRO
        { $$ = $1; }
    | FALSO
        { $$ = $1; }
    ;

%%

void yyerror(const char* mensagem) {
    char buffer[256];

    const char* texto = (mensagem != 0) ? mensagem : "erro sintático";
    if (yychar == 0) {
        texto = "fim inesperado do arquivo";
    }
    // O Bison para sozinho quando a pilha dele passa de YYMAXDEPTH (10 000) e
    // avisa em inglês. Chega aqui antes do `LIMITA_PROFUNDIDADE` em tudo o que
    // aninha para a direita — `((((`, `----`, `repita` dentro de `repita` —,
    // porque nesses casos o Bison empilha todos os tokens antes de reduzir o
    // primeiro nó (CMP-08, #44).
    if (std::strcmp(texto, "memory exhausted") == 0) {
        texto = "aninhamento passa do limite do analisador sintático";
    }

    std::snprintf(buffer, sizeof(buffer), "Linha %d, Coluna %d: %s",
                  mus::scanner::tokenAtual.linha,
                  mus::scanner::tokenAtual.coluna,
                  texto);

    mus::scanner::erroSintatico = buffer;
}

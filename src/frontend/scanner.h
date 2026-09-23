#ifndef MUS_FRONTEND_SCANNER_H
#define MUS_FRONTEND_SCANNER_H

#include <string>
#include <vector>

#include "frontend/frontend.h"
#include "interpreter/ast.h"

// Estado compartilhado entre o `lexer.l` e o `frontend.cpp`. É interno ao
// frontend: quem está de fora usa `tokenize()`, `parseString()` e
// `parseToAst()`.
//
// `tokenAtual` continua existindo à parte do `yylval` do Bison: um Token
// carrega mais coisa (lexema, linha, coluna, semitom, oitava) do que caberia
// confortavelmente num `%union`, e é usado por `tokenize()`/`--tokens`, que
// não corre o parser. O valor semântico de verdade — os nós de AST que as
// regras do Bison constroem — vive em `yylval` (ver `parser/parser.y` e
// `lexer/lexer.l`, que preenchem `yylval.expressao`/`yylval.nome` na mesma
// ação que já registra `tokenAtual`), desde a INT-03 (#19).
//
// Consequência aceita: isto não é reentrante. O `yylex()` do Flex também não é.
namespace mus {
namespace scanner {

// Preenchido pela ação da regra que casou; lido logo em seguida.
extern Token tokenAtual;

// Raiz da AST construída pela última chamada a `yyparse()` bem-sucedida —
// preenchida pela ação de `programa` em `parser.y`. `parseToAst()` devolve
// este ponteiro; os nós não têm dono (ver comentário em `interpreter/ast.h`).
extern std::vector<ast::Comando*>* arvoreAtual;

// Última mensagem que o `yyerror()` do Bison produziu. Crua, sem formatação:
// montar a mensagem de erro voltada ao usuário, com "Linha X, Coluna Y", é a
// CMP-03 (#15). Aqui ela só é guardada para `parseString()` devolver.
extern std::string erroSintatico;

// Zera posição, token e AST. Chamado no início de cada varredura.
void reiniciar();

// Chamado por YY_USER_ACTION antes de toda ação: fixa em `tokenAtual` a posição
// onde este lexema começa e então avança os contadores por cima do texto
// consumido. Contar aqui, e não na ação, é o que faz um comentário de bloco de
// várias linhas mexer na contagem sem precisar de regra própria.
void avancarPosicao(const char* texto, int tamanho);

// Registra tipo e lexema do token que acabou de casar. A posição já foi fixada
// por `avancarPosicao`.
void registrarToken(TipoToken tipo, const char* texto, int tamanho);

// Além de registrar, decompõe a nota em semitom, oitava e MIDI.
void registrarNota(const char* texto, int tamanho);

// Além de registrar, converte o lexema em número.
void registrarNumero(TipoToken tipo, const char* texto, int tamanho);

// Decompõe `do4`, `re#`, `mib`, `sib3`. Devolve false se o lexema não for uma
// nota — não deve acontecer vindo do lexer, mas os testes exercitam isso.
//
// `oitava` sai -1 quando não foi escrita, e nesse caso `midi` também sai -1.
// Com oitava: midi = (oitava + 1) * 12 + semitom, o que põe `do4` em 60 e
// `la4` em 69, casando com o kMidiA4 de `core/pitch.h`.
bool analisarNota(const std::string& lexema, int* semitom, int* oitava, int* midi);

}  // namespace scanner
}  // namespace mus

#endif  // MUS_FRONTEND_SCANNER_H

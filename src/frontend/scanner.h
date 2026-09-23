#ifndef MUS_FRONTEND_SCANNER_H
#define MUS_FRONTEND_SCANNER_H

#include <string>

#include "frontend/frontend.h"

// Estado compartilhado entre o `lexer.l` e o `frontend.cpp`. É interno ao
// frontend: quem está de fora usa `tokenize()` e `parseString()`.
//
// O valor semântico do token vive aqui, e não no `yylval` do Bison, por dois
// motivos. Primeiro, um Token carrega mais coisa (lexema, linha, coluna,
// semitom, oitava) do que caberia confortavelmente num `%union`. Segundo, o
// parser de hoje é só um reconhecedor e não precisa de valor semântico nenhum,
// então a gramática fica sem `%union` — o menor compromisso possível com a API
// do Bison, que varia de versão entre as máquinas do grupo e a CI. O CMP-04
// (#21) introduz o valor semântico do zero.
//
// Consequência aceita: isto não é reentrante. O `yylex()` do Flex também não é.
namespace mus {
namespace scanner {

// Preenchido pela ação da regra que casou; lido logo em seguida.
extern Token tokenAtual;

// Última mensagem que o `yyerror()` do Bison produziu. Crua, sem formatação:
// montar a mensagem de erro voltada ao usuário, com "Linha X, Coluna Y", é a
// CMP-03 (#15). Aqui ela só é guardada para `parseString()` devolver.
extern std::string erroSintatico;

// Zera posição e token. Chamado no início de cada varredura.
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

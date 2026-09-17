#ifndef MUS_FRONTEND_FRONTEND_H
#define MUS_FRONTEND_FRONTEND_H

#include <string>
#include <vector>

namespace mus {

// O vocabulário da linguagem, decidido na reunião de definição de sintaxe:
// português, notas em `do re mi`, duração em tempos.
//
// Este enum é deliberadamente independente dos códigos que o Bison gera em
// `parser.tab.h`: os testes e a CLI falam nesta linguagem, e não no cabeçalho
// gerado. Quando o CMP-04 (#21) reescrever a gramática para construir a AST, os
// códigos do Bison mudam sem arrastar junto quem só queria olhar tokens.
enum class TipoToken {
    FimDeArquivo,

    // Literais e nomes
    Nota,           // do, re#, mib, sol4, sib3
    Inteiro,        // 4        — separado do real: é o NFR da CMP-02 (#9)
    Real,           // 0.5
    Identificador,  // tempo, arpejo

    // Comandos
    Toca,
    Pausa,
    Por,       // separa altura de duração: `toca do4 por tempo`
    Andamento, // BPM
    Oitava,    // oitava corrente
    Volume,

    // Controle de fluxo
    Repita,
    Vezes,
    Se,
    Entao,
    Senao,
    Defina,
    Retorna,

    // Booleanos e operadores lógicos
    Verdadeiro,
    Falso,
    // Prefixados porque o Bison gera `#define E`, `#define OU` e `#define NAO`
    // para os tokens de mesmo nome, e um membro de enum chamado `E` viraria o
    // número da macro no pré-processador.
    OperadorE,
    OperadorOu,
    OperadorNao,

    // Aritmética
    Soma,
    Subtracao,
    Multiplicacao,
    Divisao,

    // Comparação
    Igual,        // ==
    Diferente,    // !=
    Menor,        // <
    MenorOuIgual, // <=
    Maior,        // >
    MaiorOuIgual, // >=

    // Pontuação
    Atribuicao,  // =
    AbreParen,
    FechaParen,
    AbreChave,
    FechaChave,
    Virgula,

    // Caractere que não casa com nenhuma regra. O lexer registra e segue em
    // frente, em vez de abortar — quem decide o que fazer é o parser.
    Desconhecido
};

struct Token {
    TipoToken tipo = TipoToken::FimDeArquivo;
    std::string lexema;

    // Posição do PRIMEIRO caractere do token, ambas começando em 1. Adianta
    // metade da CMP-03 (#15); a formatação da mensagem de erro é lá.
    int linha = 0;
    int coluna = 0;

    // Válido para Inteiro e Real.
    double valor = 0.0;

    // Válidos para Nota. `oitava` é -1 quando o programa não a escreveu
    // (`toca do`), caso em que vale a oitava corrente do interpretador e por
    // isso `midi` também fica -1 — o lexer não tem como saber a altura final.
    // Semitom dentro da oitava, com o acidente já aplicado. Normalmente 0..11,
    // mas `dob` dá -1 e `si#` dá 12 — e isso está certo: dó bemol 4 é o si 3, e
    // si sustenido 4 é o dó 5. A conta do MIDI absorve os dois sem caso especial.
    int semitom = -1;
    int oitava = -1;
    int midi = -1;
};

// Nome legível do token, para a tabela do `--tokens` e para as mensagens de
// erro dos testes.
const char* nomeToken(TipoToken tipo);

// Converte o programa inteiro em tokens. Não lança: caractere inválido vira um
// token `Desconhecido` e a varredura continua.
std::vector<Token> tokenize(const std::string& fonte);

// Roda o analisador sintático sobre o programa. Hoje é só um reconhecedor:
// responde se o texto pertence à linguagem, sem construir árvore (isso é a
// CMP-04, #21). Quando `erro` não é nulo, recebe a mensagem crua do Bison.
bool parseString(const std::string& fonte, std::string* erro = nullptr);

}  // namespace mus

#endif  // MUS_FRONTEND_FRONTEND_H

/**
 * @file frontend.h
 * @brief Interface C++ do analisador léxico (Flex) e sintático (Bison).
 */

#ifndef MUS_FRONTEND_FRONTEND_H
#define MUS_FRONTEND_FRONTEND_H

#include <string>
#include <vector>

#include "interpreter/ast.h"

namespace mus {

/**
 * @brief Categorias de tokens reconhecidas pelo analisador léxico da DSL.
 *
 * Definido de forma independente dos códigos internos que o Bison gera em
 * `parser.tab.h`, mantendo testes e CLI (`tokenize()`, `--tokens`) desacoplados
 * das alterações de numeração do parser gerado.
 */
enum class TipoToken {
    FimDeArquivo,   ///< Fim do fluxo de entrada (`EOF`).

    // Literais e nomes
    Nota,           ///< Nota musical (ex.: `do`, `re#`, `mib`, `sol4`, `sib3`).
    Inteiro,        ///< Literal inteiro (ex.: `4`), distinto de `Real` (CMP-02, #9).
    Real,           ///< Literal decimal ou figura rítmica (ex.: `0.5`, `colcheia`).
    String,         ///< Literal textual entre aspas duplas (nome de sample, INT-05).
    Identificador,  ///< Identificador de variável (ex.: `tempo`, `arpejo`).

    // Comandos
    Toca,           ///< Palavra reservada `toca`.
    Pausa,          ///< Palavra reservada `pausa`.
    Por,            ///< Separador obrigatório `por` entre altura e duração.
    Andamento,      ///< Palavra reservada `andamento` (BPM).
    Oitava,         ///< Palavra reservada `oitava`.
    Volume,         ///< Palavra reservada `volume`.

    // Controle de fluxo
    Repita,         ///< Palavra reservada `repita`.
    Vezes,          ///< Palavra reservada `vezes` (ou `vez`).
    Se,             ///< Palavra reservada `se` (planejada).
    Entao,          ///< Palavra reservada `entao` / `então` (planejada).
    Senao,          ///< Palavra reservada `senao` / `senão` (planejada).
    Defina,         ///< Palavra reservada `defina` (planejada).
    Retorna,        ///< Palavra reservada `retorna` (planejada).

    // Booleanos e operadores lógicos
    Verdadeiro,     ///< Literal booleano `verdadeiro`.
    Falso,          ///< Literal booleano `falso`.
    // Prefixados porque o Bison gera `#define E`, `#define OU` e `#define NAO`
    // para os tokens de mesmo nome, e um membro de enum chamado `E` viraria o
    // número da macro no pré-processador.
    OperadorE,      ///< Operador lógico `e`.
    OperadorOu,     ///< Operador lógico `ou`.
    OperadorNao,    ///< Operador lógico `nao` / `não`.

    // Aritmética
    Soma,           ///< Operador `+`.
    Subtracao,      ///< Operador `-`.
    Multiplicacao,  ///< Operador `*`.
    Divisao,        ///< Operador `/`.

    // Comparação
    Igual,          ///< Operador relacional `==`.
    Diferente,      ///< Operador relacional `!=`.
    Menor,          ///< Operador relacional `<`.
    MenorOuIgual,   ///< Operador relacional `<=`.
    Maior,          ///< Operador relacional `>`.
    MaiorOuIgual,   ///< Operador relacional `>=`.

    // Pontuação
    Atribuicao,     ///< Operador de atribuição `=`.
    AbreParen,      ///< Parêntese de abertura `(`.
    FechaParen,     ///< Parêntese de fechamento `)`.
    AbreChave,      ///< Chave de abertura `{`.
    FechaChave,     ///< Chave de fechamento `}`.
    Virgula,        ///< Vírgula `,` separadora de trilhas paralelas.

    /**
     * Caractere que não casa com nenhuma regra léxica; o lexer registra e
     * delega a recusa ao parser em vez de abortar imediatamente.
     */
    Desconhecido
};

/**
 * @brief Token extraído pelo analisador léxico com metadados de posição e valor.
 */
struct Token {
    TipoToken tipo = TipoToken::FimDeArquivo; ///< Tipo léxico do token.
    std::string lexema;                       ///< Texto exato extraído do código-fonte.

    int linha = 0;  ///< Linha (base 1) do primeiro caractere do token.
    int coluna = 0; ///< Coluna (base 1) do primeiro caractere do token.

    double valor = 0.0; ///< Valor numérico preenchido para tokens `Inteiro` e `Real`.

    /**
     * @name Metadados de Nota
     * Para tokens `Nota`, `oitava` e `midi` valem `-1` quando a oitava não foi
     * escrita explicitamente (`toca do`), pois a altura final depende da oitava
     * corrente no interpretador. `semitom` pode valer `-1` (`dob`) ou `12` (`si#`).
     * @{
     */
    int semitom = -1; ///< Semitom relativo à oitava (normalmente 0..11; `-1` em `dob`, `12` em `si#`).
    int oitava = -1;  ///< Oitava explícita (0..9) ou `-1` para oitava corrente.
    int midi = -1;    ///< Número MIDI pré-calculado quando a oitava é explícita; `-1` caso contrário.
    /** @} */
};

/**
 * @brief Devolve o nome legível de um `TipoToken` para `--tokens` e diagnósticos.
 * @param tipo Categoria do token.
 * @return String literal estática descrevendo o token.
 */
const char* nomeToken(TipoToken tipo);

/**
 * @brief Executa a análise léxica sobre todo o código-fonte `.mus`.
 *
 * Não lança exceções: caracteres inválidos viram tokens `TipoToken::Desconhecido`
 * e a varredura prossegue até o fim do arquivo.
 *
 * @param fonte Conteúdo completo do programa `.mus`.
 * @return Sequência de tokens extraídos (sem incluir `FimDeArquivo`).
 */
std::vector<Token> tokenize(const std::string& fonte);

/**
 * @brief Executa o analisador sintático apenas para validar a gramática do programa.
 *
 * Usado por `test_parser.cpp` e casos de validação sem construção da AST.
 *
 * @param fonte Código-fonte `.mus` a validar.
 * @param erro Ponteiro opcional que recebe a mensagem crua do Bison em falha.
 * @return `true` se o texto pertence à gramática da linguagem; `false` caso contrário.
 */
bool parseString(const std::string& fonte, std::string* erro = nullptr);

/**
 * @brief Executa a análise sintática e constrói a Árvore Sintática Abstrata (AST).
 *
 * @param fonte Código-fonte `.mus` completo.
 * @param erro Ponteiro opcional preenchido com a mensagem de erro sintático em falha.
 * @return `ast::Arvore` dona da lista raiz de comandos, ou `nullptr` em caso de erro.
 */
ast::Arvore parseToAst(const std::string& fonte, std::string* erro = nullptr);

}  // namespace mus

#endif  // MUS_FRONTEND_FRONTEND_H

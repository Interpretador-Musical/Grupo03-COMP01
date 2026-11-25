/**
 * @file ast.h
 * @brief Definição dos nós, posições e helpers de ciclo de vida da AST.
 */

#ifndef MUS_INTERPRETER_AST_H
#define MUS_INTERPRETER_AST_H

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace mus {
namespace ast {

/**
 * @brief Operadores binários suportados pelas expressões da linguagem.
 *
 * AST mínima da INT-03 (#19): cobre atribuição, toca, pausa, andamento,
 * oitava, volume, repita+bloco, grupo de trilhas e expressões aritméticas e
 * lógicas sobre nós `struct` simples em `std::variant` (C++17).
 *
 * @note `ELogico`/`OuLogico` (e não `E`/`Ou`) existem porque o Bison gera
 * `#define E <código>` para o token `E` da gramática (ver `TipoToken::OperadorE`
 * em `frontend.h`). Em versões como o Bison 2.3 do macOS, um enumerador chamado
 * `E` seria substituído pela macro no meio do `enum`, quebrando a compilação.
 */
enum class OperadorBinario {
    Soma,          ///< Soma aritmética ou transposição `nota + numero`.
    Subtracao,     ///< Subtração aritmética, transposição ou intervalo.
    Multiplicacao, ///< Multiplicação aritmética.
    Divisao,       ///< Divisão aritmética.
    Igual,         ///< Igualdade (`==`).
    Diferente,     ///< Diferença (`!=`).
    Menor,         ///< Menor que (`<`).
    Maior,         ///< Maior que (`>`).
    MenorOuIgual,  ///< Menor ou igual (`<=`).
    MaiorOuIgual,  ///< Maior ou igual (`>=`).
    ELogico,       ///< Conjunção lógica (`e`).
    OuLogico       ///< Disjunção lógica (`ou`).
};

/**
 * @brief Operadores unários de prefixo da linguagem.
 */
enum class OperadorUnario {
    Negacao, ///< Menos unário (`-x`).
    Nao      ///< Negação lógica (`nao` / `não`).
};

/**
 * @brief Posição de origem de um nó no código-fonte `.mus`.
 *
 * Linha e coluna começam em 1, seguindo a convenção de `Token`, para que a
 * análise semântica (CMP-13, #95) reporte `"Linha X, Coluna Y"`. Zero indica
 * nó sintético sem posição (ex.: montado manualmente em testes).
 */
struct Posicao {
    int linha = 0;  ///< Linha base 1 no arquivo-fonte (0 se sintético).
    int coluna = 0; ///< Coluna base 1 no arquivo-fonte (0 se sintético).
};

/**
 * @brief Limite máximo de profundidade permitido na AST (CMP-08, #44).
 *
 * Semântica, interpretador e `liberar()` descem a árvore recursivamente; uma
 * árvore muito funda estouraria a pilha do processo (ex.: `x = 1+1+...+1` com
 * 10 000 termos, já que a recursão à esquerda mantém a pilha do Bison rasa).
 * O parser recusa qualquer nó que exceda este teto, mantendo margem segura
 * mesmo com AddressSanitizer ou threads de pilha reduzida.
 */
constexpr int kProfundidadeMaxima = 256;

struct Expressao;

/**
 * @brief Literal numérico (inteiro ou real armazenado em `double`).
 */
struct ExpressaoLiteralNumero {
    double valor = 0.0; ///< Valor numérico do literal.
};

/**
 * @brief Literal booleano (`verdadeiro` ou `falso`).
 */
struct ExpressaoLiteralBooleano {
    bool valor = false; ///< Valor lógico do literal.
};

/**
 * @brief Literal textual usado para identificar samples WAV pelo nome (INT-05).
 */
struct ExpressaoLiteralString {
    std::string valor; ///< Conteúdo da string sem as aspas delimitadoras.
};

/**
 * @brief Referência a uma nota musical com oitava opcional.
 *
 * Não resolve o valor MIDI no parser: `oitava == -1` indica uso da oitava
 * corrente do interpretador em tempo de execução, pois uma mesma nota dentro
 * de um `repita` pode ser revisitada após um comando `oitava`.
 */
struct ExpressaoNota {
    int semitom = 0; ///< Deslocamento em semitons dentro da oitava.
    int oitava = -1; ///< Oitava explícita (0..9) ou -1 para oitava corrente.
};

/**
 * @brief Referência a uma variável pelo identificador.
 */
struct ExpressaoIdentificador {
    std::string nome; ///< Nome da variável referenciada.
};

/**
 * @brief Expressão unária aplicada sobre um operando.
 */
struct ExpressaoUnaria {
    OperadorUnario operador = OperadorUnario::Negacao; ///< Operador unário aplicado.
    Expressao* operando = nullptr;                    ///< Subexpressão operando (propriedade deste nó).
};

/**
 * @brief Expressão binária entre duas subexpressões.
 */
struct ExpressaoBinaria {
    OperadorBinario operador = OperadorBinario::Soma; ///< Operador binário aplicado.
    Expressao* esquerda = nullptr;                    ///< Operando esquerdo (propriedade deste nó).
    Expressao* direita = nullptr;                     ///< Operando direito (propriedade deste nó).
};

/**
 * @brief Nó variante que representa qualquer expressão da AST.
 *
 * Folhas carregam a posição do próprio token; nós binários usam a posição do
 * operando à esquerda; nós unários usam a posição do operador (`-` ou `nao`).
 */
struct Expressao : std::variant<ExpressaoLiteralNumero, ExpressaoLiteralBooleano,
                                ExpressaoLiteralString, ExpressaoNota,
                                ExpressaoIdentificador, ExpressaoUnaria,
                                ExpressaoBinaria> {
    using variant::variant;

    Posicao posicao;      ///< Localização de origem no código-fonte `.mus`.
    int profundidade = 1; ///< 1 em folhas; 1 + maior profundidade dos filhos nos demais.
};

struct Comando;

/**
 * @brief Comando de atribuição (`nome = valor`), que declara ou atualiza variável.
 */
struct ComandoAtribuicao {
    std::string nome;             ///< Nome da variável à esquerda do `=`.
    Expressao* valor = nullptr;   ///< Expressão atribuída (propriedade deste nó).
    Posicao posicao;              ///< Posição do identificador à esquerda do `=`.
};

/**
 * @brief Comando `toca <altura> por <duracao>`.
 */
struct ComandoToca {
    Expressao* altura = nullptr;  ///< Expressão de altura (nota, MIDI ou nome de sample).
    Expressao* duracao = nullptr; ///< Expressão numérica de duração em tempos.
};

/**
 * @brief Comando `pausa por <duracao>`.
 */
struct ComandoPausa {
    Expressao* duracao = nullptr; ///< Expressão numérica de duração da pausa em tempos.
};

/**
 * @brief Comando `andamento <valor>` que altera o BPM a partir do cursor atual.
 */
struct ComandoAndamento {
    Expressao* valor = nullptr; ///< Expressão numérica do novo andamento em BPM.
};

/**
 * @brief Comando `oitava <valor>` que altera a oitava padrão de notas sem oitava.
 */
struct ComandoOitava {
    Expressao* valor = nullptr; ///< Expressão numérica da nova oitava (`-1` a `9`).
};

/**
 * @brief Comando `volume <valor>` que altera o ganho corrente (`0.0` a `1.0`).
 */
struct ComandoVolume {
    Expressao* valor = nullptr; ///< Expressão numérica do novo volume.
};

/**
 * @brief Comando de repetição `repita <vezes> vezes { <corpo> }`.
 */
struct ComandoRepita {
    Expressao* vezes = nullptr;              ///< Quantidade de iterações (truncada para inteiro).
    std::vector<Comando*>* corpo = nullptr;  ///< Sequência de comandos do bloco.
};

/**
 * @brief Grupo de trilhas paralelas `{ a, b, ... }` (CMP-07, #39).
 *
 * Cada trilha é uma sequência de comandos iniciada no mesmo instante de tempo.
 * A gramática exige ao menos duas trilhas separadas por vírgula; qualquer trilha
 * individual pode ser vazia.
 */
struct ComandoGrupo {
    std::vector<std::vector<Comando*>*>* trilhas = nullptr; ///< Lista de trilhas paralelas.
};

/**
 * @brief Nó variante que representa qualquer comando executável da AST.
 */
struct Comando : std::variant<ComandoAtribuicao, ComandoToca, ComandoPausa,
                               ComandoAndamento, ComandoOitava, ComandoVolume,
                               ComandoRepita, ComandoGrupo> {
    using variant::variant;

    int profundidade = 1; ///< 1 + maior profundidade entre expressões, corpo ou trilhas.
};

/**
 * @brief Libera recursivamente uma expressão e todas as suas subexpressões.
 * @param expressao Ponteiro para a expressão a liberar (aceita `nullptr`).
 */
void liberar(Expressao* expressao);

/**
 * @brief Libera recursivamente um comando e todos os nós filhos associados.
 * @param comando Ponteiro para o comando a liberar (aceita `nullptr`).
 */
void liberar(Comando* comando);

/**
 * @brief Libera um vetor de comandos e cada comando apontado por ele.
 * @param comandos Ponteiro para a lista de comandos a liberar (aceita `nullptr`).
 */
void liberar(std::vector<Comando*>* comandos);

/**
 * @brief Libera a lista de trilhas de um `ComandoGrupo` e todos os seus comandos.
 * @param trilhas Ponteiro para o vetor de trilhas a liberar (aceita `nullptr`).
 */
void liberar(std::vector<std::vector<Comando*>*>* trilhas);

/**
 * @brief Deleter customizado usado por `ast::Arvore` para destruir a AST via RAII.
 */
struct LiberaArvore {
    /**
     * @brief Invoca `liberar()` sobre o vetor raiz de comandos.
     * @param comandos Ponteiro para a raiz da AST.
     */
    void operator()(std::vector<Comando*>* comandos) const { liberar(comandos); }
};

/**
 * @brief Ponteiro inteligente dono da AST completa de um programa `.mus`.
 *
 * Libera automaticamente todos os nós da árvore ao sair do escopo de quem
 * recebeu o retorno de `parseToAst()`.
 */
using Arvore = std::unique_ptr<std::vector<Comando*>, LiberaArvore>;

}  // namespace ast
}  // namespace mus

#endif  // MUS_INTERPRETER_AST_H

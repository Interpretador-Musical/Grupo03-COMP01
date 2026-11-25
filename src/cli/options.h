/**
 * @file options.h
 * @brief Definição e parsing das opções de linha de comando da CLI.
 */

#ifndef MUS_CLI_OPTIONS_H
#define MUS_CLI_OPTIONS_H

#include <string>

#include "cli/logger.h"

namespace mus {

/** @brief Versão semântica exibida pela flag `--version`. */
extern const char* const kProgramVersion;

/**
 * @brief Opções consolidadas a partir dos argumentos da linha de comando.
 */
struct Options {
    std::string inputPath;               ///< Caminho do `.mus`; vazio se não informado.
    std::string outputWavPath;           ///< Caminho do `.wav` de saída (COMP-C, #108).
    LogLevel logLevel = LogLevel::Info;  ///< Nível de detalhamento do logger (`stderr`).
    bool showHelp = false;               ///< Exibe ajuda (`-h` / `--help`) e sai.
    bool showVersion = false;            ///< Exibe versão (`--version`) e sai.
    bool noAudio = false;                ///< Pula a inicialização do dispositivo de áudio.
    bool testTone = false;               ///< Toca a senoide de teste de 440 Hz (DSP-01, #6).
    bool demo = false;                   ///< Imprime uma timeline de exemplo sem arquivo `.mus`.
    bool loop = false;                   ///< Mantém o motor de tempo rodando até `Ctrl+C`.
    bool showTokens = false;             ///< Imprime a tabela de tokens (`--tokens`).
    bool showSymbols = false;            ///< Imprime a tabela de símbolos (`--simbolos`).
    bool emitTimeline = false;           ///< Emite a Timeline compilada em CSV (`--emit-timeline`).
    bool otimizar = true;                ///< Ativa a dobra de constantes (COMP-D, #110).
    double bpm = 120.0;                  ///< Andamento em BPM usado pelo `--demo`.
    bool ok = true;                      ///< `false` quando ocorreu erro de uso nos argumentos.
    std::string error;                   ///< Mensagem de erro pronta para exibição.
};

/**
 * @brief Interpreta `argc`/`argv` sem abortar nem escrever em saída padrão/erro.
 * @param argc Contagem de argumentos recebida por `main()`.
 * @param argv Vetor de strings de argumentos recebido por `main()`.
 * @return Estrutura `Options` preenchida (com `ok == false` e `error` em falha).
 */
Options parseArguments(int argc, char** argv);

/**
 * @brief Imprime o texto de ajuda e lista de opções da CLI no `stdout`.
 * @param programName Nome do executável (`argv[0]`).
 */
void printUsage(const char* programName);

/**
 * @brief Imprime o nome e a versão do compilador musical no `stdout`.
 */
void printVersion();

}  // namespace mus

#endif  // MUS_CLI_OPTIONS_H

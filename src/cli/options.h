#ifndef MUS_CLI_OPTIONS_H
#define MUS_CLI_OPTIONS_H

#include <string>

#include "cli/logger.h"

namespace mus {

extern const char* const kProgramVersion;

struct Options {
    std::string inputPath;               // caminho do .mus; vazio se não informado
    LogLevel logLevel = LogLevel::Info;
    bool showHelp = false;
    bool showVersion = false;
    bool noAudio = false;                // pula a inicialização do motor de áudio
    bool testTone = false;               // toca a senoide de teste (DSP-01, #6)
    bool demo = false;                   // imprime uma timeline de exemplo
    bool loop = false;                   // roda o motor até Ctrl+C
    double bpm = 120.0;                  // andamento usado pelo --demo
    bool ok = true;                      // false quando houve erro de uso
    std::string error;                   // mensagem pronta para exibição
};

// Nunca aborta nem escreve na saída: devolve o erro dentro de Options para que
// quem chamou decida como reportar.
Options parseArguments(int argc, char** argv);

void printUsage(const char* programName);
void printVersion();

}  // namespace mus

#endif  // MUS_CLI_OPTIONS_H

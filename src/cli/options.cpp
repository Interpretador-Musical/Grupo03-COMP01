#include "cli/options.h"

#include <cstdio>
#include <cstdlib>

namespace mus {

const char* const kProgramVersion = "0.1.0";

Options parseArguments(int argc, char** argv) {
    Options options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            options.showHelp = true;
        } else if (arg == "--version") {
            options.showVersion = true;
        } else if (arg == "-v" || arg == "--verbose") {
            options.logLevel = LogLevel::Debug;
        } else if (arg == "-q" || arg == "--quiet") {
            options.logLevel = LogLevel::Error;
        } else if (arg == "--no-audio") {
            options.noAudio = true;
        } else if (arg == "--test-tone") {
            options.testTone = true;
        } else if (arg == "--demo") {
            options.demo = true;
        } else if (arg == "--bpm") {
            if (i + 1 >= argc) {
                options.ok = false;
                options.error = "--bpm exige um valor (ex.: --bpm 140)";
                return options;
            }
            const std::string value = argv[++i];
            const double bpm = std::strtod(value.c_str(), nullptr);
            if (!(bpm > 0.0)) {
                options.ok = false;
                options.error = "BPM inválido: '" + value + "'";
                return options;
            }
            options.bpm = bpm;
        } else if (arg.size() > 1 && arg[0] == '-') {
            options.ok = false;
            options.error = "opção desconhecida: " + arg;
            return options;
        } else if (options.inputPath.empty()) {
            options.inputPath = arg;
        } else {
            options.ok = false;
            options.error = "mais de um arquivo de entrada informado ('" +
                            options.inputPath + "' e '" + arg + "')";
            return options;
        }
    }

    return options;
}

void printUsage(const char* programName) {
    std::printf(
        "Uso: %s [opções] <arquivo.mus>\n"
        "\n"
        "Interpretador da linguagem musical do Grupo 03 — Compiladores 1 (FCTE/UnB).\n"
        "\n"
        "Opções:\n"
        "  -h, --help        mostra esta ajuda e sai\n"
        "      --version     mostra a versão e sai\n"
        "  -v, --verbose     inclui mensagens de DEBUG no log\n"
        "  -q, --quiet       mostra apenas erros\n"
        "      --no-audio    não inicializa o dispositivo de áudio\n"
        "      --test-tone   toca a senoide de teste de 440 Hz e sai\n"
        "      --demo        imprime uma timeline de exemplo e sai\n"
        "      --bpm <n>     andamento do --demo (padrão: 120)\n",
        programName);
}

void printVersion() {
    std::printf("interpretador musical %s\n", kProgramVersion);
}

}  // namespace mus

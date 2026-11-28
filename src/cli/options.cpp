#include "cli/options.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace mus {

#ifndef MUS_VERSION
#define MUS_VERSION "0.0.0"
#endif

const char* const kProgramVersion = MUS_VERSION;

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
        } else if (arg == "--loop") {
            options.loop = true;
        } else if (arg == "--tokens") {
            options.showTokens = true;
        } else if (arg == "--simbolos") {
            options.showSymbols = true;
        } else if (arg == "--emit-timeline") {
            options.emitTimeline = true;
        } else if (arg == "--sem-otimizacao") {
            options.otimizar = false;
        } else if (arg == "-o" || arg == "--saida") {
            if (i + 1 >= argc) {
                options.ok = false;
                options.error = arg + " exige o caminho do arquivo de saída WAV "
                                      "(ex.: " + arg + " saida.wav)";
                return options;
            }
            const std::string caminho = argv[++i];
            if (caminho.empty()) {
                options.ok = false;
                options.error = arg + " exige um caminho de saída não vazio";
                return options;
            }
            options.outputWavPath = caminho;
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
        } else if (arg == "--volume") {
            if (i + 1 >= argc) {
                options.ok = false;
                options.error =
                    "--volume exige um valor entre 0 e 1 (ex.: --volume 0.8)";
                return options;
            }
            const std::string value = argv[++i];
            if (value.empty()) {
                options.ok = false;
                options.error =
                    "volume master inválido: '' (esperado número entre 0 e 1)";
                return options;
            }
            char* fim = nullptr;
            const double vol = std::strtod(value.c_str(), &fim);
            if (fim == value.c_str() || *fim != '\0' || !std::isfinite(vol) ||
                vol < 0.0 || vol > 1.0) {
                options.ok = false;
                options.error = "volume master inválido: '" + value +
                                "' (esperado número entre 0 e 1)";
                return options;
            }
            options.masterVolume = static_cast<float>(vol);
        } else if (arg == "--lpf") {
            if (i + 1 >= argc) {
                options.ok = false;
                options.error =
                    "--lpf exige uma frequência em Hz entre 20 e 20000, ou 0 para desligado "
                    "(ex.: --lpf 800)";
                return options;
            }
            const std::string value = argv[++i];
            if (value.empty()) {
                options.ok = false;
                options.error =
                    "frequência de --lpf inválida: '' (esperado 0 ou valor entre 20 e 20000 Hz)";
                return options;
            }
            char* fim = nullptr;
            const double hz = std::strtod(value.c_str(), &fim);
            if (fim == value.c_str() || *fim != '\0' || !std::isfinite(hz) ||
                !(hz == 0.0 || (hz >= 20.0 && hz <= 20000.0))) {
                options.ok = false;
                options.error = "frequência de --lpf inválida: '" + value +
                                "' (esperado 0 ou valor entre 20 e 20000 Hz)";
                return options;
            }
            options.lpfCutoffHz = static_cast<float>(hz);
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

    // O `--emit-timeline` existe para ser lido por outro programa: a saída
    // padrão precisa ter só o CSV. Juntar com outra tabela misturaria dois
    // formatos no mesmo stdout.
    if (options.emitTimeline && (options.showTokens || options.showSymbols)) {
        options.ok = false;
        options.error = "--emit-timeline não pode ser combinado com --tokens "
                        "ou --simbolos";
        return options;
    }

    if (!options.outputWavPath.empty() &&
        (options.showTokens || options.showSymbols || options.demo ||
         options.loop || options.testTone)) {
        options.ok = false;
        options.error = "-o/--saida não pode ser combinado com --tokens, "
                        "--simbolos, --demo, --loop ou --test-tone";
        return options;
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
        "      --bpm <n>     andamento do --demo (padrão: 120)\n"
        "      --volume <n>  volume master final entre 0 e 1 (padrão: 1.0)\n"
        "      --lpf <hz>    filtro passa-baixa BiQuad no master bus em Hz\n"
        "                    (20 a 20000, ou 0 para desligado; padrão: 0)\n"
        "      --loop        roda o motor de tempo até Ctrl+C\n"
        "      --tokens      imprime a tabela de tokens do arquivo e sai\n"
        "      --simbolos    imprime a tabela de símbolos do arquivo e sai\n"
        "      --emit-timeline\n"
        "                    compila o arquivo e imprime a timeline de eventos\n"
        "                    em CSV, sem tocar som\n"
        "  -o, --saida <arquivo>\n"
        "                    compila o arquivo e gera a saída-alvo em WAV PCM\n"
        "                    16-bit (44,1 kHz, mono), sem tocar som\n"
        "      --sem-otimizacao\n"
        "                    desliga a dobra de constantes (para comparar a\n"
        "                    saída com e sem otimização)\n",
        programName);
}

void printVersion() {
    std::printf("compilador musical %s\n", kProgramVersion);
}

}  // namespace mus

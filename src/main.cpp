#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>

#include "audio/audio_engine.h"
#include "cli/logger.h"
#include "cli/options.h"

namespace {

constexpr int kExitOk = 0;
constexpr int kExitFailure = 1;
constexpr int kExitUsage = 2;

// Duração da senoide de teste do --test-tone. Substitui o getchar() bloqueante
// da versão original; o controle de tempo de vida de verdade chega no SCH-01 (#4).
constexpr auto kTestToneDuration = std::chrono::seconds(3);

bool fileIsReadable(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

// Erros de uso não passam pelo logger: precisam aparecer mesmo com --quiet e
// vir acompanhados do modo correto de chamar o programa.
int usageError(const std::string& message, const char* programName) {
    std::fprintf(stderr, "erro: %s\n\n", message.c_str());
    mus::printUsage(programName);
    return kExitUsage;
}

int playTestTone() {
    mus::AudioEngine audio;
    if (!audio.start()) {
        return kExitFailure;
    }

    LOG_INFO << "tocando senoide de 440 Hz por 3 s";
    std::this_thread::sleep_for(kTestToneDuration);
    audio.stop();
    return kExitOk;
}

}  // namespace

int main(int argc, char** argv) {
    const char* programName = (argc > 0) ? argv[0] : "compilador";
    const mus::Options options = mus::parseArguments(argc, argv);

    if (!options.ok) {
        return usageError(options.error, programName);
    }

    mus::setLogLevel(options.logLevel);

    if (options.showHelp) {
        mus::printUsage(programName);
        return kExitOk;
    }

    if (options.showVersion) {
        mus::printVersion();
        return kExitOk;
    }

    if (options.testTone) {
        if (options.noAudio) {
            return usageError("--test-tone e --no-audio se contradizem", programName);
        }
        return playTestTone();
    }

    if (options.inputPath.empty()) {
        return usageError("nenhum arquivo de entrada informado", programName);
    }

    // Checagem antes de qualquer trabalho pesado: o NFR do INT-02 (#13) pede
    // que um arquivo inexistente falhe de forma limpa, sem chegar ao Flex.
    if (!fileIsReadable(options.inputPath)) {
        LOG_ERROR << "não foi possível abrir '" << options.inputPath << "'";
        return kExitFailure;
    }

    LOG_INFO << "arquivo de entrada: " << options.inputPath;
    LOG_WARN << "o pipeline de compilação ainda não está ligado ao CLI (INT-02, #13)";
    return kExitOk;
}

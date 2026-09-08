#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>

#include "audio/audio_engine.h"
#include "cli/logger.h"
#include "cli/options.h"
#include "core/playhead.h"
#include "engine/engine.h"

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


// Monta uma timeline de exemplo e a imprime. Existe para tornar o playhead
// verificável a olho nu enquanto a linguagem ainda não está definida — é o
// mesmo trabalho que o interpretador vai fazer ao percorrer a AST (INT-03, #19).
int runDemo(double bpm) {
    mus::Playhead playhead(bpm);
    mus::Timeline timeline;

    // Escala de Dó maior ascendente, uma colcheia por nota. As durações estão
    // em tempos; é o playhead quem as converte em segundos.
    const double scale[] = {60, 62, 64, 65, 67, 69, 71, 72};
    for (const double midi : scale) {
        timeline.add(playhead.play(midi, 0.5));
    }

    playhead.rest(0.5);

    // Acorde de Dó maior: as três notas saem no mesmo instante porque só a
    // última chamada avança o cursor.
    timeline.add(playhead.playHere(mus::Note{60.0, 0.8f}, 2.0));
    timeline.add(playhead.playHere(mus::Note{64.0, 0.8f}, 2.0));
    timeline.add(playhead.play(mus::Note{67.0, 0.8f}, 2.0));

    timeline.sortByStartTime();

    std::printf("timeline de exemplo — %.1f BPM (%.3f s por tempo)\n\n",
                playhead.bpm(), playhead.secondsPerBeat());
    std::printf("   #   início(s)   duração(s)   freq(Hz)   volume\n");
    std::printf("  ---  ----------  -----------  ---------  ------\n");

    int index = 1;
    for (const mus::SoundEvent& event : timeline.events()) {
        std::printf("  %3d  %10.3f  %11.3f  %9.3f  %6.2f\n", index++,
                    event.startTime, event.duration, event.frequency,
                    static_cast<double>(event.volume));
    }

    std::printf("\n%zu eventos, %.3f s de música, cursor em %.2f tempos "
                "(%.2f ciclos)\n",
                timeline.size(), timeline.duration(),
                playhead.positionInBeats(), playhead.positionInCycles());
    return kExitOk;
}


// Sobe a thread de tempo e segura a thread principal até o Ctrl+C. Quando o
// áudio está ligado, as duas threads rodam lado a lado — é a demonstração de
// que o relógio musical não depende de nada que aconteça no terminal.
int runLoop(const mus::Options& options) {
    mus::installShutdownHandler();

    mus::AudioEngine audio;
    if (!options.noAudio && !audio.start()) {
        LOG_WARN << "seguindo sem áudio";
    }

    mus::Engine engine;
    if (!engine.start()) {
        LOG_ERROR << "não foi possível iniciar a thread de tempo";
        return kExitFailure;
    }

    LOG_INFO << "motor de tempo rodando — Ctrl+C para encerrar";
    engine.runUntilStopped();

    audio.stop();

    // Jitter do agendador: quanto o intervalo real se afasta do nominal. É a
    // medida que justifica derivar o tempo musical do relógio em vez de supor
    // que cada volta durou exatamente o intervalo configurado.
    const mus::Engine::TickStats stats = engine.stats();
    LOG_INFO << "encerrado após " << stats.ticks << " ticks em "
             << stats.elapsedSeconds << " s";
    LOG_INFO << "delta médio " << stats.averageDelta() * 1000.0
             << " ms (mín " << stats.minDelta * 1000.0 << " ms, máx "
             << stats.maxDelta * 1000.0 << " ms)";
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

    if (options.demo) {
        return runDemo(options.bpm);
    }

    if (options.loop) {
        return runLoop(options);
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

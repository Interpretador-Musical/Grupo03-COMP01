#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <thread>
#include <algorithm>

#include "audio/audio_engine.h"
#include "cli/logger.h"
#include "cli/options.h"
#include "core/playhead.h"
#include "engine/engine.h"
#include "frontend/frontend.h"
#include "interpreter/interpreter.h"

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

// Lê o arquivo inteiro para uma string. É o que o Flex consome via
// yy_scan_string, em vez do stdin — o primeiro critério da INT-02 (#13).
bool readWholeFile(const std::string& path, std::string* contents) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    contents->assign(std::istreambuf_iterator<char>(file),
                     std::istreambuf_iterator<char>());
    return true;
}

// Imprime a tabela de tokens do programa. É o que torna o analisador léxico
// visível sem depender do parser, do interpretador ou de placa de som.
int printTokens(const std::string& source) {
    const std::vector<mus::Token> tokens = mus::tokenize(source);

    std::printf("  linha  coluna  token           lexema           valor\n");
    std::printf("  -----  ------  --------------  ---------------  -----------------\n");

    for (const mus::Token& token : tokens) {
        std::printf("  %5d  %6d  %-14s  %-15s  ", token.linha, token.coluna,
                    mus::nomeToken(token.tipo), token.lexema.c_str());

        switch (token.tipo) {
            case mus::TipoToken::Inteiro:
            case mus::TipoToken::Real:
                std::printf("%g", token.valor);
                break;
            case mus::TipoToken::Nota:
                if (token.oitava >= 0) {
                    std::printf("midi %d (oitava %d)", token.midi, token.oitava);
                } else {
                    std::printf("semitom %d, oitava corrente", token.semitom);
                }
                break;
            default:
                break;
        }
        std::printf("\n");
    }

    std::printf("\n%zu tokens\n", tokens.size());
    return kExitOk;
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
    if (!engine.setBpm(options.bpm)) {
        LOG_WARN << "BPM inválido, usando " << engine.bpm();
    }

    if (!engine.start()) {
        LOG_ERROR << "não foi possível iniciar a thread de tempo";
        return kExitFailure;
    }

    LOG_INFO << "motor de tempo rodando a " << engine.bpm()
             << " BPM — Ctrl+C para encerrar";
    engine.runUntilStopped();

    audio.stop();

    // Jitter do agendador: quanto o intervalo real se afasta do nominal. É a
    // medida que justifica derivar o tempo musical do relógio em vez de supor
    // que cada volta durou exatamente o intervalo configurado.
    const mus::Engine::TickStats stats = engine.stats();
    LOG_INFO << "encerrado após " << stats.ticks << " ticks em "
             << stats.elapsedSeconds << " s";
    LOG_INFO << "cursor musical em " << engine.positionInBeats()
             << " tempos (" << engine.positionInCycles() << " ciclos)";
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

    std::string source;
    if (!readWholeFile(options.inputPath, &source)) {
        LOG_ERROR << "não foi possível ler '" << options.inputPath << "'";
        return kExitFailure;
    }

    LOG_INFO << "arquivo de entrada: " << options.inputPath;

    if (options.showTokens) {
        return printTokens(source);
    }

    // A partir daqui é o fluxo completo da INT-03 (#19): parseia para AST,
    // interpreta (produz a Timeline, sem tocar som) e só então entrega o
    // programa compilado ao motor de áudio, que o executa sample-accurate de
    // dentro do próprio callback — ver core/timeline_player.h.
    std::string erro;
    auto* programa = mus::parseToAst(source, &erro);
    if (programa == nullptr) {
        LOG_ERROR << "erro de sintaxe: " << erro;
        return kExitFailure;
    }

    mus::Interpretador interpretador;
    if (!interpretador.executar(*programa)) {
        LOG_ERROR << "erro de execução: " << interpretador.erro();
        return kExitFailure;
    }

    const mus::Timeline& timeline = interpretador.timeline();
    LOG_INFO << timeline.size() << " eventos, " << timeline.duration()
             << " s de música";

    if (options.noAudio) {
        return kExitOk;
    }

    // A engine só olha o evento da frente da fila (um único "pendente"), então
    // o produtor precisa entregar em ordem de início. A timeline aqui é const,
    // por isso ordenamos uma cópia. stable_sort mantém a ordem original entre
    // eventos simultâneos (as notas de um acorde).
    std::vector<mus::SoundEvent> eventos(timeline.events().begin(),
                                         timeline.events().end());
    std::stable_sort(eventos.begin(), eventos.end(),
                     [](const mus::SoundEvent& a, const mus::SoundEvent& b) {
                         return a.startTime < b.startTime;
                     });

    // O buffer é declarado ANTES da engine: o C++ destrói na ordem inversa,
    // então a engine (e a thread de áudio) some primeiro e nunca lê um buffer
    // já destruído.
    mus::SpscRingBuffer<mus::SoundEvent> ringBuffer(eventos.size() + 10);
    mus::AudioEngine audio;

    for (const mus::SoundEvent& evento : eventos) {
        ringBuffer.push(evento);
    }

    audio.setRingBuffer(&ringBuffer);
    audio.setExpectedDuration(timeline.duration());
    
    if (!audio.start()) {
        LOG_WARN << "seguindo sem áudio";
        return kExitOk;
    }

    // A âncora agora usa a variável expectedDuration_
    audio.waitUntilFinished();
    audio.stop();
    return kExitOk;
}

#include "doctest.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "audio/audio_engine.h"
#include "core/playhead.h"
#include "core/ring_buffer.h"
#include "core/timeline_emit.h"
#include "core/timeline_queue.h"
#include "frontend/frontend.h"
#include "interpreter/interpreter.h"
#include "semantic/analisador_semantico.h"

using namespace mus;

// Cargas de profiling — SCH-07 (#41). Os resultados e os comandos estão em
// `docs/profiling.md`.
//
// Os dois casos são pulados por padrão (doctest::skip): servem para o Valgrind
// e para medir tempo, não para pegar regressão, e deixariam o CTest lento.
// Para rodar:
//
//   ./build-prof/tests/mus_tests --no-skip --test-case="perfil:*"
//
// Cada caso também mede o próprio tempo de parede com steady_clock e o
// imprime. Sob o Valgrind esse número não vale nada (tudo fica dezenas de
// vezes mais lento); ele serve para a execução direta, fora do Valgrind.

namespace {

using Relogio = std::chrono::steady_clock;

double milissegundosDesde(Relogio::time_point inicio) {
    return std::chrono::duration<double, std::milli>(Relogio::now() - inicio).count();
}

// Um programa com `linhas` comandos `toca`, um por linha, com expressão na
// altura e variável na duração. Precisa de muitas linhas de verdade: um
// `repita 20000 vezes` exercitaria o interpretador, mas não o lexer nem o
// parser.
std::string programaGrande(int linhas) {
    std::string fonte = "andamento 120\nvolume 0.8\nt = 0.25\n";
    for (int i = 0; i < linhas; ++i) {
        fonte += "toca do4 + " + std::to_string(i % 12) + " por t\n";
    }
    return fonte;
}

}  // namespace

TEST_CASE("perfil: compilar e emitir um programa de 20 mil linhas" * doctest::skip()) {
    constexpr int kLinhas = 20000;
    const std::string fonte = programaGrande(kLinhas);

    const auto inicio = Relogio::now();

    std::string erro;
    auto programa = parseToAst(fonte, &erro);
    REQUIRE_MESSAGE(programa != nullptr, "erro de sintaxe: " << erro);

    semantica::AnalisadorSemantico analisador;
    REQUIRE(analisador.analisar(*programa));

    Interpretador interpretador;
    REQUIRE(interpretador.executar(*programa));

    const std::string csv = emitirTimeline(interpretador.timeline());

    MESSAGE("compilação + emissão de " << kLinhas << " linhas: "
            << milissegundosDesde(inicio) << " ms");
    CHECK(interpretador.timeline().size() == static_cast<std::size_t>(kLinhas));
    CHECK_FALSE(csv.empty());
}

TEST_CASE("perfil: renderizar 10 s de áudio com todas as vozes ocupadas" * doctest::skip()) {
    // O pior caso do callback: um acorde de kMaxVoices notas a cada meio
    // segundo, então todas as vozes estão sempre ativas e cada acorde novo
    // passa pelo voice stealing (com o fade-out da DSP-07).
    constexpr double kSegundos = 10.0;
    constexpr int kVozes = AudioEngine::kMaxVoices;

    Timeline timeline;
    Playhead playhead(120.0);  // meio segundo por tempo
    for (int acorde = 0; acorde < static_cast<int>(kSegundos * 2); ++acorde) {
        for (int v = 0; v < kVozes; ++v) {
            timeline.add(playhead.playHere(Note{48.0 + v, 0.5f}, 1.0));
        }
        playhead.advance(1.0);
    }

    SpscRingBuffer<SoundEvent> fila(timeline.size());
    REQUIRE(carregarFila(timeline, fila));

    AudioEngine engine;
    engine.setRingBuffer(&fila);
    engine.setExpectedDuration(timeline.duration());

    // Em blocos do tamanho que o dispositivo pede, como o callback real.
    const auto totalFrames =
        static_cast<std::uint32_t>(kSegundos * AudioEngine::kSampleRate);
    std::vector<float> bloco(AudioEngine::kPeriodSizeInFrames);

    const auto inicio = Relogio::now();
    for (std::uint32_t feitos = 0; feitos < totalFrames;
         feitos += AudioEngine::kPeriodSizeInFrames) {
        engine.render(bloco.data(), AudioEngine::kPeriodSizeInFrames);
    }
    const double ms = milissegundosDesde(inicio);

    // Fração de um núcleo que o callback ocupa: tempo de render dividido pelo
    // tempo de áudio produzido.
    MESSAGE("render de " << kSegundos << " s com " << kVozes << " vozes: " << ms
            << " ms (" << 100.0 * ms / (kSegundos * 1000.0) << "% de um núcleo)");
    SoundEvent sobra;
    CHECK_FALSE(fila.pop(&sobra));  // todos os eventos foram consumidos
}

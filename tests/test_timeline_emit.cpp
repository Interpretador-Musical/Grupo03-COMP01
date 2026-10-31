#include "doctest.h"

#include <cstddef>
#include <string>

#include "core/playhead.h"
#include "core/timeline_emit.h"
#include "frontend/frontend.h"
#include "interpreter/interpreter.h"
#include "semantic/analisador_semantico.h"

using namespace mus;

namespace {

// Mesmo caminho do `main.cpp` até a Timeline: parser, semântica e
// interpretador. Devolve a string vazia se alguma fase recusar o programa —
// é o que o `--emit-timeline` faz (não imprime nada no stdout).
std::string compilarEEmitir(const std::string& fonte) {
    std::string erro;
    auto programa = parseToAst(fonte, &erro);
    if (programa == nullptr) {
        return "";
    }

    semantica::AnalisadorSemantico analisador;
    if (!analisador.analisar(*programa)) {
        return "";
    }

    Interpretador interpretador;
    if (!interpretador.executar(*programa)) {
        return "";
    }
    return emitirTimeline(interpretador.timeline());
}

std::size_t contarLinhas(const std::string& texto) {
    std::size_t linhas = 0;
    for (const char c : texto) {
        if (c == '\n') {
            ++linhas;
        }
    }
    return linhas;
}

SoundEvent evento(double inicio, double duracao, double frequencia,
                  float volume = 1.0f) {
    return SoundEvent{inicio, duracao, frequencia, volume};
}

}  // namespace

TEST_CASE("timeline vazia emite só o cabeçalho") {
    CHECK(emitirTimeline(Timeline{}) == std::string(kCabecalhoTimeline) + "\n");
}

TEST_CASE("cada evento vira uma linha com seis casas decimais fixas") {
    Timeline timeline;
    timeline.add(evento(0.0, 0.5, 440.0, 0.8f));

    CHECK(emitirTimeline(timeline) ==
          "indice,inicio_s,duracao_s,frequencia_hz,volume\n"
          "1,0.000000,0.500000,440.000000,0.800000\n");
}

TEST_CASE("eventos saem ordenados pelo início, sem mexer na timeline original") {
    Timeline timeline;
    timeline.add(evento(1.0, 0.5, 330.0));
    timeline.add(evento(0.0, 0.5, 220.0));

    CHECK(emitirTimeline(timeline) ==
          "indice,inicio_s,duracao_s,frequencia_hz,volume\n"
          "1,0.000000,0.500000,220.000000,1.000000\n"
          "2,1.000000,0.500000,330.000000,1.000000\n");

    // A cópia é que foi ordenada.
    CHECK(timeline.events()[0].startTime == 1.0);
}

TEST_CASE("eventos simultâneos mantêm a ordem em que foram agendados") {
    // Um acorde de Dó maior agendado como o --demo faz: três notas no mesmo
    // instante. A ordenação estável é o que garante a mesma saída sempre.
    Playhead playhead(120.0);
    Timeline timeline;
    timeline.add(playhead.play(72.0, 1.0));
    timeline.add(playhead.playHere(Note{67.0, 1.0f}, 1.0));
    timeline.add(playhead.playHere(Note{60.0, 1.0f}, 1.0));
    timeline.add(playhead.playHere(Note{64.0, 1.0f}, 1.0));

    CHECK(emitirTimeline(timeline) ==
          "indice,inicio_s,duracao_s,frequencia_hz,volume\n"
          "1,0.000000,0.500000,523.251131,1.000000\n"
          "2,0.500000,0.500000,391.995436,1.000000\n"
          "3,0.500000,0.500000,261.625565,1.000000\n"
          "4,0.500000,0.500000,329.627557,1.000000\n");
}

TEST_CASE("programa simples emite os eventos que o interpretador gerou") {
    // 120 BPM: um tempo dura 0.5 s. la4 = 440 Hz; la4 - 2 = sol4.
    CHECK(compilarEEmitir("andamento 120\n"
                          "volume 0.5\n"
                          "toca la4 por 1\n"
                          "toca la4 - 2 por 0.5\n") ==
          "indice,inicio_s,duracao_s,frequencia_hz,volume\n"
          "1,0.000000,0.500000,440.000000,0.500000\n"
          "2,0.500000,0.250000,391.995436,0.500000\n");
}

TEST_CASE("pausa aparece como evento silencioso de 0 Hz e volume 0") {
    CHECK(compilarEEmitir("andamento 60\npausa por 1\ntoca do4 por 1\n") ==
          "indice,inicio_s,duracao_s,frequencia_hz,volume\n"
          "1,0.000000,1.000000,0.000000,0.000000\n"
          "2,1.000000,1.000000,261.625565,1.000000\n");
}

TEST_CASE("repita desenrola o laço em eventos consecutivos") {
    const std::string saida =
        compilarEEmitir("andamento 120\nrepita 3 vezes { toca do4 por 1 }\n");

    CHECK(saida ==
          "indice,inicio_s,duracao_s,frequencia_hz,volume\n"
          "1,0.000000,0.500000,261.625565,1.000000\n"
          "2,0.500000,0.500000,261.625565,1.000000\n"
          "3,1.000000,0.500000,261.625565,1.000000\n");
}

TEST_CASE("compilar duas vezes o mesmo programa dá a mesma saída") {
    const std::string fonte =
        "andamento 97\n"
        "tempo = 1 / 3\n"
        "repita 50 vezes { toca do4 por tempo\n toca sol4 por tempo }\n";

    const std::string primeira = compilarEEmitir(fonte);
    REQUIRE(contarLinhas(primeira) == 101);  // cabeçalho + 100 eventos
    CHECK(compilarEEmitir(fonte) == primeira);
}

TEST_CASE("programa inválido não emite timeline") {
    // Uma falha por fase: sintaxe, semântica e execução.
    CHECK(compilarEEmitir("toca do4 por").empty());
    CHECK(compilarEEmitir("toca do4 por compasso").empty());
    CHECK(compilarEEmitir("repita 0 - 1 vezes { toca do4 por 1 }").empty());
}

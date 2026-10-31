#include "doctest.h"

#include <cmath>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>

#include "core/playhead.h"
#include "core/timeline_emit.h"
#include "frontend/frontend.h"
#include "interpreter/interpreter.h"
#include "semantic/analisador_semantico.h"

using namespace mus;

// Testes da representação intermediária (COMP-A, #104): as invariantes que
// `docs/representacao_intermediaria.md` promete para toda Timeline gerada a
// partir de um programa válido, e a independência do IR em relação à sintaxe.

namespace {

// Mesmo caminho do `main.cpp` até a Timeline. Falha o teste se alguma fase
// recusar o programa.
Timeline compilar(const std::string& fonte) {
    std::string erro;
    auto programa = parseToAst(fonte, &erro);
    REQUIRE_MESSAGE(programa != nullptr, "erro de sintaxe: " << erro);

    semantica::AnalisadorSemantico analisador;
    REQUIRE_MESSAGE(analisador.analisar(*programa), "erro semântico inesperado");

    Interpretador interpretador;
    REQUIRE_MESSAGE(interpretador.executar(*programa),
                    "erro de execução: " << interpretador.erro());
    return interpretador.timeline();
}

std::string lerArquivo(const std::string& caminho) {
    std::ifstream arquivo(caminho, std::ios::binary);
    REQUIRE_MESSAGE(arquivo.good(), "não abriu " << caminho);
    return std::string(std::istreambuf_iterator<char>(arquivo),
                       std::istreambuf_iterator<char>());
}

// As invariantes I1–I6 do documento, na ordem em que lá aparecem.
void verificarInvariantes(const Timeline& timeline) {
    const auto& eventos = timeline.events();
    double somaDuracoes = 0.0;

    for (std::size_t i = 0; i < eventos.size(); ++i) {
        const SoundEvent& e = eventos[i];
        CAPTURE(i);

        // I1: todo campo é um número finito.
        CHECK(std::isfinite(e.startTime));
        CHECK(std::isfinite(e.duration));
        CHECK(std::isfinite(e.frequency));
        CHECK(std::isfinite(e.volume));

        // I2: tempos não negativos.
        CHECK(e.startTime >= 0.0);
        CHECK(e.duration >= 0.0);

        // I3: volume é um ganho em [0, 1].
        CHECK(e.volume >= 0.0f);
        CHECK(e.volume <= 1.0f);

        // I4: nota tem frequência positiva; pausa é 0 Hz com volume 0.
        CHECK(e.frequency >= 0.0);
        if (e.frequency == 0.0) {
            CHECK(e.volume == 0.0f);
        }

        if (i > 0) {
            // I5: ordem de emissão não decrescente no início.
            CHECK(e.startTime >= eventos[i - 1].startTime);
            // I6: voz única e contígua — cada evento começa onde o anterior
            // termina. Aproximado: os segundos vêm de tempos × (60 / BPM).
            CHECK(e.startTime == doctest::Approx(eventos[i - 1].endTime()));
        }
        somaDuracoes += e.duration;
    }

    // Consequência de I6: a peça dura a soma das durações.
    CHECK(timeline.duration() == doctest::Approx(somaDuracoes));
}

}  // namespace

TEST_CASE("todo exemplo válido do repositório gera um IR que cumpre as invariantes") {
    const std::string raiz = MUS_SOURCE_DIR;
    for (const char* caminho :
         {"/exemplos/escala.mus", "/exemplos/arpejo.mus",
          "/tests/ir/notas_e_pausas.mus", "/tests/ir/andamento.mus",
          "/tests/ir/repeticao.mus", "/tests/ir/expressoes.mus"}) {
        CAPTURE(caminho);
        const Timeline timeline = compilar(lerArquivo(raiz + caminho));
        CHECK_FALSE(timeline.empty());
        verificarInvariantes(timeline);
    }
}

TEST_CASE("a emissão do interpretador já sai em ordem de início") {
    // Com uma voz só, ordenar é um no-op: a ordenação estável do
    // `emitirTimeline` existe para quando houver eventos simultâneos.
    const Timeline timeline = compilar(lerArquivo(
        std::string(MUS_SOURCE_DIR) + "/tests/ir/repeticao.mus"));

    Timeline ordenada = timeline;
    ordenada.sortByStartTime();
    CHECK(emitirTimeline(ordenada) == emitirTimeline(timeline));
}

TEST_CASE("IR independe da sintaxe: variável e literal geram o mesmo evento") {
    CHECK(emitirTimeline(compilar("andamento 120\ntoca la4 por 1\n")) ==
          emitirTimeline(compilar("bpm = 60 * 2\nandamento bpm\n"
                                  "nota = la4\nd = 0.5 + 0.5\n"
                                  "toca nota por d\n")));
}

TEST_CASE("IR independe da sintaxe: repita é igual ao corpo escrito N vezes") {
    CHECK(emitirTimeline(compilar("repita 3 vezes { toca do4 por 1\n pausa por 1 }")) ==
          emitirTimeline(compilar("toca do4 por 1\npausa por 1\n"
                                  "toca do4 por 1\npausa por 1\n"
                                  "toca do4 por 1\npausa por 1\n")));
}

TEST_CASE("IR independe da sintaxe: nota sem oitava é resolvida na compilação") {
    // `oitava` não aparece no IR: só a frequência que ela produziu.
    CHECK(emitirTimeline(compilar("oitava 5\ntoca do por 1\n")) ==
          emitirTimeline(compilar("toca do5 por 1\n")));
}

TEST_CASE("IR independe da sintaxe: o BPM some, ficam só os segundos") {
    // Um tempo a 60 BPM e dois tempos a 120 BPM são o mesmo segundo.
    CHECK(emitirTimeline(compilar("andamento 60\ntoca sol4 por 1\n")) ==
          emitirTimeline(compilar("andamento 120\ntoca sol4 por 2\n")));
}

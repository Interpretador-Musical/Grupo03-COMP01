#include "doctest.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "audio/audio_engine.h"
#include "core/playhead.h"
#include "core/ring_buffer.h"
#include "core/timeline_queue.h"
#include "frontend/frontend.h"
#include "interpreter/interpreter.h"
#include "semantic/analisador_semantico.h"

using namespace mus;

// Trilhas paralelas `{ a, b }` — CMP-07 (#39): como um grupo vira IR.
//
// Como em `test_calculos.cpp` (MAT-07), os esperados são LITERAIS calculados
// fora do projeto: 440 · 2^((midi − 69) / 12) para as alturas e 60 / BPM para
// os tempos. A 120 BPM, que é o andamento padrão, 1 tempo = 0,5 s.

namespace {

constexpr double kPrecisao = 1e-12;

constexpr double kDo2 = 65.40639132514966;
constexpr double kSol3 = 195.99771799087463;
constexpr double kDo4 = 261.6255653005986;
constexpr double kMi4 = 329.6275569128699;
constexpr double kSol4 = 391.99543598174927;
constexpr double kDo5 = 523.2511306011972;
constexpr double kDo3 = 130.8127826502993;

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

struct Esperado {
    double inicio;
    double duracao;
    double hz;
    float volume = 1.0f;
};

// Compara a Timeline inteira, evento a evento, na ordem em que ela sai.
void confere(const Timeline& timeline, const std::vector<Esperado>& esperados) {
    const auto& eventos = timeline.events();
    REQUIRE(eventos.size() == esperados.size());
    for (std::size_t i = 0; i < eventos.size(); ++i) {
        CAPTURE(i);
        CHECK(eventos[i].startTime == doctest::Approx(esperados[i].inicio).epsilon(kPrecisao));
        CHECK(eventos[i].duration == doctest::Approx(esperados[i].duracao).epsilon(kPrecisao));
        CHECK(eventos[i].frequency == doctest::Approx(esperados[i].hz).epsilon(kPrecisao));
        CHECK(eventos[i].volume == esperados[i].volume);
    }
}

}  // namespace

TEST_CASE("as trilhas começam juntas e o grupo dura a mais longa") {
    SUBCASE("a mais longa é a primeira") {
        // Trilha 1: 2 tempos (1 s). Trilha 2: 1 + 0,5 tempo (0,75 s).
        confere(compilar("{ toca do4 por 2, toca mi4 por 1 toca sol4 por 0.5 }\n"
                         "toca do5 por 1"),
                {{0.0, 1.0, kDo4},
                 {0.0, 0.5, kMi4},
                 {0.5, 0.25, kSol4},
                 {1.0, 0.5, kDo5}});  // continua do fim da trilha 1
    }

    SUBCASE("a mais longa é a última") {
        confere(compilar("{ toca do4 por 1, toca mi4 por 3 }\ntoca sol4 por 1"),
                {{0.0, 0.5, kDo4}, {0.0, 1.5, kMi4}, {1.5, 0.5, kSol4}});
    }
}

TEST_CASE("três contra quatro: as durações escritas valem, sem reescala") {
    // O `por` continua sendo tempo absoluto: 3 × 1/3 e 4 × 1/4 de tempo
    // cabem, as duas, em 1 tempo — é isso que faz a polirritmia.
    confere(compilar("{ repita 3 vezes { toca do4 por 1 / 3 },\n"
                     "  repita 4 vezes { toca sol3 por 1 / 4 } }\n"
                     "toca do5 por 1"),
            {{0.0, 0.16666666666666666, kDo4},
             {0.0, 0.125, kSol3},
             {0.125, 0.125, kSol3},
             {0.16666666666666666, 0.16666666666666666, kDo4},
             {0.25, 0.125, kSol3},
             {0.3333333333333333, 0.16666666666666666, kDo4},
             {0.375, 0.125, kSol3},
             {0.5, 0.5, kDo5}});
}

TEST_CASE("oitava e volume valem só dentro da trilha") {
    SUBCASE("a trilha seguinte não vê a mudança") {
        confere(compilar("volume 0.8\n"
                         "{ oitava 2 volume 0.5 toca do por 1, toca do por 1 }\n"
                         "toca do por 1"),
                {{0.0, 0.5, kDo2, 0.5f}, {0.0, 0.5, kDo4, 0.8f}, {0.5, 0.5, kDo4, 0.8f}});
    }

    SUBCASE("o resto do programa não vê a mudança da última trilha") {
        // É a última trilha que deixaria o estado mudado se o grupo não o
        // restaurasse no fim.
        confere(compilar("volume 0.8\n"
                         "{ toca do por 1, oitava 2 volume 0.5 toca do por 1 }\n"
                         "toca do por 1"),
                {{0.0, 0.5, kDo4, 0.8f}, {0.0, 0.5, kDo2, 0.5f}, {0.5, 0.5, kDo4, 0.8f}});
    }
}

TEST_CASE("trilha vazia dura zero e a vírgula no fim não muda nada") {
    confere(compilar("{ , }\ntoca do4 por 1"), {{0.0, 0.5, kDo4}});
    confere(compilar("{ toca do4 por 1, }\ntoca mi4 por 1"),
            {{0.0, 0.5, kDo4}, {0.5, 0.5, kMi4}});
}

TEST_CASE("acorde: trilhas de uma nota só, na ordem em que foram escritas") {
    // Ordem das trilhas, e não da altura: a ordenação estável do interpretador
    // mantém os simultâneos como foram agendados, e o `--emit-timeline` fica
    // determinístico.
    const Timeline timeline =
        compilar("{ toca sol4 por 1, toca do4 por 1, toca mi4 por 1 }");
    confere(timeline, {{0.0, 0.5, kSol4}, {0.0, 0.5, kDo4}, {0.0, 0.5, kMi4}});
    CHECK(timeline.duration() == 0.5);
}

TEST_CASE("grupo dentro de grupo") {
    // Trilha 1: o grupo de dentro dura 2 tempos (mi4), e o sol4 vem depois
    // dele. Trilha 2: 2 tempos. O grupo de fora acaba em 3 tempos (1,5 s).
    confere(compilar("{ { toca do4 por 1, toca mi4 por 2 } toca sol4 por 1,\n"
                     "  toca do3 por 2 }\n"
                     "toca do5 por 1"),
            {{0.0, 0.5, kDo4},
             {0.0, 1.0, kMi4},
             {0.0, 1.0, kDo3},
             {1.0, 0.5, kSol4},
             {1.5, 0.5, kDo5}});
}

TEST_CASE("repita sobre grupo repete as trilhas juntas") {
    const Timeline timeline = compilar("repita 2 vezes { toca do4 por 1, toca mi4 por 2 }");
    confere(timeline,
            {{0.0, 0.5, kDo4}, {0.0, 1.0, kMi4}, {1.0, 0.5, kDo4}, {1.0, 1.0, kMi4}});
    CHECK(timeline.duration() == 2.0);
}

TEST_CASE("o cursor depois de mil grupos não acumula erro") {
    // A mesma garantia da MAT-04 (#22): o cursor que sai do grupo é o da
    // trilha mais longa, contado em tempos, e não um instante em segundos
    // convertido de volta. Fórmula fechada: 1000 · (1/3) · (60/97) s.
    const Timeline timeline = compilar(
        "andamento 97\nrepita 1000 vezes { toca do4 por 1 / 3, toca mi4 por 1 / 4 }");
    REQUIRE(timeline.size() == 2000);
    CHECK(timeline.duration() == doctest::Approx(206.18556701030928).epsilon(kPrecisao));
    CHECK(timeline.events().back().startTime ==
          doctest::Approx(205.97938144329896).epsilon(kPrecisao));  // 999 · (1/3) · (60/97)
}

TEST_CASE("erro de execução dentro de uma trilha não gera IR") {
    std::string erro;
    auto programa = parseToAst("{ toca do4 por 1, volume 2 toca mi4 por 1 }\ntoca sol4 por 1",
                               &erro);
    REQUIRE(programa != nullptr);

    Interpretador interpretador;
    CHECK_FALSE(interpretador.executar(*programa));
    CHECK(interpretador.erro().find("volume precisa estar entre 0 e 1") != std::string::npos);
}

TEST_CASE("um grupo de quatro trilhas toca pela engine sem passar de [-1, 1]") {
    // Do IR ao som: as quatro notas simultâneas ocupam quatro vozes da engine,
    // a fila esvazia até o fim da música e nenhuma amostra sai da faixa — o
    // limitador do master bus (DSP-10, #57) segura a soma.
    const Timeline timeline = compilar(
        "{ toca do4 por 1, toca mi4 por 1, toca sol4 por 1, toca do5 por 1 }\n"
        "toca do4 por 1");

    SpscRingBuffer<SoundEvent> fila(timeline.size());
    REQUIRE(carregarFila(timeline, fila));

    AudioEngine engine;
    engine.setRingBuffer(&fila);
    engine.setExpectedDuration(timeline.duration());

    const auto totalFrames = static_cast<std::uint32_t>(
        std::ceil(timeline.duration() * AudioEngine::kSampleRate));
    std::vector<float> bloco(AudioEngine::kPeriodSizeInFrames);
    bool limitada = true;
    float pico = 0.0f;
    for (std::uint32_t feitos = 0; feitos < totalFrames;
         feitos += AudioEngine::kPeriodSizeInFrames) {
        engine.render(bloco.data(), AudioEngine::kPeriodSizeInFrames);
        for (const float amostra : bloco) {
            limitada = limitada && std::isfinite(amostra) && std::fabs(amostra) <= 1.0f;
            pico = std::fmax(pico, std::fabs(amostra));
        }
    }

    CHECK(limitada);
    CHECK(pico > 0.0f);  // tocou de verdade
    SoundEvent sobra;
    CHECK_FALSE(fila.pop(&sobra));
}

#include "doctest.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "audio/audio_engine.h"
#include "core/playhead.h"
#include "core/ring_buffer.h"
#include "frontend/frontend.h"
#include "interpreter/interpreter.h"
#include "semantic/analisador_semantico.h"

using namespace mus;

// Cálculos que o pipeline batch realmente executa — MAT-07 (#40). A matriz de
// casos e a cobertura estão em `docs/calculos.md`.
//
// Os valores esperados aqui são LITERAIS, calculados fora do projeto (fórmula
// fechada do temperamento igual e 60/BPM), e não chamando `midiToFrequency` ou
// `beatsToSeconds`: um teste que usa a função testada para calcular o
// esperado passa mesmo quando a função está errada.

namespace {

// Tolerância relativa apertada: os literais têm 16 dígitos, e as contas do
// pipeline não acumulam erro (ver MAT-02 e MAT-04).
constexpr double kPrecisao = 1e-12;

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

// Frequência do único evento de um programa de uma nota só.
double frequenciaDe(const std::string& fonte) {
    const Timeline timeline = compilar(fonte);
    REQUIRE(timeline.size() == 1);
    return timeline.events()[0].frequency;
}

double duracaoDe(const std::string& fonte) {
    const Timeline timeline = compilar(fonte);
    REQUIRE(timeline.size() == 1);
    return timeline.events()[0].duration;
}

}  // namespace

// ---- Altura: nota escrita → MIDI → Hz --------------------------------------

TEST_CASE("altura: notas escritas viram a frequência do temperamento igual") {
    struct Caso {
        const char* nota;
        double hz;  // 440 · 2^((midi − 69) / 12), calculado fora do projeto
    };
    const Caso casos[] = {
        {"la0", 27.5},                  // MIDI 21, a tecla mais grave do piano
        {"la1", 55.0},
        {"do3", 130.8127826502993},
        {"sib3", 233.08188075904496},   // bemol
        {"dob4", 246.94165062806206},   // dó bemol 4 = si 3: atravessa a oitava
        {"do4", 261.6255653005986},     // dó central, MIDI 60
        {"do#4", 277.1826309768721},    // sustenido
        {"mi4", 329.6275569128699},
        {"sol4", 391.99543598174927},
        {"la4", 440.0},                 // referência
        {"si#4", 523.2511306011972},    // si sustenido 4 = dó 5
        {"do5", 523.2511306011972},
        {"do8", 4186.009044809578},     // MIDI 108, a tecla mais aguda
    };

    for (const Caso& caso : casos) {
        CAPTURE(caso.nota);
        CHECK(frequenciaDe(std::string("toca ") + caso.nota + " por 1") ==
              doctest::Approx(caso.hz).epsilon(kPrecisao));
    }
}

TEST_CASE("altura: as pontas do MIDI continuam finitas e positivas") {
    // Não há como escrever estas notas com nome; chegam por expressão.
    CHECK(frequenciaDe("toca do4 - 60 por 1") ==
          doctest::Approx(8.175798915643707).epsilon(kPrecisao));   // MIDI 0
    CHECK(frequenciaDe("toca do4 + 67 por 1") ==
          doctest::Approx(12543.853951415975).epsilon(kPrecisao));  // MIDI 127
}

TEST_CASE("altura: oitava corrente e truncamento do comando oitava") {
    // `oitava` é truncado para inteiro (em direção a zero), não arredondado.
    CHECK(frequenciaDe("oitava 4.9\ntoca do por 1") ==
          doctest::Approx(261.6255653005986).epsilon(kPrecisao));   // oitava 4
    CHECK(frequenciaDe("oitava 0 - 1\ntoca do por 1") ==
          doctest::Approx(8.175798915643707).epsilon(kPrecisao));   // oitava −1: MIDI 0
    CHECK(frequenciaDe("oitava 0 - 0.5\ntoca do por 1") ==
          doctest::Approx(16.351597831287414).epsilon(kPrecisao));  // −0.5 trunca para 0
}

TEST_CASE("altura: aritmética de semitons respeita precedência e sinal") {
    CHECK(frequenciaDe("toca do4 + 7 por 1") ==
          doctest::Approx(391.99543598174927).epsilon(kPrecisao));  // quinta justa
    CHECK(frequenciaDe("toca do4 + 2 * 3 por 1") ==
          doctest::Approx(369.9944227116344).epsilon(kPrecisao));  // 60 + 6, não 62 · 3
    CHECK(frequenciaDe("toca (do4 + 2) - 2 * 1 por 1") ==
          doctest::Approx(261.6255653005986).epsilon(kPrecisao));
    CHECK(frequenciaDe("toca do4 - -2 por 1") ==
          doctest::Approx(293.6647679174076).epsilon(kPrecisao));   // menos unário
    CHECK(frequenciaDe("toca la4 + 0.5 por 1") ==
          doctest::Approx(452.8929841231365).epsilon(kPrecisao));   // microtom
}

// ---- Duração: tempos → segundos --------------------------------------------

TEST_CASE("duração: um tempo vale 60 / BPM segundos") {
    struct Caso {
        int bpm;
        double segundos;  // 60 / bpm
    };
    const Caso casos[] = {
        {7, 8.571428571428571},  // andamento extremo, divisão não exata
        {60, 1.0},
        {90, 0.6666666666666666},
        {100, 0.6},
        {120, 0.5},
        {128, 0.46875},
        {140, 0.42857142857142855},
    };

    for (const Caso& caso : casos) {
        CAPTURE(caso.bpm);
        CHECK(duracaoDe("andamento " + std::to_string(caso.bpm) + "\ntoca do4 por 1") ==
              doctest::Approx(caso.segundos).epsilon(kPrecisao));
    }
}

TEST_CASE("duração: frações de tempo e expressões") {
    // 120 BPM: um tempo = 0.5 s.
    CHECK(duracaoDe("toca do4 por 1 / 3") ==
          doctest::Approx(0.16666666666666666).epsilon(kPrecisao));
    CHECK(duracaoDe("toca do4 por (1 + 1) * 0.5") ==
          doctest::Approx(0.5).epsilon(kPrecisao));
    CHECK(duracaoDe("toca do4 por 1 - 0.25 * 2") ==
          doctest::Approx(0.25).epsilon(kPrecisao));  // precedência: 1 − 0.5
    CHECK(duracaoDe("toca do4 por 0") == 0.0);           // permitido (MAT-04)
}

// ---- Posição: o cursor ao longo da peça ------------------------------------

TEST_CASE("posição: troca de andamento ancora o tempo já decorrido") {
    // 2 tempos a 90 BPM (4/3 s), depois 3 tempos a 140 BPM (9/7 s).
    const Timeline timeline = compilar(
        "andamento 90\ntoca do4 por 2\nandamento 140\ntoca re4 por 3\ntoca mi4 por 1");
    const auto& e = timeline.events();

    REQUIRE(e.size() == 3);
    CHECK(e[1].startTime == doctest::Approx(1.3333333333333333).epsilon(kPrecisao));
    CHECK(e[1].duration == doctest::Approx(1.2857142857142858).epsilon(kPrecisao));
    CHECK(e[2].startTime == doctest::Approx(2.619047619047619).epsilon(kPrecisao));
    CHECK(timeline.duration() ==
          doctest::Approx(3.0476190476190474).epsilon(kPrecisao));
}

TEST_CASE("posição: dez mil terços de tempo não acumulam erro") {
    // A fórmula fechada do fim é 10000 · (1/3) · (60/97) s. Se o cursor somasse
    // segundos nota a nota, o erro cresceria com o número de notas.
    const Timeline timeline =
        compilar("andamento 97\nrepita 10000 vezes { toca do4 por 1 / 3 }");

    REQUIRE(timeline.size() == 10000);
    CHECK(timeline.duration() ==
          doctest::Approx(2061.855670103093).epsilon(kPrecisao));
    CHECK(timeline.events().back().startTime ==
          doctest::Approx(2061.6494845360826).epsilon(kPrecisao));  // 9999 · (1/3) · (60/97)
}

TEST_CASE("repetição: a quantidade é truncada para inteiro") {
    CHECK(compilar("repita 2.7 vezes { toca do4 por 1 }").size() == 2);
    CHECK(compilar("repita 0.9 vezes { toca do4 por 1 }").size() == 0);
    CHECK(compilar("repita 0 vezes { toca do4 por 1 }").size() == 0);
}

// ---- Reprodução: segundos → frames de áudio --------------------------------

namespace {

// Renderiza uma timeline com a engine, sem dispositivo, e devolve as amostras.
std::vector<float> renderizar(const Timeline& timeline, std::uint32_t frames) {
    SpscRingBuffer<SoundEvent> fila(timeline.size());
    for (const SoundEvent& evento : timeline.events()) {
        REQUIRE(fila.push(evento));
    }

    AudioEngine engine;
    engine.setRingBuffer(&fila);
    engine.setExpectedDuration(timeline.duration());

    std::vector<float> saida(frames, 0.0f);
    engine.render(saida.data(), frames);
    return saida;
}

// Índice da primeira amostra diferente de zero, ou o tamanho se não houver.
std::size_t primeiroSom(const std::vector<float>& amostras) {
    for (std::size_t i = 0; i < amostras.size(); ++i) {
        if (amostras[i] != 0.0f) {
            return i;
        }
    }
    return amostras.size();
}

}  // namespace

TEST_CASE("reprodução: o evento começa no frame round(início · 44100)") {
    // No frame do disparo a voz começa na fase 0, então a primeira amostra
    // audível é a seguinte: frame de início + 1.
    struct Caso {
        double inicio;
        std::size_t frame;  // round(inicio · 44100)
    };
    const Caso casos[] = {
        {0.0, 0},
        {0.5, 22050},
        {1.0 / 3.0, 14700},             // 1/3 de segundo cai exatamente num frame
        {0.25 / 44100.0, 0},            // um quarto de frame arredonda para 0
        {0.75 / 44100.0, 1},            // três quartos arredondam para 1
    };

    for (const Caso& caso : casos) {
        CAPTURE(caso.inicio);
        Timeline timeline;
        timeline.add(SoundEvent{caso.inicio, 0.1, 440.0, 1.0f});

        const std::vector<float> saida = renderizar(timeline, 30000);
        CHECK(primeiroSom(saida) == caso.frame + 1);
    }
}

TEST_CASE("reprodução: a nota se cala até o frame do fim, com o release dentro dela") {
    // 0.1 s = 4410 frames. O release de 5 ms (220 frames) acontece DENTRO da
    // duração, então depois do frame 4410 só há silêncio.
    Timeline timeline;
    timeline.add(SoundEvent{0.0, 0.1, 440.0, 1.0f});

    const std::vector<float> saida = renderizar(timeline, 6000);

    float energiaAntes = 0.0f;
    for (std::size_t i = 4000; i < 4190; ++i) {
        energiaAntes += std::fabs(saida[i]);
    }
    CHECK(energiaAntes > 0.0f);  // ainda soando antes do release

    bool silencioDepois = true;
    for (std::size_t i = 4411; i < saida.size(); ++i) {
        silencioDepois = silencioDepois && saida[i] == 0.0f;
    }
    CHECK(silencioDepois);
}

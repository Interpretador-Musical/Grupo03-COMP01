#include "doctest.h"

#include <chrono>
#include <thread>
#include <vector>

#include "engine/clock.h"

using namespace mus;
using namespace std::chrono_literals;

TEST_CASE("a fonte do relógio é monotônica") {
    // O static_assert em clock.h já trava isto na compilação. O caso existe
    // para que a exigência apareça no relatório do ctest: na libstdc++ o
    // high_resolution_clock é apelido do system_clock, que ANDA PARA TRÁS
    // quando o NTP ajusta a máquina — e um delta negativo faria a música
    // saltar. É o NFR da issue.
    CHECK(Clock::Source::is_steady);
}

TEST_CASE("o delta nunca é negativo") {
    Clock clock;

    for (int i = 0; i < 50; ++i) {
        CHECK(clock.tick() >= 0.0);
    }
}

TEST_CASE("o primeiro tick mede desde o reset") {
    Clock clock;
    std::this_thread::sleep_for(5ms);

    // Sem afirmar quanto: o escalonador não deve nada a ninguém. O que se
    // afirma é que houve tempo, não quanto.
    CHECK(clock.tick() > 0.0);
}

TEST_CASE("elapsed é monotônico e não zera entre ticks") {
    // A diferença essencial entre os dois: tick() consome o intervalo,
    // elapsed() não. É por isso que o tempo musical sai do elapsed e não da
    // soma dos deltas.
    Clock clock;

    double anterior = clock.elapsed();
    for (int i = 0; i < 20; ++i) {
        clock.tick();
        const double agora = clock.elapsed();
        CHECK(agora >= anterior);
        anterior = agora;
    }
    CHECK(anterior > 0.0);
}

TEST_CASE("reset devolve o relógio à origem") {
    Clock clock;
    std::this_thread::sleep_for(5ms);
    const double antes = clock.elapsed();

    clock.reset();

    CHECK(antes > 0.0);
    CHECK(clock.elapsed() < antes);
}

TEST_CASE("a soma dos deltas não é a fonte do tempo") {
    // Somar deltas acumula erro de arredondamento a cada volta — é o drift
    // progressivo que o MAT-08 (#45) vai caçar. Aqui só se verifica que as
    // duas medidas descrevem o mesmo intervalo, dentro de uma folga larga:
    // afirmar igualdade estreita seria afirmar o escalonador.
    Clock clock;

    double somaDosDeltas = 0.0;
    for (int i = 0; i < 100; ++i) {
        somaDosDeltas += clock.tick();
    }
    const double absoluto = clock.elapsed();

    CHECK(absoluto >= somaDosDeltas);
    CHECK(absoluto - somaDosDeltas < 0.5);
}

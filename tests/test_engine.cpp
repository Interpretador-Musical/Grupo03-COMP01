#include "doctest.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

#include "engine/engine.h"

using namespace mus;
using namespace std::chrono_literals;

namespace {

// Intervalo curto o bastante para o teste não demorar, longo o bastante para
// não depender da granularidade fina do escalonador.
constexpr std::chrono::microseconds kIntervaloDeTeste{500};  // 0.5 ms

// Espera até que `condicao` seja verdadeira ou o prazo estoure.
//
// Os testes do motor não podem afirmar TEMPO — o sistema operacional não tem
// obrigação nenhuma de respeitar o intervalo pedido, e afirmar "N ticks em T
// segundos" transformaria o CI em fonte de falso negativo. O que se afirma
// aqui são propriedades: que avança, que para, que não retrocede.
template <typename F>
bool esperaPor(F condicao, std::chrono::milliseconds prazo = 2000ms) {
    const auto limite = std::chrono::steady_clock::now() + prazo;
    while (std::chrono::steady_clock::now() < limite) {
        if (condicao()) {
            return true;
        }
        std::this_thread::sleep_for(1ms);
    }
    return condicao();
}

}  // namespace

TEST_CASE("um motor recém-criado está parado e sem ticks") {
    Engine engine(kIntervaloDeTeste);

    CHECK_FALSE(engine.isRunning());
    CHECK(engine.tickCount() == 0);
}

TEST_CASE("start sobe a thread e o contador anda") {
    Engine engine(kIntervaloDeTeste);

    REQUIRE(engine.start());
    CHECK(engine.isRunning());

    REQUIRE(esperaPor([&] { return engine.tickCount() > 0; }));

    engine.stop();
    CHECK_FALSE(engine.isRunning());
}

TEST_CASE("start com o motor já rodando devolve false") {
    // Não é erro nem exceção: quem chamou decide o que fazer.
    Engine engine(kIntervaloDeTeste);

    REQUIRE(engine.start());
    CHECK_FALSE(engine.start());
    CHECK(engine.isRunning());

    engine.stop();
}

TEST_CASE("stop é idempotente") {
    Engine engine(kIntervaloDeTeste);

    REQUIRE(engine.start());
    engine.stop();
    engine.stop();  // não pode travar nem dar join duas vezes
    CHECK_FALSE(engine.isRunning());
}

TEST_CASE("stop antes de start não faz nada") {
    Engine engine(kIntervaloDeTeste);

    engine.stop();
    CHECK_FALSE(engine.isRunning());
    CHECK(engine.tickCount() == 0);
}

TEST_CASE("o contador de ticks é monotônico") {
    Engine engine(kIntervaloDeTeste);
    REQUIRE(engine.start());

    std::uint64_t anterior = 0;
    for (int i = 0; i < 20; ++i) {
        const std::uint64_t atual = engine.tickCount();
        CHECK(atual >= anterior);
        anterior = atual;
        std::this_thread::sleep_for(1ms);
    }

    engine.stop();
}

TEST_CASE("o contador para de andar depois do stop") {
    Engine engine(kIntervaloDeTeste);
    REQUIRE(engine.start());
    REQUIRE(esperaPor([&] { return engine.tickCount() > 0; }));

    engine.stop();
    const std::uint64_t noFim = engine.tickCount();
    std::this_thread::sleep_for(20ms);

    // Se o join não tivesse acontecido de fato, a thread continuaria contando.
    CHECK(engine.tickCount() == noFim);
}

TEST_CASE("o callback é chamado com o número do tick, em sequência") {
    Engine engine(kIntervaloDeTeste);

    std::atomic<std::uint64_t> ultimo{0};
    std::atomic<bool> forDeOrdem{false};
    std::atomic<int> chamadas{0};

    engine.setTickCallback([&](std::uint64_t tick, double) {
        if (tick <= ultimo.load(std::memory_order_relaxed) && tick != 0) {
            forDeOrdem.store(true, std::memory_order_relaxed);
        }
        ultimo.store(tick, std::memory_order_relaxed);
        chamadas.fetch_add(1, std::memory_order_relaxed);
    });

    REQUIRE(engine.start());
    REQUIRE(esperaPor([&] { return chamadas.load() >= 5; }));
    engine.stop();

    CHECK_FALSE(forDeOrdem.load());
    CHECK(chamadas.load() >= 5);
}

TEST_CASE("o callback não é mais chamado depois do stop") {
    Engine engine(kIntervaloDeTeste);

    std::atomic<int> chamadas{0};
    engine.setTickCallback([&](std::uint64_t, double) {
        chamadas.fetch_add(1, std::memory_order_relaxed);
    });

    REQUIRE(engine.start());
    REQUIRE(esperaPor([&] { return chamadas.load() > 0; }));
    engine.stop();

    // Depois do join nada mais pode tocar no lambda — ele captura variáveis
    // desta pilha, e uma chamada tardia seria acesso a objeto destruído.
    const int noFim = chamadas.load();
    std::this_thread::sleep_for(20ms);
    CHECK(chamadas.load() == noFim);
}

TEST_CASE("um motor sem callback roda sem quebrar") {
    // O TickCallback é opcional: o std::function vazio não pode ser chamado.
    Engine engine(kIntervaloDeTeste);

    REQUIRE(engine.start());
    REQUIRE(esperaPor([&] { return engine.tickCount() > 0; }));
    engine.stop();
}

TEST_CASE("requestStop sinaliza sem bloquear, e o stop seguinte dá o join") {
    Engine engine(kIntervaloDeTeste);
    REQUIRE(engine.start());

    engine.requestStop();  // não faz join
    engine.stop();         // faz

    CHECK_FALSE(engine.isRunning());
}

TEST_CASE("o destrutor para a thread — não há caminho que a vaze") {
    std::atomic<int> chamadas{0};
    {
        Engine engine(kIntervaloDeTeste);
        engine.setTickCallback([&](std::uint64_t, double) {
            chamadas.fetch_add(1, std::memory_order_relaxed);
        });
        REQUIRE(engine.start());
        REQUIRE(esperaPor([&] { return chamadas.load() > 0; }));
        // Sai de escopo sem stop() explícito.
    }

    const int depoisDoDestrutor = chamadas.load();
    std::this_thread::sleep_for(20ms);
    CHECK(chamadas.load() == depoisDoDestrutor);
}

TEST_CASE("subir e parar em sequência é seguro") {
    Engine engine(kIntervaloDeTeste);

    for (int i = 0; i < 3; ++i) {
        REQUIRE(engine.start());
        REQUIRE(esperaPor([&] { return engine.tickCount() > 0; }));
        engine.stop();
        CHECK_FALSE(engine.isRunning());
    }
}

TEST_CASE("requestStop é seguro de chamar de outra thread") {
    // É o cenário real: o Ctrl+C chega numa thread e o motor roda em outra.
    Engine engine(kIntervaloDeTeste);
    REQUIRE(engine.start());

    std::thread outra([&] { engine.requestStop(); });
    outra.join();

    REQUIRE(esperaPor([&] { return !engine.isRunning(); }));
    engine.stop();
    CHECK_FALSE(engine.isRunning());
}

TEST_CASE("runUntilStopped devolve quando o motor é parado de fora") {
    // É o laço principal: a thread que o chama fica dentro dele até alguém
    // pedir parada. Se ele não devolvesse, este teste estouraria o timeout do
    // ctest em vez de falhar.
    Engine engine(kIntervaloDeTeste);
    REQUIRE(engine.start());

    std::thread parador([&] {
        std::this_thread::sleep_for(30ms);
        engine.requestStop();
    });

    engine.runUntilStopped();
    parador.join();

    CHECK_FALSE(engine.isRunning());
}

TEST_CASE("o delta entregue ao callback é sempre positivo e finito") {
    // A consequência prática do steady_clock: nunca chega um delta negativo
    // ao callback, que faria a música saltar para trás.
    Engine engine(kIntervaloDeTeste);

    std::atomic<bool> viuNaoPositivo{false};
    std::atomic<int> chamadas{0};

    engine.setTickCallback([&](std::uint64_t tick, double delta) {
        // O primeiro tick mede desde o reset do relógio, então também é > 0.
        if (!(delta > 0.0) || !std::isfinite(delta)) {
            viuNaoPositivo.store(true, std::memory_order_relaxed);
        }
        (void)tick;
        chamadas.fetch_add(1, std::memory_order_relaxed);
    });

    REQUIRE(engine.start());
    REQUIRE(esperaPor([&] { return chamadas.load() >= 20; }));
    engine.stop();

    CHECK_FALSE(viuNaoPositivo.load());
}

TEST_CASE("as estatísticas de tick são coerentes entre si") {
    Engine engine(kIntervaloDeTeste);
    REQUIRE(engine.start());
    REQUIRE(esperaPor([&] { return engine.tickCount() >= 20; }));
    engine.stop();

    const Engine::TickStats stats = engine.stats();

    CHECK(stats.ticks >= 20);
    CHECK(stats.elapsedSeconds > 0.0);
    CHECK(stats.minDelta > 0.0);
    CHECK(stats.maxDelta >= stats.minDelta);

    // A média tem de cair entre o mínimo e o máximo — é o que mostra que os
    // três números descrevem a mesma amostra.
    CHECK(stats.averageDelta() >= stats.minDelta);
    CHECK(stats.averageDelta() <= stats.maxDelta);
}

TEST_CASE("averageDelta é zero enquanto não há intervalo medido") {
    // Com 0 ou 1 tick não existe intervalo entre voltas, e dividir assim
    // mesmo daria divisão por zero.
    Engine::TickStats vazio;
    CHECK(vazio.averageDelta() == doctest::Approx(0.0));

    Engine::TickStats umTick;
    umTick.ticks = 1;
    umTick.elapsedSeconds = 0.5;
    CHECK(umTick.averageDelta() == doctest::Approx(0.0));

    Engine::TickStats tres;
    tres.ticks = 3;
    tres.elapsedSeconds = 1.0;
    CHECK(tres.averageDelta() == doctest::Approx(0.5));  // 2 intervalos
}

TEST_CASE("stats pode ser lido com o motor rodando") {
    // Sai sob o mesmo mutex que o wait_for já toma, então ler daqui não pode
    // travar nem devolver lixo.
    Engine engine(kIntervaloDeTeste);
    REQUIRE(engine.start());

    for (int i = 0; i < 10; ++i) {
        const Engine::TickStats stats = engine.stats();
        CHECK(stats.maxDelta >= stats.minDelta);
        std::this_thread::sleep_for(1ms);
    }

    engine.stop();
}

TEST_CASE("o BPM do motor é definido antes do start e recusa valor inválido") {
    Engine engine(kIntervaloDeTeste);

    CHECK(engine.bpm() == doctest::Approx(Playhead::kDefaultBpm));
    CHECK(engine.setBpm(90.0));
    CHECK(engine.bpm() == doctest::Approx(90.0));

    CHECK_FALSE(engine.setBpm(0.0));
    CHECK_FALSE(engine.setBpm(-30.0));
    CHECK(engine.bpm() == doctest::Approx(90.0));  // nada mudou
}

TEST_CASE("a thread de tempo empurra o playhead") {
    // É a segunda DoD da SCH-01 (#4): a thread "gerenciando a progressão do
    // tempo (playhead)". Antes deste PR o setTickCallback não tinha consumidor.
    Engine engine(kIntervaloDeTeste);
    REQUIRE(engine.setBpm(120.0));

    CHECK(engine.positionInBeats() == doctest::Approx(0.0));

    REQUIRE(engine.start());
    REQUIRE(esperaPor([&] { return engine.positionInSeconds() > 0.0; }));
    engine.stop();

    const double segundos = engine.positionInSeconds();
    const double tempos = engine.positionInBeats();

    CHECK(segundos > 0.0);
    // A 120 BPM, um tempo é meio segundo. A relação é exata mesmo sem saber
    // quanto tempo real passou — o que se verifica é a conversão, não o
    // escalonador.
    CHECK(tempos == doctest::Approx(segundos / 0.5));
    CHECK(engine.positionInCycles() == doctest::Approx(tempos / 4.0));
}

TEST_CASE("a posição musical nunca retrocede") {
    Engine engine(kIntervaloDeTeste);
    REQUIRE(engine.start());

    double anterior = 0.0;
    for (int i = 0; i < 30; ++i) {
        const double atual = engine.positionInBeats();
        CHECK(atual >= anterior);
        anterior = atual;
        std::this_thread::sleep_for(1ms);
    }

    engine.stop();
    CHECK(anterior > 0.0);
}

TEST_CASE("um andamento mais rápido produz mais tempos no mesmo tempo real") {
    // A verificação que o PR fez à mão com --loop em 60/120/240 BPM, agora
    // sem depender de quanto tempo o processo de fato rodou: compara-se a
    // posição musical com o tempo real medido pelo próprio motor.
    Engine engine(kIntervaloDeTeste);
    REQUIRE(engine.setBpm(240.0));
    REQUIRE(engine.start());
    REQUIRE(esperaPor([&] { return engine.positionInSeconds() > 0.0; }));
    engine.stop();

    // 240 BPM = 4 tempos por segundo.
    CHECK(engine.positionInBeats() ==
          doctest::Approx(engine.positionInSeconds() * 4.0));
}

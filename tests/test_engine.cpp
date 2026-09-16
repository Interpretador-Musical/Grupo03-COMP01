#include "doctest.h"

#include <atomic>
#include <chrono>
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

    engine.setTickCallback([&](std::uint64_t tick) {
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
    engine.setTickCallback([&](std::uint64_t) {
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
        engine.setTickCallback([&](std::uint64_t) {
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

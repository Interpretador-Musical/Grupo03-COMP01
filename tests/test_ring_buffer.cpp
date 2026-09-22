#include "doctest.h"

#include <chrono>
#include <cstddef>
#include <thread>
#include <vector>

#include "core/ring_buffer.h"
#include "core/sound_event.h"

using mus::SoundEvent;
using mus::SpscRingBuffer;
using namespace std::chrono_literals;

namespace {

// Prazo de segurança para os laços de espera ativa dos testes de concorrência
// abaixo: se a implementação tiver um bug que trave push()/pop(), o teste
// falha por não completar N itens dentro do prazo, em vez de travar o CI para
// sempre.
constexpr auto kPrazoDeSeguranca = 5s;

}  // namespace

TEST_CASE("buffer recém-criado está vazio") {
    SpscRingBuffer<int> buffer(4);
    int item = -1;
    CHECK_FALSE(buffer.pop(&item));
    CHECK(item == -1);  // pop() em buffer vazio não mexe em *item
}

TEST_CASE("capacity() reflete o valor pedido no construtor") {
    SpscRingBuffer<int> buffer(5);
    CHECK(buffer.capacity() == 5);
}

TEST_CASE("push preenche até a capacidade e recusa depois, sem corromper nada") {
    SpscRingBuffer<int> buffer(3);
    CHECK(buffer.push(10));
    CHECK(buffer.push(20));
    CHECK(buffer.push(30));
    CHECK_FALSE(buffer.push(40));  // cheio: capacidade é 3, não 4

    int item = 0;
    CHECK(buffer.pop(&item));
    CHECK(item == 10);
    CHECK(buffer.pop(&item));
    CHECK(item == 20);
    CHECK(buffer.pop(&item));
    CHECK(item == 30);
    CHECK_FALSE(buffer.pop(&item));  // só tinha 3, não 4
}

TEST_CASE("pop devolve os itens na ordem em que foram inseridos (FIFO)") {
    SpscRingBuffer<int> buffer(8);
    for (int i = 0; i < 5; ++i) {
        CHECK(buffer.push(i));
    }

    int item = -1;
    for (int i = 0; i < 5; ++i) {
        CHECK(buffer.pop(&item));
        CHECK(item == i);
    }
}

TEST_CASE("buffer esvaziado aceita novos itens, cruzando a fronteira do índice") {
    // Capacidade pequena de propósito: em poucas rodadas os índices já dão a
    // volta no vetor interno (capacidade+1 = 4 posições), o que é justamente
    // o caminho que um teste só com push/pop dentro da capacidade não cobre.
    SpscRingBuffer<int> buffer(3);

    int proximoEsperado = 0;
    for (int rodada = 0; rodada < 10; ++rodada) {
        CHECK(buffer.push(rodada * 100));
        CHECK(buffer.push(rodada * 100 + 1));

        int item = -1;
        CHECK(buffer.pop(&item));
        CHECK(item == proximoEsperado * 100);
        ++proximoEsperado;
        CHECK(buffer.pop(&item));
        CHECK(item == (proximoEsperado - 1) * 100 + 1);
    }
}

TEST_CASE("SpscRingBuffer aceita SoundEvent, o tipo real da SCH-03") {
    SpscRingBuffer<SoundEvent> buffer(2);

    const SoundEvent enviado{1.5, 0.25, 440.0, 0.8f};
    CHECK(buffer.push(enviado));

    SoundEvent recebido;
    CHECK(buffer.pop(&recebido));
    CHECK(recebido.startTime == doctest::Approx(1.5));
    CHECK(recebido.duration == doctest::Approx(0.25));
    CHECK(recebido.frequency == doctest::Approx(440.0));
    CHECK(recebido.volume == doctest::Approx(0.8f));
}

TEST_CASE("produtor e consumidor em threads reais entregam todos os itens em ordem") {
    // O teste que realmente importa para a SCH-03: push() numa thread, pop()
    // em outra, ao mesmo tempo, sem lock. Capacidade pequena força bastante
    // contenção e wraparound enquanto os N itens atravessam o buffer.
    constexpr int kCapacidade = 16;
    constexpr int kTotalDeItens = 20000;

    SpscRingBuffer<int> buffer(kCapacidade);
    std::vector<int> recebidos;
    recebidos.reserve(kTotalDeItens);

    std::thread produtor([&buffer]() {
        const auto prazo = std::chrono::steady_clock::now() + kPrazoDeSeguranca;
        for (int i = 0; i < kTotalDeItens; ++i) {
            while (!buffer.push(i)) {
                if (std::chrono::steady_clock::now() > prazo) {
                    return;  // evita travar o teste para sempre num bug real
                }
                std::this_thread::yield();
            }
        }
    });

    std::thread consumidor([&buffer, &recebidos]() {
        const auto prazo = std::chrono::steady_clock::now() + kPrazoDeSeguranca;
        int item = -1;
        while (static_cast<int>(recebidos.size()) < kTotalDeItens) {
            if (buffer.pop(&item)) {
                recebidos.push_back(item);
            } else if (std::chrono::steady_clock::now() > prazo) {
                return;
            } else {
                std::this_thread::yield();
            }
        }
    });

    produtor.join();
    consumidor.join();

    REQUIRE(recebidos.size() == static_cast<std::size_t>(kTotalDeItens));
    for (int i = 0; i < kTotalDeItens; ++i) {
        CHECK(recebidos[static_cast<std::size_t>(i)] == i);
    }
}

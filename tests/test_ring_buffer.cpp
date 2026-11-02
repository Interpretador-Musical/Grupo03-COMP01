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

// ---- Integridade na reprodução (SCH-05, #29) -------------------------------

namespace {

// Evento identificável pela frequência: o i-ésimo vale 100 + i Hz.
SoundEvent eventoNumero(int i, double inicio = 0.0) {
    return SoundEvent{inicio, 0.25, 100.0 + i, 0.5f};
}

bool camposIguais(const SoundEvent& a, const SoundEvent& b) {
    // Comparação exata: a fila copia byte a byte, não pode arredondar nada.
    return a.startTime == b.startTime && a.duration == b.duration &&
           a.frequency == b.frequency && a.volume == b.volume;
}

}  // namespace

TEST_CASE("capacidade zero recusa todo push e continua vazia") {
    SpscRingBuffer<SoundEvent> buffer(0);
    CHECK(buffer.capacity() == 0);
    CHECK_FALSE(buffer.push(eventoNumero(0)));

    SoundEvent item;
    CHECK_FALSE(buffer.pop(&item));
}

TEST_CASE("rajada de mil eventos simultâneos sai inteira, em ordem e sem alteração") {
    // O cenário denso da issue: um bloco grande de eventos com o mesmo
    // startTime, como um acorde enorme ou muitas notas curtíssimas.
    constexpr int kTotal = 1000;
    SpscRingBuffer<SoundEvent> buffer(kTotal);

    for (int i = 0; i < kTotal; ++i) {
        REQUIRE(buffer.push(eventoNumero(i, 2.0)));
    }
    CHECK_FALSE(buffer.push(eventoNumero(kTotal, 2.0)));  // cheia, e diz que está

    for (int i = 0; i < kTotal; ++i) {
        SoundEvent recebido;
        REQUIRE(buffer.pop(&recebido));
        CHECK(camposIguais(recebido, eventoNumero(i, 2.0)));
    }
    SoundEvent sobra;
    CHECK_FALSE(buffer.pop(&sobra));
}

TEST_CASE("SoundEvent atravessa muitas voltas do buffer sem mudar nenhum campo") {
    // Capacidade 3 e 1000 itens: cada slot é reaproveitado centenas de vezes.
    SpscRingBuffer<SoundEvent> buffer(3);

    for (int i = 0; i < 1000; ++i) {
        const SoundEvent enviado{i * 0.125, 1.0 / 3.0, 100.0 + i, 0.1f * (i % 10)};
        REQUIRE(buffer.push(enviado));

        SoundEvent recebido;
        REQUIRE(buffer.pop(&recebido));
        CHECK(camposIguais(recebido, enviado));
    }
}

TEST_CASE("fila cheia com consumidor concorrente: o produtor detecta e nada se perde") {
    // Capacidade minúscula para o produtor bater na fila cheia muitas vezes.
    // Cada recusa é visível (push devolve false) e o evento é reenviado: no
    // fim, todos chegaram, em ordem, com os campos intactos.
    constexpr int kCapacidade = 4;
    constexpr int kTotal = 20000;

    SpscRingBuffer<SoundEvent> buffer(kCapacidade);
    std::vector<SoundEvent> recebidos;
    recebidos.reserve(kTotal);

    std::thread produtor([&buffer]() {
        const auto prazo = std::chrono::steady_clock::now() + kPrazoDeSeguranca;
        for (int i = 0; i < kTotal; ++i) {
            // false = fila cheia: o evento não entrou, e o produtor sabe.
            while (!buffer.push(eventoNumero(i))) {
                if (std::chrono::steady_clock::now() > prazo) {
                    return;
                }
                std::this_thread::yield();
            }
        }
    });

    std::thread consumidor([&buffer, &recebidos]() {
        const auto prazo = std::chrono::steady_clock::now() + kPrazoDeSeguranca;
        SoundEvent item;
        while (static_cast<int>(recebidos.size()) < kTotal) {
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

    REQUIRE(recebidos.size() == static_cast<std::size_t>(kTotal));
    for (int i = 0; i < kTotal; ++i) {
        CHECK(camposIguais(recebidos[static_cast<std::size_t>(i)], eventoNumero(i)));
    }
}

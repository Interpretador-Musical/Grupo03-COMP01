#include "doctest.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "interpreter/arena.h"

using mus::ast::Arena;

// Arena da AST — MAT-10 (#55). Esta parte testa o alocador sozinho; a parte
// de baixo, que a AST inteira mora nele.

namespace {

bool alinhado(const void* endereco, std::size_t alinhamento) {
    return reinterpret_cast<std::uintptr_t>(endereco) % alinhamento == 0;
}

// O maior alinhamento que a arena aceita. Varia por plataforma: 16 no x86-64
// Linux, 8 no ARM do macOS.
struct alignas(alignof(std::max_align_t)) AlinhamentoMaximo {
    unsigned char bytes[alignof(std::max_align_t)];
};

// Conta e ordena as destruições: cada objeto anota o próprio número no vetor
// quando morre.
struct Anotador {
    Anotador(std::vector<int>* registro, int numero) : registro_(registro), numero_(numero) {}
    ~Anotador() { registro_->push_back(numero_); }
    Anotador(const Anotador&) = delete;
    Anotador& operator=(const Anotador&) = delete;

    std::vector<int>* registro_;
    int numero_;
};

}  // namespace

TEST_CASE("arena: cada objeto sai alinhado para o próprio tipo") {
    Arena arena;
    // Alternar tamanhos e alinhamentos força o cursor a se ajustar entre um e
    // outro, em vez de ficar sempre num múltiplo de 8 por sorte.
    for (int i = 0; i < 100; ++i) {
        CHECK(alinhado(arena.criar<char>('x'), alignof(char)));
        CHECK(alinhado(arena.criar<std::int32_t>(i), alignof(std::int32_t)));
        CHECK(alinhado(arena.criar<double>(1.5), alignof(double)));
        CHECK(alinhado(arena.criar<AlinhamentoMaximo>(), alignof(std::max_align_t)));
    }
}

TEST_CASE("arena: o primeiro bloco já existe antes da primeira alocação") {
    Arena arena;
    CHECK(arena.quantidadeDeBlocos() == 1);
    CHECK(arena.bytesReservados() == Arena::kPrimeiroBloco);
    CHECK(arena.bytesUsados() == 0);
}

TEST_CASE("arena: blocos novos não mexem no que já foi entregue") {
    Arena arena(1024);
    std::vector<std::int64_t*> valores;
    for (std::int64_t i = 0; i < 10000; ++i) {
        valores.push_back(arena.criar<std::int64_t>(i * 7));
    }
    CHECK(arena.quantidadeDeBlocos() > 1);
    CHECK(arena.bytesUsados() == 10000 * sizeof(std::int64_t));
    CHECK(arena.bytesReservados() >= arena.bytesUsados());
    for (std::int64_t i = 0; i < 10000; ++i) {
        REQUIRE(*valores[static_cast<std::size_t>(i)] == i * 7);
        CHECK(arena.contem(valores[static_cast<std::size_t>(i)]));
    }
}

TEST_CASE("arena: o tamanho dos blocos dobra até o teto") {
    Arena arena(1024);
    // Cada pedido é maior que o que sobrou do bloco anterior e cabe no bloco
    // seguinte, então abre exatamente um bloco novo.
    for (std::size_t tamanho : {1000, 2000, 4000, 8000}) {
        arena.alocar(tamanho, 1);
    }
    CHECK(arena.quantidadeDeBlocos() == 4);
    CHECK(arena.bytesReservados() == 1024 + 2048 + 4096 + 8192);
}

TEST_CASE("arena: pedido maior que o bloco ganha um bloco do próprio tamanho") {
    Arena arena(1024);
    auto* grande = static_cast<unsigned char*>(arena.alocar(100000, 1));
    std::memset(grande, 0xAB, 100000);
    CHECK(arena.contem(grande));
    CHECK(arena.contem(grande + 99999));
    CHECK(arena.quantidadeDeBlocos() == 2);

    // O pedido gigante não infla o bloco seguinte: ele continua sendo o dobro
    // do primeiro.
    arena.alocar(1500, 1);
    CHECK(arena.bytesReservados() == 1024 + 100000 + 2048);
}

TEST_CASE("arena: endereço de fora não é dela") {
    Arena arena;
    int local = 0;
    CHECK_FALSE(arena.contem(&local));
    CHECK_FALSE(arena.contem(nullptr));
    CHECK(arena.contem(arena.criar<int>(1)));
}

TEST_CASE("arena: tipo trivial não entra no registro de destrutores") {
    Arena arena;
    arena.criar<int>(1);
    arena.criar<double>(2.0);
    arena.copiarTexto("abc");
    CHECK(arena.objetosComDestrutor() == 0);
}

TEST_CASE("arena: destrutores rodam uma vez, na ordem inversa da criação") {
    std::vector<int> mortos;
    {
        Arena arena;
        for (int i = 0; i < 50; ++i) {
            arena.criar<Anotador>(&mortos, i);
        }
        CHECK(arena.objetosComDestrutor() == 50);
        CHECK(mortos.empty());
    }
    REQUIRE(mortos.size() == 50);
    for (int i = 0; i < 50; ++i) {
        CHECK(mortos[static_cast<std::size_t>(i)] == 49 - i);
    }
}

TEST_CASE("arena: um std::vector dentro dela é destruído e não vaza") {
    // O vetor guarda os elementos no heap comum, fora da arena. Quem prova que
    // esse buffer volta é o LeakSanitizer do job de ASan da CI.
    Arena arena;
    auto* lista = arena.criar<std::vector<int>>();
    for (int i = 0; i < 1000; ++i) {
        lista->push_back(i);
    }
    CHECK(arena.objetosComDestrutor() == 1);
    CHECK(lista->size() == 1000);
}

TEST_CASE("arena: copiarTexto devolve uma cópia que mora na arena") {
    Arena arena;
    std::string original = "PERC_09 com espaço e acento: ç";
    const std::string_view copia = arena.copiarTexto(original);
    original.assign(original.size(), '#');  // a cópia não depende da origem

    CHECK(copia == "PERC_09 com espaço e acento: ç");
    CHECK(arena.contem(copia.data()));
    CHECK(arena.copiarTexto("").empty());
}

TEST_CASE("arena: reiniciar() volta o cursor ao começo do primeiro bloco") {
    std::vector<int> mortos;
    Arena arena(1024);
    void* primeiro = arena.alocar(8, 8);
    for (int i = 0; i < 1000; ++i) {
        arena.criar<double>(1.0);
    }
    arena.criar<Anotador>(&mortos, 7);
    REQUIRE(arena.quantidadeDeBlocos() > 1);

    arena.reiniciar();

    CHECK(mortos == std::vector<int>{7});
    CHECK(arena.objetosComDestrutor() == 0);
    CHECK(arena.bytesUsados() == 0);
    CHECK(arena.quantidadeDeBlocos() == 1);
    CHECK(arena.bytesReservados() == 1024);
    // É isto que a issue pede: destruir tudo é só resetar o ponteiro, e a
    // próxima alocação cai no mesmo lugar da primeira.
    CHECK(arena.alocar(8, 8) == primeiro);

    // E a arena continua utilizável, com o crescimento recomeçando do dobro
    // do primeiro bloco.
    arena.alocar(1500, 1);
    CHECK(arena.bytesReservados() == 1024 + 2048);
}

#if defined(MUS_ARENA_ASAN)
TEST_CASE("arena: com ASan, memória não entregue ou já devolvida fica envenenada") {
    Arena arena;
    auto* valor = arena.criar<std::int64_t>(42);
    CHECK_FALSE(__asan_address_is_poisoned(valor));
    // O resto do bloco ainda não foi entregue a ninguém.
    CHECK(__asan_address_is_poisoned(valor + 1));

    arena.reiniciar();
    // O ponteiro antigo não vale mais: lê-lo agora seria acusado pelo ASan.
    CHECK(__asan_address_is_poisoned(valor));
}
#endif

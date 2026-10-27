#include "doctest.h"

#include <vector>

#include "core/subdivision.h"

using namespace mus;

namespace {

// Atalho para montar as árvores dos testes: nota MIDI direto.
Step nota(double midi) { return Step::of(Note{midi, 1.0f}); }

}  // namespace

TEST_CASE("grupo plano divide o arco igualmente entre as notas") {
    // [do re mi fa]
    const Step raiz = Step::group({nota(60), nota(62), nota(64), nota(65)});
    const std::vector<Hap> haps = subdivide(raiz, Arc{0.0, 1.0});

    REQUIRE(haps.size() == 4);
    CHECK(haps[0].whole == Arc{0.0, 0.25});
    CHECK(haps[1].whole == Arc{0.25, 0.5});
    CHECK(haps[2].whole == Arc{0.5, 0.75});
    CHECK(haps[3].whole == Arc{0.75, 1.0});
    CHECK(haps[2].value.midi == doctest::Approx(64.0));
}

TEST_CASE("[do [re mi]]: o subgrupo divide só o espaço que recebeu") {
    // O exemplo da issue: `do` fica com metade do ciclo, e `re` e `mi`
    // dividem a outra metade.
    const Step raiz = Step::group({nota(60), Step::group({nota(62), nota(64)})});
    const std::vector<Hap> haps = subdivide(raiz, Arc{0.0, 1.0});

    REQUIRE(haps.size() == 3);
    CHECK(haps[0].whole == Arc{0.0, 0.5});
    CHECK(haps[1].whole == Arc{0.5, 0.75});
    CHECK(haps[2].whole == Arc{0.75, 1.0});

    CHECK(haps[0].value.midi == doctest::Approx(60.0));
    CHECK(haps[1].value.midi == doctest::Approx(62.0));
    CHECK(haps[2].value.midi == doctest::Approx(64.0));
}

TEST_CASE("tercina dentro de um grupo binário") {
    // [do [re mi fa]]: a segunda metade vira três sextos.
    const Step raiz = Step::group(
        {nota(60), Step::group({nota(62), nota(64), nota(65)})});
    const std::vector<Hap> haps = subdivide(raiz, Arc{0.0, 1.0});

    REQUIRE(haps.size() == 4);
    CHECK(haps[0].whole == Arc{0.0, 0.5});
    CHECK(haps[1].whole == Arc{0.5, 0.5 + 1.0 / 6.0});
    CHECK(haps[2].whole == Arc{0.5 + 1.0 / 6.0, 0.5 + 2.0 / 6.0});
    CHECK(haps[3].whole == Arc{0.5 + 2.0 / 6.0, 1.0});
}

TEST_CASE("pausa ocupa espaço mas não gera Hap") {
    // [do ~ mi]
    const Step raiz = Step::group({nota(60), Step::rest(), nota(64)});
    const std::vector<Hap> haps = subdivide(raiz, Arc{0.0, 3.0});

    REQUIRE(haps.size() == 2);
    CHECK(haps[0].whole == Arc{0.0, 1.0});
    CHECK(haps[1].whole == Arc{2.0, 3.0});
}

TEST_CASE("subdivisão em um ciclo que não é o primeiro") {
    const Step raiz = Step::group({nota(60), Step::group({nota(62), nota(64)})});
    const std::vector<Hap> haps = subdivide(raiz, Arc{3.0, 4.0});

    REQUIRE(haps.size() == 3);
    CHECK(haps[0].whole == Arc{3.0, 3.5});
    CHECK(haps[1].whole == Arc{3.5, 3.75});
    CHECK(haps[2].whole == Arc{3.75, 4.0});
}

TEST_CASE("cada Hap sai inteiro e com ataque") {
    const Step raiz = Step::group({nota(60), Step::group({nota(62), nota(64)})});

    for (const Hap& hap : subdivide(raiz, Arc{0.0, 1.0})) {
        CHECK(hap.whole == hap.part);
        CHECK(hap.hasOnset());
    }
}

TEST_CASE("folha solta ocupa o arco inteiro") {
    const std::vector<Hap> haps = subdivide(nota(67), Arc{0.0, 2.0});

    REQUIRE(haps.size() == 1);
    CHECK(haps[0].whole == Arc{0.0, 2.0});
}

TEST_CASE("arco vazio, pausa solta ou grupo de pausas não geram nada") {
    CHECK(subdivide(nota(60), Arc{1.0, 1.0}).empty());
    CHECK(subdivide(Step::rest(), Arc{0.0, 1.0}).empty());
    CHECK(subdivide(Step::group({Step::rest(), Step::rest()}), Arc{0.0, 1.0})
              .empty());
}

TEST_CASE("aninhamento profundo continua contíguo e fecha no fim do arco") {
    // [do [do [do [... [do do] ...]]]] com 30 níveis: cada nível guarda metade
    // do espaço para o próximo. É o caso que mais estressaria uma soma de
    // durações acumulada; aqui as notas têm de encostar uma na outra sem buraco
    // e a última tem de terminar exatamente no fim do ciclo.
    constexpr int kNiveis = 30;
    Step raiz = Step::group({nota(60), nota(60)});
    for (int i = 0; i < kNiveis; ++i) {
        raiz = Step::group({nota(60), raiz});
    }

    const Arc ciclo{0.0, 1.0};
    const std::vector<Hap> haps = subdivide(raiz, ciclo);

    REQUIRE(haps.size() == static_cast<std::size_t>(kNiveis + 2));
    CHECK(haps.front().whole.begin == ciclo.begin);
    CHECK(haps.back().whole.end == ciclo.end);

    double total = 0.0;
    for (std::size_t i = 0; i < haps.size(); ++i) {
        CHECK_FALSE(haps[i].whole.isEmpty());
        if (i > 0) {
            CHECK(haps[i].whole.begin == haps[i - 1].whole.end);
        }
        total += haps[i].duration();
    }
    CHECK(total == doctest::Approx(ciclo.duration()));
}

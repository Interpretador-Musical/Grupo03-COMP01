#include "doctest.h"

#include <vector>

#include "core/pattern.h"

using namespace mus;

namespace {

Hap notaEm(double begin, double end, double midi) {
    Note note;
    note.midi = midi;
    return Hap{Arc{begin, end}, Arc{begin, end}, note};
}

}  // namespace

TEST_CASE("um Pattern novo está vazio") {
    Pattern pattern;

    CHECK(pattern.empty());
    CHECK(pattern.size() == 0);
    CHECK(pattern.haps().empty());
}

TEST_CASE("add acumula e clear esvazia") {
    Pattern pattern;
    pattern.add(notaEm(0.0, 1.0, 60.0));
    pattern.add(notaEm(1.0, 2.0, 62.0));

    CHECK(pattern.size() == 2);
    CHECK_FALSE(pattern.empty());

    pattern.clear();
    CHECK(pattern.empty());
}

TEST_CASE("query devolve só o que intersecta o span") {
    Pattern pattern;
    pattern.add(notaEm(0.0, 1.0, 60.0));
    pattern.add(notaEm(1.0, 2.0, 62.0));
    pattern.add(notaEm(2.0, 3.0, 64.0));

    const std::vector<Hap> resultado = pattern.query(Arc{0.5, 2.5});

    REQUIRE(resultado.size() == 3);
    CHECK(resultado[0].value.midi == doctest::Approx(60.0));
    CHECK(resultado[2].value.midi == doctest::Approx(64.0));
}

TEST_CASE("query recorta o part e preserva o whole") {
    // É esse par que mantém hasOnset() funcionando depois do recorte.
    Pattern pattern;
    pattern.add(notaEm(0.0, 4.0, 60.0));

    const std::vector<Hap> resultado = pattern.query(Arc{1.0, 2.0});

    REQUIRE(resultado.size() == 1);
    CHECK(resultado[0].part == Arc{1.0, 2.0});
    CHECK(resultado[0].whole == Arc{0.0, 4.0});
    CHECK(resultado[0].duration() == doctest::Approx(4.0));
    CHECK_FALSE(resultado[0].hasOnset());
}

TEST_CASE("query fora do alcance do padrão devolve vazio") {
    Pattern pattern;
    pattern.add(notaEm(0.0, 1.0, 60.0));

    CHECK(pattern.query(Arc{5.0, 6.0}).empty());
    // Encostar na fronteira não conta, pelo mesmo motivo de sempre: [0,1) não
    // compartilha instante nenhum com [1,2).
    CHECK(pattern.query(Arc{1.0, 2.0}).empty());
}

TEST_CASE("query com span vazio devolve vazio") {
    Pattern pattern;
    pattern.add(notaEm(0.0, 4.0, 60.0));

    CHECK(pattern.query(Arc{1.0, 1.0}).empty());
    CHECK(pattern.query(Arc{2.0, 1.0}).empty());
}

TEST_CASE("sortByOnset ordena por início do part") {
    Pattern pattern;
    pattern.add(notaEm(2.0, 3.0, 64.0));
    pattern.add(notaEm(0.0, 1.0, 60.0));
    pattern.add(notaEm(1.0, 2.0, 62.0));

    pattern.sortByOnset();

    REQUIRE(pattern.size() == 3);
    CHECK(pattern.haps()[0].value.midi == doctest::Approx(60.0));
    CHECK(pattern.haps()[1].value.midi == doctest::Approx(62.0));
    CHECK(pattern.haps()[2].value.midi == doctest::Approx(64.0));
}

TEST_CASE("sortByOnset é estável entre notas simultâneas") {
    // É o que preserva a ordem de escrita das notas de um acorde — todas
    // começam no mesmo instante, e stable_sort garante que elas não trocam
    // de lugar entre execuções.
    Pattern pattern;
    pattern.add(notaEm(0.0, 1.0, 60.0));
    pattern.add(notaEm(0.0, 1.0, 64.0));
    pattern.add(notaEm(0.0, 1.0, 67.0));

    pattern.sortByOnset();

    CHECK(pattern.haps()[0].value.midi == doctest::Approx(60.0));
    CHECK(pattern.haps()[1].value.midi == doctest::Approx(64.0));
    CHECK(pattern.haps()[2].value.midi == doctest::Approx(67.0));
}

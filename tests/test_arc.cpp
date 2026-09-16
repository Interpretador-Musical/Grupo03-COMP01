#include "doctest.h"

#include <vector>

#include "core/arc.h"

using namespace mus;

TEST_CASE("duration e isEmpty descrevem o arco") {
    CHECK(Arc{0.0, 1.0}.duration() == doctest::Approx(1.0));
    CHECK(Arc{0.5, 2.25}.duration() == doctest::Approx(1.75));

    CHECK(Arc{1.0, 1.0}.isEmpty());
    // Arco invertido conta como vazio, e não como duração negativa: é o que
    // impede um intervalo malformado de virar evento mais adiante.
    CHECK(Arc{2.0, 1.0}.isEmpty());
    CHECK_FALSE(Arc{0.0, 0.001}.isEmpty());
}

TEST_CASE("contains é semiaberto: inclui o começo, exclui o fim") {
    const Arc arc{1.0, 2.0};

    CHECK(arc.contains(1.0));
    CHECK(arc.contains(1.5));
    CHECK_FALSE(arc.contains(2.0));   // já pertence ao próximo arco
    CHECK_FALSE(arc.contains(0.999));

    // É esta regra que impede uma nota de disparar duas vezes na fronteira
    // entre dois ciclos.
    const Arc proximo{2.0, 3.0};
    CHECK(proximo.contains(2.0));
}

TEST_CASE("shifted move os dois extremos e preserva a duração") {
    const Arc arc{0.5, 1.5};
    const Arc movido = arc.shifted(2.0);

    CHECK(movido.begin == doctest::Approx(2.5));
    CHECK(movido.end == doctest::Approx(3.5));
    CHECK(movido.duration() == doctest::Approx(arc.duration()));

    CHECK(arc.shifted(-0.5) == Arc{0.0, 1.0});
    CHECK(arc.shifted(0.0) == arc);
}

TEST_CASE("sect devolve a sobreposição") {
    CHECK(Arc{0.0, 2.0}.sect(Arc{1.0, 3.0}) == Arc{1.0, 2.0});
    CHECK(Arc{0.0, 4.0}.sect(Arc{1.0, 2.0}) == Arc{1.0, 2.0});
    CHECK(Arc{0.0, 1.0}.sect(Arc{0.0, 1.0}) == Arc{0.0, 1.0});
}

TEST_CASE("sect de arcos disjuntos é vazio") {
    CHECK(Arc{0.0, 1.0}.sect(Arc{2.0, 3.0}).isEmpty());
    CHECK(Arc{2.0, 3.0}.sect(Arc{0.0, 1.0}).isEmpty());
}

TEST_CASE("arcos que só se encostam não se sobrepõem") {
    // Consequência direta do intervalo ser semiaberto: [0,1) e [1,2) não
    // compartilham nenhum instante.
    CHECK(Arc{0.0, 1.0}.sect(Arc{1.0, 2.0}).isEmpty());
    CHECK_FALSE(Arc{0.0, 1.0}.overlaps(Arc{1.0, 2.0}));
    CHECK(Arc{0.0, 1.0}.overlaps(Arc{0.999, 2.0}));
}

TEST_CASE("cycles quebra o arco nas fronteiras de ciclo inteiro") {
    const std::vector<Arc> pedacos = Arc{0.5, 2.25}.cycles();

    REQUIRE(pedacos.size() == 3);
    CHECK(pedacos[0] == Arc{0.5, 1.0});
    CHECK(pedacos[1] == Arc{1.0, 2.0});
    CHECK(pedacos[2] == Arc{2.0, 2.25});
}

TEST_CASE("os pedaços de cycles são contíguos e somam o arco original") {
    const Arc original{1.3, 5.7};
    const std::vector<Arc> pedacos = original.cycles();

    REQUIRE_FALSE(pedacos.empty());
    CHECK(pedacos.front().begin == doctest::Approx(original.begin));
    CHECK(pedacos.back().end == doctest::Approx(original.end));

    double total = 0.0;
    for (std::size_t i = 0; i < pedacos.size(); ++i) {
        CHECK_FALSE(pedacos[i].isEmpty());
        if (i > 0) {
            CHECK(pedacos[i].begin == doctest::Approx(pedacos[i - 1].end));
        }
        total += pedacos[i].duration();
    }
    CHECK(total == doctest::Approx(original.duration()));
}

TEST_CASE("arco alinhado ao ciclo não gera pedaço vazio") {
    // O caso que o `floor(cursor) + 1` no laço existe para tratar: começando
    // exatamente em 1.0, a primeira fronteira não pode ser o próprio 1.0,
    // senão sairia um pedaço de duração zero (ou um laço infinito).
    const std::vector<Arc> pedacos = Arc{1.0, 3.0}.cycles();

    REQUIRE(pedacos.size() == 2);
    CHECK(pedacos[0] == Arc{1.0, 2.0});
    CHECK(pedacos[1] == Arc{2.0, 3.0});
}

TEST_CASE("arco menor que um ciclo sai inteiro em um pedaço só") {
    const std::vector<Arc> pedacos = Arc{0.25, 0.75}.cycles();

    REQUIRE(pedacos.size() == 1);
    CHECK(pedacos[0] == Arc{0.25, 0.75});
}

TEST_CASE("arco vazio ou invertido não gera ciclo nenhum") {
    CHECK(Arc{1.0, 1.0}.cycles().empty());
    CHECK(Arc{3.0, 1.0}.cycles().empty());
}

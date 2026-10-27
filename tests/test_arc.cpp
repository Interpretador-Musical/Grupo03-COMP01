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

TEST_CASE("divide parte o arco em N pedaços de mesma duração") {
    const std::vector<Arc> pedacos = Arc{0.0, 1.0}.divide(4);

    REQUIRE(pedacos.size() == 4);
    CHECK(pedacos[0] == Arc{0.0, 0.25});
    CHECK(pedacos[1] == Arc{0.25, 0.5});
    CHECK(pedacos[2] == Arc{0.5, 0.75});
    CHECK(pedacos[3] == Arc{0.75, 1.0});
}

TEST_CASE("divide respeita um arco que não começa no zero") {
    const std::vector<Arc> pedacos = Arc{2.0, 2.5}.divide(2);

    REQUIRE(pedacos.size() == 2);
    CHECK(pedacos[0] == Arc{2.0, 2.25});
    CHECK(pedacos[1] == Arc{2.25, 2.5});
}

TEST_CASE("divide em terços fecha exatamente nas pontas, sem buraco entre pedaços") {
    // 1/3 não tem representação exata em double. Se as fronteiras fossem
    // somadas a um cursor, o último pedaço poderia terminar em 0.99999...;
    // por isso a comparação aqui é exata (==), e não com tolerância.
    const Arc original{0.0, 1.0};
    const std::vector<Arc> pedacos = original.divide(3);

    REQUIRE(pedacos.size() == 3);
    CHECK(pedacos.front().begin == original.begin);
    CHECK(pedacos.back().end == original.end);
    for (std::size_t i = 1; i < pedacos.size(); ++i) {
        CHECK(pedacos[i].begin == pedacos[i - 1].end);
        CHECK(pedacos[i].duration() == doctest::Approx(1.0 / 3.0));
    }
}

TEST_CASE("divide por 1 devolve o próprio arco") {
    const std::vector<Arc> pedacos = Arc{0.5, 1.5}.divide(1);

    REQUIRE(pedacos.size() == 1);
    CHECK(pedacos[0].begin == 0.5);
    CHECK(pedacos[0].end == 1.5);
}

TEST_CASE("divide por zero ou de arco vazio não gera pedaço nenhum") {
    CHECK(Arc{0.0, 1.0}.divide(0).empty());
    CHECK(Arc{1.0, 1.0}.divide(4).empty());
    CHECK(Arc{3.0, 1.0}.divide(2).empty());
}

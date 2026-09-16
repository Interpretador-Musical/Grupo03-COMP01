#include "doctest.h"

#include "core/hap.h"

using namespace mus;

TEST_CASE("Note guarda a nota em MIDI e converte para Hz na saída") {
    Note note;
    CHECK(note.midi == doctest::Approx(60.0));   // Dó 4 é o padrão
    CHECK(note.volume == doctest::Approx(1.0f));

    note.midi = 69.0;
    CHECK(note.frequency() == doctest::Approx(440.0));
}

TEST_CASE("transpor é somar semitons") {
    // A razão de a nota ser MIDI e não Hz: é esta aritmética que a linguagem
    // vai expor ao usuário (`nota + 2` sobe um tom).
    Note note;
    note.midi = 60.0;

    Note oitavaAcima = note;
    oitavaAcima.midi += 12.0;

    CHECK(oitavaAcima.frequency() == doctest::Approx(2.0 * note.frequency()));
}

TEST_CASE("um Hap inteiro dentro de um ciclo tem ataque") {
    const Hap hap{Arc{0.0, 1.0}, Arc{0.0, 1.0}, Note{}};

    CHECK(hap.hasOnset());
    CHECK(hap.duration() == doctest::Approx(1.0));
}

TEST_CASE("a continuação de uma nota cortada NÃO tem ataque") {
    // Nota de 0.5 a 2.5, vista em dois pedaços. Só o primeiro dispara; sem
    // essa distinção a nota re-atacaria a cada ciclo em vez de continuar
    // soando.
    const Arc whole{0.5, 2.5};

    const Hap primeiro{whole, Arc{0.5, 1.0}, Note{}};
    const Hap meio{whole, Arc{1.0, 2.0}, Note{}};
    const Hap ultimo{whole, Arc{2.0, 2.5}, Note{}};

    CHECK(primeiro.hasOnset());
    CHECK_FALSE(meio.hasOnset());
    CHECK_FALSE(ultimo.hasOnset());
}

TEST_CASE("a duração vem do whole, não do part") {
    // O sintetizador precisa saber quanto a nota dura por inteiro, não o
    // tamanho do fragmento que a consulta atual devolveu.
    const Hap fragmento{Arc{0.0, 4.0}, Arc{1.0, 2.0}, Note{}};

    CHECK(fragmento.duration() == doctest::Approx(4.0));
    CHECK(fragmento.part.duration() == doctest::Approx(1.0));
}

TEST_CASE("hasOnset tolera erro de ponto flutuante") {
    // Compara com nearlyEqual: o começo do part vem de somas de durações e
    // raramente bate bit a bit com o começo do whole.
    const double inicio = 0.1 + 0.2;
    const Hap hap{Arc{0.3, 1.0}, Arc{inicio, 1.0}, Note{}};

    CHECK(inicio != 0.3);          // de fato não são iguais em binário
    CHECK(hap.hasOnset());         // mas contam como o mesmo instante
}

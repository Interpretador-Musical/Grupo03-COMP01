#include "doctest.h"

#include <cmath>

#include "core/playhead.h"

using namespace mus;

TEST_CASE("o BPM padrão converte um tempo em meio segundo") {
    const Playhead playhead;

    CHECK(playhead.bpm() == doctest::Approx(Playhead::kDefaultBpm));
    CHECK(playhead.secondsPerBeat() == doctest::Approx(0.5));
    CHECK(playhead.secondsPerCycle() == doctest::Approx(2.0));  // 4/4
    CHECK(playhead.now() == doctest::Approx(0.0));
}

TEST_CASE("beatsToSeconds e secondsToBeats são inversas") {
    const Playhead playhead(90.0);

    CHECK(playhead.beatsToSeconds(1.0) == doctest::Approx(60.0 / 90.0));
    CHECK(playhead.secondsToBeats(playhead.beatsToSeconds(3.5)) ==
          doctest::Approx(3.5));
}

TEST_CASE("BPM não positivo é recusado na construção e no setter") {
    // O core não conhece a CLI: devolve false e deixa quem chamou decidir como
    // reportar, em vez de logar ou abortar.
    const Playhead zero(0.0);
    const Playhead negativo(-120.0);
    CHECK(zero.bpm() == doctest::Approx(Playhead::kDefaultBpm));
    CHECK(negativo.bpm() == doctest::Approx(Playhead::kDefaultBpm));

    Playhead playhead(100.0);
    CHECK_FALSE(playhead.setBpm(0.0));
    CHECK_FALSE(playhead.setBpm(-1.0));
    CHECK_FALSE(playhead.setBpm(std::nan("")));
    CHECK(playhead.bpm() == doctest::Approx(100.0));  // nada mudou

    CHECK(playhead.setBpm(140.0));
    CHECK(playhead.bpm() == doctest::Approx(140.0));
}

TEST_CASE("play agenda na posição atual e empurra o cursor") {
    // O mecanismo central da ata de 19/08: tocar uma nota não executa nada na
    // hora, agenda e anda.
    Playhead playhead(120.0);

    const SoundEvent primeira = playhead.play(69.0, 1.0);
    CHECK(primeira.startTime == doctest::Approx(0.0));
    CHECK(primeira.duration == doctest::Approx(0.5));
    CHECK(primeira.frequency == doctest::Approx(440.0));
    CHECK(playhead.now() == doctest::Approx(0.5));

    const SoundEvent segunda = playhead.play(69.0, 1.0);
    CHECK(segunda.startTime == doctest::Approx(0.5));
    CHECK(playhead.now() == doctest::Approx(1.0));
}

TEST_CASE("playHere agenda sem mover o cursor — é o acorde") {
    // Três notas no mesmo instante, uma única chamada a play() no fim para
    // andar. Sem playHere não haveria como escrever um acorde.
    Playhead playhead(120.0);

    const SoundEvent do_ = playhead.playHere(Note{60.0, 0.8f}, 2.0);
    const SoundEvent mi = playhead.playHere(Note{64.0, 0.8f}, 2.0);
    const SoundEvent sol = playhead.play(Note{67.0, 0.8f}, 2.0);

    CHECK(do_.startTime == doctest::Approx(0.0));
    CHECK(mi.startTime == doctest::Approx(0.0));
    CHECK(sol.startTime == doctest::Approx(0.0));
    CHECK(do_.duration == doctest::Approx(1.0));

    // Só o play() final andou.
    CHECK(playhead.now() == doctest::Approx(1.0));
}

TEST_CASE("rest avança sem agendar nada") {
    Playhead playhead(120.0);

    playhead.rest(2.0);
    CHECK(playhead.now() == doctest::Approx(1.0));

    const SoundEvent depois = playhead.play(60.0, 1.0);
    CHECK(depois.startTime == doctest::Approx(1.0));
}

TEST_CASE("o cursor nunca anda para trás") {
    Playhead playhead(120.0);
    playhead.advance(4.0);
    const double antes = playhead.now();

    playhead.advance(-10.0);
    playhead.advance(0.0);

    CHECK(playhead.now() == doctest::Approx(antes));
    CHECK(playhead.positionInBeats() == doctest::Approx(4.0));
}

TEST_CASE("a posição é legível em tempos e em ciclos") {
    Playhead playhead(120.0);
    playhead.advance(6.0);

    CHECK(playhead.positionInBeats() == doctest::Approx(6.0));
    CHECK(playhead.positionInCycles() == doctest::Approx(1.5));  // 4/4
}

TEST_CASE("mudar o andamento não reescreve o passado") {
    // A âncora existe exatamente para isto. Se o cursor guardasse só tempos,
    // um setBpm() no meio da peça recalcularia o instante de tudo que veio
    // antes, e a música saltaria.
    Playhead playhead(120.0);
    playhead.advance(1.0);
    CHECK(playhead.now() == doctest::Approx(0.5));

    CHECK(playhead.setBpm(60.0));
    CHECK(playhead.now() == doctest::Approx(0.5));  // não virou 1.0

    const SoundEvent depois = playhead.play(60.0, 1.0);
    CHECK(depois.startTime == doctest::Approx(0.5));
    CHECK(depois.duration == doctest::Approx(1.0));  // já no andamento novo
    CHECK(playhead.now() == doctest::Approx(1.5));
}

TEST_CASE("setBpm para o mesmo valor é inofensivo") {
    Playhead playhead(120.0);
    playhead.advance(3.0);
    const double antes = playhead.now();

    CHECK(playhead.setBpm(120.0));
    CHECK(playhead.now() == doctest::Approx(antes));
    CHECK(playhead.positionInBeats() == doctest::Approx(3.0));
}

TEST_CASE("trocas sucessivas de andamento acumulam corretamente") {
    Playhead playhead(120.0);
    playhead.advance(2.0);            // 1.0 s
    playhead.setBpm(60.0);
    playhead.advance(2.0);            // +2.0 s
    playhead.setBpm(240.0);
    playhead.advance(4.0);            // +1.0 s

    CHECK(playhead.now() == doctest::Approx(4.0));
    CHECK(playhead.positionInBeats() == doctest::Approx(8.0));
}

TEST_CASE("10000 colcheias não acumulam erro de arredondamento") {
    // O motivo de o cursor acumular tempos e converter só na emissão: somar
    // segundos a cada nota produziria o drift que o MAT-08 (#45) vai caçar.
    Playhead playhead(120.0);
    for (int i = 0; i < 10000; ++i) {
        playhead.play(60.0, 0.5);
    }

    // 10000 * 0.5 tempo * 0.5 s/tempo = 2500 s. A comparação é exata, e não
    // por aproximação, de propósito: é o erro zero que se está afirmando aqui.
    // Com uma tolerância qualquer o teste passaria mesmo se o drift voltasse.
    CHECK(playhead.now() == 2500.0);
    CHECK(playhead.positionInBeats() == 5000.0);
}

TEST_CASE("reset devolve o cursor à origem mas preserva o andamento") {
    Playhead playhead(120.0);
    playhead.setBpm(90.0);
    playhead.advance(8.0);

    playhead.reset();

    CHECK(playhead.now() == doctest::Approx(0.0));
    CHECK(playhead.positionInBeats() == doctest::Approx(0.0));
    CHECK(playhead.bpm() == doctest::Approx(90.0));
}

TEST_CASE("uma Timeline nova está vazia e não dura nada") {
    Timeline timeline;

    CHECK(timeline.empty());
    CHECK(timeline.size() == 0);
    CHECK(timeline.duration() == doctest::Approx(0.0));
}

TEST_CASE("a duração da Timeline é quando o último som termina") {
    // Não é o maior startTime: o INT-03 (#19) precisa disso para não fechar o
    // programa no meio da última nota.
    Timeline timeline;
    timeline.add(SoundEvent{0.0, 4.0, 440.0, 1.0f});   // termina em 4.0
    timeline.add(SoundEvent{1.0, 0.5, 440.0, 1.0f});   // termina em 1.5

    CHECK(timeline.duration() == doctest::Approx(4.0));
}

TEST_CASE("sortByStartTime ordena e é estável no acorde") {
    // Os eventos entram em ordem de agendamento, que não é ordem de início
    // quando há acorde.
    Timeline timeline;
    timeline.add(SoundEvent{2.0, 1.0, 100.0, 1.0f});
    timeline.add(SoundEvent{0.0, 1.0, 200.0, 1.0f});
    timeline.add(SoundEvent{0.0, 1.0, 300.0, 1.0f});

    timeline.sortByStartTime();

    REQUIRE(timeline.size() == 3);
    CHECK(timeline.events()[0].frequency == doctest::Approx(200.0));
    CHECK(timeline.events()[1].frequency == doctest::Approx(300.0));
    CHECK(timeline.events()[2].frequency == doctest::Approx(100.0));
}

TEST_CASE("clear esvazia a Timeline") {
    Timeline timeline;
    timeline.add(SoundEvent{0.0, 1.0, 440.0, 1.0f});
    timeline.clear();

    CHECK(timeline.empty());
    CHECK(timeline.duration() == doctest::Approx(0.0));
}

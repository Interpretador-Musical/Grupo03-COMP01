#include "doctest.h"

#include <string>

#include "core/pitch.h"
#include "core/playhead.h"
#include "frontend/frontend.h"
#include "interpreter/interpreter.h"

namespace {

// Parseia e interpreta um programa que já se sabe válido; falha o teste (com
// a mensagem de erro) se não for. A aceitação/rejeição da gramática já vive
// em `test_parser.cpp`/`test_ast.cpp` — aqui só interessa a semântica.
mus::Interpretador interpretar(const std::string& fonte) {
    std::string erroSintaxe;
    auto programa = mus::parseToAst(fonte, &erroSintaxe);
    REQUIRE_MESSAGE(programa != nullptr,
                    "erro de sintaxe inesperado: " << erroSintaxe);

    mus::Interpretador interpretador;
    REQUIRE_MESSAGE(interpretador.executar(*programa),
                    "erro de execução inesperado: " << interpretador.erro());
    return interpretador;
}

}  // namespace

TEST_CASE("atribuição e leitura de variável produz o evento certo") {
    const auto interpretador =
        interpretar("andamento 120\ntempo = 0.5\ntoca do4 por tempo");
    const auto& eventos = interpretador.timeline().events();

    REQUIRE(eventos.size() == 1);
    CHECK(eventos[0].frequency == doctest::Approx(mus::midiToFrequency(60)));
    CHECK(eventos[0].duration ==
          doctest::Approx(mus::Playhead(120.0).beatsToSeconds(0.5)));
}

TEST_CASE("transposição e duração calculada batem com a conta manual do site") {
    // La4 (MIDI 69) - 2 semitons = Sol4 (MIDI 67); duração = 0.5 * 2 = 1 tempo.
    const auto interpretador =
        interpretar("andamento 120\ntempo=0.5\ntoca la4 - 2 por tempo * 2");
    const auto& eventos = interpretador.timeline().events();

    REQUIRE(eventos.size() == 1);
    CHECK(eventos[0].frequency == doctest::Approx(mus::midiToFrequency(67)));
    CHECK(eventos[0].duration ==
          doctest::Approx(mus::Playhead(120.0).beatsToSeconds(1.0)));
}

TEST_CASE("nota sem oitava usa a oitava corrente") {
    SUBCASE("sem nenhum comando oitava, vale a padrão (4)") {
        const auto interpretador = interpretar("toca do por 1.0");
        CHECK(interpretador.timeline().events()[0].frequency ==
              doctest::Approx(mus::midiToFrequency(60)));
    }
    SUBCASE("depois de `oitava 5`, vale 5") {
        const auto interpretador = interpretar("oitava 5\ntoca do por 1.0");
        CHECK(interpretador.timeline().events()[0].frequency ==
              doctest::Approx(mus::midiToFrequency(72)));
    }
}

TEST_CASE("oitava muda só as notas seguintes, não as já tocadas") {
    const auto interpretador =
        interpretar("toca do por 1.0\noitava 5\ntoca do por 1.0");
    const auto& eventos = interpretador.timeline().events();

    REQUIRE(eventos.size() == 2);
    CHECK(eventos[0].frequency == doctest::Approx(mus::midiToFrequency(60)));
    CHECK(eventos[1].frequency == doctest::Approx(mus::midiToFrequency(72)));
}

TEST_CASE("repita N vezes gera N eventos avançando o playhead") {
    // 60 BPM = 1 segundo por tempo, então cada `toca ... por 1.0` ocupa
    // exatamente 1 s e a próxima começa onde a anterior terminou.
    const auto interpretador =
        interpretar("andamento 60\nrepita 3 vezes { toca do4 por 1.0 }");
    const auto& eventos = interpretador.timeline().events();

    REQUIRE(eventos.size() == 3);
    CHECK(eventos[0].startTime == doctest::Approx(0.0));
    CHECK(eventos[1].startTime == doctest::Approx(1.0));
    CHECK(eventos[2].startTime == doctest::Approx(2.0));
}

TEST_CASE("pausa gera um evento silencioso que avança o tempo") {
    const auto interpretador =
        interpretar("andamento 60\npausa por 1.0\ntoca do4 por 1.0");
    const auto& eventos = interpretador.timeline().events();

    REQUIRE(eventos.size() == 2);
    CHECK(eventos[0].frequency == doctest::Approx(0.0));
    CHECK(eventos[0].volume == doctest::Approx(0.0f));
    CHECK(eventos[0].duration == doctest::Approx(1.0));
    CHECK(eventos[1].startTime == doctest::Approx(1.0));
}

TEST_CASE("variável usada antes de definida é erro de execução, não crash") {
    std::string erroSintaxe;
    auto programa = mus::parseToAst("toca tempo por 1.0", &erroSintaxe);
    REQUIRE(programa != nullptr);

    mus::Interpretador interpretador;
    CHECK_FALSE(interpretador.executar(*programa));
    CHECK_FALSE(interpretador.erro().empty());
}

TEST_CASE("andamento não positivo é erro de execução") {
    std::string erroSintaxe;
    auto programa = mus::parseToAst("andamento 0\ntoca do4 por 1.0", &erroSintaxe);
    REQUIRE(programa != nullptr);

    mus::Interpretador interpretador;
    CHECK_FALSE(interpretador.executar(*programa));
    CHECK_FALSE(interpretador.erro().empty());
}

TEST_CASE("volume fora de [0, 1] é erro de execução") {
    // Invariante do IR (COMP-A, #104). `1 - 2` e `0 / 0` passam pela
    // semântica (são números) e só o valor calculado revela o problema.
    for (const char* fonte : {"volume 1.5\ntoca do4 por 1.0",
                              "volume 1 - 2\ntoca do4 por 1.0",
                              "volume 0 / 0\ntoca do4 por 1.0"}) {
        std::string erroSintaxe;
        auto programa = mus::parseToAst(fonte, &erroSintaxe);
        REQUIRE(programa != nullptr);

        mus::Interpretador interpretador;
        CHECK_FALSE(interpretador.executar(*programa));
        CHECK(interpretador.erro().find("volume") != std::string::npos);
        CHECK(interpretador.timeline().empty());
    }
}

TEST_CASE("altura que não dá frequência finita e positiva é erro de execução") {
    // `1 / 0` daria inf Hz; `0 - 20000` daria 0 Hz, que no IR é reservado
    // para pausa e viraria uma nota muda com volume 1.
    for (const char* fonte : {"toca 1 / 0 por 1", "toca 0 - 20000 por 1"}) {
        CAPTURE(fonte);
        std::string erroSintaxe;
        auto programa = mus::parseToAst(fonte, &erroSintaxe);
        REQUIRE(programa != nullptr);

        mus::Interpretador interpretador;
        CHECK_FALSE(interpretador.executar(*programa));
        CHECK(interpretador.erro().find("altura de nota invalida") != std::string::npos);
        CHECK(interpretador.timeline().empty());
    }
}

TEST_CASE("volume nas pontas do intervalo é aceito") {
    const auto interpretador =
        interpretar("volume 0\ntoca do4 por 1.0\nvolume 1\ntoca do4 por 1.0");
    const auto& eventos = interpretador.timeline().events();

    REQUIRE(eventos.size() == 2);
    CHECK(eventos[0].volume == 0.0f);
    CHECK(eventos[1].volume == 1.0f);
}

TEST_CASE("volume corrente é aplicado ao evento") {
    const auto interpretador = interpretar("volume 0.5\ntoca do4 por 1.0");
    CHECK(interpretador.timeline().events()[0].volume == doctest::Approx(0.5f));
}

TEST_CASE("o programa de referência arpejo.mus interpreta sem erro") {
    // Mesmo conteúdo de exemplos/arpejo.mus (CMP-02, #9) — 4 x 3 notas do
    // repita, mais a nota de fechamento e a pausa final: 14 eventos.
    const std::string arpejo = R"(
andamento 120
volume 0.8

tempo = 0.5

repita 4 vezes {
    toca do4  por tempo
    toca mi4  por tempo
    toca sol4 por tempo
}

toca la4 - 2 por tempo * 2

pausa por 1.0
)";

    const auto interpretador = interpretar(arpejo);
    CHECK(interpretador.timeline().events().size() == 14);
    CHECK(interpretador.timeline().duration() > 0.0);
}


TEST_CASE("MAT-04: compilador rejeita duracao negativa") {
    const std::string programas[] = {
        "toca do4 por -1",
        "pausa por -0.5"
    };

    for (const auto& fonte : programas) {
        std::string erroSintaxe;

        auto programa =
            mus::parseToAst(fonte, &erroSintaxe);

        REQUIRE_MESSAGE(
            programa != nullptr,
            "erro de sintaxe inesperado: " << erroSintaxe
        );

        mus::Interpretador interpretador;

        CHECK_FALSE(interpretador.executar(*programa));
        CHECK_FALSE(interpretador.erro().empty());
        CHECK(interpretador.timeline().empty());
    }
}


TEST_CASE("MAT-04: timeline com pausas e mudanca de BPM") {
    const auto interpretador = interpretar(
        "andamento 120\n"
        "repita 2 vezes {\n"
        "    toca do4 por 0.5\n"
        "}\n"
        "pausa por 1\n"
        "andamento 60\n"
        "toca re4 por 0.5"
    );

    const auto& eventos =
        interpretador.timeline().events();

    REQUIRE(eventos.size() == 4);

    // Duas notas de 0.25 s em 120 BPM.
    CHECK(
        eventos[0].startTime ==
        doctest::Approx(0.0)
    );
    CHECK(
        eventos[0].duration ==
        doctest::Approx(0.25)
    );
    CHECK(
        eventos[1].startTime ==
        doctest::Approx(0.25)
    );

    // Pausa de um beat a 120 BPM = 0.5 s.
    CHECK(
        eventos[2].startTime ==
        doctest::Approx(0.5)
    );
    CHECK(
        eventos[2].duration ==
        doctest::Approx(0.5)
    );

    // A ultima nota começa em 1 s.
    // Em 60 BPM, 0.5 beat equivale a 0.5 s.
    CHECK(
        eventos[3].startTime ==
        doctest::Approx(1.0)
    );
    CHECK(
        eventos[3].duration ==
        doctest::Approx(0.5)
    );

    CHECK(
        interpretador.timeline().duration() ==
        doctest::Approx(1.5)
    );
}

TEST_CASE("closure não enxerga variável criada depois da declaração") {
    auto corpo = mus::parseToAst("toca do4 por x");
    REQUIRE(corpo != nullptr);

    mus::Interpretador interp;
    interp.declararClosure("f", {}, corpo.get());
    interp.ambiente().atribuir("x", 1.0);   // depois da declaração

    CHECK_FALSE(interp.invocarClosure("f"));
    CHECK(interp.erro().find("variável 'x'") != std::string::npos);
}

TEST_CASE("closure usa o valor capturado na declaração") {
    auto corpo = mus::parseToAst("toca do4 por tempo");
    REQUIRE(corpo != nullptr);

    mus::Interpretador interp;
    interp.ambiente().atribuir("tempo", 0.5);
    interp.declararClosure("f", {}, corpo.get());
    interp.ambiente().atribuir("tempo", 100.0);   // não deve afetar a closure

    CHECK(interp.invocarClosure("f"));
    CHECK(interp.timeline().size() == 1);
}

TEST_CASE("closure com parâmetro, aridade errada e nome inexistente") {
    auto corpo = mus::parseToAst("toca do4 por a");
    REQUIRE(corpo != nullptr);

    mus::Interpretador interp;
    interp.declararClosure("f", {"a"}, corpo.get());

    CHECK_FALSE(interp.invocarClosure("f"));   // faltou o argumento
    CHECK(interp.erro().find("esperava 1 argumento(s)") != std::string::npos);

    mus::Interpretador outro;
    outro.declararClosure("f", {"a"}, corpo.get());
    CHECK(outro.invocarClosure("f", {mus::Valor{0.5}}));
    CHECK(outro.timeline().size() == 1);

    mus::Interpretador vazio;
    CHECK_FALSE(vazio.invocarClosure("nao_existe"));
}
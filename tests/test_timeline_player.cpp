#include "doctest.h"

#include <cmath>
#include <cstdint>
#include <vector>

#include "core/sound_event.h"
#include "core/timeline_player.h"

using mus::SoundEvent;
using mus::TimelinePlayer;

// Testa só a lógica pura de seleção de evento por amostra — sem dispositivo
// de áudio, sem thread. Os números abaixo são calculados à mão (comentados
// junto de cada caso) com a mesma fórmula de seno que `render()` usa, uma
// taxa de amostragem pequena (4 ou 8 Hz) deixa a conta simples de conferir.
// O que importa verificar aqui não é `std::sin` em si, e sim se o cursor
// escolhe o evento certo no frame certo — inclusive nas fronteiras.

TEST_CASE("timeline vazia produz silêncio e duração zero") {
    TimelinePlayer player(8);
    player.load({});
    CHECK(player.totalDuration() == doctest::Approx(0.0));

    // Buffer "sujo" de propósito: se render() não escrever nada, o teste
    // não pegaria o bug.
    std::vector<float> saida(4, 1.0f);
    player.render(saida.data(), 4);
    for (float amostra : saida) {
        CHECK(amostra == doctest::Approx(0.0f));
    }
}

TEST_CASE("um evento único sintetiza a senoide certa, com o ganho aplicado") {
    // 1 Hz a 4 Hz de taxa = 4 amostras por ciclo completo, incremento de fase
    // de π/2 por frame: sin(0), sin(π/2), sin(π), sin(3π/2) = 0, 1, ~0, -1,
    // escalado pelo volume 0.5 do evento.
    TimelinePlayer player(4);
    player.load({SoundEvent{0.0, 1.0, 1.0, 0.5f}});

    std::vector<float> saida(4, -99.0f);
    player.render(saida.data(), 4);

    CHECK(saida[0] == doctest::Approx(0.0f));
    CHECK(saida[1] == doctest::Approx(0.5f));
    CHECK(saida[2] == doctest::Approx(0.0f));
    CHECK(saida[3] == doctest::Approx(-0.5f));
}

TEST_CASE("evento de pausa (frequência e volume zerados) produz silêncio") {
    TimelinePlayer player(8);
    player.load({SoundEvent{0.0, 1.0, 0.0, 0.0f}});

    std::vector<float> saida(8, -1.0f);
    player.render(saida.data(), 8);
    for (float amostra : saida) {
        CHECK(amostra == doctest::Approx(0.0f));
    }
}

TEST_CASE("dois eventos sequenciais trocam de frequência exatamente no frame certo") {
    // Taxa 8 Hz. Evento 1: 2 Hz por 0.5 s (frames 0-3), incremento π/2.
    // Fases 0, π/2, π, 3π/2 -> amostras 0, 1, ~0, -1.
    // Depois do frame 3, a fase acumulada fecha em 3π/2 + π/2 = 2π, que
    // envolve para 0 — é essa fase 0 que o evento 2 herda no frame 4.
    // Evento 2: 1 Hz por 0.5 s (frames 4-7), incremento π/4, partindo de
    // fase 0: fases 0, π/4, π/2, 3π/4 -> amostras 0, √2/2, 1, √2/2.
    TimelinePlayer player(8);
    player.load({
        SoundEvent{0.0, 0.5, 2.0, 1.0f},
        SoundEvent{0.5, 0.5, 1.0, 1.0f},
    });

    std::vector<float> saida(8, -99.0f);
    player.render(saida.data(), 8);

    const float raizDeDoisMeios = static_cast<float>(std::sqrt(2.0) / 2.0);

    // Evento 1 (frames 0-3).
    CHECK(saida[0] == doctest::Approx(0.0f));
    CHECK(saida[1] == doctest::Approx(1.0f));
    CHECK(saida[2] == doctest::Approx(0.0f));
    CHECK(saida[3] == doctest::Approx(-1.0f));

    // Evento 2 (frames 4-7) — nem um frame adiantado, nem atrasado.
    CHECK(saida[4] == doctest::Approx(0.0f));
    CHECK(saida[5] == doctest::Approx(raizDeDoisMeios));
    CHECK(saida[6] == doctest::Approx(1.0f));
    CHECK(saida[7] == doctest::Approx(raizDeDoisMeios));
}

TEST_CASE("gap real entre dois eventos produz silêncio durante o intervalo") {
    // Caso defensivo: o Interpretador nunca deixa um buraco (pausa também
    // vira evento), mas TimelinePlayer não deve presumir isso.
    // Taxa 8 Hz. Evento 0 cobre [0.0, 0.25) = frames 0-1. Evento 1 só começa
    // em 0.5 = frame 4. Frames 2 e 3 (elapsed 0.25 e 0.375) caem no gap.
    TimelinePlayer player(8);
    player.load({
        SoundEvent{0.0, 0.25, 2.0, 1.0f},
        SoundEvent{0.5, 0.25, 1.0, 1.0f},
    });

    std::vector<float> saida(6, -99.0f);
    player.render(saida.data(), 6);

    CHECK(saida[2] == doctest::Approx(0.0f));
    CHECK(saida[3] == doctest::Approx(0.0f));
}

TEST_CASE("render além do fim da timeline é silêncio, sem acessar fora do vetor") {
    TimelinePlayer player(8);
    player.load({SoundEvent{0.0, 0.25, 2.0, 1.0f}});  // só frames 0-1

    std::vector<float> saida(6, -99.0f);
    player.render(saida.data(), 6);
    CHECK(saida[2] == doctest::Approx(0.0f));
    CHECK(saida[3] == doctest::Approx(0.0f));
    CHECK(saida[4] == doctest::Approx(0.0f));
    CHECK(saida[5] == doctest::Approx(0.0f));

    // Chamar de novo depois do fim (cursor já esgotado) não deve estourar o
    // vetor nem travar — só continua produzindo silêncio.
    std::vector<float> maisSaida(4, -99.0f);
    player.render(maisSaida.data(), 4);
    for (float amostra : maisSaida) {
        CHECK(amostra == doctest::Approx(0.0f));
    }
}

TEST_CASE("totalDuration reflete o fim do último evento carregado") {
    TimelinePlayer player(8);
    player.load({
        SoundEvent{0.0, 0.25, 2.0, 1.0f},
        SoundEvent{0.5, 0.25, 1.0, 1.0f},
    });
    CHECK(player.totalDuration() == doctest::Approx(0.75));
}

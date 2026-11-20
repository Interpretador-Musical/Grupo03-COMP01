#include "doctest.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "audio/audio_engine.h"
#include "core/frames.h"
#include "core/playhead.h"
#include "core/ring_buffer.h"
#include "core/timeline_player.h"
#include "core/timeline_queue.h"
#include "frontend/frontend.h"
#include "interpreter/interpreter.h"
#include "semantic/analisador_semantico.h"

using namespace mus;

// Drift temporal em músicas finitas — MAT-08 (#45). Metodologia, medições e
// tolerâncias em `docs/drift_temporal.md`.
//
// O erro total de um início é separado em três parcelas, medidas cada uma
// contra a sua própria referência:
//
//   geração     |startTime − instante exato|, em segundos. O "exato" é a
//               fórmula fechada calculada em long double a partir das
//               durações racionais escritas no programa.
//   quantização |frame / taxa − startTime| ≤ meio frame. É inerente a tocar
//               a 44,1 kHz e não se acumula: cada início é convertido do seu
//               valor absoluto (core/frames.h).
//   reprodução  frame em que a voz realmente disparou − frame agendado
//               (segundosParaFrame(startTime)). Tem de ser exatamente 0.
//
// A reprodução é observada de forma determinística, sem dispositivo de
// áudio: o callback (renderRealTime) é chamado um frame por vez, e depois de
// cada frame as vozes em ataque com startFrame == frame atual são os eventos
// que dispararam nele.

namespace {

constexpr std::uint32_t kTaxa = AudioEngine::kSampleRate;

// Tolerância de geração, em segundos. Antes da soma compensada no Playhead,
// 2·10^5 notas de 1/3 de tempo já passavam disso (erro medido de ~5·10^-8 s);
// depois dela o erro fica em ~10^-11 s. Um frame a 44,1 kHz são ~2,3·10^-5 s.
constexpr double kToleranciaGeracao = 1e-9;

// Um trecho de andamento constante: `notas` notas de `numerador/denominador`
// tempos cada, a `bpm`.
struct Trecho {
    double bpm;
    long notas;
    int numerador;
    int denominador;
};

// Gera a Timeline pelo Playhead, como o interpretador faz, e a referência
// exata em long double pela fórmula fechada de cada trecho.
struct Medicao {
    Timeline timeline;
    std::vector<long double> exatos;
};

Medicao gerar(const std::vector<Trecho>& trechos) {
    Medicao m;
    Playhead playhead;
    long double ancora = 0.0L;
    for (const Trecho& t : trechos) {
        REQUIRE(playhead.setBpm(t.bpm));
        const double duracao = static_cast<double>(t.numerador) / t.denominador;
        const long double segundosPorNota =
            static_cast<long double>(t.numerador) * 60.0L /
            (static_cast<long double>(t.denominador) * static_cast<long double>(t.bpm));
        for (long i = 0; i < t.notas; ++i) {
            m.timeline.add(playhead.play(60.0 + static_cast<double>(i % 12), duracao, 0.5f));
            m.exatos.push_back(ancora + static_cast<long double>(i) * segundosPorNota);
        }
        ancora += static_cast<long double>(t.notas) * segundosPorNota;
    }
    return m;
}

struct Erros {
    double geracaoMaxima = 0.0;       // segundos
    double quantizacaoMaxima = 0.0;   // segundos
    long iniciosForaDoFrameExato = 0; // frame(startTime) != frame(exato)
};

Erros medirGeracao(const Medicao& m) {
    Erros e;
    const auto& eventos = m.timeline.events();
    for (std::size_t i = 0; i < eventos.size(); ++i) {
        const double inicio = eventos[i].startTime;
        const double erro =
            static_cast<double>(std::fabs(static_cast<long double>(inicio) - m.exatos[i]));
        e.geracaoMaxima = std::max(e.geracaoMaxima, erro);

        const std::uint64_t frame = segundosParaFrame(inicio, kTaxa);
        const double quantizacao =
            std::fabs(static_cast<double>(frame) / kTaxa - inicio);
        e.quantizacaoMaxima = std::max(e.quantizacaoMaxima, quantizacao);

        const auto frameExato = static_cast<std::uint64_t>(
            std::llroundl(m.exatos[i] * static_cast<long double>(kTaxa)));
        if (frame != frameExato) {
            ++e.iniciosForaDoFrameExato;
        }
    }
    return e;
}

// Disparos observados na reprodução: em que frame cada voz começou e em que
// frame voltou a ficar ociosa.
struct Disparo {
    std::uint64_t frame;
    double frequencia;
};

struct Observacao {
    std::vector<Disparo> disparos;
    std::vector<std::uint64_t> fins;  // frames em que uma voz ficou ociosa
};

Observacao observarReproducao(const Timeline& timeline) {
    Timeline ordenada = timeline;
    ordenada.sortByStartTime();

    AudioEngine engine;
    SpscRingBuffer<SoundEvent> fila(ordenada.size());
    REQUIRE(carregarFila(ordenada, fila));
    engine.setRingBuffer(&fila);
    engine.setExpectedDuration(ordenada.duration());

    const auto totalFrames = segundosParaFrame(ordenada.duration(), kTaxa) + kTaxa / 10;

    Observacao obs;
    std::array<AudioEngine::EnvState, AudioEngine::kMaxVoices> antes{};
    float amostra = 0.0f;
    for (std::uint64_t frame = 0; frame < totalFrames; ++frame) {
        engine.renderRealTime(&amostra, 1);
        const auto& vozes = engine.voices();
        for (std::size_t v = 0; v < vozes.size(); ++v) {
            if (vozes[v].envState == AudioEngine::EnvState::Attack &&
                vozes[v].startFrame == frame) {
                obs.disparos.push_back({frame, vozes[v].frequency});
            }
            if (antes[v] != AudioEngine::EnvState::Idle &&
                vozes[v].envState == AudioEngine::EnvState::Idle) {
                obs.fins.push_back(frame);
            }
            antes[v] = vozes[v].envState;
        }
    }
    return obs;
}

Timeline compilar(const std::string& fonte) {
    std::string erro;
    auto programa = parseToAst(fonte, &erro);
    REQUIRE_MESSAGE(programa != nullptr, "erro de sintaxe: " << erro);

    semantica::AnalisadorSemantico analisador;
    REQUIRE_MESSAGE(analisador.analisar(*programa), "erro semântico inesperado");

    Interpretador interpretador;
    REQUIRE_MESSAGE(interpretador.executar(*programa),
                    "erro de execução: " << interpretador.erro());
    return interpretador.timeline();
}

}  // namespace

// ---------------------------------------------------------------- geração

TEST_CASE("drift: 2·10^5 notas de 1/3 de tempo não acumulam erro de geração") {
    // O pior caso medido antes da correção: 1/3 não tem representação exata,
    // e a soma ingênua perdia um pouco a cada nota.
    const Medicao m = gerar({{120.0, 200000, 1, 3}});
    const Erros e = medirGeracao(m);

    CHECK(e.geracaoMaxima < kToleranciaGeracao);
    CHECK(e.iniciosForaDoFrameExato == 0);
    CHECK(e.quantizacaoMaxima <= 0.5 / kTaxa);
}

TEST_CASE("drift: trocas de andamento não acumulam erro de geração") {
    // Cada troca congela a âncora em segundos; o erro de cada trecho não pode
    // se somar ao dos anteriores além da tolerância.
    std::vector<Trecho> trechos;
    const double andamentos[] = {90.0, 120.0, 137.0, 61.0, 173.0};
    for (int i = 0; i < 500; ++i) {
        trechos.push_back({andamentos[i % 5], 97, 1, (i % 2 == 0) ? 7 : 10});
    }
    const Medicao m = gerar(trechos);
    const Erros e = medirGeracao(m);

    CHECK(e.geracaoMaxima < kToleranciaGeracao);
    CHECK(e.iniciosForaDoFrameExato == 0);
}

TEST_CASE("drift: soma compensada vale também quando a nota é maior que o acumulado") {
    // Neumaier e não Kahan: logo depois de uma troca de andamento o acumulado
    // é 0 e a primeira duração é maior que ele.
    Playhead playhead(120.0);
    for (int i = 0; i < 100000; ++i) {
        playhead.advance(0.1);
        playhead.advance(1e6);
    }
    // 10^5 · (10^6 + 0.1) tempos, exatos em long double.
    const long double esperado = 100000.0L * (1e6L + 0.1L);
    CHECK(static_cast<double>(std::fabs(playhead.positionInBeats() - esperado)) < 1e-3);
}

TEST_CASE("drift: interpretador ponta a ponta com repita, andamento e trilhas") {
    // 4 × (andamento X; 5000 notas de 0.1 tempo; 500 acordes de 1/3) — o
    // instante exato do último evento sai da fórmula fechada.
    const Timeline timeline = compilar(
        "andamento 97\n"
        "repita 5000 vezes { toca do4 por 0.1 }\n"
        "repita 500 vezes { toca do4 por 1 / 3, toca mi4 por 1 / 3 }\n"
        "andamento 137\n"
        "repita 5000 vezes { toca do4 por 0.1 }\n"
        "repita 500 vezes { toca do4 por 1 / 3, toca mi4 por 1 / 3 }\n");
    REQUIRE(timeline.size() == 2 * (5000 + 1000));

    const long double trecho97 = (5000.0L * 0.1L + 500.0L / 3.0L) * 60.0L / 97.0L;
    const long double trecho137 = (5000.0L * 0.1L + 500.0L / 3.0L) * 60.0L / 137.0L;
    const long double fimExato = trecho97 + trecho137;

    CHECK(static_cast<double>(std::fabs(timeline.duration() - fimExato)) < kToleranciaGeracao);

    // Notas de acorde saem com o mesmo startTime, bit a bit: são simultâneas
    // também depois de 6000 notas acumuladas.
    const auto& ev = timeline.events();
    const SoundEvent& a = ev[ev.size() - 2];
    const SoundEvent& b = ev[ev.size() - 1];
    CHECK(a.startTime == b.startTime);
    const long double inicioExato = fimExato - (1.0L / 3.0L) * 60.0L / 137.0L;
    CHECK(static_cast<double>(std::fabs(a.startTime - inicioExato)) < kToleranciaGeracao);
}

// -------------------------------------------------------------- reprodução

TEST_CASE("drift: cada evento dispara exatamente no frame agendado") {
    // Notas curtas (0,025 s) com troca de andamento no meio e um acorde a cada
    // 16 notas: o frame observado no callback é comparado ao frame agendado.
    Playhead playhead(240.0);
    Timeline timeline;
    for (int i = 0; i < 1200; ++i) {
        if (i == 600) {
            REQUIRE(playhead.setBpm(173.0));
        }
        if (i % 16 == 0) {
            timeline.add(playhead.playHere(Note{64.0, 0.3f}, 0.1));
            timeline.add(playhead.playHere(Note{67.0, 0.3f}, 0.1));
        }
        timeline.add(playhead.play(60.0, 0.1, 0.3f));
    }

    Timeline ordenada = timeline;
    ordenada.sortByStartTime();
    const Observacao obs = observarReproducao(ordenada);

    REQUIRE(obs.disparos.size() == ordenada.size());
    long erroDeReproducao = 0;
    for (std::size_t i = 0; i < obs.disparos.size(); ++i) {
        const std::uint64_t agendado =
            segundosParaFrame(ordenada.events()[i].startTime, kTaxa);
        if (obs.disparos[i].frame != agendado) {
            ++erroDeReproducao;
        }
        CHECK(obs.disparos[i].frequencia == ordenada.events()[i].frequency);
    }
    CHECK(erroDeReproducao == 0);
}

TEST_CASE("drift: o fim de uma nota coincide com o início da seguinte") {
    // Duração de 1000,7 frames: truncar dava 1000 e o início seguinte,
    // arredondado, caía em 1001 — um frame de buraco a cada nota. Agora o
    // fim também sai do instante absoluto.
    const double duracao = 1000.7 / kTaxa;
    Timeline timeline;
    for (int i = 0; i < 50; ++i) {
        timeline.add(SoundEvent{i * duracao, duracao, 440.0, 0.3f});
    }
    const Observacao obs = observarReproducao(timeline);

    REQUIRE(obs.disparos.size() == 50);
    REQUIRE(obs.fins.size() == 50);
    for (std::size_t i = 0; i + 1 < obs.disparos.size(); ++i) {
        const std::uint64_t fimAgendado =
            segundosParaFrame(timeline.events()[i].endTime(), kTaxa);
        CHECK(fimAgendado == obs.disparos[i + 1].frame);
        // O release cabe dentro da duração: a última amostra não nula da
        // nota é a do frame fim − 1, e a voz fica ociosa no próprio frame do
        // fim, o mesmo em que a nota seguinte dispara.
        CHECK(obs.fins[i] == fimAgendado);
    }
}

TEST_CASE("drift: TimelinePlayer e AudioEngine usam a mesma regra de frame") {
    // Taxa 8 Hz, início em 0,3 frame: arredonda para o frame 0. Antes, o
    // TimelinePlayer comparava em segundos e só começava no frame 1.
    // Evento de 2 Hz: incremento π/2, então o frame 1 é sin(π/2) = 1 se a nota
    // começou no frame 0, e sin(0) = 0 se começou no frame 1.
    TimelinePlayer player(8);
    player.load({SoundEvent{0.3 / 8.0, 1.0, 2.0, 1.0f}});
    std::vector<float> saida(4, -99.0f);
    player.render(saida.data(), 4);
    CHECK(saida[1] == doctest::Approx(1.0f));

    CHECK(segundosParaFrame(0.3 / 8.0, 8) == 0);
    CHECK(segundosParaFrame(0.6 / 8.0, 8) == 1);
}

TEST_CASE("drift: segundosParaFrame recusa entradas fora do domínio") {
    CHECK(segundosParaFrame(-1.0, kTaxa) == 0);
    CHECK(segundosParaFrame(std::nan(""), kTaxa) == 0);
    CHECK(segundosParaFrame(INFINITY, kTaxa) == 0);
    CHECK(segundosParaFrame(1.0, kTaxa) == kTaxa);
    CHECK(segundosParaFrame(3600.0, kTaxa) == 3600ull * kTaxa);
}

// --------------------------------------------------------------- medição

// Gera a tabela publicada em docs/drift_temporal.md. Pulado por padrão (leva
// alguns segundos); para rodar:
//
//   ./build/tests/mus_tests --no-skip --test-case="drift: medição*"
TEST_CASE("drift: medição de referência" * doctest::skip()) {
    const std::vector<Trecho> casos = {
        {120.0, 100000, 1, 10},
        {120.0, 100000, 1, 3},
        {97.0, 200000, 1, 7},
        {137.0, 1000000, 1, 10},
        {173.0, 1000000, 1, 3},
    };
    std::printf("| notas | duração (tempos) | BPM | fim (s) | geração máx (s) | "
                "geração máx (frames) | quantização máx (frames) | inícios fora do frame |\n");
    std::printf("|---:|---:|---:|---:|---:|---:|---:|---:|\n");
    for (const Trecho& t : casos) {
        const Medicao m = gerar({t});
        const Erros e = medirGeracao(m);
        std::printf("| %ld | %d/%d | %g | %.1f | %.3e | %.3e | %.3f | %ld |\n", t.notas,
                    t.numerador, t.denominador, t.bpm, m.timeline.duration(),
                    e.geracaoMaxima, e.geracaoMaxima * kTaxa,
                    e.quantizacaoMaxima * kTaxa, e.iniciosForaDoFrameExato);
    }
}

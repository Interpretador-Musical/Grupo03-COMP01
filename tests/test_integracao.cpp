#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "audio/audio_engine.h"
#include "core/pitch.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"
#include "frontend/frontend.h"
#include "interpreter/interpreter.h"
#include "semantic/analisador_semantico.h"

// Suíte de testes de integração ponta a ponta (INT-06, #37).
// Valida o pipeline completo do compilador musical em modo batch:
//   Código .mus -> Flex (Lexer) -> Bison (Parser) -> AST ->
//   Análise Semântica -> Interpretador -> Timeline (IR) -> Motor de Áudio.
//
// Cobre casos positivos determinísticos e falhas seguras em cada estágio,
// garantindo que entradas inválidas interrompam o fluxo na fase apropriada.

namespace {

enum class EstagioFalha {
    Nenhum,
    SintaxeLexer,
    Semantica,
    Execucao
};

struct ResultadoPipeline {
    bool sucesso = false;
    EstagioFalha falhaEm = EstagioFalha::Nenhum;
    std::string mensagemErro;
    mus::Timeline timeline;
    std::vector<float> amostrasAudio;
};

ResultadoPipeline executarPipeline(const std::string& fonte, bool sintetizarAudio = true) {
    ResultadoPipeline resultado;

    // 1. Análise Léxica e Sintática (Flex + Bison -> AST)
    std::string erroSintaxe;
    auto ast = mus::parseToAst(fonte, &erroSintaxe);
    if (ast == nullptr) {
        resultado.sucesso = false;
        resultado.falhaEm = EstagioFalha::SintaxeLexer;
        resultado.mensagemErro = erroSintaxe;
        return resultado;
    }

    // 2. Análise Semântica (Escopos, Tipos, Tabela de Símbolos)
    mus::semantica::AnalisadorSemantico semantico;
    if (!semantico.analisar(*ast)) {
        resultado.sucesso = false;
        resultado.falhaEm = EstagioFalha::Semantica;
        if (!semantico.erros().empty()) {
            resultado.mensagemErro = semantico.erros().front().formatado();
        } else {
            resultado.mensagemErro = "erro semântico detectado";
        }
        return resultado;
    }

    // 3. Interpretação da AST (Geração da Timeline IR)
    mus::Interpretador interpretador;
    if (!interpretador.executar(*ast)) {
        resultado.sucesso = false;
        resultado.falhaEm = EstagioFalha::Execucao;
        resultado.mensagemErro = interpretador.erro();
        return resultado;
    }

    resultado.timeline = interpretador.timeline();
    resultado.sucesso = true;

    // 4. Síntese de Áudio sem dispositivo físico (reprodução em memória via AudioEngine)
    if (sintetizarAudio && !resultado.timeline.empty()) {
        std::vector<mus::SoundEvent> eventos(resultado.timeline.events().begin(),
                                             resultado.timeline.events().end());
        std::stable_sort(eventos.begin(), eventos.end(),
                         [](const mus::SoundEvent& a, const mus::SoundEvent& b) {
                             return a.startTime < b.startTime;
                         });

        mus::SpscRingBuffer<mus::SoundEvent> ringBuffer(eventos.size() + 16);
        for (const auto& ev : eventos) {
            ringBuffer.push(ev);
        }

        mus::AudioEngine engine;
        engine.setRingBuffer(&ringBuffer);
        engine.setExpectedDuration(resultado.timeline.duration());

        // Renderiza as amostras até a duração total (ou até no máximo 3 segundos por teste)
        const std::uint32_t totalFrames = static_cast<std::uint32_t>(
            std::ceil(resultado.timeline.duration() * mus::AudioEngine::kSampleRate));
        const std::uint32_t framesParaRenderizar =
            std::min(totalFrames, mus::AudioEngine::kSampleRate * 3u);

        if (framesParaRenderizar > 0) {
            resultado.amostrasAudio.resize(framesParaRenderizar, 0.0f);
            engine.render(resultado.amostrasAudio.data(), framesParaRenderizar);
        }
    }

    return resultado;
}

std::string lerArquivoExemplo(const std::string& caminhoRelativo) {
    const std::string caminhoCompleto = std::string(MUS_SOURCE_DIR) + caminhoRelativo;
    std::ifstream arq(caminhoCompleto, std::ios::binary);
    REQUIRE_MESSAGE(arq.good(), "não foi possível abrir o arquivo: " << caminhoCompleto);
    return std::string(std::istreambuf_iterator<char>(arq),
                       std::istreambuf_iterator<char>());
}

}  // namespace

// ============================================================================
// Casos Positivos: Fluxo Completo Ponta a Ponta
// ============================================================================

TEST_CASE("INT-06: notas, duracoes, pausas e andamento geram timeline e audio deterministico") {
    // 120 BPM: 1 tempo = 0.5 s.
    // Pausa de 0.5 tempo = 0.25 s (silêncio).
    // Dó 4 (MIDI 60 = 261.6255 Hz) por 1.0 tempo = 0.5 s, volume 0.8.
    const std::string codigo =
        "andamento 120\n"
        "volume 0.8\n"
        "pausa por 0.5\n"
        "toca do4 por 1.0\n";

    const auto res = executarPipeline(codigo, /*sintetizarAudio=*/true);
    REQUIRE(res.sucesso);
    CHECK(res.falhaEm == EstagioFalha::Nenhum);

    const auto& eventos = res.timeline.events();
    REQUIRE(eventos.size() == 2);

    // Evento 1: Pausa
    CHECK(eventos[0].startTime == doctest::Approx(0.0));
    CHECK(eventos[0].duration == doctest::Approx(0.25));
    CHECK(eventos[0].frequency == doctest::Approx(0.0));
    CHECK(eventos[0].volume == doctest::Approx(0.0f));

    // Evento 2: Dó 4
    CHECK(eventos[1].startTime == doctest::Approx(0.25));
    CHECK(eventos[1].duration == doctest::Approx(0.5));
    CHECK(eventos[1].frequency == doctest::Approx(mus::midiToFrequency(60)));
    CHECK(eventos[1].volume == doctest::Approx(0.8f));

    // Verificação da síntese PCM de áudio em memória
    REQUIRE_FALSE(res.amostrasAudio.empty());

    // Durante os primeiros 0.20s (dentro da pausa de 0.25s), todas as amostras devem ser silêncio
    const std::uint32_t framesPausa =
        static_cast<std::uint32_t>(0.20 * mus::AudioEngine::kSampleRate);
    float maxAbsPausa = 0.0f;
    for (std::uint32_t i = 0; i < framesPausa; ++i) {
        maxAbsPausa = std::max(maxAbsPausa, std::fabs(res.amostrasAudio[i]));
    }
    CHECK(maxAbsPausa == doctest::Approx(0.0f));

    // Após 0.30s (durante a nota Dó 4), deve haver sinal oscilante não-nulo
    const std::uint32_t frameMeioNota =
        static_cast<std::uint32_t>(0.35 * mus::AudioEngine::kSampleRate);
    float maxAbsNota = 0.0f;
    for (std::uint32_t i = frameMeioNota; i < frameMeioNota + 100; ++i) {
        maxAbsNota = std::max(maxAbsNota, std::fabs(res.amostrasAudio[i]));
    }
    CHECK(maxAbsNota > 0.05f);
}

TEST_CASE("INT-06: variaveis e expressoes aritmeticas afetam tom e duracao") {
    // 100 BPM: 60/100 = 0.6 s por tempo.
    // tempo = 0.5 tempos = 0.3 s.
    // la4 - 2 semitons = sol4 (MIDI 67 = 391.995 Hz).
    // duracao: tempo * 2 = 1.0 tempo = 0.6 s.
    const std::string codigo =
        "andamento 100\n"
        "tempo = 0.5\n"
        "toca la4 - 2 por tempo * 2\n";

    const auto res = executarPipeline(codigo, /*sintetizarAudio=*/true);
    REQUIRE(res.sucesso);

    const auto& eventos = res.timeline.events();
    REQUIRE(eventos.size() == 1);

    CHECK(eventos[0].startTime == doctest::Approx(0.0));
    CHECK(eventos[0].duration == doctest::Approx(0.6));
    CHECK(eventos[0].frequency == doctest::Approx(mus::midiToFrequency(67)));

    // Checagem das amostras PCM geradas: sem NaN, sem Inf e limitadas a [-1.0, 1.0]
    REQUIRE_FALSE(res.amostrasAudio.empty());
    bool todasFinitas = true;
    float maxAbs = 0.0f;
    for (float amostra : res.amostrasAudio) {
        if (!std::isfinite(amostra)) {
            todasFinitas = false;
        }
        maxAbs = std::max(maxAbs, std::fabs(amostra));
    }
    CHECK(todasFinitas);
    CHECK(maxAbs <= 1.0f);
}

TEST_CASE("INT-06: comando repita acumula eventos em sequencia temporal contigua") {
    // 60 BPM = 1.0 s por tempo.
    // 3 iterações de (0.5 s nota + 0.5 s pausa) = 6 eventos contíguos de 0.5 s cada.
    const std::string codigo =
        "andamento 60\n"
        "repita 3 vezes {\n"
        "    toca mi4 por 0.5\n"
        "    pausa por 0.5\n"
        "}\n";

    const auto res = executarPipeline(codigo, /*sintetizarAudio=*/false);
    REQUIRE(res.sucesso);

    const auto& eventos = res.timeline.events();
    REQUIRE(eventos.size() == 6);

    for (std::size_t i = 0; i < eventos.size(); ++i) {
        CHECK(eventos[i].startTime == doctest::Approx(i * 0.5));
        CHECK(eventos[i].duration == doctest::Approx(0.5));
        if (i % 2 == 0) {
            // Nota Mi 4 (MIDI 64)
            CHECK(eventos[i].frequency == doctest::Approx(mus::midiToFrequency(64)));
        } else {
            // Pausa
            CHECK(eventos[i].frequency == doctest::Approx(0.0));
            CHECK(eventos[i].volume == doctest::Approx(0.0f));
        }
    }
}

TEST_CASE("INT-06: demonstracao completa do fluxo com escala.mus") {
    const std::string fonte = lerArquivoExemplo("/exemplos/escala.mus");
    const auto res = executarPipeline(fonte, /*sintetizarAudio=*/true);

    REQUIRE(res.sucesso);
    CHECK(res.falhaEm == EstagioFalha::Nenhum);

    // escala.mus contém 8 notas de 1.0 tempo + 2 acidentes de 0.5 tempo = 10 eventos
    REQUIRE(res.timeline.size() == 10);
    CHECK(res.timeline.duration() > 0.0);

    // Todas as amostras de áudio devem estar no intervalo seguro [-1.0, 1.0]
    REQUIRE_FALSE(res.amostrasAudio.empty());
    bool todasFinitas = true;
    float maxAbs = 0.0f;
    for (float sample : res.amostrasAudio) {
        if (!std::isfinite(sample)) {
            todasFinitas = false;
        }
        maxAbs = std::max(maxAbs, std::fabs(sample));
    }
    CHECK(todasFinitas);
    CHECK(maxAbs <= 1.0f);
}

TEST_CASE("INT-06: demonstracao completa do fluxo com arpejo.mus") {
    const std::string fonte = lerArquivoExemplo("/exemplos/arpejo.mus");
    const auto res = executarPipeline(fonte, /*sintetizarAudio=*/true);

    REQUIRE(res.sucesso);
    CHECK(res.falhaEm == EstagioFalha::Nenhum);

    // 4 repetições de 3 notas = 12 notas + 1 fechamento (la4 - 2) + 1 pausa = 14 eventos
    REQUIRE(res.timeline.size() == 14);
    CHECK(res.timeline.duration() == doctest::Approx(4.0)); // 120 BPM: 6 tempos de notas + 1 tempo + 1 pausa = 8 tempos = 4.0s

    REQUIRE_FALSE(res.amostrasAudio.empty());
    float maxAbs = 0.0f;
    for (float sample : res.amostrasAudio) {
        maxAbs = std::max(maxAbs, std::fabs(sample));
    }
    CHECK(maxAbs > 0.01f);
}

// ============================================================================
// Falhas Seguras: Interrupção nos Estágios Corretos
// ============================================================================

TEST_CASE("INT-06: falha segura - erro lexico eh barrado antes da semantica e do audio") {
    // Caractere '@' não faz parte da gramática
    const std::string codigo =
        "andamento 120\n"
        "toca @ por 1.0\n";

    const auto res = executarPipeline(codigo, /*sintetizarAudio=*/true);
    CHECK_FALSE(res.sucesso);
    CHECK(res.falhaEm == EstagioFalha::SintaxeLexer);
    CHECK(res.mensagemErro.find("Linha 2") != std::string::npos);
}

TEST_CASE("INT-06: falha segura - erro sintatico eh barrado pelo parser") {
    SUBCASE("comando incompleto sem expressao de duracao") {
        const std::string codigo = "toca do4 por";
        const auto res = executarPipeline(codigo, /*sintetizarAudio=*/true);
        CHECK_FALSE(res.sucesso);
        CHECK(res.falhaEm == EstagioFalha::SintaxeLexer);
    }

    SUBCASE("bloco nao fechado") {
        const std::string codigo = "repita 2 vezes { toca do4 por 1.0";
        const auto res = executarPipeline(codigo, /*sintetizarAudio=*/true);
        CHECK_FALSE(res.sucesso);
        CHECK(res.falhaEm == EstagioFalha::SintaxeLexer);
    }
}

TEST_CASE("INT-06: falha segura - erro semantico de escopo/uso antes de declarar") {
    const std::string codigo =
        "andamento 120\n"
        "toca do4 por duracao_inexistente\n";

    const auto res = executarPipeline(codigo, /*sintetizarAudio=*/true);
    CHECK_FALSE(res.sucesso);
    CHECK(res.falhaEm == EstagioFalha::Semantica);
    CHECK(res.mensagemErro.find("duracao_inexistente") != std::string::npos);
}

TEST_CASE("INT-06: falha segura - erro semantico de tipos incompatíveis") {
    const std::string codigo =
        "andamento 120\n"
        "flag = verdadeiro\n"
        "toca do4 por flag\n"; // duração espera tipo numérico

    const auto res = executarPipeline(codigo, /*sintetizarAudio=*/true);
    CHECK_FALSE(res.sucesso);
    CHECK(res.falhaEm == EstagioFalha::Semantica);
}

TEST_CASE("INT-06: falha segura - erro de execucao com volume invalido") {
    // Passa no parser e semântico (volume aceita número), mas viola o intervalo [0, 1] em runtime
    const std::string codigo =
        "toca do4 por 1.0\n"
        "volume 2.5\n"
        "toca re4 por 1.0\n";

    const auto res = executarPipeline(codigo, /*sintetizarAudio=*/true);
    CHECK_FALSE(res.sucesso);
    CHECK(res.falhaEm == EstagioFalha::Execucao);
    CHECK(res.mensagemErro.find("volume precisa estar entre 0 e 1") != std::string::npos);
}

TEST_CASE("INT-06: falha segura - erro de execucao com andamento negativo") {
    const std::string codigo =
        "andamento -10\n"
        "toca do4 por 1.0\n";

    const auto res = executarPipeline(codigo, /*sintetizarAudio=*/true);
    CHECK_FALSE(res.sucesso);
    CHECK(res.falhaEm == EstagioFalha::Execucao);
    CHECK(res.mensagemErro.find("andamento precisa ser positivo") != std::string::npos);
}

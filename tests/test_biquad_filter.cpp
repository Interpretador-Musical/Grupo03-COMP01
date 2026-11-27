#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "audio/audio_engine.h"
#include "audio/biquad_filter.h"
#include "audio/sample_bank.h"
#include "codegen/gerador_wav.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"
#include "core/timeline_queue.h"
#include "frontend/frontend.h"
#include "interpreter/interpreter.h"
#include "otimizacao/dobra_constantes.h"
#include "semantic/analisador_semantico.h"

namespace {

constexpr double kTwoPi = 6.28318530717958647692;

std::vector<float> gerarSenoide(double hz, float amplitude, std::size_t frames) {
    std::vector<float> sinal(frames, 0.0f);
    for (std::size_t i = 0; i < frames; ++i) {
        sinal[i] = amplitude * static_cast<float>(std::sin(
                                   (kTwoPi * hz * static_cast<double>(i)) /
                                   static_cast<double>(mus::AudioEngine::kSampleRate)));
    }
    return sinal;
}

float picoSegundaMetade(const std::vector<float>& s) {
    float maior = 0.0f;
    for (std::size_t i = s.size() / 2u; i < s.size(); ++i) {
        maior = std::max(maior, std::fabs(s[i]));
    }
    return maior;
}

double energiaDiferencaPrimeiraOrdem(const float* dados, std::size_t n) {
    double soma = 0.0;
    for (std::size_t i = 1; i < n; ++i) {
        const double d = static_cast<double>(dados[i]) - static_cast<double>(dados[i - 1]);
        soma += d * d;
    }
    return soma;
}

std::int16_t lerPcm16LE(const std::string& bytes, std::size_t offset) {
    const auto b0 = static_cast<unsigned char>(bytes[offset]);
    const auto b1 = static_cast<unsigned char>(bytes[offset + 1]);
    const auto u = static_cast<std::uint16_t>(b0 | (b1 << 8u));
    return static_cast<std::int16_t>(u);
}

}  // namespace

// ---- Matemática do Filtro BiQuad (Audio EQ Cookbook RBJ) ------------------

TEST_CASE("dsp-11: BiquadFilter em bypass devolve o sinal intacto bit a bit") {
    mus::BiquadFilter filtro;
    CHECK_FALSE(filtro.isEnabled());

    const std::vector<float> entrada = gerarSenoide(1000.0, 0.8f, 512);
    std::vector<float> saida = entrada;
    filtro.processBuffer(saida.data(), static_cast<std::uint32_t>(saida.size()));

    for (std::size_t i = 0; i < entrada.size(); ++i) {
        CHECK(saida[i] == entrada[i]);
    }

    // Configurar corte <= 0 ou NaN desliga de volta para bypass.
    filtro.configureLowPass(mus::AudioEngine::kSampleRate, 800.0f);
    CHECK(filtro.isEnabled());
    filtro.configureLowPass(mus::AudioEngine::kSampleRate, 0.0f);
    CHECK_FALSE(filtro.isEnabled());
    filtro.configureLowPass(mus::AudioEngine::kSampleRate,
                            std::numeric_limits<float>::quiet_NaN());
    CHECK_FALSE(filtro.isEnabled());
}

TEST_CASE("dsp-11: resposta em frequencia RBJ Butterworth tem ganho DC 1, -3 dB no corte e -12 dB/oitava") {
    constexpr std::uint32_t kFs = mus::AudioEngine::kSampleRate;

    // 1. Ganho DC (0 Hz): degrau unitário converge para 1.0 exato.
    {
        mus::BiquadFilter lpf;
        lpf.configureLowPass(kFs, 500.0f);
        float y = 0.0f;
        for (int i = 0; i < 4096; ++i) {
            y = lpf.processSample(1.0f);
        }
        CHECK(y == doctest::Approx(1.0f).epsilon(1e-4));
    }

    // 2. Ponto de meia potência de Butterworth (Q = 1/sqrt(2)): em f == fc, |H| = 1/sqrt(2) (~0.7071).
    {
        mus::BiquadFilter lpf;
        lpf.configureLowPass(kFs, 1000.0f, mus::kBiquadDefaultQ);
        std::vector<float> tomNoCorte = gerarSenoide(1000.0, 1.0f, 4096);
        lpf.processBuffer(tomNoCorte.data(), static_cast<std::uint32_t>(tomNoCorte.size()));
        CHECK(picoSegundaMetade(tomNoCorte) == doctest::Approx(0.70710678f).epsilon(0.01));
    }

    // 3. Banda passante vs. banda de rejeição (fc = 400 Hz): 80 Hz passa íntegro;
    // 3200 Hz (3 oitavas acima -> ~ -36 dB) é fortemente atenuado.
    {
        mus::BiquadFilter lpfGrave;
        lpfGrave.configureLowPass(kFs, 400.0f);
        std::vector<float> grave = gerarSenoide(80.0, 1.0f, 4096);
        lpfGrave.processBuffer(grave.data(), static_cast<std::uint32_t>(grave.size()));
        CHECK(picoSegundaMetade(grave) > 0.98f);

        mus::BiquadFilter lpfAgudo;
        lpfAgudo.configureLowPass(kFs, 400.0f);
        std::vector<float> agudo = gerarSenoide(3200.0, 1.0f, 4096);
        lpfAgudo.processBuffer(agudo.data(), static_cast<std::uint32_t>(agudo.size()));
        CHECK(picoSegundaMetade(agudo) < 0.025f);
    }
}

TEST_CASE("dsp-11: Q ressonante amplifica no corte e o Master Bus protege contra clipping") {
    mus::BiquadFilter ressonante;
    ressonante.configureLowPass(mus::AudioEngine::kSampleRate, 1000.0f, 4.0f);

    std::vector<float> onda = gerarSenoide(1000.0, 0.5f, 4096);
    ressonante.processBuffer(onda.data(), static_cast<std::uint32_t>(onda.size()));
    CHECK(picoSegundaMetade(onda) > 1.5f);

    // No AudioEngine, o filtro roda antes do LimitadorDePico + softClip: nada passa de ±1.0.
    mus::AudioEngine engine;
    engine.setToneFrequency(1000.0);
    engine.setGain(0.6f);
    engine.setLowPassCutoff(1000.0f);
    engine.setLowPassQ(6.0f);

    std::vector<float> saida(4096, 0.0f);
    engine.render(saida.data(), static_cast<std::uint32_t>(saida.size()));
    for (float s : saida) {
        CHECK(std::isfinite(s));
        CHECK(s >= -1.0f);
        CHECK(s <= 1.0f);
    }
}

TEST_CASE("dsp-11: amostra nao finita reseta o estado do BiQuad sem contaminar o fluxo") {
    mus::BiquadFilter lpf;
    lpf.configureLowPass(mus::AudioEngine::kSampleRate, 800.0f);

    CHECK(lpf.processSample(std::numeric_limits<float>::quiet_NaN()) == 0.0f);
    const float recuperada = lpf.processSample(0.5f);
    CHECK(std::isfinite(recuperada));
    CHECK(recuperada > 0.0f);
}

// ---- Pipeline do compilador: comando `filtro <expr>` ----------------------

TEST_CASE("dsp-11: comando filtro atravessa lexer, AST, semantica, dobra e interpretador") {
    const std::vector<mus::Token> tokens = mus::tokenize("filtro 400 * 2");
    REQUIRE(tokens.size() == 4);
    CHECK(tokens[0].tipo == mus::TipoToken::Filtro);
    CHECK(std::string(mus::nomeToken(tokens[0].tipo)) == "FILTRO");

    std::string erro;
    auto arvore = mus::parseToAst("filtro 400 * 2\ntoca la5 por 1\nfiltro 0\ntoca la5 por 1\n", &erro);
    REQUIRE_MESSAGE(arvore != nullptr, erro);

    mus::semantica::AnalisadorSemantico semantica;
    REQUIRE(semantica.analisar(*arvore));

    const auto dobra = mus::otimizacao::dobrarConstantes(*arvore);
    CHECK(dobra.expressoesDobradas == 1);

    mus::Interpretador interp;
    REQUIRE_MESSAGE(interp.executar(*arvore), interp.erro());
    const mus::Timeline& tl = interp.timeline();
    REQUIRE(tl.size() == 2);
    CHECK(tl.events()[0].cutoffHz == doctest::Approx(800.0f));
    CHECK(tl.events()[1].cutoffHz == 0.0f);
}

TEST_CASE("dsp-11: semantica e interpretador recusam valores invalidos em filtro") {
    // Erros semânticos de tipo
    for (const char* fonte : {"filtro \"PERC_09\"\n", "filtro verdadeiro\n", "filtro do4\n"}) {
        auto arvore = mus::parseToAst(fonte);
        REQUIRE(arvore != nullptr);
        mus::semantica::AnalisadorSemantico semantica;
        CHECK_FALSE(semantica.analisar(*arvore));
    }

    // Erros de execução por faixa inválida (negativo, entre 0 e 20, > 20000, inf, NaN)
    for (const char* fonte : {"filtro -50\n", "filtro 10\n", "filtro 20001\n",
                              "filtro 1 / 0\n", "filtro 0 / 0\n"}) {
        auto arvore = mus::parseToAst(fonte);
        REQUIRE(arvore != nullptr);
        mus::semantica::AnalisadorSemantico semantica;
        REQUIRE(semantica.analisar(*arvore));
        mus::Interpretador interp;
        CAPTURE(fonte);
        CHECK_FALSE(interp.executar(*arvore));
        CHECK_FALSE(interp.erro().empty());
    }
}

TEST_CASE("dsp-11: filtro fica isolado por trilha em grupos paralelos { , }") {
    const std::string fonte =
        "andamento 120\n"
        "filtro 1200\n"
        "{\n"
        "    filtro 300\n"
        "    toca la5 por 1,\n"
        "    toca mi5 por 1\n"
        "}\n"
        "toca do5 por 1\n";

    auto arvore = mus::parseToAst(fonte);
    REQUIRE(arvore != nullptr);
    mus::semantica::AnalisadorSemantico semantica;
    REQUIRE(semantica.analisar(*arvore));

    mus::Interpretador interp;
    REQUIRE_MESSAGE(interp.executar(*arvore), interp.erro());
    const mus::Timeline& tl = interp.timeline();
    REQUIRE(tl.size() == 3);

    // Trilha 1 usou 300 Hz; Trilha 2 herdou 1200 Hz da entrada; após o `}`, voltou para 1200 Hz!
    CHECK(tl.events()[0].cutoffHz == doctest::Approx(300.0f));
    CHECK(tl.events()[1].cutoffHz == doctest::Approx(1200.0f));
    CHECK(tl.events()[2].cutoffHz == doctest::Approx(1200.0f));
}

TEST_CASE("dsp-11: varredura dinamica de filtro (sweep) modula timbre de sample WAV sem quebras ritmicas") {
    mus::SampleBank bank;
    const std::string caminho = std::string(MUS_SOURCE_DIR) + "/samples/PERC_09.wav";
    REQUIRE(bank.load("PERC_09", caminho).has_value());

    // Abre progressivamente o corte de 250 Hz até 6750 Hz em 4 batidas de 0.25 s (240 BPM)
    const std::string fonte =
        "andamento 240\n"
        "corte = 250\n"
        "repita 4 vezes {\n"
        "    filtro corte\n"
        "    toca \"PERC_09\" por 1\n"
        "    corte = corte * 3\n"
        "}\n";

    auto arvore = mus::parseToAst(fonte);
    REQUIRE(arvore != nullptr);
    mus::semantica::AnalisadorSemantico semantica;
    REQUIRE(semantica.analisar(*arvore));

    mus::Interpretador interp;
    interp.setSampleBank(&bank);
    REQUIRE_MESSAGE(interp.executar(*arvore), interp.erro());

    const mus::Timeline& tl = interp.timeline();
    REQUIRE(tl.size() == 4);
    CHECK(tl.duration() == doctest::Approx(1.0));

    mus::SpscRingBuffer<mus::SoundEvent> fila(tl.size());
    REQUIRE(mus::carregarFila(tl, fila));

    mus::AudioEngine engine;
    engine.setSampleBank(&bank);
    engine.setRingBuffer(&fila);
    engine.setExpectedDuration(tl.duration());

    const std::size_t framesPorBatida = mus::AudioEngine::kSampleRate / 4u;
    std::vector<float> audio(framesPorBatida * 4u, 0.0f);
    engine.render(audio.data(), static_cast<std::uint32_t>(audio.size()));

    // NFR: nenhum underrun e contagem exata de frames.
    CHECK_FALSE(engine.checkAndClearUnderrun());
    CHECK(engine.framesRenderizados() == audio.size());

    // Energia de alta frequência (diferença de 1ª ordem) cresce conforme o filtro abre!
    const double hf0 = energiaDiferencaPrimeiraOrdem(audio.data() + 0u * framesPorBatida, framesPorBatida);
    const double hf1 = energiaDiferencaPrimeiraOrdem(audio.data() + 1u * framesPorBatida, framesPorBatida);
    const double hf2 = energiaDiferencaPrimeiraOrdem(audio.data() + 2u * framesPorBatida, framesPorBatida);
    const double hf3 = energiaDiferencaPrimeiraOrdem(audio.data() + 3u * framesPorBatida, framesPorBatida);

    CHECK(hf0 > 0.0);
    CHECK(hf0 < hf1);
    CHECK(hf1 < hf2);
    CHECK(hf2 < hf3);
}

TEST_CASE("dsp-11: geracao WAV com lpfCutoffHz atenua frequencias acima do corte") {
    mus::Timeline tl;
    // Nota aguda lá 6 (1760 Hz) por 0.2 s
    tl.add(mus::SoundEvent{0.0, 0.2, 1760.0, 0.7f});

    std::ostringstream wavAberto;
    std::ostringstream wavFiltrado;
    REQUIRE(mus::codegen::gerarWav(tl, wavAberto, nullptr, 1.0f, 0.0f).ok);
    REQUIRE(mus::codegen::gerarWav(tl, wavFiltrado, nullptr, 1.0f, 220.0f).ok);

    const std::string bAberto = wavAberto.str();
    const std::string bFiltrado = wavFiltrado.str();
    REQUIRE(bAberto.size() == bFiltrado.size());

    // Mede o pico na metade final (regime permanente) dos dois WAVs.
    const std::size_t meio = mus::codegen::kWavHeaderSize +
                             ((bAberto.size() - mus::codegen::kWavHeaderSize) / 4u) * 2u;
    int picoAberto = 0;
    int picoFiltrado = 0;
    for (std::size_t off = meio; off + 1 < bAberto.size(); off += 2) {
        picoAberto = std::max(picoAberto, std::abs(static_cast<int>(lerPcm16LE(bAberto, off))));
        picoFiltrado = std::max(picoFiltrado, std::abs(static_cast<int>(lerPcm16LE(bFiltrado, off))));
    }

    CHECK(picoAberto > 18000);
    CHECK(picoFiltrado < picoAberto / 10);
}

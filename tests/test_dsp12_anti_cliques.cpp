#include "doctest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "audio/audio_engine.h"
#include "audio/parameter_smoother.h"
#include "audio/sample_bank.h"
#include "codegen/gerador_wav.h"
#include "core/frames.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"
#include "core/timeline_queue.h"
#include "frontend/frontend.h"
#include "interpreter/interpreter.h"
#include "semantic/analisador_semantico.h"

namespace {

constexpr double kTwoPi = 6.28318530717958647692;

float picoAbsoluto(const float* dados, std::size_t n) {
    float maior = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        maior = std::max(maior, std::fabs(dados[i]));
    }
    return maior;
}

float picoAbsoluto(const std::vector<float>& v) {
    return picoAbsoluto(v.data(), v.size());
}

float maiorSaltoAdjacente(const std::vector<float>& s,
                          std::size_t inicio = 1,
                          std::size_t fim = 0) {
    const std::size_t limite = (fim == 0 || fim > s.size()) ? s.size() : fim;
    float maior = 0.0f;
    for (std::size_t i = std::max<std::size_t>(1, inicio); i < limite; ++i) {
        maior = std::max(maior, std::fabs(s[i] - s[i - 1]));
    }
    return maior;
}

// Energia acima de 4 kHz sobre a total em dB (passa-altas RBJ de 8ª ordem).
double energiaAcimaDe4kHzEmDb(const std::vector<float>& sinal) {
    const double w0 = kTwoPi * 4000.0 / mus::AudioEngine::kSampleRate;
    const double alfa = std::sin(w0) / (2.0 * 0.7071067811865476);
    const double c = std::cos(w0);
    const double a0 = 1.0 + alfa;
    const double b0 = (1.0 + c) / 2.0 / a0;
    const double b1 = -(1.0 + c) / a0;
    const double a1 = -2.0 * c / a0;
    const double a2 = (1.0 - alfa) / a0;

    double estado[4][4] = {};
    double agudo = 0.0;
    double total = 0.0;
    for (float amostra : sinal) {
        double y = amostra;
        for (auto& e : estado) {
            const double saida =
                b0 * y + b1 * e[0] + b0 * e[1] - a1 * e[2] - a2 * e[3];
            e[1] = e[0];
            e[0] = y;
            e[3] = e[2];
            e[2] = saida;
            y = saida;
        }
        agudo += y * y;
        total += static_cast<double>(amostra) * amostra;
    }
    return (total > 0.0 && agudo > 0.0) ? 10.0 * std::log10(agudo / total) : -999.0;
}

struct WavTemporarioTruncado {
    std::filesystem::path caminho;

    explicit WavTemporarioTruncado(const std::string& nome, std::uint32_t frames)
        : caminho(std::filesystem::temp_directory_path() / nome) {
        std::ofstream out(caminho, std::ios::binary);
        const auto put32 = [&](std::uint32_t v) {
            out.write(reinterpret_cast<const char*>(&v), 4);
        };
        const auto put16 = [&](std::uint16_t v) {
            out.write(reinterpret_cast<const char*>(&v), 2);
        };

        const std::uint32_t dataBytes = frames * 2u;
        out.write("RIFF", 4);
        put32(36u + dataBytes);
        out.write("WAVEfmt ", 8);
        put32(16u);
        put16(1u);
        put16(1u);
        put32(mus::AudioEngine::kSampleRate);
        put32(mus::AudioEngine::kSampleRate * 2u);
        put16(2u);
        put16(16u);
        out.write("data", 4);
        put32(dataBytes);

        // Onda constante em +0.90 nas últimas amostras para simular um WAV mal
        // cortado longe do cruzamento por zero (pior caso de clique de borda).
        for (std::uint32_t i = 0; i < frames; ++i) {
            const float env = (i < 64u) ? (static_cast<float>(i) / 64.0f) : 1.0f;
            const float s = 0.90f * env;
            const auto pcm = static_cast<std::int16_t>(std::lround(s * 32767.0f));
            put16(static_cast<std::uint16_t>(pcm));
        }
    }

    ~WavTemporarioTruncado() {
        std::error_code ec;
        std::filesystem::remove(caminho, ec);
    }
};

}  // namespace

// ---- 1. Matemática do One-Pole Parameter Smoother -------------------------

TEST_CASE("dsp-12: OnePoleSmoother converge exponencialmente sem overshoot nem subnormais") {
    constexpr std::uint32_t kFs = mus::AudioEngine::kSampleRate;
    mus::OnePoleSmoother smoother(kFs, 0.005, 0.0f, 1e-4f);
    CHECK_FALSE(smoother.isSmoothing());
    CHECK(smoother.current() == 0.0f);

    smoother.setTarget(1.0f);
    CHECK(smoother.isSmoothing());

    // Em 1 constante de tempo tau (0.005 * 44100 ~= 220.5 amostras), um filtro de
    // 1 polo atinge 1 - 1/e (~0.63212).
    float v = 0.0f;
    for (int i = 0; i < 220; ++i) {
        const float proximo = smoother.next();
        CHECK(proximo > v);
        CHECK(proximo <= 1.0f);
        v = proximo;
    }
    CHECK(v == doctest::Approx(1.0f - std::exp(-220.0 / 220.5)).epsilon(0.005));

    // Inversão de alvo no meio da subida: não tem salto nem overshoot.
    smoother.setTarget(0.2f);
    const float logoAposInversao = smoother.next();
    CHECK(logoAposInversao < v);
    CHECK((v - logoAposInversao) < 0.01f);

    // Avanço fechado `advance(k)` coincide com `k` passos individuais.
    mus::OnePoleSmoother passoAPasso(kFs, 0.005, 1000.0f, 0.05f);
    mus::OnePoleSmoother fechado(kFs, 0.005, 1000.0f, 0.05f);
    passoAPasso.setTarget(200.0f);
    fechado.setTarget(200.0f);
    for (int i = 0; i < 150; ++i) {
        passoAPasso.next();
    }
    fechado.advance(150);
    CHECK(fechado.current() == doctest::Approx(passoAPasso.current()).epsilon(1e-3));

    // Convergência exata crava o alvo e desliga `isSmoothing()`.
    fechado.advance(4000);
    CHECK_FALSE(fechado.isSmoothing());
    CHECK(fechado.current() == 200.0f);
}

// ---- 2. Auditoria de Envelopes (Attack / Release sem drift de float32) ----

TEST_CASE("dsp-12: envelope indexado por inteiros encerra exatamente no frame agendado em todas as duracoes") {
    constexpr std::uint32_t kFs = mus::AudioEngine::kSampleRate;

    // Antes da DSP-12, subtrair `1.0f / R` em float32 encerrava a voz 1 frame antes
    // em 96 das 220 durações de release (ex.: R = 1, 2, 3, 4, 6, 8, 10, 20, 110...)
    // e deixava Attack de 220 frames terminar apenas no frame 221.
    for (std::uint32_t totalFrames :
         {1u, 2u, 3u, 4u, 6u, 8u, 12u, 20u, 40u, 88u, 220u, 438u, 440u, 600u}) {
        CAPTURE(totalFrames);

        mus::AudioEngine engine;
        mus::SpscRingBuffer<mus::SoundEvent> fila(4);
        const double duracao = static_cast<double>(totalFrames) / static_cast<double>(kFs);
        // 5512.5 Hz (fs / 8, passo π/4): começando em π/2 em notas de 2 frames,
        // produz sin(π/2) = 1.0 no frame 0 e sin(3π/4) = 0.7071 no frame 1.
        REQUIRE(fila.push(mus::SoundEvent{0.0, duracao, 5512.5, 0.8f}));
        engine.setRingBuffer(&fila);

        std::uint64_t frameEmQueFicouIdle = 0;
        bool jaAtivou = false;
        std::vector<float> amostras(totalFrames + 16u, 0.0f);

        for (std::uint32_t f = 0; f < amostras.size(); ++f) {
            engine.renderRealTime(&amostras[f], 1);
            const auto estado = engine.voices()[0].envState;
            if (f == 0) {
                REQUIRE(estado != mus::AudioEngine::EnvState::Idle);
                jaAtivou = true;
            } else if (jaAtivou && frameEmQueFicouIdle == 0 &&
                       estado == mus::AudioEngine::EnvState::Idle) {
                frameEmQueFicouIdle = f;
            }

            // Em notas >= 440 frames (attackFrames = 220), o frame 219 (220º frame)
            // já deve atingir Sustain com envLevel == 1.0f exato.
            if (totalFrames >= 440u && f == 219u) {
                CHECK(estado == mus::AudioEngine::EnvState::Sustain);
                CHECK(engine.voices()[0].envLevel == 1.0f);
            }
        }

        CHECK(frameEmQueFicouIdle == totalFrames);

        // Uma nota de 2 frames deve soar nos 2 frames (antes o 2º frame saía 0.0f).
        if (totalFrames == 2u) {
            CHECK(std::fabs(amostras[0]) > 0.5f);
            CHECK(std::fabs(amostras[1]) > 0.2f);
        }

        // Silêncio absoluto a partir de `totalFrames`.
        for (std::uint32_t f = totalFrames; f < amostras.size(); ++f) {
            CHECK(amostras[f] == 0.0f);
        }
    }
}

TEST_CASE("dsp-12: notas coladas em pool de 1 voz e eventos de 0 frames nao geram cauda fantasma nem roubo") {
    constexpr std::uint32_t kFs = mus::AudioEngine::kSampleRate;

    // 1. Nota de 1 frame seguida imediatamente de outra em `maxVoices == 1`:
    // como a primeira já terminou no frame 0, não deve abrir fadeOut de 88 frames.
    {
        mus::AudioEngine engine;
        REQUIRE(engine.setMaxVoices(1));
        mus::SpscRingBuffer<mus::SoundEvent> fila(4);
        const double umFrame = 1.0 / static_cast<double>(kFs);
        REQUIRE(fila.push(mus::SoundEvent{0.0, umFrame, 440.0, 0.9f}));
        REQUIRE(fila.push(mus::SoundEvent{umFrame, 0.02, 880.0, 0.5f}));
        engine.setRingBuffer(&fila);

        float s0 = 0.0f;
        float s1 = 0.0f;
        engine.renderRealTime(&s0, 1);
        engine.renderRealTime(&s1, 1);
        CHECK(engine.voices()[0].fadeOutFramesRemaining == 0u);
    }

    // 2. Evento de 0 frames (`toca do4 por 0`) sobre pool cheio não rouba voz audível.
    {
        const std::vector<mus::SoundEvent> apenasAcorde = {
            mus::SoundEvent{0.0, 0.1, 261.63, 0.2f},
            mus::SoundEvent{0.0, 0.1, 329.63, 0.2f},
            mus::SoundEvent{0.0, 0.1, 392.00, 0.2f},
            mus::SoundEvent{0.0, 0.1, 523.25, 0.2f},
        };
        std::vector<mus::SoundEvent> comNotaZero = apenasAcorde;
        comNotaZero.push_back(mus::SoundEvent{0.02, 0.0, 1000.0, 0.9f});

        mus::AudioEngine engRef;
        REQUIRE(engRef.setMaxVoices(4));
        mus::SpscRingBuffer<mus::SoundEvent> filaRef(8);
        for (const auto& ev : apenasAcorde) {
            REQUIRE(filaRef.push(ev));
        }
        engRef.setRingBuffer(&filaRef);

        mus::AudioEngine engZero;
        REQUIRE(engZero.setMaxVoices(4));
        mus::SpscRingBuffer<mus::SoundEvent> filaZero(8);
        for (const auto& ev : comNotaZero) {
            REQUIRE(filaZero.push(ev));
        }
        engZero.setRingBuffer(&filaZero);

        std::vector<float> ref(2048, 0.0f);
        std::vector<float> obtido(2048, 0.0f);
        engRef.renderRealTime(ref.data(), 2048);
        engZero.renderRealTime(obtido.data(), 2048);

        CHECK(obtido == ref);
    }
}

TEST_CASE("dsp-12: sample WAV truncado em amplitude alta recebe release anti-clique antes do fim fisico") {
    constexpr std::uint32_t kSampleFrames = 2000u;
    WavTemporarioTruncado temp("dsp12_sample_truncado.wav", kSampleFrames);

    mus::SampleBank bank;
    const auto id = bank.load("TRUNCADO", temp.caminho.string());
    REQUIRE(id.has_value());

    mus::AudioEngine engine;
    mus::SpscRingBuffer<mus::SoundEvent> fila(4);
    engine.setSampleBank(&bank);
    engine.setRingBuffer(&fila);

    // Pede duração musical (0.2 s = 8820 frames) muito maior que o WAV (2000 frames).
    // Sem limitar o envelope ao tamanho físico do sample, o sinal caía de +0.90 a 0.00
    // num único frame ao bater em `frameCount` (clique de ~0.90).
    mus::SoundEvent ev{};
    ev.startTime = 0.0;
    ev.duration = 0.2;
    ev.volume = 0.8f;
    ev.type = mus::SoundEventType::Sample;
    ev.sampleId = *id;
    REQUIRE(fila.push(ev));

    std::vector<float> out(3000, 0.0f);
    engine.renderRealTime(out.data(), static_cast<std::uint32_t>(out.size()));

    CHECK(picoAbsoluto(out) > 0.6f);
    // Na região final do WAV (frames 1700..2010), a rampa de release de 5 ms (220 frames)
    // reduz o degrau máximo por amostra de ~0.72 para ~0.004!
    CHECK(maiorSaltoAdjacente(out, 1700, 2020) < 0.02f);

    for (std::size_t i = kSampleFrames + 1u; i < out.size(); ++i) {
        CHECK(out[i] == 0.0f);
    }
}

TEST_CASE("dsp-12: roubo duplo de voz em menos de 2 ms preserva a cauda mais forte sem clique") {
    mus::AudioEngine engine;
    REQUIRE(engine.setMaxVoices(1));
    mus::SpscRingBuffer<mus::SoundEvent> fila(8);
    engine.setRingBuffer(&fila);

    // Voz 0 em 440 Hz sobe até Sustain; no pico (~10.57 ms, sin ~= +1.0) sofre
    // dois roubos consecutivos separados por apenas 5 frames (~0.11 ms < 2 ms).
    const double tRoubo1 = 0.010 + 1.0 / (4.0 * 440.0);
    const double tRoubo2 = tRoubo1 + 5.0 / mus::AudioEngine::kSampleRate;

    REQUIRE(fila.push(mus::SoundEvent{0.0, 0.2, 440.0, 0.75f}));
    REQUIRE(fila.push(mus::SoundEvent{tRoubo1, 0.2, 554.37, 0.75f}));
    REQUIRE(fila.push(mus::SoundEvent{tRoubo2, 0.2, 659.25, 0.75f}));

    std::vector<float> saida(1024, 0.0f);
    engine.render(saida.data(), static_cast<std::uint32_t>(saida.size()));

    // Sem proteger a cauda mais forte, `tRoubo2` sobrescrevia o fadeOut (~0.70)
    // com o ataque recém-nascido (~0.02), gerando salto > 0.60 num único frame.
    CHECK(maiorSaltoAdjacente(saida) < 0.18f);
}

// ---- 3. Suavização One-Pole de Automações (Filtro LPF e Ganho de Teste) ---

TEST_CASE("dsp-12: mudanca abrupta de Filtro LPF (corte, Q e bypass) nao produz zipper noise nem clique") {
    SUBCASE("salto abrupto entre dois cortes ativos (15 kHz -> 200 Hz -> 12 kHz)") {
        // Nota musical de 880 Hz com envelope normal (evita quina de partida em t=0).
        // Sem OnePoleSmoother, saltar de 200 Hz para 12 kHz em DF2T disparava um pico
        // transitório de ~5.78 nos estados z1_/z2_ e jogava o agudo (> 4 kHz) a -42.3 dB;
        // com suavização de 1 polo, o agudo cai para ~ -83.9 dB (< -75 dB).
        mus::SpscRingBuffer<mus::SoundEvent> fila(4);
        const double duracao = (32.0 * mus::AudioEngine::kPeriodSizeInFrames) /
                               static_cast<double>(mus::AudioEngine::kSampleRate);
        REQUIRE(fila.push(mus::SoundEvent{0.0, duracao, 880.0, 0.6f}));

        mus::AudioEngine engine;
        engine.setRingBuffer(&fila);
        engine.setLowPassCutoff(15000.0f);

        std::vector<float> total;
        std::vector<float> bloco(mus::AudioEngine::kPeriodSizeInFrames, 0.0f);

        // 8 blocos em 15 kHz
        for (int b = 0; b < 8; ++b) {
            engine.render(bloco.data(), mus::AudioEngine::kPeriodSizeInFrames);
            total.insert(total.end(), bloco.begin(), bloco.end());
        }
        // Fecha abruptamente para 200 Hz
        engine.setLowPassCutoff(200.0f);
        for (int b = 0; b < 12; ++b) {
            engine.render(bloco.data(), mus::AudioEngine::kPeriodSizeInFrames);
            total.insert(total.end(), bloco.begin(), bloco.end());
        }
        // Abre abruptamente de volta para 12 kHz
        engine.setLowPassCutoff(12000.0f);
        for (int b = 0; b < 12; ++b) {
            engine.render(bloco.data(), mus::AudioEngine::kPeriodSizeInFrames);
            total.insert(total.end(), bloco.begin(), bloco.end());
        }

        // Derivada natural máxima de 0.6 * sin(2π*880*t) é ~0.075/frame.
        CHECK(maiorSaltoAdjacente(total) < 0.10f);
        CHECK(energiaAcimaDe4kHzEmDb(total) < -75.0);

        // Também valida automação abrupta de Q ressonante (0.707 -> 3.0 -> 0.707) sem clipping.
        mus::AudioEngine engQ;
        engQ.setToneFrequency(880.0);
        engQ.setGain(0.5f);
        engQ.setLowPassCutoff(900.0f);
        engQ.render(bloco.data(), mus::AudioEngine::kPeriodSizeInFrames);
        engQ.setLowPassQ(3.0f);
        for (int b = 0; b < 8; ++b) {
            engQ.render(bloco.data(), mus::AudioEngine::kPeriodSizeInFrames);
            for (float s : bloco) {
                CHECK(std::isfinite(s));
                CHECK(s >= -1.0f);
                CHECK(s <= 1.0f);
            }
        }
    }

    SUBCASE("ativacao e desativacao dinamica do LPF (bypass 0 Hz <-> 220 Hz) no pico da onda") {
        mus::AudioEngine engine;
        engine.setToneFrequency(1200.0);
        engine.setGain(0.7f);
        engine.setLowPassCutoff(0.0f);

        // Avança 9 frames: 1200 Hz a 44.1 kHz chega perto do pico positivo (~0.67).
        std::vector<float> antes(9, 0.0f);
        engine.render(antes.data(), static_cast<std::uint32_t>(antes.size()));
        REQUIRE(antes.back() > 0.60f);

        // Liga abruptamente LPF em 220 Hz (que atenua 1200 Hz quase a zero).
        // Sem crossfade one-pole, a amostra seguinte despencava de +0.67 para ~0.00!
        engine.setLowPassCutoff(220.0f);
        std::vector<float> fechando(2048, 0.0f);
        engine.render(fechando.data(), static_cast<std::uint32_t>(fechando.size()));

        const float saltoLiga = std::fabs(fechando.front() - antes.back());
        CHECK(saltoLiga < 0.16f);
        CHECK(maiorSaltoAdjacente(fechando) < 0.16f);
        // No fim do bloco de 2048 frames (~46 ms), o filtro já convergiu e atenuou 1200 Hz.
        CHECK(picoAbsoluto(fechando.data() + 1500, 548) < 0.05f);

        // Agora desliga abruptamente de volta para bypass (`0 Hz`).
        engine.setLowPassCutoff(0.0f);
        std::vector<float> abrindo(2048, 0.0f);
        engine.render(abrindo.data(), static_cast<std::uint32_t>(abrindo.size()));

        const float saltoDesliga = std::fabs(abrindo.front() - fechando.back());
        CHECK(saltoDesliga < 0.16f);
        CHECK(maiorSaltoAdjacente(abrindo) < 0.16f);
        CHECK(picoAbsoluto(abrindo.data() + 1500, 548) == doctest::Approx(0.7f).epsilon(0.02));
    }

    SUBCASE("mudanca abrupta de setGain no modo --test-tone nao gera clique") {
        mus::AudioEngine engine;
        engine.setToneFrequency(440.0);
        engine.setGain(0.75f);

        std::vector<float> inicio(25, 0.0f);
        engine.render(inicio.data(), static_cast<std::uint32_t>(inicio.size()));
        REQUIRE(inicio.back() > 0.65f);

        engine.setGain(0.0f);
        std::vector<float> queda(2048, 0.0f);
        engine.render(queda.data(), static_cast<std::uint32_t>(queda.size()));

        CHECK(std::fabs(queda.front() - inicio.back()) < 0.07f);
        CHECK(maiorSaltoAdjacente(queda) < 0.07f);
        CHECK(picoAbsoluto(queda.data() + 1800, 248) < 1e-3f);
    }
}

// ---- 4. Bateria Final Fim a Fim (Transições Complexas .mus + WAV) ---------

TEST_CASE("dsp-12: atestado acustico fim a fim em peca complexa com trilhas, staccato, sweep LPF e samples") {
    mus::SampleBank bank;
    REQUIRE(bank.load("PERC_09", std::string(MUS_SOURCE_DIR) + "/samples/PERC_09.wav").has_value());
    REQUIRE(bank.load("BASS_1", std::string(MUS_SOURCE_DIR) + "/samples/BASS_1.wav").has_value());

    const std::string pecaComplexa =
        "andamento 180\n"
        "{\n"
        "    // Trilha 1: staccato rápido com automação de volume e filtro\n"
        "    vol = 0.25\n"
        "    corte = 350\n"
        "    repita 8 vezes {\n"
        "        volume vol\n"
        "        filtro corte\n"
        "        toca do5 por semifusa\n"
        "        toca mi5 por semifusa\n"
        "        vol = vol + 0.08\n"
        "        corte = corte + 600\n"
        "    },\n"
        "    // Trilha 2: acordes sustentados\n"
        "    volume 0.6\n"
        "    { toca do3 por 1, toca sol3 por 1, toca mi4 por 1 },\n"
        "    // Trilha 3: percussão com varredura de LPF\n"
        "    filtro 500\n"
        "    toca \"BASS_1\" por colcheia\n"
        "    filtro 4000\n"
        "    toca \"PERC_09\" por colcheia\n"
        "}\n"
        "pausa por colcheia\n";

    std::string erro;
    auto arvore = mus::parseToAst(pecaComplexa, &erro);
    REQUIRE_MESSAGE(arvore != nullptr, erro);

    mus::semantica::AnalisadorSemantico semantica;
    REQUIRE(semantica.analisar(*arvore));

    mus::Interpretador interp;
    interp.setSampleBank(&bank);
    REQUIRE_MESSAGE(interp.executar(*arvore), interp.erro());
    const mus::Timeline& tl = interp.timeline();
    REQUIRE(tl.size() > 15);

    // Renderiza em blocos de 256 frames (idêntico ao callback do miniaudio)
    mus::SpscRingBuffer<mus::SoundEvent> fila(tl.size());
    REQUIRE(mus::carregarFila(tl, fila));

    mus::AudioEngine engine;
    engine.setSampleBank(&bank);
    engine.setRingBuffer(&fila);
    engine.setExpectedDuration(tl.duration());

    const std::size_t totalFrames =
        mus::segundosParaFrame(tl.duration(), mus::AudioEngine::kSampleRate) + 512u;
    std::vector<float> mix;
    mix.reserve(totalFrames + 256u);
    std::vector<float> bloco(mus::AudioEngine::kPeriodSizeInFrames, 0.0f);

    while (mix.size() < totalFrames) {
        engine.render(bloco.data(), mus::AudioEngine::kPeriodSizeInFrames);
        mix.insert(mix.end(), bloco.begin(), bloco.end());
    }

    CHECK_FALSE(engine.checkAndClearUnderrun());
    CHECK(engine.activeVoiceCount() == 0);
    CHECK(picoAbsoluto(mix) > 0.3f);

    for (float s : mix) {
        CHECK(std::isfinite(s));
        CHECK(s >= -1.0f);
        CHECK(s <= 1.0f);
    }

    // Pausa final deve terminar em silêncio absoluto.
    const std::size_t inicioSilencioFinal =
        mus::segundosParaFrame(tl.duration() - 0.05, mus::AudioEngine::kSampleRate);
    CHECK(picoAbsoluto(mix.data() + inicioSilencioFinal,
                       mix.size() - inicioSilencioFinal) == 0.0f);

    // Determinismo bit a bit na exportação WAV offline com `--lpf` ativo.
    std::ostringstream wav1(std::ios::binary);
    std::ostringstream wav2(std::ios::binary);
    REQUIRE(mus::codegen::gerarWav(tl, wav1, &bank, 0.9f, 3500.0f).ok);
    REQUIRE(mus::codegen::gerarWav(tl, wav2, &bank, 0.9f, 3500.0f).ok);
    CHECK(wav1.str() == wav2.str());
}

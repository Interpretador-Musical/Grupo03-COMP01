#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "audio/audio_buffer.h"
#include "audio/audio_engine.h"
#include "audio/gain_smoother.h"
#include "audio/sample_bank.h"
#include "codegen/gerador_wav.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"
#include "core/timeline_queue.h"
#include "frontend/frontend.h"
#include "interpreter/interpreter.h"
#include "semantic/analisador_semantico.h"

namespace {

float picoModulo(const float* dados, std::size_t n) {
    float maior = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        maior = std::max(maior, std::fabs(dados[i]));
    }
    return maior;
}

float picoModulo(const std::vector<float>& buffer) {
    return picoModulo(buffer.data(), buffer.size());
}

float maiorSaltoAdjacente(const std::vector<float>& s) {
    float maior = 0.0f;
    for (std::size_t i = 1; i < s.size(); ++i) {
        maior = std::max(maior, std::fabs(s[i] - s[i - 1]));
    }
    return maior;
}

std::int16_t lerPcm16LE(const std::string& bytes, std::size_t offset) {
    const auto b0 = static_cast<unsigned char>(bytes[offset]);
    const auto b1 = static_cast<unsigned char>(bytes[offset + 1]);
    const auto u = static_cast<std::uint16_t>(b0 | (b1 << 8u));
    return static_cast<std::int16_t>(u);
}

}  // namespace

// ---- Suavizador linear e higienização (GainSmoother) ---------------------

TEST_CASE("dsp-09: sanitizeGain protege contra NaN, negativos e valores acima do teto") {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    CHECK(mus::sanitizeGain(nan, 1.0f) == 0.0f);
    CHECK(mus::sanitizeGain(-inf, 1.0f) == 0.0f);
    CHECK(mus::sanitizeGain(-0.5f, 1.0f) == 0.0f);
    CHECK(mus::sanitizeGain(0.0f, 1.0f) == 0.0f);
    CHECK(mus::sanitizeGain(0.65f, 1.0f) == doctest::Approx(0.65f));
    CHECK(mus::sanitizeGain(1.5f, 1.0f) == 1.0f);
    CHECK(mus::sanitizeGain(inf, 4.0f) == 4.0f);
}

TEST_CASE("dsp-09: GainSmoother interpola linearmente em 5 ms e crava o alvo exato") {
    mus::GainSmoother smoother(mus::AudioEngine::kSampleRate, 1.0f);
    const std::uint32_t rampFrames = smoother.rampFrames();
    CHECK(rampFrames == 220u);
    CHECK_FALSE(smoother.isRamping());

    smoother.setTarget(0.0f);
    CHECK(smoother.isRamping());

    float anterior = 1.0f;
    for (std::uint32_t i = 0; i < rampFrames; ++i) {
        const float atual = smoother.next();
        CHECK(atual < anterior);
        CHECK(atual >= 0.0f);
        CHECK((anterior - atual) == doctest::Approx(1.0f / static_cast<float>(rampFrames)).epsilon(1e-4));
        anterior = atual;
    }

    // No fim da rampa o valor precisa ser zero exato, sem resíduo de float.
    CHECK_FALSE(smoother.isRamping());
    CHECK(smoother.current() == 0.0f);
    CHECK(smoother.next() == 0.0f);
}

TEST_CASE("dsp-09: mudanca de alvo no meio da rampa parte do valor instantaneo sem salto") {
    mus::GainSmoother smoother(mus::AudioEngine::kSampleRate, 1.0f);
    smoother.setTarget(0.0f);

    // Avança metade da rampa (~0.5f) e inverte a direção para 0.8f.
    for (std::uint32_t i = 0; i < smoother.rampFrames() / 2u; ++i) {
        smoother.next();
    }
    const float noMeio = smoother.current();
    CHECK(noMeio == doctest::Approx(0.5f).epsilon(0.02));

    smoother.setTarget(0.8f);
    const float logoDepois = smoother.next();
    CHECK(logoDepois > noMeio);
    CHECK((logoDepois - noMeio) < 0.01f);
}

TEST_CASE("dsp-09: advance salta multiplos frames em O(1) preservando a mesma trajetoria") {
    mus::GainSmoother passoAPasso(mus::AudioEngine::kSampleRate, 1.0f);
    mus::GainSmoother emSalto(mus::AudioEngine::kSampleRate, 1.0f);

    passoAPasso.setTarget(0.2f);
    emSalto.setTarget(0.2f);

    for (int i = 0; i < 100; ++i) {
        passoAPasso.next();
    }
    emSalto.advance(100);
    CHECK(emSalto.current() == doctest::Approx(passoAPasso.current()).epsilon(1e-5));

    emSalto.advance(500);
    CHECK_FALSE(emSalto.isRamping());
    CHECK(emSalto.current() == doctest::Approx(0.2f));
}

// ---- Master Bus no AudioEngine -------------------------------------------

TEST_CASE("dsp-09: Volume Master escala os tres modos do master bus e zera em 0.0") {
    // 1. Modo senoide (--test-tone)
    {
        mus::AudioEngine engine;
        engine.setGain(0.6f);
        engine.setMasterVolume(0.5f);

        std::vector<float> saida(2048, 0.0f);
        engine.render(saida.data(), static_cast<std::uint32_t>(saida.size()));
        CHECK(picoModulo(saida) == doctest::Approx(0.3f).epsilon(0.002));
    }

    // 2. Modo TimelinePlayer
    {
        mus::Timeline timeline;
        timeline.add(mus::SoundEvent{0.0, 0.1, 440.0, 0.6f});

        mus::AudioEngine engine;
        engine.loadTimeline(timeline);
        engine.setMasterVolume(0.5f);

        std::vector<float> saida(2048, 0.0f);
        engine.render(saida.data(), static_cast<std::uint32_t>(saida.size()));
        CHECK(picoModulo(saida) == doctest::Approx(0.3f).epsilon(0.002));
    }

    // 3. Modo RingBuffer polifônico: acorde cheio com master 0.25 nunca passa de 0.25,
    // e master 0.0 produz silêncio absoluto.
    {
        mus::SpscRingBuffer<mus::SoundEvent> fila(8);
        for (double hz : {261.63, 329.63, 392.00, 523.25}) {
            REQUIRE(fila.push(mus::SoundEvent{0.0, 0.2, hz, 1.0f}));
        }

        mus::AudioEngine engine;
        engine.setRingBuffer(&fila);
        engine.setMasterVolume(0.25f);

        std::vector<float> saida(4096, 0.0f);
        engine.render(saida.data(), static_cast<std::uint32_t>(saida.size()));
        CHECK(picoModulo(saida) > 0.15f);
        CHECK(picoModulo(saida) <= 0.25f);

        mus::AudioEngine mudo;
        mus::SpscRingBuffer<mus::SoundEvent> filaMuda(4);
        REQUIRE(filaMuda.push(mus::SoundEvent{0.0, 0.2, 440.0, 1.0f}));
        mudo.setRingBuffer(&filaMuda);
        mudo.setMasterVolume(0.0f);

        std::vector<float> silencio(2048, 1.0f);
        mudo.render(silencio.data(), static_cast<std::uint32_t>(silencio.size()));
        CHECK(picoModulo(silencio) == 0.0f);
    }
}

TEST_CASE("dsp-09: NFR de suavizacao elimina clique em mudanca abrupta do Volume Master") {
    mus::AudioEngine engine;
    engine.setToneFrequency(440.0);
    engine.setGain(0.7f);
    engine.setMasterVolume(1.0f);

    // Avança até perto do pico positivo da senoide de 440 Hz (~1/4 de ciclo = ~25 frames),
    // onde um corte seco de 1.0 -> 0.0 causaria um degrau vertical de ~0.7!
    std::vector<float> blocoInicial(25, 0.0f);
    engine.render(blocoInicial.data(), static_cast<std::uint32_t>(blocoInicial.size()));
    REQUIRE(std::fabs(blocoInicial.back()) > 0.6f);

    // Muda o volume master de 1.0 para 0.0 em plena oscilação.
    engine.setMasterVolume(0.0f);

    std::vector<float> blocoTransicao(512, 0.0f);
    engine.render(blocoTransicao.data(), static_cast<std::uint32_t>(blocoTransicao.size()));

    // Borda entre blocos e interior da rampa: derivada máxima de 0.7 * sin(2π*440*t)
    // a 44,1 kHz é ~0.044 por amostra. Sem suavização, o salto na borda passaria de 0.6.
    const float saltoNaBorda = std::fabs(blocoTransicao.front() - blocoInicial.back());
    CHECK(saltoNaBorda < 0.06f);
    CHECK(maiorSaltoAdjacente(blocoTransicao) < 0.06f);

    // Após os 220 frames (~5 ms) de rampa, o sinal deve estar completamente mudo.
    for (std::size_t i = 225; i < blocoTransicao.size(); ++i) {
        CHECK(blocoTransicao[i] == 0.0f);
    }
}

// ---- Proporção Percussão (Sample) vs. Síntese (Synth) --------------------

TEST_CASE("dsp-09: ganhos de subgrupo controlam proporcao independente entre Sample e Synth") {
    mus::SampleBank bank;
    const std::string caminho = std::string(MUS_SOURCE_DIR) + "/samples/PERC_09.wav";
    const auto sampleId = bank.load("PERC_09", caminho);
    REQUIRE(sampleId.has_value());

    mus::SoundEvent evSynth{};
    evSynth.startTime = 0.0;
    evSynth.duration = 0.1;
    evSynth.frequency = 440.0;
    evSynth.volume = 0.4f;
    evSynth.type = mus::SoundEventType::Synth;

    mus::SoundEvent evSample{};
    evSample.startTime = 0.0;
    evSample.duration = 0.1;
    evSample.volume = 0.4f;
    evSample.type = mus::SoundEventType::Sample;
    evSample.sampleId = *sampleId;

    constexpr std::uint32_t kFrames = 2048;

    // 1. Isola apenas a síntese (sampleGain = 0)
    std::vector<float> soSynth(kFrames, 0.0f);
    {
        mus::AudioEngine engine;
        mus::SpscRingBuffer<mus::SoundEvent> fila(4);
        engine.setSampleBank(&bank);
        engine.setRingBuffer(&fila);
        engine.setSynthGain(1.0f);
        engine.setSampleGain(0.0f);
        REQUIRE(fila.push(evSynth));
        REQUIRE(fila.push(evSample));
        engine.render(soSynth.data(), kFrames);
    }

    // Referência com apenas o evento Synth na fila: deve coincidir.
    std::vector<float> refSynth(kFrames, 0.0f);
    {
        mus::AudioEngine engine;
        mus::SpscRingBuffer<mus::SoundEvent> fila(4);
        engine.setRingBuffer(&fila);
        REQUIRE(fila.push(evSynth));
        engine.render(refSynth.data(), kFrames);
    }
    for (std::uint32_t i = 0; i < kFrames; ++i) {
        CHECK(soSynth[i] == doctest::Approx(refSynth[i]).epsilon(1e-6));
    }

    // 2. Isola apenas o sample percussivo (synthGain = 0) e aplica boost de 1.5x
    std::vector<float> sampleNormal(kFrames, 0.0f);
    std::vector<float> sampleBoost(kFrames, 0.0f);
    {
        mus::AudioEngine engine1;
        mus::SpscRingBuffer<mus::SoundEvent> fila1(4);
        engine1.setSampleBank(&bank);
        engine1.setRingBuffer(&fila1);
        engine1.setSynthGain(0.0f);
        engine1.setSampleGain(0.5f);
        REQUIRE(fila1.push(evSynth));
        REQUIRE(fila1.push(evSample));
        engine1.render(sampleNormal.data(), kFrames);

        mus::AudioEngine engine2;
        mus::SpscRingBuffer<mus::SoundEvent> fila2(4);
        engine2.setSampleBank(&bank);
        engine2.setRingBuffer(&fila2);
        engine2.setSynthGain(0.0f);
        engine2.setSampleGain(1.5f);
        REQUIRE(fila2.push(evSynth));
        REQUIRE(fila2.push(evSample));
        engine2.render(sampleBoost.data(), kFrames);
    }

    CHECK(picoModulo(sampleNormal) > 0.0f);
    CHECK(picoModulo(sampleBoost) > picoModulo(sampleNormal));
    CHECK(picoModulo(sampleBoost) <= 1.0f);
}

// ---- Linguagem .mus (variáveis dinâmicas de Volume) + Geração WAV --------

TEST_CASE("dsp-09: fade-in dinamico por variaveis na linguagem .mus e escala no WAV") {
    const std::string codigoMus =
        "andamento 240\n"
        "vol = 0.2\n"
        "repita 4 vezes {\n"
        "    volume vol\n"
        "    toca la4 por 1\n"
        "    vol = vol + 0.2\n"
        "}\n";

    std::string erroSintaxe;
    auto arvore = mus::parseToAst(codigoMus, &erroSintaxe);
    REQUIRE_MESSAGE(arvore != nullptr, erroSintaxe);

    mus::semantica::AnalisadorSemantico semantica;
    REQUIRE(semantica.analisar(*arvore));

    mus::Interpretador interpretador;
    REQUIRE_MESSAGE(interpretador.executar(*arvore), interpretador.erro());

    const mus::Timeline& timeline = interpretador.timeline();
    REQUIRE(timeline.size() == 4);
    CHECK(timeline.events()[0].volume == doctest::Approx(0.2f));
    CHECK(timeline.events()[1].volume == doctest::Approx(0.4f));
    CHECK(timeline.events()[2].volume == doctest::Approx(0.6f));
    CHECK(timeline.events()[3].volume == doctest::Approx(0.8f));

    // Renderiza a peça e verifica que o pico cresce nota a nota (fade-in real no áudio).
    mus::SpscRingBuffer<mus::SoundEvent> fila(timeline.size());
    REQUIRE(mus::carregarFila(timeline, fila));

    mus::AudioEngine engine;
    engine.setRingBuffer(&fila);
    engine.setExpectedDuration(timeline.duration());

    const std::size_t framesPorNota =
        static_cast<std::size_t>(0.25 * mus::AudioEngine::kSampleRate);
    std::vector<float> audioCompleto(framesPorNota * 4, 0.0f);
    engine.render(audioCompleto.data(), static_cast<std::uint32_t>(audioCompleto.size()));

    const float p0 = picoModulo(audioCompleto.data() + 0 * framesPorNota, framesPorNota);
    const float p1 = picoModulo(audioCompleto.data() + 1 * framesPorNota, framesPorNota);
    const float p2 = picoModulo(audioCompleto.data() + 2 * framesPorNota, framesPorNota);
    const float p3 = picoModulo(audioCompleto.data() + 3 * framesPorNota, framesPorNota);

    CHECK(p0 < p1);
    CHECK(p1 < p2);
    CHECK(p2 < p3);

    // Exporta em WAV com masterVolume = 1.0 e masterVolume = 0.5 e compara os picos PCM16.
    std::ostringstream wavCheio;
    std::ostringstream wavMeio;
    REQUIRE(mus::codegen::gerarWav(timeline, wavCheio, nullptr, 1.0f).ok);
    REQUIRE(mus::codegen::gerarWav(timeline, wavMeio, nullptr, 0.5f).ok);

    const std::string bytesCheio = wavCheio.str();
    const std::string bytesMeio = wavMeio.str();
    REQUIRE(bytesCheio.size() == bytesMeio.size());

    int maiorCheio = 0;
    int maiorMeio = 0;
    for (std::size_t off = mus::codegen::kWavHeaderSize; off + 1 < bytesCheio.size(); off += 2) {
        maiorCheio = std::max(maiorCheio, std::abs(static_cast<int>(lerPcm16LE(bytesCheio, off))));
        maiorMeio = std::max(maiorMeio, std::abs(static_cast<int>(lerPcm16LE(bytesMeio, off))));
    }

    CHECK(maiorCheio > 20000);
    CHECK(static_cast<double>(maiorMeio) ==
          doctest::Approx(0.5 * static_cast<double>(maiorCheio)).epsilon(0.01));
}

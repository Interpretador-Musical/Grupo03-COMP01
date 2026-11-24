#include "doctest.h"
#include <cmath>
#include <vector>
#include "audio/audio_engine.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"
#include <algorithm>

using namespace mus;

TEST_CASE("envelope: sobe suavemente e volta a zero") {
    AudioEngine engine;
    SpscRingBuffer<SoundEvent> buffer(4);
    buffer.push(SoundEvent{0.0, 0.02, 440.0, 1.0f});   // 20 ms
    engine.setRingBuffer(&buffer);

    const std::uint32_t n = static_cast<std::uint32_t>(0.03 * AudioEngine::kSampleRate);
    std::vector<float> out(n, 0.0f);
    engine.renderRealTime(out.data(), n);

    // Sem click: os primeiros frames têm amplitude bem pequena.
    CHECK(std::fabs(out[5]) < 0.05f);
    // Meio do attack: já soa.
    CHECK(out[110] != 0.0f);
    // 30 ms > 20 ms: a nota já terminou por completo.
    CHECK(out[n - 1] == 0.0f);
}

TEST_CASE("envelope: duração total respeita o evento") {
    AudioEngine engine;
    SpscRingBuffer<SoundEvent> buffer(4);
    buffer.push(SoundEvent{0.0, 0.02, 440.0, 1.0f});
    engine.setRingBuffer(&buffer);

    const std::uint32_t n = 1000;
    std::vector<float> out(n, 0.0f);
    engine.renderRealTime(out.data(), n);

    // Nota de 882 frames: depois disso, silêncio.
    for (std::uint32_t i = 890; i < n; ++i) {
        CHECK(out[i] == 0.0f);
    }
}

TEST_CASE("eventos simultâneos soam juntos (acorde)") {
    AudioEngine solo, acorde;
    SpscRingBuffer<SoundEvent> b1(4), b2(4);
    b1.push(SoundEvent{0.0, 0.05, 440.0, 0.4f});
    b2.push(SoundEvent{0.0, 0.05, 440.0, 0.4f});
    b2.push(SoundEvent{0.0, 0.05, 660.0, 0.4f});
    solo.setRingBuffer(&b1);
    acorde.setRingBuffer(&b2);

    std::vector<float> a(1000), c(1000);
    solo.renderRealTime(a.data(), 1000);
    acorde.renderRealTime(c.data(), 1000);

    CHECK(c != a);   // a segunda voz alterou a mistura
}

TEST_CASE("evento futuro só dispara no frame certo") {
    AudioEngine engine;
    SpscRingBuffer<SoundEvent> buffer(4);
    buffer.push(SoundEvent{0.01, 0.05, 440.0, 1.0f});   // começa em 10 ms = frame 441
    engine.setRingBuffer(&buffer);

    std::vector<float> out(600, 0.0f);
    engine.renderRealTime(out.data(), 600);

    for (int i = 0; i < 441; ++i) {
        CHECK(out[i] == 0.0f);   // silêncio antes do início
    }
    CHECK(out[520] != 0.0f);     // soando depois
}

TEST_CASE("envelope: notas curtas produzem audio audivel") {
    for (double duracao : {0.0025, 0.00625}) {
        CAPTURE(duracao);

        AudioEngine engine;
        SpscRingBuffer<SoundEvent> buffer(4);

        REQUIRE(buffer.push(
            SoundEvent{0.0, duracao, 440.0, 1.0f}));

        engine.setRingBuffer(&buffer);

        const std::uint32_t n =
            static_cast<std::uint32_t>(
                std::ceil(duracao * AudioEngine::kSampleRate)) + 256;

        std::vector<float> out(n, 0.0f);
        engine.renderRealTime(out.data(), n);

        float pico = 0.0f;
        for (float amostra : out) {
            pico = std::max(pico, std::fabs(amostra));
        }

        CHECK(pico > 0.5f);
        CHECK(out.back() == 0.0f);
    }
}

TEST_CASE("envelope: nota de um frame encerra a voz") {
    AudioEngine engine;
    SpscRingBuffer<SoundEvent> buffer(4);

    constexpr double duracao =
        1.0 / AudioEngine::kSampleRate;

    REQUIRE(buffer.push(
        SoundEvent{0.0, duracao, 440.0, 1.0f}));

    engine.setRingBuffer(&buffer);

    std::vector<float> out(256, 0.0f);
    engine.renderRealTime(out.data(), 256);
    // A amostra deve ter amplitude significativa, não apenas ser diferente de zero.
    // Detecta regressões na inicialização da fase após otimizações do oscilador.
    CHECK(std::fabs(out[0]) > 0.5f);

    CHECK(engine.activeVoiceCount() == 0);

    // Uma nota de um frame deve produzir somente a primeira amostra.
    for (std::uint32_t i = 1; i < 256; ++i) {
        CHECK(out[i] == 0.0f);
    }
}

TEST_CASE("envelope: nota de dois frames produz som") {
    AudioEngine engine;
    SpscRingBuffer<SoundEvent> buffer(4);

    constexpr double duracao =
        2.0 / AudioEngine::kSampleRate;

    REQUIRE(buffer.push(
        SoundEvent{0.0, duracao, 11025.0, 1.0f}));

    engine.setRingBuffer(&buffer);

    std::vector<float> out(256, 0.0f);
    engine.renderRealTime(out.data(), 256);

    const float pico =
        std::max(std::fabs(out[0]), std::fabs(out[1]));

    CHECK(pico > 0.0f);
    CHECK(engine.activeVoiceCount() == 0);

    for (std::uint32_t i = 2; i < 256; ++i) {
        CHECK(out[i] == 0.0f);
    }
}

TEST_CASE("envelope: notas curtas possuem transicoes suaves") {
    constexpr double frequencia = 440.0;
    constexpr float volume = 1.0f;

    // Duracoes próximas dos limites de ataque e release.
    for (double duracao : {0.005, 0.00625, 0.009, 0.010}) {
        CAPTURE(duracao);

        AudioEngine engine;
        SpscRingBuffer<SoundEvent> buffer(4);

        REQUIRE(buffer.push(
            SoundEvent{0.0, duracao, frequencia, volume}));

        engine.setRingBuffer(&buffer);

        const std::uint32_t framesNota =
            static_cast<std::uint32_t>(
                std::llround(duracao * AudioEngine::kSampleRate));

        std::vector<float> out(framesNota + 256, 0.0f);

        engine.renderRealTime(
            out.data(),
            static_cast<std::uint32_t>(out.size()));

        // A nota deve começar próxima do silêncio.
        CHECK(std::fabs(out.front()) < 0.05f);

        // Depois do evento, a saída deve permanecer silenciosa.
        for (std::size_t i = framesNota; i < out.size(); ++i) {
            CHECK(out[i] == doctest::Approx(0.0f));
        }

        // Verifica saltos abruptos entre amostras consecutivas.
        float maiorSalto = 0.0f;

        for (std::size_t i = 1; i < out.size(); ++i) {
            const float salto =
                std::fabs(out[i] - out[i - 1]);

            maiorSalto = std::max(maiorSalto, salto);
        }

        CAPTURE(maiorSalto);

        // A 440 Hz e 44.1 kHz, a variacao natural maxima
        // entre amostras da senoide é aproximadamente 0.063.
        // A margem de 0.10 permite a modulacao do envelope,
        // mas identifica descontinuidades muito maiores.
        CHECK(maiorSalto < 0.10f);
    }
}
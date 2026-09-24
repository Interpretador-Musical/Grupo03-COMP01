#include "doctest.h"
#include <cmath>
#include <vector>
#include "audio/audio_engine.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"

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
    CHECK(out[n - 1] == doctest::Approx(0.0f));
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
        CHECK(out[i] == doctest::Approx(0.0f));
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
#include "doctest.h"
#include <cstdint>
#include <vector>
#include "audio/audio_engine.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"

using namespace mus;

TEST_CASE("underrun: aplica zero-stuffing em buffer sujo e aciona flag") {
    AudioEngine engine;
    SpscRingBuffer<SoundEvent> buffer(4);
    engine.setRingBuffer(&buffer);
    engine.setExpectedDuration(1.0);

    const std::uint32_t n = 256;
    std::vector<float> out(n, 0.75f);

    engine.renderRealTime(out.data(), n);

    for (std::uint32_t i = 0; i < n; ++i) {
        CHECK(out[i] == 0.0f);
    }

    CHECK(engine.checkAndClearUnderrun() == true);
    CHECK(engine.checkAndClearUnderrun() == false);
}

TEST_CASE("underrun: nao gera falso positivo durante nota ativa ou no fim natural") {
    AudioEngine engine;
    SpscRingBuffer<SoundEvent> buffer(4);
    buffer.push(SoundEvent{0.0, 0.02, 440.0, 1.0f});
    engine.setRingBuffer(&buffer);
    engine.setExpectedDuration(0.02);

    const std::uint32_t n = 1000;
    std::vector<float> out(n, 0.0f);
    engine.renderRealTime(out.data(), n);

    CHECK_FALSE(engine.checkAndClearUnderrun());
}

TEST_CASE("underrun: detecta inanicao apos termino precoce das vozes") {
    AudioEngine engine;
    SpscRingBuffer<SoundEvent> buffer(4);
    buffer.push(SoundEvent{0.0, 0.01, 440.0, 1.0f});
    engine.setRingBuffer(&buffer);
    engine.setExpectedDuration(1.0);

    std::vector<float> bloco1(256, 0.0f);
    engine.renderRealTime(bloco1.data(), 256);
    CHECK_FALSE(engine.checkAndClearUnderrun());
    CHECK(bloco1[100] != 0.0f);

    std::vector<float> bloco2(512, 0.5f);
    engine.renderRealTime(bloco2.data(), 512);
    CHECK(engine.checkAndClearUnderrun());
    CHECK(bloco2[511] == 0.0f);
}

TEST_CASE("underrun: pausa musical antes de evento futuro nao gera falso positivo") {
    AudioEngine engine;
    SpscRingBuffer<SoundEvent> buffer(4);
    buffer.push(SoundEvent{0.01, 0.02, 440.0, 1.0f});
    engine.setRingBuffer(&buffer);
    engine.setExpectedDuration(0.05);

    std::vector<float> out(256, 0.0f);
    engine.renderRealTime(out.data(), 256);

    CHECK_FALSE(engine.checkAndClearUnderrun());
    for (std::uint32_t i = 0; i < 256; ++i) {
        CHECK(out[i] == 0.0f);
    }
}

TEST_CASE("underrun: recupera sintese normalmente quando novos eventos chegam") {
    AudioEngine engine;
    SpscRingBuffer<SoundEvent> buffer(4);
    engine.setRingBuffer(&buffer);
    engine.setExpectedDuration(1.0);

    std::vector<float> blocoUnderrun(256, 0.5f);
    engine.renderRealTime(blocoUnderrun.data(), 256);
    CHECK(engine.checkAndClearUnderrun() == true);
    CHECK(blocoUnderrun[0] == 0.0f);

    const double startTimeRecuperacao = 256.0 / AudioEngine::kSampleRate;
    buffer.push(SoundEvent{startTimeRecuperacao, 0.05, 440.0, 1.0f});

    std::vector<float> blocoRecuperado(256, 0.0f);
    engine.renderRealTime(blocoRecuperado.data(), 256);

    CHECK_FALSE(engine.checkAndClearUnderrun());
    CHECK(blocoRecuperado[100] != 0.0f);
}

#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "audio/audio_engine.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"

namespace {

float picoAbsoluto(const std::vector<float>& buffer) {
    float pico = 0.0f;
    for (float amostra : buffer) {
        pico = std::max(pico, std::fabs(amostra));
    }
    return pico;
}

}  // namespace

TEST_CASE("dsp-05: mixagem polifonica no master bus respeita headroom [-1.0, 1.0]") {
    mus::AudioEngine engine;
    mus::SpscRingBuffer<mus::SoundEvent> ringBuffer(32);
    engine.setRingBuffer(&ringBuffer);

    // 4 vozes simultâneas com ganho máximo (1.0) no mesmo instante (startTime = 0.0).
    // A soma bruta ultrapassaria [-1.0, 1.0], exigindo atuação do limitador no master bus.
    const double frequencias[] = {261.63, 329.63, 392.00, 523.25};
    for (double freq : frequencias) {
        mus::SoundEvent ev{};
        ev.startTime = 0.0;
        ev.duration = 0.5;
        ev.frequency = freq;
        ev.volume = 1.0f;
        REQUIRE(ringBuffer.push(ev));
    }

    const std::uint32_t totalFrames = 4096;
    std::vector<float> output(totalFrames, 0.0f);
    engine.render(output.data(), totalFrames);

    for (std::uint32_t i = 0; i < totalFrames; ++i) {
        CHECK(std::isfinite(output[i]));
        CHECK(output[i] >= -1.0f);
        CHECK(output[i] <= 1.0f);
    }
    CHECK(picoAbsoluto(output) > 0.5f);
}

TEST_CASE("dsp-05: voz unica mantem amplitude util sem atenuacao destrutiva") {
    mus::AudioEngine engine;
    mus::SpscRingBuffer<mus::SoundEvent> ringBuffer(8);
    engine.setRingBuffer(&ringBuffer);

    mus::SoundEvent nota{};
    nota.startTime = 0.0;
    nota.duration = 0.5;
    nota.frequency = 440.0;
    nota.volume = 0.8f;
    REQUIRE(ringBuffer.push(nota));

    // Renderiza janelas suficientes para passar pelo Attack e atingir o Sustain
    const std::uint32_t totalFrames = 4096;
    std::vector<float> output(totalFrames, 0.0f);
    engine.render(output.data(), totalFrames);

    const float pico = picoAbsoluto(output);
    // Sem divisão fixa por 16 vozes (que derrubaria 0.8 para 0.05),
    // uma voz isolada deve preservar volume forte na saída.
    CHECK(pico > 0.4f);
    CHECK(pico <= 1.0f);
}

TEST_CASE("dsp-05: eventos simultaneos produzem energia maior que voz isolada") {
    const std::uint32_t totalFrames = 2048;

    // Cenário 1: 1 voz com volume moderado (0.2)
    mus::AudioEngine engineSolo;
    mus::SpscRingBuffer<mus::SoundEvent> bufferSolo(8);
    engineSolo.setRingBuffer(&bufferSolo);

    mus::SoundEvent ev1{};
    ev1.startTime = 0.0;
    ev1.duration = 0.5;
    ev1.frequency = 440.0;
    ev1.volume = 0.2f;
    REQUIRE(bufferSolo.push(ev1));

    std::vector<float> outSolo(totalFrames, 0.0f);
    engineSolo.render(outSolo.data(), totalFrames);

    // Cenário 2: 2 vozes simultâneas com volume moderado (0.2 cada)
    mus::AudioEngine engineDuo;
    mus::SpscRingBuffer<mus::SoundEvent> bufferDuo(8);
    engineDuo.setRingBuffer(&bufferDuo);

    mus::SoundEvent ev2 = ev1;
    ev2.frequency = 554.37;
    REQUIRE(bufferDuo.push(ev1));
    REQUIRE(bufferDuo.push(ev2));

    std::vector<float> outDuo(totalFrames, 0.0f);
    engineDuo.render(outDuo.data(), totalFrames);

    CHECK(picoAbsoluto(outDuo) > picoAbsoluto(outSolo));
}

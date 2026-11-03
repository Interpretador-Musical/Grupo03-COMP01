#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "audio/audio_engine.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"
#include "audio/sample_bank.h"

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


TEST_CASE("INT-05: AudioEngine reproduz sample WAV carregado") {
    mus::SampleBank bank;

    const std::string caminho =
        std::string(MUS_SOURCE_DIR) + "/samples/PERC_09.wav";

    const auto id = bank.load("percussao", caminho);
    REQUIRE(id.has_value());

    mus::AudioEngine engine;
    mus::SpscRingBuffer<mus::SoundEvent> ringBuffer(8);

    engine.setSampleBank(&bank);
    engine.setRingBuffer(&ringBuffer);

    mus::SoundEvent evento{};
    evento.startTime = 0.0;
    evento.duration = 0.1;
    evento.volume = 1.0f;
    evento.type = mus::SoundEventType::Sample;
    evento.sampleId = *id;

    REQUIRE(ringBuffer.push(evento));

    std::vector<float> output(4096, 0.0f);
    engine.render(output.data(),
                  static_cast<std::uint32_t>(output.size()));

    CHECK(picoAbsoluto(output) > 0.0f);

    for (float amostra : output) {
        CHECK(std::isfinite(amostra));
        CHECK(amostra >= -1.0f);
        CHECK(amostra <= 1.0f);
    }
}

TEST_CASE("INT-05: sample inexistente produz silencio") {
    mus::SampleBank bank;

    mus::AudioEngine engine;
    mus::SpscRingBuffer<mus::SoundEvent> ringBuffer(8);

    engine.setSampleBank(&bank);
    engine.setRingBuffer(&ringBuffer);

    mus::SoundEvent evento{};
    evento.startTime = 0.0;
    evento.duration = 0.1;
    evento.volume = 1.0f;
    evento.type = mus::SoundEventType::Sample;
    evento.sampleId = 999;

    REQUIRE(ringBuffer.push(evento));

    std::vector<float> output(1024, 0.0f);
    engine.render(output.data(),
                  static_cast<std::uint32_t>(output.size()));

    CHECK(picoAbsoluto(output) == doctest::Approx(0.0f));
}


TEST_CASE("INT-05: duas vozes reproduzem o mesmo sample independentemente") {
    mus::SampleBank bank;

    const std::string caminho =
        std::string(MUS_SOURCE_DIR) + "/samples/PERC_09.wav";

    const auto id = bank.load("percussao", caminho);
    REQUIRE(id.has_value());

    mus::SoundEvent evento{};
    evento.startTime = 0.0;
    evento.duration = 0.1;
    evento.volume = 0.3f;
    evento.type = mus::SoundEventType::Sample;
    evento.sampleId = *id;

    constexpr std::uint32_t totalFrames = 2048;

    // Primeira execucao: apenas uma voz.
    mus::AudioEngine solo;
    mus::SpscRingBuffer<mus::SoundEvent> filaSolo(8);

    solo.setSampleBank(&bank);
    solo.setRingBuffer(&filaSolo);

    REQUIRE(filaSolo.push(evento));

    std::vector<float> outputSolo(totalFrames, 0.0f);
    solo.render(outputSolo.data(), totalFrames);

    // Segunda execucao: duas vozes com o mesmo sample.
    mus::AudioEngine duplo;
    mus::SpscRingBuffer<mus::SoundEvent> filaDupla(8);

    duplo.setSampleBank(&bank);
    duplo.setRingBuffer(&filaDupla);

    REQUIRE(filaDupla.push(evento));
    REQUIRE(filaDupla.push(evento));

    std::vector<float> outputDuplo(totalFrames, 0.0f);
    duplo.render(outputDuplo.data(), totalFrames);

    const float picoSolo = picoAbsoluto(outputSolo);
    const float picoDuplo = picoAbsoluto(outputDuplo);

    CHECK(picoSolo > 0.0f);
    CHECK(picoDuplo > picoSolo);

    for (float amostra : outputDuplo) {
        CHECK(std::isfinite(amostra));
        CHECK(amostra >= -1.0f);
        CHECK(amostra <= 1.0f);
    }
}


TEST_CASE("INT-05: reproducao termina ao atingir o fim do WAV") {
    mus::SampleBank bank;

    const std::string caminho =
        std::string(MUS_SOURCE_DIR) + "/samples/PERC_09.wav";

    const auto id = bank.load("percussao", caminho);
    REQUIRE(id.has_value());

    const mus::AudioBuffer* sample = bank.get(*id);
    REQUIRE(sample != nullptr);
    REQUIRE(sample->frameCount > 0);
    REQUIRE(sample->sampleRate > 0);

    mus::AudioEngine engine;
    mus::SpscRingBuffer<mus::SoundEvent> fila(8);

    engine.setSampleBank(&bank);
    engine.setRingBuffer(&fila);

    mus::SoundEvent evento{};
    evento.startTime = 0.0;
    evento.duration =
        static_cast<double>(sample->frameCount) / sample->sampleRate + 0.2;
    evento.volume = 1.0f;
    evento.type = mus::SoundEventType::Sample;
    evento.sampleId = *id;

    REQUIRE(fila.push(evento));

    const std::uint32_t frames =
        static_cast<std::uint32_t>(
            evento.duration * mus::AudioEngine::kSampleRate) + 1024;

    std::vector<float> output(frames, 0.0f);
    engine.render(output.data(), frames);

    CHECK(picoAbsoluto(output) > 0.0f);

    // Depois do fim fisico do WAV, a voz deve produzir silencio.
    const std::uint32_t fimWav =
        static_cast<std::uint32_t>(
            std::ceil(static_cast<double>(sample->frameCount) *
                      mus::AudioEngine::kSampleRate / sample->sampleRate));

    for (std::uint32_t i = fimWav + 1; i < frames; ++i) {
        CHECK(output[i] == doctest::Approx(0.0f));
    }
}


TEST_CASE("INT-05: sample respeita duracao musical do evento") {
    mus::SampleBank bank;

    const std::string caminho =
        std::string(MUS_SOURCE_DIR) + "/samples/PERC_09.wav";

    const auto id = bank.load("percussao", caminho);
    REQUIRE(id.has_value());

    mus::AudioEngine engine;
    mus::SpscRingBuffer<mus::SoundEvent> fila(8);

    engine.setSampleBank(&bank);
    engine.setRingBuffer(&fila);

    mus::SoundEvent evento{};
    evento.startTime = 0.0;
    evento.duration = 0.01; // 10 ms
    evento.volume = 1.0f;
    evento.type = mus::SoundEventType::Sample;
    evento.sampleId = *id;

    REQUIRE(fila.push(evento));

    std::vector<float> output(2048, 0.0f);
    engine.render(output.data(),
                  static_cast<std::uint32_t>(output.size()));

    CHECK(picoAbsoluto(output) > 0.0f);

    // O evento dura aproximadamente 441 frames.
    // Apos esse limite, o envelope deve ter encerrado a voz.
    for (std::uint32_t i = 450; i < output.size(); ++i) {
        CHECK(output[i] == doctest::Approx(0.0f));
    }
}

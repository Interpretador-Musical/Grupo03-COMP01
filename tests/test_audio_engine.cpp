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

TEST_CASE("dsp-07: limite de vozes configurado no motor de sintese") {
    mus::AudioEngine engine;

    // Limite padrão de 32 osciladores
    CHECK(engine.maxVoices() == 32);

    // Configuração de limite customizado
    engine.setMaxVoices(8);
    CHECK(engine.maxVoices() == 8);

    // Validação de clamping nos limites [1, kMaxVoices]
    engine.setMaxVoices(0);
    CHECK(engine.maxVoices() == 1);

    engine.setMaxVoices(-5);
    CHECK(engine.maxVoices() == 1);

    engine.setMaxVoices(100);
    CHECK(engine.maxVoices() == mus::AudioEngine::kMaxVoices);
}

TEST_CASE("dsp-07: algoritmo prioriza voz ociosa antes de roubar vozes ativas") {
    mus::AudioEngine engine;
    engine.setMaxVoices(4);

    // Com vozes ociosas, deve alocar a primeira ociosa sem roubo
    mus::AudioEngine::Voice* v0 = engine.allocateVoice();
    REQUIRE(v0 == &engine.voices()[0]);
    v0->envState = mus::AudioEngine::EnvState::Sustain;
    v0->envLevel = 1.0f;
    v0->volume = 1.0f;

    mus::AudioEngine::Voice* v1 = engine.allocateVoice();
    REQUIRE(v1 == &engine.voices()[1]);
}

TEST_CASE("dsp-07: algoritmo detecta e prioriza oscilador em fase de release para roubo") {
    // 1. Teste direto da função heurística de comparação
    mus::AudioEngine::Voice sustentada{};
    sustentada.envState = mus::AudioEngine::EnvState::Sustain;
    sustentada.volume = 1.0f;
    sustentada.envLevel = 1.0f;
    sustentada.startFrame = 100;

    mus::AudioEngine::Voice emRelease{};
    emRelease.envState = mus::AudioEngine::EnvState::Release;
    emRelease.volume = 1.0f;
    emRelease.envLevel = 0.5f;
    emRelease.startFrame = 200;

    CHECK(mus::AudioEngine::isBetterToSteal(emRelease, sustentada) == true);
    CHECK(mus::AudioEngine::isBetterToSteal(sustentada, emRelease) == false);

    // 2. Teste via allocateVoice com pool lotado
    mus::AudioEngine engine;
    engine.setMaxVoices(3);

    auto& v = engine.voices();
    v[0].envState = mus::AudioEngine::EnvState::Sustain;
    v[0].volume = 0.8f;
    v[0].envLevel = 1.0f;

    v[1].envState = mus::AudioEngine::EnvState::Release;
    v[1].volume = 0.8f;
    v[1].envLevel = 0.4f;

    v[2].envState = mus::AudioEngine::EnvState::Sustain;
    v[2].volume = 0.8f;
    v[2].envLevel = 1.0f;

    mus::AudioEngine::Voice* escolhida = engine.allocateVoice();
    CHECK(escolhida == &v[1]);
}

TEST_CASE("dsp-07: algoritmo detecta e prioriza oscilador com menor amplitude") {
    // 1. Ambas em Release: a mais fraca é roubada primeiro
    mus::AudioEngine::Voice relForte{};
    relForte.envState = mus::AudioEngine::EnvState::Release;
    relForte.volume = 1.0f;
    relForte.envLevel = 0.6f;

    mus::AudioEngine::Voice relFraca{};
    relFraca.envState = mus::AudioEngine::EnvState::Release;
    relFraca.volume = 1.0f;
    relFraca.envLevel = 0.1f;

    CHECK(mus::AudioEngine::isBetterToSteal(relFraca, relForte) == true);
    CHECK(mus::AudioEngine::isBetterToSteal(relForte, relFraca) == false);

    // 2. Ambas ativas: a com menor amplitude efetiva é roubada
    mus::AudioEngine::Voice sustAlta{};
    sustAlta.envState = mus::AudioEngine::EnvState::Sustain;
    sustAlta.volume = 0.9f;
    sustAlta.envLevel = 1.0f;

    mus::AudioEngine::Voice sustBaixa{};
    sustBaixa.envState = mus::AudioEngine::EnvState::Sustain;
    sustBaixa.volume = 0.2f;
    sustBaixa.envLevel = 1.0f;

    CHECK(mus::AudioEngine::isBetterToSteal(sustBaixa, sustAlta) == true);
    CHECK(mus::AudioEngine::isBetterToSteal(sustAlta, sustBaixa) == false);

    // 3. Verificação no pool do motor
    mus::AudioEngine engine;
    engine.setMaxVoices(3);
    auto& v = engine.voices();

    v[0].envState = mus::AudioEngine::EnvState::Sustain;
    v[0].volume = 0.9f;
    v[0].envLevel = 1.0f;

    v[1].envState = mus::AudioEngine::EnvState::Sustain;
    v[1].volume = 0.15f;
    v[1].envLevel = 1.0f;

    v[2].envState = mus::AudioEngine::EnvState::Sustain;
    v[2].volume = 0.7f;
    v[2].envLevel = 1.0f;

    mus::AudioEngine::Voice* escolhida = engine.allocateVoice();
    CHECK(escolhida == &v[1]);
}

TEST_CASE("dsp-07: em empate de amplitude o sistema rouba a voz mais antiga (FIFO)") {
    mus::AudioEngine engine;
    engine.setMaxVoices(3);
    auto& v = engine.voices();

    // 3 vozes com a mesma amplitude, iniciadas em momentos diferentes
    v[0].envState = mus::AudioEngine::EnvState::Sustain;
    v[0].volume = 0.8f;
    v[0].envLevel = 1.0f;
    v[0].startFrame = 1000;

    v[1].envState = mus::AudioEngine::EnvState::Sustain;
    v[1].volume = 0.8f;
    v[1].envLevel = 1.0f;
    v[1].startFrame = 200; // mais antiga!

    v[2].envState = mus::AudioEngine::EnvState::Sustain;
    v[2].volume = 0.8f;
    v[2].envLevel = 1.0f;
    v[2].startFrame = 3500;

    mus::AudioEngine::Voice* escolhida = engine.allocateVoice();
    CHECK(escolhida == &v[1]);
}

TEST_CASE("dsp-07: seguranca acustica com fade-out rapido anti-clique") {
    // Configura motor com limite de 2 vozes
    mus::AudioEngine engine;
    engine.setMaxVoices(2);
    mus::SpscRingBuffer<mus::SoundEvent> ringBuffer(8);
    engine.setRingBuffer(&ringBuffer);

    // Toca 2 notas simultâneas em t=0 (preenche as 2 vozes)
    mus::SoundEvent n1{0.0, 0.05, 440.0, 1.0f};
    mus::SoundEvent n2{0.0, 0.05, 554.37, 1.0f};
    REQUIRE(ringBuffer.push(n1));
    REQUIRE(ringBuffer.push(n2));

    // Renderiza 10 ms: vozes entram em Sustain com amplitude alta
    const std::uint32_t frames10ms = static_cast<std::uint32_t>(0.010 * mus::AudioEngine::kSampleRate);
    std::vector<float> bloco1(frames10ms, 0.0f);
    engine.render(bloco1.data(), frames10ms);

    // Insere uma terceira nota em t=0.010 (frame exato da transição), forçando roubo de voz
    mus::SoundEvent n3{0.010, 0.05, 880.0, 1.0f};
    REQUIRE(ringBuffer.push(n3));

    // Renderiza os próximos 5 ms abrangendo o roubo e o fade-out
    const std::uint32_t frames5ms = static_cast<std::uint32_t>(0.005 * mus::AudioEngine::kSampleRate);
    std::vector<float> bloco2(frames5ms, 0.0f);
    engine.render(bloco2.data(), frames5ms);

    // Analisa a transição entre o último frame do bloco1 e o primeiro do bloco2
    const float saltoNaBorda = std::fabs(bloco2[0] - bloco1.back());
    // Sem fade-out, o corte abrupto de uma senoide em amplitude cheia causa descontinuidade > 0.5.
    // Com fade-out anti-clique contínuo, a derivada se mantém suave.
    CHECK(saltoNaBorda < 0.25f);

    // Confere que ao longo de todo o bloco de roubo não há nenhum salto espúrio (degrau / clique)
    for (std::size_t i = 1; i < bloco2.size(); ++i) {
        const float delta = std::fabs(bloco2[i] - bloco2[i - 1]);
        CHECK(delta < 0.35f);
    }
}

TEST_CASE("dsp-07: saturacao polifonica e voice stealing massivo terminam em silencio sem travar") {
    mus::AudioEngine engine;
    engine.setMaxVoices(4); // Apenas 4 osciladores disponíveis

    mus::SpscRingBuffer<mus::SoundEvent> ringBuffer(32);
    engine.setRingBuffer(&ringBuffer);

    // Dispara 20 eventos com início escalonado e sobreposição maciça
    for (int i = 0; i < 20; ++i) {
        mus::SoundEvent ev{};
        ev.startTime = i * 0.002; // a cada 2 ms
        ev.duration = 0.040;      // dura 40 ms (todas se sobrepõem)
        ev.frequency = 220.0 + i * 30.0;
        ev.volume = 0.8f;
        REQUIRE(ringBuffer.push(ev));
    }

    // Duração total esperada: ~80 ms
    const std::uint32_t totalFrames = static_cast<std::uint32_t>(0.090 * mus::AudioEngine::kSampleRate);
    std::vector<float> output(totalFrames, 0.0f);
    engine.render(output.data(), totalFrames);

    // Todas as amostras devem ser numéricas válidas e limitadas a [-1.0, 1.0]
    for (float amostra : output) {
        CHECK(std::isfinite(amostra));
        CHECK(amostra >= -1.0f);
        CHECK(amostra <= 1.0f);
    }

    // Após o término de todos os eventos e tempos de release/fade-out, deve haver silêncio
    CHECK(output.back() == doctest::Approx(0.0f));
}

#include "doctest.h"
#include "audio/audio_engine.h"

using namespace mus;

TEST_CASE("DSP-05: Mixagem polifonica divide o ganho e previne clipping nativo") {
    AudioEngine engine;
    
    // Instancia um RingBuffer com capacidade suficiente para o acorde
    SpscRingBuffer<SoundEvent> ringBuffer(16);
    engine.setRingBuffer(&ringBuffer);
    
    // Simula um acorde de 4 notas tocando exatamente no mesmo momento (tempo 0.0)
    // Com volume máximo (1.0f). Se a soma vetorial não for dividida pelas vozes ativas,
    // o valor passará facilmente de 1.0f (clipping).
    ringBuffer.push(SoundEvent{0.0, 1.0, 261.63, 1.0f}); // Dó
    ringBuffer.push(SoundEvent{0.0, 1.0, 329.63, 1.0f}); // Mi
    ringBuffer.push(SoundEvent{0.0, 1.0, 392.00, 1.0f}); // Sol
    ringBuffer.push(SoundEvent{0.0, 1.0, 523.25, 1.0f}); // Dó agudo
    
    // Prepara um buffer de saída pequeno simulando o callback do miniaudio
    constexpr std::uint32_t kFrames = 256;
    float outputBuffer[kFrames] = {0.0f};
    
    // Roda a engine para processar os eventos e preencher o buffer PCM
    engine.renderRealTime(outputBuffer, kFrames);
    
    // Verifica a Restrição Não-Funcional (NFR): amplitude estritamente dentro de [-1.0, 1.0]
    bool sofreuClipping = false;
    for (std::uint32_t i = 0; i < kFrames; ++i) {
        if (outputBuffer[i] > 1.0f || outputBuffer[i] < -1.0f) {
            sofreuClipping = true;
            break;
        }
    }
    
    CHECK_FALSE(sofreuClipping);
}
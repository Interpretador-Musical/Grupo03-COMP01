#ifndef MUS_AUDIO_AUDIO_ENGINE_H
#define MUS_AUDIO_AUDIO_ENGINE_H

#include <atomic>
#include <cstdint>

// Declaração adiantada de propósito: o miniaudio.h tem ~95 mil linhas e sua
// implementação precisa viver em uma única unidade de tradução, então quem só
// quer ligar e desligar o áudio não deve arrastá-lo junto.
struct ma_device;

namespace mus {

// Envolve o dispositivo de saída do miniaudio e sintetiza uma senoide de teste.
// É a base sobre a qual o DSP-03 (#18) vai consumir SoundEvents de verdade.
class AudioEngine {
public:
    static constexpr std::uint32_t kSampleRate = 44100;
    // NFR do DSP-01 (#6): buffer curto para manter a latência baixa.
    static constexpr std::uint32_t kPeriodSizeInFrames = 256;

    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // Abre o dispositivo. Loga e devolve false em caso de falha.
    bool init();

    // Inicia a reprodução (chama init() se ainda não foi chamado).
    bool start();

    // Idempotente; também é chamado pelo destrutor.
    void stop();

    bool isRunning() const { return running_; }

    void setToneFrequency(double hz);
    void setGain(float gain);

private:
    static void dataCallback(ma_device* device,
                             void* output,
                             const void* input,
                             std::uint32_t frameCount);

    void renderSine(float* output, std::uint32_t frameCount);

    // Dono do ponteiro. Precisa ser alocado no heap porque ma_device é opaco
    // aqui — e, principalmente, porque o miniaudio guarda esse endereço: no
    // código original ele era uma variável local, o que tornava qualquer uso
    // fora daquela função um acesso a memória já destruída.
    ma_device* device_ = nullptr;

    bool initialized_ = false;
    bool running_ = false;

    // Escritos pela thread principal e lidos pela thread de áudio.
    std::atomic<double> frequency_{440.0};
    std::atomic<float> gain_{0.2f};

    // Só a thread de áudio encosta na fase.
    double phase_ = 0.0;
};

}  // namespace mus

#endif  // MUS_AUDIO_AUDIO_ENGINE_H

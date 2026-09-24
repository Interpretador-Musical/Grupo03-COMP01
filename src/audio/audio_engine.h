#ifndef MUS_AUDIO_AUDIO_ENGINE_H
#define MUS_AUDIO_AUDIO_ENGINE_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <array>

#include "core/playhead.h"
#include "core/timeline_player.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"

// Declaração adiantada de propósito: o miniaudio.h tem ~95 mil linhas e sua
// implementação precisa viver em uma única unidade de tradução, então quem só
// quer ligar e desligar o áudio não deve arrastá-lo junto.
struct ma_device;

namespace mus {

// Envolve o dispositivo de saída do miniaudio. Tem dois modos de síntese:
// a senoide de teste contínua (--test-tone, via setToneFrequency/setGain) e a
// execução de uma Timeline já compilada (loadTimeline, INT-03 #19) através do
// TimelinePlayer — que roda de dentro do próprio callback de áudio, sample-
// accurate, sem loop de espera por nota na thread principal.
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

    // Carrega o programa compilado que o callback vai executar (INT-03,
    // #19). Só é seguro chamar com o device parado — antes de start() ou
    // depois de stop() — nunca com isRunning()==true: não há sincronização
    // para trocar o programa com a thread de áudio lendo-o ao mesmo tempo.
    void loadTimeline(const Timeline& timeline);

    // Bloqueia até a duração total do programa carregado ter decorrido desde
    // o início da reprodução (ancorado em start(), não somado voltas de
    // sleep) — é o que garante o NFR de não fechar antes do último evento
    // terminar. Não depende de sinal nenhum vindo da thread de áudio: um
    // condition_variable acordado de dentro do callback arriscaria inversão
    // de prioridade, e o callback já segue a regra de não bloquear.
    void waitUntilFinished() const;

    void setRingBuffer(SpscRingBuffer<SoundEvent>* buffer);
    void setExpectedDuration(double durationSeconds);

    // Deixado público para permitir testes unitários isolados da engine
    void renderRealTime(float* output, std::uint32_t frameCount);

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

    // Só a thread de áudio encosta na fase (usada só pelo modo --test-tone).
    double phase_ = 0.0;

    // Modo de síntese escolhido pelo dataCallback. Não é atômico de
    // propósito: só muda antes de start() (ou depois de stop(), que em
    // miniaudio bloqueia até a thread de áudio sair do callback), então a
    // própria criação/despertar da thread de áudio já publica o valor —
    // trocar depois de isRunning()==true não é seguro.
    bool usandoPrograma_ = false;
    TimelinePlayer player_{kSampleRate};

    // Ancorado em start(); waitUntilFinished() dorme até este instante mais
    // player_.totalDuration(), uma única vez — mesma lógica de âncora que
    // Clock (engine/clock.h) usa para não acumular erro de agendador.
    std::chrono::steady_clock::time_point playbackStart_;

    enum class EnvState { Idle, Attack, Sustain, Release };

    struct Voice {
        double phase = 0.0;
        double frequency = 0.0;
        float volume = 0.0f;
        std::uint64_t framesRemaining = 0;
        float envLevel = 0.0f;
        EnvState envState = EnvState::Idle;
    };

    // Polifonia: 16 vozes cobrem acordes e notas sobrepostas. Array de
    // tamanho fixo, então nenhuma alocação acontece na thread de áudio.
    static constexpr int kMaxVoices = 16;
    std::array<Voice, kMaxVoices> voices_{};

    // Escolhe uma voz livre; se todas estiverem ocupadas, "rouba" a mais fraca.
    Voice* allocateVoice();

    SpscRingBuffer<SoundEvent>* ringBuffer_ = nullptr;
    SoundEvent pendingEvent_;
    std::uint64_t pendingStartFrame_ = 0;   // startTime do pendente, em frames
    bool hasPendingEvent_ = false;
    std::uint64_t currentFrame_ = 0;
    double expectedDuration_ = 0.0;
};

}  // namespace mus

#endif  // MUS_AUDIO_AUDIO_ENGINE_H

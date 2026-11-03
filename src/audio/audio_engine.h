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

    // Polifonia: capacidade máxima de osciladores no pool (DSP-07 #42)
    static constexpr int kMaxVoices = 32;

    enum class EnvState { Idle, Attack, Sustain, Release };

    struct Voice {
        double phase = 0.0;
        double frequency = 0.0;
        float volume = 0.0f;
        std::uint64_t framesRemaining = 0;
        float envLevel = 0.0f;
        EnvState envState = EnvState::Idle;
        std::uint64_t startFrame = 0;

        // Segurança acústica: fade-out rápido para evitar cliques digitais (DSP-07 #42)
        double fadeOutPhase = 0.0;
        double fadeOutFrequency = 0.0;
        float fadeOutVolume = 0.0f;
        float fadeOutLevel = 0.0f;
        float fadeOutStep = 0.0f;
        std::uint32_t fadeOutFramesRemaining = 0;
    };

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

    // Limite configurável de vozes ativas no sintetizador (1 a kMaxVoices, DoD DSP-07)
    void setMaxVoices(int limit);
    int maxVoices() const;

    // Quantidade de osciladores ativos no momento
    int activeVoiceCount() const;

    // Acesso às vozes para testes unitários
    const std::array<Voice, kMaxVoices>& voices() const { return voices_; }
    std::array<Voice, kMaxVoices>& voices() { return voices_; }

    // Alocação e heurística de Voice Stealing seguro (DSP-07 #42)
    Voice* allocateVoice();
    static bool isBetterToSteal(const Voice& candidate, const Voice& currentBest);

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
    void waitUntilFinished();

    void setRingBuffer(SpscRingBuffer<SoundEvent>* buffer);
    void setExpectedDuration(double durationSeconds);

    // Lê e zera a bandeira de underrun escrita pela thread de áudio.
    bool checkAndClearUnderrun();
    
    // O master bus: escolhe o modo de síntese e passa o limitador (DSP-10,
    // #57) sobre a saída final. É tudo o que o callback de áudio faz; fica
    // público para os testes alcançarem a saída exatamente como ela vai para
    // o dispositivo, sem precisar de um.
    void render(float* output, std::uint32_t frameCount);

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

    // Limite de vozes configurado no motor (DSP-07 #42)
    std::atomic<int> maxVoices_{kMaxVoices};

    // Polifonia: array de tamanho fixo, sem alocação dinâmica na thread de áudio.
    std::array<Voice, kMaxVoices> voices_{};

    SpscRingBuffer<SoundEvent>* ringBuffer_ = nullptr;
    SoundEvent pendingEvent_;
    std::uint64_t pendingStartFrame_ = 0;   // startTime do pendente, em frames
    bool hasPendingEvent_ = false;
    std::uint64_t currentFrame_ = 0;
    double expectedDuration_ = 0.0;
    std::atomic<bool> sufferedUnderrun_{false};
};

}  // namespace mus

#endif  // MUS_AUDIO_AUDIO_ENGINE_H

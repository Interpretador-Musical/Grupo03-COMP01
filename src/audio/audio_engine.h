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
class SampleBank;
struct AudioBuffer;

class AudioEngine {
public:
    static constexpr std::uint32_t kSampleRate = 44100;
    // NFR do DSP-01 (#6): buffer curto para manter a latência baixa.
    static constexpr std::uint32_t kPeriodSizeInFrames = 256;

    // Polifonia: capacidade máxima de osciladores no pool (DSP-07 #42)
    static constexpr int kMaxVoices = 32;

    enum class EnvState { Idle, Attack, Sustain, Release };

    struct Voice {
        std::uint32_t phase = 0u;
        std::uint32_t phaseIncrement = 0u;
        double frequency = 0.0;

        // INT-05: tipo de fonte sonora e estado da reproducao PCM.
        SoundEventType type = SoundEventType::Synth;
        std::uint32_t sampleId = 0;
        const AudioBuffer* sampleBuffer = nullptr;
        double samplePlaybackRate = 1.0;
        double sampleCursor = 0.0;
        float volume = 0.0f;
        std::uint64_t framesRemaining = 0;

        // Taxas do envelope calculadas no disparo da nota.
        // Notas curtas usam ataque e release proporcionais à duração.
        float attackRate = 0.0f;
        float releaseRate = 0.0f;

        float envLevel = 0.0f;
        EnvState envState = EnvState::Idle;
        std::uint64_t startFrame = 0;

        // DSP-07: fade-out da voz anterior durante o Voice Stealing.
        // INT-05: suporta tanto sintese senoidal quanto samples PCM.
        SoundEventType fadeOutType = SoundEventType::Synth;

        // Estado anterior da sintese.
        std::uint32_t fadeOutPhase = 0u;
        std::uint32_t fadeOutPhaseIncrement = 0u;
        double fadeOutFrequency = 0.0;

        // Estado anterior da reproducao PCM.
        std::uint32_t fadeOutSampleId = 0;
        const AudioBuffer* fadeOutSampleBuffer = nullptr;
        double fadeOutSamplePlaybackRate = 1.0;
        double fadeOutSampleCursor = 0.0;

        // Envelope compartilhado pelos dois tipos.
        float fadeOutVolume = 0.0f;
        float fadeOutLevel = 0.0f;
        float fadeOutStep = 0.0f;
        std::uint32_t fadeOutFramesRemaining = 0;
    };

    // Para onde vai o áudio. `Nula` usa o backend null do miniaudio: o
    // callback roda numa thread do próprio miniaudio, no ritmo do relógio,
    // sem placa de som. É o que deixa os testes exercitarem a thread de
    // áudio de verdade (SCH-08, #46) sem tocar som na máquina de quem roda o
    // ctest. `Dispositivo` tenta os backends do sistema e, sem nenhum, também
    // cai no null — é o que acontece no runner Linux do CI.
    enum class Saida { Dispositivo, Nula };

    explicit AudioEngine(Saida saida = Saida::Dispositivo);
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

    // Limite configurável de vozes ativas no sintetizador (1 a kMaxVoices, DoD DSP-07).
    //
    // Só com o dispositivo parado, como loadTimeline e setSampleBank, e da
    // mesma thread que chama start() e stop(): a troca limpa as vozes acima
    // do novo limite, e com o áudio tocando isso seria escrever nelas no meio
    // do callback (corrida de dados achada na SCH-08, #46). Com o dispositivo
    // rodando, devolve false e não muda nada — o mesmo contrato do
    // Engine::setBpm.
    bool setMaxVoices(int limit);
    int maxVoices() const;

    // Quantidade de osciladores ativos no momento. Lê as vozes, que são da
    // thread de áudio: só com o dispositivo parado ou de dentro do callback.
    int activeVoiceCount() const;

    // Acesso às vozes para testes unitários. Mesma restrição de
    // activeVoiceCount(); vale também para allocateVoice().
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

    // Associa um banco de samples previamente carregado.
    //
    // O banco deve permanecer vivo durante toda a reproducao.
    // Chamar somente com o dispositivo de audio parado.
    void setSampleBank(const SampleBank* bank);

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

    // Frames que o master bus já entregou desde o start(). Publicado pela
    // thread de áudio a cada callback; seguro de ler de qualquer thread. É
    // como os testes sabem que o callback rodou, sem ler `currentFrame_`, que
    // pertence à thread de áudio.
    std::uint64_t framesRenderizados() const {
        return framesRenderizados_.load(std::memory_order_relaxed);
    }
    
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

    Saida saida_;
    bool initialized_ = false;
    bool running_ = false;

    // Escritos pela thread principal e lidos pela thread de áudio.
    std::atomic<double> frequency_{440.0};
    std::atomic<float> gain_{0.2f};

    // Só a thread de áudio encosta na fase (usada só pelo modo --test-tone).
    std::uint32_t phase_ = 0u;

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

    // Consumo ocioso (SCH-12, #66). `vozesAtivas_` diz se alguma voz ainda
    // soava (ou tinha cauda de fade-out) depois do último frame renderizado:
    // é o que deixa o callback pular de uma vez um trecho de silêncio, sem
    // visitar as vozes paradas amostra por amostra. `fimDoSilencio_` é o
    // frame em que termina o último evento silencioso descartado (pausa ou
    // volume 0): até ali o silêncio faz parte da música e não é underrun.
    bool vozesAtivas_ = false;
    std::uint64_t fimDoSilencio_ = 0;

    double expectedDuration_ = 0.0;
    std::atomic<bool> sufferedUnderrun_{false};
    std::atomic<std::uint64_t> framesRenderizados_{0};

    // Banco externo, preparado antes de iniciar a thread de audio.
    // Nao possui os buffers e nunca modifica o banco.
    const SampleBank* sampleBank_ = nullptr;
};

}  // namespace mus

#endif  // MUS_AUDIO_AUDIO_ENGINE_H

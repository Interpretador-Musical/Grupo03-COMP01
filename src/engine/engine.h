#ifndef MUS_ENGINE_ENGINE_H
#define MUS_ENGINE_ENGINE_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

#include "core/playhead.h"
#include "engine/clock.h"

namespace mus {

// Instala os tratadores de SIGINT e SIGTERM. Depois disso, Ctrl+C apenas marca
// uma flag — quem decide o que fazer é o loop principal.
void installShutdownHandler();

// Consultável de qualquer thread.
bool shutdownRequested();

// Separa o relógio musical do resto do programa.
//
// A thread de tempo roda em ritmo próprio e não pode ser interrompida por nada
// que dependa do usuário; a thread principal é a única que encosta no terminal.
// É essa divisão que o NFR da issue exige — sem ela, uma leitura de teclado
// travaria o andamento da música.
class Engine {
public:
    // Roda na thread de tempo, uma vez por volta.
    //
    // Não pode bloquear: nada de leitura de terminal, nada de espera em mutex
    // disputado e, de preferência, nada de log — o logger escreve em stderr, e
    // stderr é I/O.
    // deltaSeconds é o tempo real decorrido desde a volta anterior. É por ele
    // que o playhead deve andar — nunca pelo intervalo nominal, que o sistema
    // operacional não tem obrigação nenhuma de respeitar.
    using TickCallback =
        std::function<void(std::uint64_t tick, double deltaSeconds)>;

    static constexpr std::chrono::microseconds kDefaultTickInterval{1000};  // 1 ms

    explicit Engine(std::chrono::microseconds tickInterval = kDefaultTickInterval);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // Precisa ser definido antes do start().
    void setTickCallback(TickCallback callback);

    // Andamento do relógio musical. Precisa ser definido antes do start():
    // depois disso o Playhead pertence à thread de tempo, e mudar o andamento
    // ao vivo é o SCH-10 (#56). Devolve false para BPM não positivo.
    bool setBpm(double bpm);
    double bpm() const;

    // Posição musical publicada pela thread de tempo. Segura de ler de
    // qualquer thread: sai sob o mesmo mutex que o wait_for já toma.
    double positionInBeats() const;
    double positionInCycles() const;
    double positionInSeconds() const;

    // Sobe a thread de tempo. Devolve false se já estiver rodando.
    bool start();

    // Só sinaliza; seguro de chamar de qualquer thread. Não faz join.
    void requestStop();

    // Sinaliza e espera a thread de tempo terminar. Idempotente.
    void stop();

    bool isRunning() const { return running_.load(std::memory_order_acquire); }

    std::uint64_t tickCount() const {
        return tickCount_.load(std::memory_order_relaxed);
    }

    // Estatísticas do delta time, para diagnosticar jitter do agendador.
    struct TickStats {
        std::uint64_t ticks = 0;
        double elapsedSeconds = 0.0;
        double minDelta = 0.0;
        double maxDelta = 0.0;

        double averageDelta() const {
            return ticks > 1 ? elapsedSeconds / static_cast<double>(ticks - 1)
                             : 0.0;
        }
    };

    // Seguro de chamar a qualquer momento: lê sob o mesmo mutex que a thread
    // de tempo já usa entre as voltas.
    TickStats stats() const;

    // O laço principal, executado na thread que chamou. Devolve quando o
    // motor for parado ou quando chegar um SIGINT/SIGTERM.
    void runUntilStopped();

private:
    void timeLoop();

    std::chrono::microseconds tickInterval_;
    TickCallback tickCallback_;

    // Depois do start() o cursor pertence à thread de tempo — é ela que o
    // empurra, uma vez por volta. É o que a DoD da SCH-01 (#4) pede ao falar
    // em "thread gerenciando a progressão do tempo (playhead)".
    Playhead playhead_;

    std::thread timeThread_;
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> tickCount_{0};

    // Protege as estatísticas e serve de espera entre as voltas. A thread de
    // tempo já o tomava para o wait_for, então registrar o delta aqui dentro
    // não acrescenta contenção nenhuma.
    mutable std::mutex mutex_;
    std::condition_variable stopCondition_;

    TickStats stats_;

    // Cópias publicadas sob mutex_ para a thread principal poder ler sem
    // encostar no Playhead, que é da thread de tempo.
    double positionInBeats_ = 0.0;
    double positionInSeconds_ = 0.0;
};

}  // namespace mus

#endif  // MUS_ENGINE_ENGINE_H

#ifndef MUS_ENGINE_ENGINE_H
#define MUS_ENGINE_ENGINE_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

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
    using TickCallback = std::function<void(std::uint64_t tick)>;

    static constexpr std::chrono::microseconds kDefaultTickInterval{1000};  // 1 ms

    explicit Engine(std::chrono::microseconds tickInterval = kDefaultTickInterval);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // Precisa ser definido antes do start().
    void setTickCallback(TickCallback callback);

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

    // O laço principal, executado na thread que chamou. Devolve quando o
    // motor for parado ou quando chegar um SIGINT/SIGTERM.
    void runUntilStopped();

private:
    void timeLoop();

    std::chrono::microseconds tickInterval_;
    TickCallback tickCallback_;

    std::thread timeThread_;
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> tickCount_{0};

    std::mutex mutex_;
    std::condition_variable stopCondition_;
};

}  // namespace mus

#endif  // MUS_ENGINE_ENGINE_H

#include "engine/engine.h"

#include <csignal>

namespace mus {
namespace {

// sig_atomic_t volátil é o único tipo que o padrão garante ser seguro de
// escrever de dentro de um tratador de sinal.
volatile std::sig_atomic_t g_shutdownRequested = 0;

extern "C" void handleShutdownSignal(int) {
    g_shutdownRequested = 1;
}

}  // namespace

void installShutdownHandler() {
    std::signal(SIGINT, handleShutdownSignal);
    std::signal(SIGTERM, handleShutdownSignal);
}

bool shutdownRequested() { return g_shutdownRequested != 0; }

Engine::Engine(std::chrono::microseconds tickInterval)
    : tickInterval_(tickInterval) {}

Engine::~Engine() { stop(); }

void Engine::setTickCallback(TickCallback callback) {
    tickCallback_ = std::move(callback);
}

bool Engine::start() {
    if (running_.load(std::memory_order_acquire)) {
        return false;
    }
    running_.store(true, std::memory_order_release);
    tickCount_.store(0, std::memory_order_relaxed);
    timeThread_ = std::thread(&Engine::timeLoop, this);
    return true;
}

void Engine::requestStop() {
    running_.store(false, std::memory_order_release);
    // Acorda a thread de tempo na hora, em vez de deixá-la esperar o intervalo
    // corrente terminar.
    stopCondition_.notify_all();
}

void Engine::stop() {
    requestStop();
    if (timeThread_.joinable()) {
        timeThread_.join();
    }
}

Engine::TickStats Engine::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void Engine::timeLoop() {
    Clock clock;

    while (running_.load(std::memory_order_acquire)) {
        const std::uint64_t tick =
            tickCount_.fetch_add(1, std::memory_order_relaxed) + 1;
        const double delta = clock.tick();

        if (tickCallback_) {
            tickCallback_(tick, delta);
        }

        // Espera com predicado em vez de sleep puro: assim a parada é imediata
        // e o laço não consome CPU girando em vazio (SCH-12, #66).
        std::unique_lock<std::mutex> lock(mutex_);

        stats_.ticks = tick;
        // O tempo decorrido vem do relógio, não da soma dos deltas: somar
        // acumularia erro de arredondamento a cada volta (MAT-08, #45).
        stats_.elapsedSeconds = clock.elapsed();

        // O primeiro delta mede o intervalo entre a construção do relógio e a
        // primeira volta, que não é um intervalo entre ticks.
        if (tick == 2) {
            stats_.minDelta = delta;
            stats_.maxDelta = delta;
        } else if (tick > 2) {
            if (delta < stats_.minDelta) {
                stats_.minDelta = delta;
            }
            if (delta > stats_.maxDelta) {
                stats_.maxDelta = delta;
            }
        }

        stopCondition_.wait_for(lock, tickInterval_, [this] {
            return !running_.load(std::memory_order_acquire);
        });
    }
}

void Engine::runUntilStopped() {
    // A flag de sinal não pode ser acordada por condition_variable — chamar
    // notify de dentro de um tratador de sinal é comportamento indefinido.
    // Por isso a thread principal a consulta periodicamente, em fatias curtas
    // o bastante para o Ctrl+C parecer instantâneo e longas o bastante para
    // não pesar na CPU.
    constexpr auto kPollInterval = std::chrono::milliseconds(20);

    while (isRunning() && !shutdownRequested()) {
        std::this_thread::sleep_for(kPollInterval);
    }

    stop();
}

}  // namespace mus

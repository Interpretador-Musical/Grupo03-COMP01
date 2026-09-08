#ifndef MUS_ENGINE_CLOCK_H
#define MUS_ENGINE_CLOCK_H

#include <chrono>

namespace mus {

// Relógio monotônico usado para medir o intervalo entre voltas da thread de
// tempo (delta time).
//
// Usa steady_clock, e não high_resolution_clock, apesar do nome da issue. Na
// libstdc++ o high_resolution_clock é um apelido do system_clock, que segue a
// hora do sistema e pode ANDAR PARA TRÁS quando o NTP ajusta o relógio da
// máquina — um delta negativo faria a música saltar. O steady_clock é o único
// que o padrão obriga a ser monotônico, que é literalmente o NFR da issue.
class Clock {
public:
    using Source = std::chrono::steady_clock;
    using Seconds = std::chrono::duration<double>;

    static_assert(Source::is_steady,
                  "o relógio precisa ser monotônico para nunca produzir um "
                  "delta negativo");

    Clock() { reset(); }

    void reset() {
        start_ = Source::now();
        last_ = start_;
    }

    // Segundos desde a chamada anterior (ou desde o reset, na primeira).
    double tick() {
        const Source::time_point now = Source::now();
        const double delta = Seconds(now - last_).count();
        last_ = now;
        return delta;
    }

    // Segundos desde o reset. É esta a fonte do tempo musical, e não a soma
    // dos deltas: somar acumula erro de arredondamento a cada volta, que é o
    // drift progressivo que o MAT-08 (#45) vai caçar.
    double elapsed() const { return Seconds(Source::now() - start_).count(); }

private:
    Source::time_point start_;
    Source::time_point last_;
};

}  // namespace mus

#endif  // MUS_ENGINE_CLOCK_H

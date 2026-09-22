#ifndef MUS_CORE_SOUND_EVENT_H
#define MUS_CORE_SOUND_EVENT_H

#include <type_traits>

namespace mus {

// A moeda de troca entre a linguagem e o áudio, conforme a ata de 19/08: o
// interpretador não toca som, ele produz estes eventos.
//
// Tempos em double porque eles são acumulados ao longo da peça inteira e float
// perderia resolução ao fim de alguns minutos (NFR do MAT-02, #10). O volume
// fica em float: ele não acumula, é só um ganho aplicado por evento.
struct SoundEvent {
    double startTime = 0.0;  // segundos desde o início da execução
    double duration = 0.0;   // segundos
    double frequency = 0.0;  // Hz
    float volume = 1.0f;     // 0.0 a 1.0

    constexpr double endTime() const { return startTime + duration; }
};

// É esta garantia que permite o SoundEvent atravessar o ring buffer lock-free
// do SCH-03 (#17): copiável byte a byte, sem construtor a executar e sem
// nenhuma alocação escondida.
static_assert(std::is_trivially_copyable<SoundEvent>::value,
              "SoundEvent precisa ser trivialmente copiável para trafegar no "
              "ring buffer da thread de áudio");
static_assert(std::is_standard_layout<SoundEvent>::value,
              "SoundEvent precisa ter layout padrão");

}  // namespace mus

#endif  // MUS_CORE_SOUND_EVENT_H

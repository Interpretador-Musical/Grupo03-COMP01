
#ifndef MUS_CORE_SOUND_EVENT_H
#define MUS_CORE_SOUND_EVENT_H

#include <cstdint>
#include <type_traits>

namespace mus {

// Identifica a origem do audio sem armazenar strings ou
// ponteiros dentro dos eventos transportados pelo RingBuffer.
enum class SoundEventType : std::uint8_t {
    Synth,
    Sample
};

// Evento musical produzido pelo interpretador.
//
// Os quatro primeiros campos preservam a representacao
// utilizada pelas implementacoes anteriores.
//
// Eventos Synth utilizam frequency.
// Eventos Sample utilizam sampleId para identificar um
// AudioBuffer previamente carregado.
//
// O evento nao possui buffers nem realiza alocacoes.
struct SoundEvent {
    double startTime = 0.0;
    double duration = 0.0;
    double frequency = 0.0;
    float volume = 1.0f;

    SoundEventType type = SoundEventType::Synth;
    std::uint32_t sampleId = 0;

    constexpr double endTime() const {
        return startTime + duration;
    }
};

// Requisito do RingBuffer lock-free (SCH-03, #17).
static_assert(std::is_trivially_copyable<SoundEvent>::value,
              "SoundEvent precisa ser trivialmente copiavel para o RingBuffer");

static_assert(std::is_standard_layout<SoundEvent>::value,
              "SoundEvent precisa ter layout padrao");

}  // namespace mus

#endif  // MUS_CORE_SOUND_EVENT_H

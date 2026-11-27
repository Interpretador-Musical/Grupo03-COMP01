/**
 * @file sound_event.h
 * @brief Definição da instrução da Representação Intermediária (`SoundEvent`).
 */

#ifndef MUS_CORE_SOUND_EVENT_H
#define MUS_CORE_SOUND_EVENT_H

#include <cstdint>
#include <type_traits>

namespace mus {

/**
 * @brief Identifica a fonte sonora de um evento sem armazenar strings ou ponteiros.
 *
 * Mantém `SoundEvent` trivialmente copiável para trafegar pelo `SpscRingBuffer`
 * lock-free sem alocações na thread de áudio.
 */
enum class SoundEventType : std::uint8_t {
    Synth,  ///< Nota sintetizada (usa `frequency`, ou `0.0` em pausas).
    Sample  ///< Sample PCM pré-carregado em `SampleBank` (usa `sampleId`).
};

/**
 * @brief Evento musical temporizado produzido pelo interpretador (IR).
 *
 * Representa uma instrução autocontida na `Timeline`:
 * - Eventos `Synth` utilizam `frequency` em Hz (`0.0` e `volume == 0.0f` indicam pausa).
 * - Eventos `Sample` utilizam `sampleId` referenciando um `AudioBuffer`.
 *
 * Não possui buffers nem realiza alocações dinâmicas.
 */
struct SoundEvent {
    double startTime = 0.0;                   ///< Instante de início em segundos.
    double duration = 0.0;                    ///< Duração do evento em segundos.
    double frequency = 0.0;                   ///< Frequência em Hz (`0.0` em pausas/samples).
    float volume = 1.0f;                      ///< Ganho escalar no intervalo `[0.0, 1.0]`.

    SoundEventType type = SoundEventType::Synth; ///< Tipo da fonte sonora.
    std::uint32_t sampleId = 0;                  ///< Identificador do sample (quando `type == Sample`).
    float cutoffHz = 0.0f;                       ///< Corte do filtro passa-baixa BiQuad em Hz (`0.0f` = desligado, DSP-11).

    /**
     * @brief Calcula o instante final do evento em segundos.
     * @return Soma `startTime + duration`.
     */
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

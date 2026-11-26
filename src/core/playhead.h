/**
 * @file playhead.h
 * @brief Representação Intermediária `Timeline` e cursor temporal `Playhead`.
 */

#ifndef MUS_CORE_PLAYHEAD_H
#define MUS_CORE_PLAYHEAD_H

#include <cstddef>
#include <vector>

#include "core/hap.h"
#include "core/sound_event.h"

namespace mus {

/**
 * @brief Lista de eventos temporizados produzida pelo interpretador (IR).
 *
 * Armazena a sequência completa de `SoundEvent`s antes da síntese sonora ou
 * exportação para arquivo.
 */
class Timeline {
public:
    /**
     * @brief Acrescenta um evento ao final da timeline na ordem de agendamento.
     * @param event Evento sonoro ou pausa a registrar.
     */
    void add(const SoundEvent& event) { events_.push_back(event); }

    /** @brief Remove todos os eventos armazenados na timeline. */
    void clear() { events_.clear(); }

    /**
     * @brief Verifica se a timeline não contém nenhum evento.
     * @return `true` se vazia; `false` caso contrário.
     */
    bool empty() const { return events_.empty(); }

    /**
     * @brief Retorna a quantidade total de eventos na timeline.
     * @return Número de `SoundEvent`s armazenados.
     */
    std::size_t size() const { return events_.size(); }

    /**
     * @brief Acessa a lista de eventos armazenados.
     * @return Referência constante para o vetor de `SoundEvent`.
     */
    const std::vector<SoundEvent>& events() const { return events_; }

    /**
     * @brief Ordena estavelmente os eventos pelo instante de início (`startTime`).
     *
     * Necessária quando há acordes ou trilhas paralelas (CMP-07, #39), onde
     * cada trilha emite seus eventos sequencialmente e a ordenação estável os
     * intercala preservando a ordem relativa de agendamento em empates.
     */
    void sortByStartTime();

    /**
     * @brief Calcula o instante em que o último evento termina de soar.
     * @return Duração total da peça em segundos (`0.0` se vazia).
     */
    double duration() const;

private:
    std::vector<SoundEvent> events_;
};

/**
 * @brief Cursor temporal que converte durações musicais (beats) em segundos.
 *
 * Tocar uma nota não bloqueia nem emite som imediatamente: agenda um evento
 * na posição atual do cursor e o empurra para a frente, transformando laços
 * em padrões rítmicos. O acúmulo é feito em beats com soma compensada de
 * Neumaier (MAT-08, #45) e convertido com âncoras ao trocar o BPM, evitando
 * reescrever o passado e eliminando drift acumulado.
 */
class Playhead {
public:
    /** @brief Andamento inicial padrão em batidas por minuto (120 BPM). */
    static constexpr double kDefaultBpm = 120.0;
    /** @brief Quantidade padrão de tempos por ciclo/compasso 4/4. */
    static constexpr double kBeatsPerCycle = 4.0;

    /**
     * @brief Constrói o cursor com um andamento inicial.
     * @param bpm Andamento inicial em BPM (usa `kDefaultBpm` se inválido).
     */
    explicit Playhead(double bpm = kDefaultBpm);

    /**
     * @brief Altera o andamento congelando o tempo já decorrido numa âncora.
     * @param bpm Novo andamento em BPM (precisa ser finito e positivo).
     * @return `true` se aplicado; `false` para BPM inválido (sem alterar estado).
     */
    bool setBpm(double bpm);

    /**
     * @brief Retorna o andamento corrente em BPM.
     * @return BPM atual.
     */
    double bpm() const { return bpm_; }

    /**
     * @brief Duração de um tempo (beat) em segundos no andamento atual.
     * @return `60.0 / bpm()`.
     */
    double secondsPerBeat() const;

    /**
     * @brief Duração de um ciclo completo (`kBeatsPerCycle` beats) em segundos.
     * @return Segundos por ciclo.
     */
    double secondsPerCycle() const;

    /**
     * @brief Converte uma duração em beats para segundos no andamento atual.
     * @param beats Quantidade de tempos.
     * @return Duração correspondente em segundos.
     */
    double beatsToSeconds(double beats) const;

    /**
     * @brief Converte uma duração em segundos para beats no andamento atual.
     * @param seconds Duração em segundos.
     * @return Quantidade correspondente de tempos.
     */
    double secondsToBeats(double seconds) const;

    /**
     * @brief Posição total acumulada do cursor em beats desde o início da peça.
     * @return Posição em tempos.
     */
    double positionInBeats() const { return anchorBeats_ + beatsDesdeAncora(); }

    /**
     * @brief Posição total do cursor expressa em ciclos de `kBeatsPerCycle` beats.
     * @return Posição em ciclos.
     */
    double positionInCycles() const { return positionInBeats() / kBeatsPerCycle; }

    /**
     * @brief Posição atual do cursor em segundos.
     * @return Instante corrente em segundos desde o início da peça.
     */
    double now() const;

    /**
     * @brief Verifica se uma duração pode avançar sem negativos, `NaN` ou overflow.
     * @param beats Duração candidata em tempos.
     * @return `true` se válida e representável; `false` caso contrário (sem alterar estado).
     */
    bool canAdvance(double beats) const;

    /**
     * @brief Avança o cursor pela duração informada usando soma compensada de Neumaier.
     * @param beats Duração em tempos (ignorada se `!canAdvance(beats)`).
     */
    void advance(double beats);

    /**
     * @brief Sincroniza o cursor com um instante absoluto do relógio (SCH-02, #11).
     *
     * Usado pelo motor de tempo real para derivar a posição diretamente do
     * relógio monotônico (nunca avança para trás).
     *
     * @param elapsedSeconds Tempo decorrido em segundos.
     */
    void syncToSeconds(double elapsedSeconds);

    /**
     * @brief Avança o cursor sem emitir evento (equivalente a `advance(beats)`).
     * @param beats Duração da pausa em tempos.
     */
    void rest(double beats) { advance(beats); }

    /** @brief Zera âncoras, contagem de beats e compensação, mantendo o BPM atual. */
    void reset();

    /**
     * @brief Agenda uma nota na posição atual e avança o cursor.
     * @pre `canAdvance(durationInBeats)` deve ser verdadeiro.
     * @param note Nota com altura MIDI e volume.
     * @param durationInBeats Duração em tempos.
     * @return `SoundEvent` preenchido com início e duração em segundos.
     */
    SoundEvent play(const Note& note, double durationInBeats);

    /**
     * @brief Agenda uma nota por número MIDI e volume na posição atual e avança.
     * @pre `canAdvance(durationInBeats)` deve ser verdadeiro.
     * @param midiNote Número MIDI (aceita fracionários para microtons).
     * @param durationInBeats Duração em tempos.
     * @param volume Ganho da nota em `[0.0, 1.0]`.
     * @return `SoundEvent` preenchido.
     */
    SoundEvent play(double midiNote, double durationInBeats, float volume = 1.0f);

    /**
     * @brief Agenda uma nota na posição atual **sem** avançar o cursor (para acordes).
     * @param note Nota com altura MIDI e volume.
     * @param durationInBeats Duração em tempos.
     * @return `SoundEvent` posicionado no instante `now()`.
     */
    SoundEvent playHere(const Note& note, double durationInBeats) const;

private:
    double bpm_;

    // O tempo é acumulado em tempos (beats), não em segundos, e convertido só
    // na emissão. Somar segundos a cada nota acumularia erro de arredondamento
    // ao longo da peça — é exatamente o drift que o MAT-08 (#45) vai caçar.
    //
    // A âncora existe porque uma mudança de andamento não pode reescrever o
    // passado: ao trocar o BPM, congelamos os segundos já decorridos em
    // anchorSeconds_ e recomeçamos a contagem de tempos a partir dali.
    double anchorSeconds_ = 0.0;
    double anchorBeats_ = 0.0;
    double beatsSinceAnchor_ = 0.0;

    // Soma compensada (Neumaier) de beatsSinceAnchor_ — MAT-08 (#45). Somar
    // durações como 0.1 ou 1/3, que não têm representação exata em binário,
    // perde um pouco a cada nota; a perda cresce com o número de notas e, numa
    // peça de 10^6 eventos, chegava a mover ~1% dos inícios para o frame
    // vizinho. compensacao_ guarda os bits que cada soma descartou, e a
    // posição é lida como beatsSinceAnchor_ + compensacao_. As medições estão
    // em docs/drift_temporal.md.
    double compensacao_ = 0.0;

    double beatsDesdeAncora() const { return beatsSinceAnchor_ + compensacao_; }
};

}  // namespace mus

#endif  // MUS_CORE_PLAYHEAD_H

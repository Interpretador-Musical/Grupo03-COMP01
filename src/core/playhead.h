#ifndef MUS_CORE_PLAYHEAD_H
#define MUS_CORE_PLAYHEAD_H

#include <cstddef>
#include <vector>

#include "core/hap.h"
#include "core/sound_event.h"

namespace mus {

// A lista de eventos produzida por uma execução — a saída do interpretador,
// antes de qualquer coisa virar som.
class Timeline {
public:
    void add(const SoundEvent& event) { events_.push_back(event); }
    void clear() { events_.clear(); }
    bool empty() const { return events_.empty(); }
    std::size_t size() const { return events_.size(); }
    const std::vector<SoundEvent>& events() const { return events_; }

    // Os eventos entram em ordem de agendamento, que não é ordem de início
    // quando há acorde ou (mais tarde) trilhas paralelas com merge.
    void sortByStartTime();

    // Instante em que o último evento termina de soar. É o que o INT-03 (#19)
    // vai usar para não fechar o programa no meio da última nota.
    double duration() const;

private:
    std::vector<SoundEvent> events_;
};

// O cursor de tempo. É o conceito central registrado na ata de 19/08: tocar
// uma nota não executa nada na hora — agenda um evento na posição atual e
// empurra o cursor para frente. É isso que faz um laço virar padrão rítmico e
// uma função virar motivo musical.
class Playhead {
public:
    static constexpr double kDefaultBpm = 120.0;
    // Um ciclo é um compasso 4/4, ou seja, quatro tempos.
    static constexpr double kBeatsPerCycle = 4.0;

    explicit Playhead(double bpm = kDefaultBpm);

    // Devolve false (e não muda nada) para BPM não positivo, deixando a
    // decisão de como reportar para quem chamou — o core não conhece a CLI.
    bool setBpm(double bpm);
    double bpm() const { return bpm_; }

    double secondsPerBeat() const;
    double secondsPerCycle() const;
    double beatsToSeconds(double beats) const;
    double secondsToBeats(double seconds) const;

    double positionInBeats() const { return totalBeats_; }
    double positionInCycles() const { return totalBeats_ / kBeatsPerCycle; }

    // Posição atual do cursor, em segundos.
    double now() const;

    // Avança sem agendar nada — é a pausa.
    void advance(double beats);
    void rest(double beats) { advance(beats); }

    void reset();

    // Agenda a nota na posição atual e avança o cursor pela duração dela.
    SoundEvent play(const Note& note, double durationInBeats);
    SoundEvent play(double midiNote, double durationInBeats, float volume = 1.0f);

    // Agenda sem avançar o cursor. É o que permite acorde: várias notas no
    // mesmo instante, com uma única chamada a play() no fim para andar.
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
    double beatsSinceAnchor_ = 0.0;

    // Posição total em tempos, só para consulta/diagnóstico.
    double totalBeats_ = 0.0;
};

}  // namespace mus

#endif  // MUS_CORE_PLAYHEAD_H

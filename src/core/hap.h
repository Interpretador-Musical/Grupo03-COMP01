#ifndef MUS_CORE_HAP_H
#define MUS_CORE_HAP_H

#include "core/arc.h"
#include "core/pitch.h"

namespace mus {

// Conteúdo musical de um Hap. A nota fica em MIDI, e não em Hz, porque
// transposição e intervalo são somas em semitons — aritmética que a linguagem
// vai expor ao usuário (ex.: `nota + 2`).
struct Note {
    double midi = 60.0;   // 60 = Dó 4
    float volume = 1.0f;

    double frequency() const { return midiToFrequency(midi); }
};

// Um evento posicionado no tempo de ciclo.
//
//   whole -> a extensão completa do evento
//   part  -> o pedaço visível na consulta atual
//
// Quando uma nota atravessa a fronteira de um ciclo, ela é consultada em mais
// de um pedaço, mas só o fragmento que começa junto com `whole` carrega o
// ataque. Sem essa distinção, uma nota longa dispararia de novo a cada ciclo.
struct Hap {
    Arc whole;
    Arc part;
    Note value;

    bool hasOnset() const { return nearlyEqual(whole.begin, part.begin); }

    double duration() const { return whole.duration(); }
};

}  // namespace mus

#endif  // MUS_CORE_HAP_H

#ifndef MUS_CORE_ARC_H
#define MUS_CORE_ARC_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "core/pitch.h"

namespace mus {

// Intervalo semiaberto [begin, end) medido em ciclos, não em segundos. Um ciclo
// é um compasso: 0.5 é a metade do primeiro compasso e 1.5 a metade do segundo.
//
// Manter o arco em unidades de ciclo é o que deixa a matemática rítmica
// independente do BPM — a conversão para segundos acontece só na hora de emitir
// o SoundEvent, no Playhead (MAT-02, #10).
struct Arc {
    double begin = 0.0;
    double end = 0.0;

    constexpr double duration() const { return end - begin; }

    constexpr bool isEmpty() const { return end <= begin; }

    // Semiaberto: o instante `end` já pertence ao próximo arco. É isso que
    // impede uma nota de disparar duas vezes na fronteira entre dois ciclos.
    constexpr bool contains(double t) const { return t >= begin && t < end; }

    constexpr Arc shifted(double offset) const {
        return Arc{begin + offset, end + offset};
    }

    // Intersecção. Devolve um arco vazio quando não há sobreposição.
    Arc sect(const Arc& other) const {
        const double b = std::max(begin, other.begin);
        const double e = std::min(end, other.end);
        if (e <= b) {
            return Arc{b, b};
        }
        return Arc{b, e};
    }

    bool overlaps(const Arc& other) const { return !sect(other).isEmpty(); }

    // Quebra o arco nas fronteiras de ciclo inteiro: [0.5, 2.25) vira
    // [0.5, 1), [1, 2) e [2, 2.25). É a base da divisão recursiva do
    // MAT-05 (#28) e da consulta por ciclo do SCH-06 (#35).
    //
    // Aloca, então não deve ser chamada de dentro do callback de áudio.
    std::vector<Arc> cycles() const {
        std::vector<Arc> result;
        if (isEmpty()) {
            return result;
        }

        double cursor = begin;
        while (cursor < end) {
            // floor(cursor) + 1 é sempre estritamente maior que cursor, então
            // o laço avança mesmo quando o arco começa exatamente em um ciclo.
            const double nextBoundary = std::floor(cursor) + 1.0;
            const double stop = std::min(nextBoundary, end);
            result.push_back(Arc{cursor, stop});
            cursor = stop;
        }
        return result;
    }

    // Divide o arco em `n` sub-arcos de mesma duração: [0, 1) dividido em 3
    // vira [0, 1/3), [1/3, 2/3) e [2/3, 1). É o passo de um nível da
    // subdivisão recursiva do MAT-05 (#28) — ver core/subdivision.h.
    //
    // Cada fronteira é calculada direto de `begin + duração * i / n`, e não
    // somando `duração / n` a um cursor: a soma repetida acumularia erro de
    // arredondamento a cada pedaço. Assim o fim de um pedaço é bit a bit o
    // começo do seguinte (mesma expressão) e o último termina exatamente em
    // `end`.
    //
    // Aloca, então não deve ser chamada de dentro do callback de áudio.
    std::vector<Arc> divide(std::size_t n) const {
        std::vector<Arc> result;
        if (n == 0 || isEmpty()) {
            return result;
        }

        result.reserve(n);
        const double span = duration();
        const double count = static_cast<double>(n);
        double previous = begin;
        for (std::size_t i = 1; i <= n; ++i) {
            const double boundary =
                (i == n) ? end : begin + span * static_cast<double>(i) / count;
            result.push_back(Arc{previous, boundary});
            previous = boundary;
        }
        return result;
    }
};

inline bool operator==(const Arc& a, const Arc& b) {
    return nearlyEqual(a.begin, b.begin) && nearlyEqual(a.end, b.end);
}

inline bool operator!=(const Arc& a, const Arc& b) { return !(a == b); }

}  // namespace mus

#endif  // MUS_CORE_ARC_H

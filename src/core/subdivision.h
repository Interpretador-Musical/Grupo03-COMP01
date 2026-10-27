#ifndef MUS_CORE_SUBDIVISION_H
#define MUS_CORE_SUBDIVISION_H

#include <cstddef>
#include <utility>
#include <vector>

#include "core/arc.h"
#include "core/hap.h"

namespace mus {

// Um passo de um agrupamento rítmico, na forma de árvore.
//
//   folha com nota  -> toca a nota no espaço que recebeu
//   folha sem nota  -> pausa: ocupa o espaço, mas não gera Hap
//   grupo           -> divide o próprio espaço igualmente entre os filhos
//
// `[do [re mi]]` vira um grupo com dois filhos: a folha `do` e o grupo
// `[re mi]`. A gramática ainda não tem colchetes, então por enquanto a árvore
// é montada à mão (nos testes); quando o parser ganhar agrupamento, a AST
// desce para cá.
struct Step {
    bool hasNote = false;
    Note note;
    std::vector<Step> children;

    bool isGroup() const { return !children.empty(); }

    static Step rest() { return Step{}; }

    static Step of(const Note& n) { return Step{true, n, {}}; }

    static Step group(std::vector<Step> steps) {
        return Step{false, Note{}, std::move(steps)};
    }
};

namespace detail {

inline void subdivideInto(const Step& step, const Arc& span,
                          std::vector<Hap>& out) {
    if (!step.isGroup()) {
        if (step.hasNote) {
            out.push_back(Hap{span, span, step.note});
        }
        return;
    }

    // Os filhos dividem o espaço do pai em partes iguais, não importa quantos
    // netos cada um tenha — é por isso que em `[do [re mi]]` o `do` dura o
    // mesmo que `re` e `mi` juntos.
    const std::vector<Arc> slots = span.divide(step.children.size());
    for (std::size_t i = 0; i < slots.size(); ++i) {
        subdivideInto(step.children[i], slots[i], out);
    }
}

}  // namespace detail

// Divisão recursiva de arcos do MAT-05 (#28): distribui a árvore `root` dentro
// de `span` e devolve um Hap por nota, em ordem de início.
//
// O resultado já sai com `whole == part`: cada nota cabe inteira no espaço que
// recebeu. Recortar por ciclo é trabalho do Pattern::query, depois.
//
// Aloca, então não deve ser chamada de dentro do callback de áudio.
inline std::vector<Hap> subdivide(const Step& root, const Arc& span) {
    std::vector<Hap> result;
    if (!span.isEmpty()) {
        detail::subdivideInto(root, span, result);
    }
    return result;
}

}  // namespace mus

#endif  // MUS_CORE_SUBDIVISION_H

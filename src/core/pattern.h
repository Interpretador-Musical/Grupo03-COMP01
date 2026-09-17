#ifndef MUS_CORE_PATTERN_H
#define MUS_CORE_PATTERN_H

#include <algorithm>
#include <cstddef>
#include <vector>

#include "core/hap.h"

namespace mus {

// Agrupamento de Haps.
//
// Deliberadamente um contêiner simples. O Pattern funcional — padrão como
// função de consulta, que é o que permite combinadores encadeados — é o
// MAT-06 (#34), na Sprint 4. Construí-lo agora seria adiantar trabalho sobre
// uma gramática que ainda nem foi definida.
class Pattern {
public:
    void add(const Hap& hap) { haps_.push_back(hap); }

    void clear() { haps_.clear(); }

    bool empty() const { return haps_.empty(); }

    std::size_t size() const { return haps_.size(); }

    const std::vector<Hap>& haps() const { return haps_; }

    void sortByOnset() {
        std::stable_sort(haps_.begin(), haps_.end(),
                         [](const Hap& a, const Hap& b) {
                             return a.part.begin < b.part.begin;
                         });
    }

    // Devolve os Haps que intersectam `span`, com `part` já recortado ao
    // pedaço visível. `whole` é preservado, para que hasOnset() continue
    // distinguindo o ataque da continuação de uma nota cortada.
    std::vector<Hap> query(const Arc& span) const {
        std::vector<Hap> result;
        for (const Hap& hap : haps_) {
            const Arc visible = hap.part.sect(span);
            if (visible.isEmpty()) {
                continue;
            }
            result.push_back(Hap{hap.whole, visible, hap.value});
        }
        return result;
    }

private:
    std::vector<Hap> haps_;
};

}  // namespace mus

#endif  // MUS_CORE_PATTERN_H

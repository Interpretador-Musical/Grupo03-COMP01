// Os tipos do core são header-only: enxutos, sem estado global e sem alocação
// escondida, como pede o NFR do MAT-01 (#3).
//
// Esta unidade de tradução existe só para que os cabeçalhos entrem na build e
// sejam compilados de verdade — sem ela, um erro neles só apareceria quando
// alguém os incluísse pela primeira vez.

#include "core/arc.h"
#include "core/hap.h"
#include "core/pattern.h"
#include "core/pitch.h"
#include "core/sound_event.h"

namespace mus {
namespace {

static_assert(sizeof(Arc) == 2 * sizeof(double), "Arc deve ser só dois doubles");
static_assert(std::is_trivially_copyable<Arc>::value, "Arc deve ser trivial");
static_assert(std::is_trivially_copyable<Note>::value, "Note deve ser trivial");
static_assert(std::is_trivially_copyable<Hap>::value, "Hap deve ser trivial");

// A afinação de referência tem de fechar exatamente.
static_assert(kMidiA4 == 69, "A4 é a nota MIDI 69");

}  // namespace
}  // namespace mus

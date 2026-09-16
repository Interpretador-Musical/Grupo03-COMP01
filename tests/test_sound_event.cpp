#include "doctest.h"

#include <type_traits>

#include "core/sound_event.h"

using namespace mus;

TEST_CASE("endTime é o começo mais a duração") {
    const SoundEvent event{1.5, 0.25, 440.0, 0.8f};

    CHECK(event.endTime() == doctest::Approx(1.75));
}

TEST_CASE("os padrões do SoundEvent são silêncio de duração zero") {
    const SoundEvent event;

    CHECK(event.startTime == doctest::Approx(0.0));
    CHECK(event.duration == doctest::Approx(0.0));
    CHECK(event.frequency == doctest::Approx(0.0));
    CHECK(event.volume == doctest::Approx(1.0f));
    CHECK(event.endTime() == doctest::Approx(0.0));
}

TEST_CASE("SoundEvent atravessa o ring buffer sem construtor nem alocação") {
    // Os static_assert em sound_event.h já travam isto na compilação; o caso
    // existe para que a exigência apareça no relatório do ctest e não seja
    // removida por engano ao mexer na struct (SCH-03, #17).
    CHECK(std::is_trivially_copyable<SoundEvent>::value);
    CHECK(std::is_standard_layout<SoundEvent>::value);
}

TEST_CASE("tempos em double e volume em float, por acúmulo de erro") {
    // Os tempos são somados ao longo da peça inteira e float perderia
    // resolução em poucos minutos (NFR do MAT-02, #10). O volume não acumula.
    CHECK(std::is_same<decltype(SoundEvent::startTime), double>::value);
    CHECK(std::is_same<decltype(SoundEvent::duration), double>::value);
    CHECK(std::is_same<decltype(SoundEvent::frequency), double>::value);
    CHECK(std::is_same<decltype(SoundEvent::volume), float>::value);
}

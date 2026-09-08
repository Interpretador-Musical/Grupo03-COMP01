#include "core/playhead.h"

#include <algorithm>

namespace mus {

void Timeline::sortByStartTime() {
    std::stable_sort(events_.begin(), events_.end(),
                     [](const SoundEvent& a, const SoundEvent& b) {
                         return a.startTime < b.startTime;
                     });
}

double Timeline::duration() const {
    double end = 0.0;
    for (const SoundEvent& event : events_) {
        end = std::max(end, event.endTime());
    }
    return end;
}

Playhead::Playhead(double bpm)
    : bpm_(bpm > 0.0 ? bpm : kDefaultBpm) {}

bool Playhead::setBpm(double bpm) {
    if (!(bpm > 0.0)) {
        return false;
    }
    if (bpm == bpm_) {
        return true;
    }

    // Congela o que já passou antes de trocar o andamento: sem isso, mudar o
    // BPM no meio da peça reescreveria os segundos de tudo que veio antes.
    anchorSeconds_ = now();
    anchorBeats_ = positionInBeats();
    beatsSinceAnchor_ = 0.0;
    bpm_ = bpm;
    return true;
}

double Playhead::secondsPerBeat() const { return 60.0 / bpm_; }

double Playhead::secondsPerCycle() const {
    return secondsPerBeat() * kBeatsPerCycle;
}

double Playhead::beatsToSeconds(double beats) const {
    return beats * secondsPerBeat();
}

double Playhead::secondsToBeats(double seconds) const {
    return seconds / secondsPerBeat();
}

double Playhead::now() const {
    return anchorSeconds_ + beatsSinceAnchor_ * secondsPerBeat();
}

void Playhead::advance(double beats) {
    if (beats <= 0.0) {
        return;  // o cursor nunca anda para trás
    }
    beatsSinceAnchor_ += beats;
}

void Playhead::syncToSeconds(double elapsedSeconds) {
    // O cursor nunca anda para trás. Com um relógio monotônico isso não
    // deveria acontecer, mas a garantia fica aqui e não na confiança.
    if (elapsedSeconds <= anchorSeconds_) {
        beatsSinceAnchor_ = 0.0;
        return;
    }
    beatsSinceAnchor_ = (elapsedSeconds - anchorSeconds_) / secondsPerBeat();
}

void Playhead::reset() {
    anchorSeconds_ = 0.0;
    anchorBeats_ = 0.0;
    beatsSinceAnchor_ = 0.0;
}

SoundEvent Playhead::playHere(const Note& note, double durationInBeats) const {
    SoundEvent event;
    event.startTime = now();
    event.duration = beatsToSeconds(durationInBeats);
    event.frequency = note.frequency();
    event.volume = note.volume;
    return event;
}

SoundEvent Playhead::play(const Note& note, double durationInBeats) {
    const SoundEvent event = playHere(note, durationInBeats);
    advance(durationInBeats);
    return event;
}

SoundEvent Playhead::play(double midiNote, double durationInBeats, float volume) {
    return play(Note{midiNote, volume}, durationInBeats);
}

}  // namespace mus

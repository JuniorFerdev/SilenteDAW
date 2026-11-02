#pragma once
#include "../AudioEngine.h"
#include <vector>

namespace silente::audio
{
// Range editing (Cubase manual, «Edición de rangos», pp. 295–305) on the events of one track.
// Times are seconds; MIDI notes are converted with the tempo. Events that cross a range edge are
// split first, so only what lies inside the range is affected.
struct TimeRange
{
    double start = 0.0, end = 0.0;
    double length() const { return end - start; }
};

void splitAt(std::vector<AudioClip>&, double seconds);                       // Dividir en una posición
void splitRange(std::vector<AudioClip>&, TimeRange);                          // Rango › Dividir (⇧X)
void deleteRange(std::vector<AudioClip>&, std::vector<MidiNote>&, TimeRange, double bpm);   // Suprimir
void deleteTime(std::vector<AudioClip>&, std::vector<MidiNote>&, TimeRange, double bpm);    // Suprimir intervalo de tiempo
void insertSilence(std::vector<AudioClip>&, std::vector<MidiNote>&, TimeRange, double bpm); // Insertar silencio
void truncate(std::vector<AudioClip>&, TimeRange);                            // Truncar
void duplicateRange(std::vector<AudioClip>&, std::vector<MidiNote>&, TimeRange, double bpm);// Duplicar
// Ajustar fundidos al rango (A): a range over the start of an event sets its fade-in, over its end
// the fade-out, and inside it both (fade-in up to the range start, fade-out from the range end).
void adjustFadesToRange(std::vector<AudioClip>&, TimeRange);

// What a range holds on one track, with times relative to the range start (clips in seconds,
// notes in beats). Used to move the range content (drag, ⌥ drag copies) and for cut/copy/paste.
struct RangeContent
{
    std::vector<AudioClip> clips;
    std::vector<MidiNote> notes;
};
RangeContent copyRange(const std::vector<AudioClip>&, const std::vector<MidiNote>&, TimeRange, double bpm);
// Places the content at `at` seconds, on top of what is there (the moved audio is heard).
void pasteContent(std::vector<AudioClip>&, std::vector<MidiNote>&, const RangeContent&, double at, double bpm);
}

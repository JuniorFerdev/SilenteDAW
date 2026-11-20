#pragma once
#include "MidiNote.h"
#include <set>
#include <utility>
#include <vector>

namespace silente::domain
{
// Note selection of the MIDI editor, by note id: it survives erasing, reordering and undo
// snapshots (which keep ids). Pure data, no UI.
class NoteSelection
{
public:
    bool contains(const juce::String& id) const { return ids.count(id) != 0; }
    void add(const juce::String& id) { ids.insert(id); }
    void remove(const juce::String& id) { ids.erase(id); }
    // Returns true when the note ends up selected.
    bool toggle(const juce::String& id);
    void clear() { ids.clear(); }
    bool empty() const { return ids.empty(); }
    int size() const { return (int) ids.size(); }
    void addAll(const NoteSelection& other) { ids.insert(other.ids.begin(), other.ids.end()); }
private:
    std::set<juce::String> ids;
};

// Pure note-editing rules shared by the piano roll and tests. Positions are in beats.
std::vector<int> selectedIndices(const std::vector<MidiNote>& notes, const NoteSelection& selection);
NoteSelection selectAllNotes(const std::vector<MidiNote>& notes);
// Notes that intersect the beat span [startBeats, endBeats) and the pitch span [lowPitch, highPitch].
NoteSelection notesInRegion(const std::vector<MidiNote>& notes, double startBeats, double endBeats, int lowPitch, int highPitch);
// grid <= 0 leaves the value unchanged.
double snapBeat(double beats, double grid);

// Gesture edits. `original` is the note list when the gesture began; `target` has the same size
// and receives the result only at `indices`. They return true when any target note changed.
// Move: the delta is snapped so the anchor note's start lands on the grid; the group keeps its
// intervals and is clamped as a whole (start >= 0, pitch 0..127).
bool moveNotes(const std::vector<MidiNote>& original, std::vector<MidiNote>& target, const std::vector<int>& indices,
               int anchor, double deltaBeats, int deltaPitch, double grid);
// Resize: the anchor's new length is snapped (never below minLength) and the same length change
// is applied to every note of the group (each one also limited to minLength).
bool resizeNotes(const std::vector<MidiNote>& original, std::vector<MidiNote>& target, const std::vector<int>& indices,
                 int anchor, double deltaBeats, double grid, double minLength = 0.0625);
// Removes the selected notes; returns how many were removed.
int eraseNotes(std::vector<MidiNote>& notes, const NoteSelection& selection);
// Split tool: cuts every note of `indices` that contains `atBeat` (start < atBeat < end, both
// parts at least minLength). The right part keeps pitch, velocity and mute and gets a new id;
// it is appended to `notes`. `parts` (optional) receives {left id, right id} per cut.
// Returns the number of cuts.
int splitNotes(std::vector<MidiNote>& notes, const std::vector<int>& indices, double atBeat,
               double minLength = 0.0625, std::vector<std::pair<juce::String, juce::String>>* parts = nullptr);
// Glue tool: the next note with the same pitch after notes[index] (earliest start at or after
// its start, other than itself), or -1.
int nextNoteWithSamePitch(const std::vector<MidiNote>& notes, int index);
// Joins notes[index] with that next note: the first note keeps its properties and now spans
// from its start to the end of the later of both; the second one is removed. Returns the removed
// note's id, or an empty string when there is nothing to glue.
juce::String glueWithNext(std::vector<MidiNote>& notes, int index);
}

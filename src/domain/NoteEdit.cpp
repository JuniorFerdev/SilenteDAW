#include "NoteEdit.h"
#include <algorithm>
#include <cmath>

namespace silente::domain
{
bool NoteSelection::toggle(const juce::String& id)
{
    if (contains(id)) { remove(id); return false; }
    add(id);
    return true;
}

std::vector<int> selectedIndices(const std::vector<MidiNote>& notes, const NoteSelection& selection)
{
    std::vector<int> result;
    for (int i = 0; i < (int) notes.size(); ++i)
        if (selection.contains(notes[(size_t) i].id)) result.push_back(i);
    return result;
}

NoteSelection selectAllNotes(const std::vector<MidiNote>& notes)
{
    NoteSelection s;
    for (const auto& n : notes) s.add(n.id);
    return s;
}

NoteSelection notesInRegion(const std::vector<MidiNote>& notes, double startBeats, double endBeats, int lowPitch, int highPitch)
{
    if (endBeats < startBeats) std::swap(startBeats, endBeats);
    if (highPitch < lowPitch) std::swap(lowPitch, highPitch);
    NoteSelection s;
    for (const auto& n : notes)
        if (n.pitch >= lowPitch && n.pitch <= highPitch && n.startBeats < endBeats && n.startBeats + n.lengthBeats > startBeats)
            s.add(n.id);
    return s;
}

double snapBeat(double beats, double grid) { return grid > 0.0 ? std::round(beats / grid) * grid : beats; }

namespace
{
bool valid(const std::vector<MidiNote>& original, const std::vector<MidiNote>& target, const std::vector<int>& indices, int anchor)
{
    if (original.size() != target.size() || anchor < 0 || anchor >= (int) original.size()) return false;
    for (int i : indices) if (i < 0 || i >= (int) original.size()) return false;
    return ! indices.empty();
}
bool assign(MidiNote& target, int pitch, double start, double length)
{
    if (target.pitch == pitch && target.startBeats == start && target.lengthBeats == length) return false;
    target.pitch = pitch; target.startBeats = start; target.lengthBeats = length;
    return true;
}
}

bool moveNotes(const std::vector<MidiNote>& original, std::vector<MidiNote>& target, const std::vector<int>& indices,
               int anchor, double deltaBeats, int deltaPitch, double grid)
{
    if (! valid(original, target, indices, anchor)) return false;
    const auto& a = original[(size_t) anchor];
    const double snapped = snapBeat(a.startBeats + deltaBeats, grid);
    double d = snapped - a.startBeats;
    double minStart = original[(size_t) indices.front()].startBeats; int lowPitch = 127, highPitch = 0;
    for (int i : indices)
    {
        const auto& n = original[(size_t) i];
        minStart = std::min(minStart, n.startBeats); lowPitch = std::min(lowPitch, n.pitch); highPitch = std::max(highPitch, n.pitch);
    }
    const bool clamped = d < -minStart;
    if (clamped) d = -minStart;
    const int p = std::clamp(deltaPitch, -lowPitch, 127 - highPitch);
    // The anchor takes the exact grid value (no floating drift); the rest keep their offset.
    const double anchorStart = std::max(0.0, clamped ? a.startBeats + d : snapped);
    bool changed = false;
    for (int i : indices)
    {
        const auto& n = original[(size_t) i];
        const double start = i == anchor ? anchorStart : std::max(0.0, n.startBeats + d);
        changed |= assign(target[(size_t) i], n.pitch + p, start, n.lengthBeats);
    }
    return changed;
}

bool resizeNotes(const std::vector<MidiNote>& original, std::vector<MidiNote>& target, const std::vector<int>& indices,
                 int anchor, double deltaBeats, double grid, double minLength)
{
    if (! valid(original, target, indices, anchor)) return false;
    const auto& a = original[(size_t) anchor];
    const double anchorLength = std::max(minLength, snapBeat(a.lengthBeats + deltaBeats, grid));
    const double d = anchorLength - a.lengthBeats;
    bool changed = false;
    for (int i : indices)
    {
        const auto& n = original[(size_t) i];
        const double length = i == anchor ? anchorLength : std::max(minLength, n.lengthBeats + d);
        changed |= assign(target[(size_t) i], n.pitch, n.startBeats, length);
    }
    return changed;
}

int eraseNotes(std::vector<MidiNote>& notes, const NoteSelection& selection)
{
    const auto before = notes.size();
    notes.erase(std::remove_if(notes.begin(), notes.end(), [&](const MidiNote& n) { return selection.contains(n.id); }), notes.end());
    return (int) (before - notes.size());
}

int splitNotes(std::vector<MidiNote>& notes, const std::vector<int>& indices, double atBeat, double minLength,
               std::vector<std::pair<juce::String, juce::String>>* parts)
{
    int cuts = 0;
    const auto count = notes.size();
    for (int i : indices)
    {
        if (i < 0 || (size_t) i >= count) continue;
        auto& n = notes[(size_t) i];
        const double end = n.startBeats + n.lengthBeats;
        if (atBeat - n.startBeats < minLength || end - atBeat < minLength) continue;
        MidiNote right = n;
        right.id = juce::Uuid().toString();
        right.startBeats = atBeat;
        right.lengthBeats = end - atBeat;
        n.lengthBeats = atBeat - n.startBeats;
        if (parts) parts->emplace_back(n.id, right.id);
        notes.push_back(std::move(right));   // may reallocate: `n` is not used afterwards
        ++cuts;
    }
    return cuts;
}

int nextNoteWithSamePitch(const std::vector<MidiNote>& notes, int index)
{
    if (index < 0 || (size_t) index >= notes.size()) return -1;
    const auto& first = notes[(size_t) index];
    int best = -1;
    for (int i = 0; i < (int) notes.size(); ++i)
    {
        const auto& n = notes[(size_t) i];
        if (i == index || n.pitch != first.pitch || n.startBeats < first.startBeats) continue;
        if (best < 0 || n.startBeats < notes[(size_t) best].startBeats) best = i;
    }
    return best;
}

juce::String glueWithNext(std::vector<MidiNote>& notes, int index)
{
    const int next = nextNoteWithSamePitch(notes, index);
    if (next < 0) return {};
    auto& first = notes[(size_t) index];
    const auto& second = notes[(size_t) next];
    const double end = std::max(first.startBeats + first.lengthBeats, second.startBeats + second.lengthBeats);
    first.lengthBeats = end - first.startBeats;
    const auto removed = second.id;
    notes.erase(notes.begin() + next);
    return removed;
}

std::vector<MidiNote> copyNotesRelative(const std::vector<MidiNote>& notes, const std::vector<int>& indices)
{
    std::vector<MidiNote> result;
    double first = 0.0;
    bool any = false;
    for (int i : indices)
    {
        if (i < 0 || i >= (int) notes.size()) continue;
        first = any ? std::min(first, notes[(size_t) i].startBeats) : notes[(size_t) i].startBeats;
        any = true;
        result.push_back(notes[(size_t) i]);
    }
    for (auto& n : result) n.startBeats -= first;
    return result;
}
}

#include "MidiEditCommands.h"
#include "../i18n/Translations.h"
#include <algorithm>
#include <cmath>

namespace silente::app
{
namespace
{
std::vector<MidiNote>* notesOf(const MidiEditContext& c)
{
    if (c.track < 0 || c.track >= (int) c.engine.tracks.size() || !c.engine.tracks[(size_t) c.track]->isMidi) return nullptr;
    return &c.engine.tracks[(size_t) c.track]->notes;
}
void record(const MidiEditContext& c, const juce::String& name)
{
    if (c.beforeEdit) c.beforeEdit(name);
}
void done(const MidiEditContext& c)
{
    if (c.changed) c.changed();
}
}

int moveSelection(const MidiEditContext& c, double deltaBeats, int deltaPitch)
{
    const juce::ScopedLock lock(c.engine.modelLock);
    auto* notes = notesOf(c);
    if (!notes) return 0;
    const auto indices = domain::selectedIndices(*notes, c.selection);
    if (indices.empty() || (deltaBeats == 0.0 && deltaPitch == 0)) return 0;
    auto target = *notes;
    if (!domain::moveNotes(*notes, target, indices, indices.front(), deltaBeats, deltaPitch, 0.0)) return 0;
    record(c, i18n::tr("Mover notas"));
    for (int i : indices) (*notes)[(size_t) i] = target[(size_t) i];
    done(c);
    return (int) indices.size();
}

int resizeSelection(const MidiEditContext& c, double deltaBeats)
{
    const juce::ScopedLock lock(c.engine.modelLock);
    auto* notes = notesOf(c);
    if (!notes) return 0;
    const auto indices = domain::selectedIndices(*notes, c.selection);
    if (indices.empty() || deltaBeats == 0.0) return 0;
    auto target = *notes;
    if (!domain::resizeNotes(*notes, target, indices, indices.front(), deltaBeats, 0.0)) return 0;
    record(c, i18n::tr("Cambiar duración de notas"));
    for (int i : indices) (*notes)[(size_t) i] = target[(size_t) i];
    done(c);
    return (int) indices.size();
}

int deleteSelection(const MidiEditContext& c)
{
    const auto targets = c.selection;
    const int n = deleteNotes(c, targets);
    if (n > 0) c.selection.clear();
    return n;
}

int deleteNotes(const MidiEditContext& c, const domain::NoteSelection& targets)
{
    const juce::ScopedLock lock(c.engine.modelLock);
    auto* notes = notesOf(c);
    if (!notes) return 0;
    const auto indices = domain::selectedIndices(*notes, targets);
    if (indices.empty()) return 0;
    record(c, i18n::tr("Borrar notas"));
    for (int i : indices) c.selection.remove((*notes)[(size_t) i].id);
    const int n = domain::eraseNotes(*notes, targets);
    done(c);
    return n;
}

int quantizeSelection(const MidiEditContext& c, const domain::QuantizeSettings& settings)
{
    const juce::ScopedLock lock(c.engine.modelLock);
    auto* notes = notesOf(c);
    if (!notes) return 0;
    auto copy = *notes;
    const int n = domain::quantizeSelectedNotes(copy, domain::selectedIndices(*notes, c.selection), settings);
    if (n == 0) return 0;
    record(c, i18n::tr("Cuantizar notas"));
    *notes = std::move(copy);
    done(c);
    return n;
}

int duplicateSelection(const MidiEditContext& c, int* newAnchor)
{
    const juce::ScopedLock lock(c.engine.modelLock);
    auto* notes = notesOf(c);
    if (!notes) return 0;
    const auto indices = domain::selectedIndices(*notes, c.selection);
    if (indices.empty()) return 0;
    double first = (*notes)[(size_t) indices.front()].startBeats, last = 0.0;
    for (int i : indices)
    {
        first = std::min(first, (*notes)[(size_t) i].startBeats);
        last = std::max(last, (*notes)[(size_t) i].startBeats + (*notes)[(size_t) i].lengthBeats);
    }
    const double offset = last - first;
    record(c, i18n::tr("Duplicar notas"));
    c.selection.clear();
    for (int i : indices)
    {
        auto copy = (*notes)[(size_t) i];
        copy.id = juce::Uuid().toString();
        copy.startBeats += offset;
        c.selection.add(copy.id);
        notes->push_back(std::move(copy));
    }
    if (newAnchor) *newAnchor = (int) notes->size() - 1;
    done(c);
    return (int) indices.size();
}

int transposeSelection(const MidiEditContext& c, int semitones)
{
    const juce::ScopedLock lock(c.engine.modelLock);
    auto* notes = notesOf(c);
    if (!notes || semitones == 0) return 0;
    const auto indices = domain::selectedIndices(*notes, c.selection);
    if (indices.empty()) return 0;
    int low = 127, high = 0;
    for (int i : indices) { low = std::min(low, (*notes)[(size_t) i].pitch); high = std::max(high, (*notes)[(size_t) i].pitch); }
    const int shift = std::clamp(semitones, -low, 127 - high);
    if (shift == 0) return 0;
    record(c, i18n::tr("Transponer notas"));
    for (int i : indices) (*notes)[(size_t) i].pitch += shift;
    done(c);
    return (int) indices.size();
}

int createNote(const MidiEditContext& c, int pitch, double startBeats, double lengthBeats, int* newIndex)
{
    const juce::ScopedLock lock(c.engine.modelLock);
    auto* notes = notesOf(c);
    if (!notes || lengthBeats <= 0.0) return 0;
    MidiNote n;
    n.pitch = std::clamp(pitch, 0, 127);
    n.startBeats = std::max(0.0, startBeats);
    n.lengthBeats = lengthBeats;
    record(c, i18n::tr("Crear nota"));
    c.selection.clear();
    c.selection.add(n.id);
    notes->push_back(std::move(n));
    if (newIndex) *newIndex = (int) notes->size() - 1;
    done(c);
    return 1;
}

int splitNotesAt(const MidiEditContext& c, const domain::NoteSelection& targets, double atBeat)
{
    const juce::ScopedLock lock(c.engine.modelLock);
    auto* notes = notesOf(c);
    if (!notes) return 0;
    const auto indices = domain::selectedIndices(*notes, targets);
    auto copy = *notes;
    std::vector<std::pair<juce::String, juce::String>> parts;
    const int cuts = domain::splitNotes(copy, indices, atBeat, 0.0625, &parts);
    if (cuts == 0) return 0;
    record(c, i18n::tr("Dividir notas"));
    // A selected note stays selected as two parts.
    for (const auto& [left, right] : parts)
        if (c.selection.contains(left)) c.selection.add(right);
    *notes = std::move(copy);
    done(c);
    return cuts;
}

int glueNote(const MidiEditContext& c, const juce::String& id)
{
    const juce::ScopedLock lock(c.engine.modelLock);
    auto* notes = notesOf(c);
    if (!notes) return 0;
    int index = -1;
    for (int i = 0; i < (int) notes->size(); ++i) if ((*notes)[(size_t) i].id == id) { index = i; break; }
    if (domain::nextNoteWithSamePitch(*notes, index) < 0) return 0;
    record(c, i18n::tr("Pegar notas"));
    c.selection.remove(domain::glueWithNext(*notes, index));
    done(c);
    return 1;
}

int setNotesMuted(const MidiEditContext& c, const domain::NoteSelection& targets, bool muted)
{
    const juce::ScopedLock lock(c.engine.modelLock);
    auto* notes = notesOf(c);
    if (!notes) return 0;
    std::vector<int> changing;
    for (int i : domain::selectedIndices(*notes, targets)) if ((*notes)[(size_t) i].muted != muted) changing.push_back(i);
    if (changing.empty()) return 0;
    record(c, muted ? i18n::tr("Enmudecer notas") : i18n::tr("Desenmudecer notas"));
    for (int i : changing) (*notes)[(size_t) i].muted = muted;
    done(c);
    return (int) changing.size();
}
}

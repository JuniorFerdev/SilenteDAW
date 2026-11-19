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
    const juce::ScopedLock lock(c.engine.modelLock);
    auto* notes = notesOf(c);
    if (!notes || domain::selectedIndices(*notes, c.selection).empty()) return 0;
    record(c, i18n::tr("Borrar notas"));
    const int n = domain::eraseNotes(*notes, c.selection);
    c.selection.clear();
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
}

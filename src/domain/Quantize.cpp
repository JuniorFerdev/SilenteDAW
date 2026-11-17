#include "Quantize.h"
#include <algorithm>
#include <cmath>

namespace silente::domain
{
double gridBeats(int division, bool triplet)
{
    const double straight = 4.0 / std::max(1, division);
    return triplet ? straight * 2.0 / 3.0 : straight;
}
double quantizeBeat(double beats, double grid, double strengthPercent)
{
    if (grid <= 0.0) return beats;
    const double s = std::clamp(strengthPercent, 0.0, 100.0) / 100.0;
    return beats + (std::round(beats / grid) * grid - beats) * s;
}
void quantizeNote(MidiNote& n, const QuantizeSettings& q)
{
    const double grid = gridBeats(q.division, q.triplet);
    n.startBeats = std::max(0.0, quantizeBeat(n.startBeats, grid, q.strengthPercent));
    if (q.quantizeLength)
    {
        const double len = quantizeBeat(n.lengthBeats, grid, q.strengthPercent);
        n.lengthBeats = len > 0.0 ? len : grid;
    }
}
int quantizeNotes(std::vector<MidiNote>& notes, int index, const QuantizeSettings& q)
{
    int changed = 0;
    for (int i = 0; i < (int) notes.size(); ++i)
    {
        if (index >= 0 && i != index) continue;
        const auto before = notes[(size_t) i];
        quantizeNote(notes[(size_t) i], q);
        if (before.startBeats != notes[(size_t) i].startBeats || before.lengthBeats != notes[(size_t) i].lengthBeats) ++changed;
    }
    return changed;
}
int quantizeSelectedNotes(std::vector<MidiNote>& notes, const std::vector<int>& indices, const QuantizeSettings& q)
{
    if (indices.empty()) return quantizeNotes(notes, -1, q);
    int changed = 0;
    for (int i : indices)
    {
        if (i < 0 || i >= (int) notes.size()) continue;
        const auto before = notes[(size_t) i];
        quantizeNote(notes[(size_t) i], q);
        if (before.startBeats != notes[(size_t) i].startBeats || before.lengthBeats != notes[(size_t) i].lengthBeats) ++changed;
    }
    return changed;
}
}

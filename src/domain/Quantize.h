#pragma once
#include "MidiNote.h"
#include <vector>

namespace silente::domain
{
// Cubase-style MIDI quantize as pure functions (no engine, no UI). Positions are in beats.
struct QuantizeSettings
{
    int division = 16;          // note value: 4 = 1/4, 8 = 1/8, 16 = 1/16, 32 = 1/32
    bool triplet = false;       // grid of 2/3 the straight length
    double strengthPercent = 100.0; // 0 leaves the note, 100 lands exactly on the grid
    bool quantizeLength = false;    // also snaps lengthBeats (never below one grid step)
};
double gridBeats(int division, bool triplet);
double quantizeBeat(double beats, double grid, double strengthPercent);
void quantizeNote(MidiNote& note, const QuantizeSettings& settings);
// index < 0 quantizes every note; otherwise only that note. Returns how many notes changed.
int quantizeNotes(std::vector<MidiNote>& notes, int index, const QuantizeSettings& settings);
}

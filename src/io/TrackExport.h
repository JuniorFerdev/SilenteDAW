#pragma once
#include "AudioFileFormats.h"
#include "../AudioEngine.h"

namespace silente::io
{
// «Exportar mezcla de audio» (Cubase manual, p. 1527): the Stereo Out mix and/or any number of
// channels, one file per channel. A channel is exported soloed, so it keeps its inserts, EQ,
// fader and pan, and the group/FX channels it feeds stay audible (solo through buses).
struct ExportRange { juce::String name; double start = 0.0, end = 0.0; }; // seconds

// What to do when a target file already exists. `ask` is resolved by the dialog before the job
// starts; exportChannels() treats it like `overwrite`.
enum class NameConflict { ask, uniqueName, overwrite };

struct ExportJob
{
    juce::File folder;
    juce::String baseName;
    EncodeSettings settings;
    bool mix = true;
    juce::StringArray trackIds;
    bool useRange = false;          // only between the left and right locators
    double rangeStart = 0.0, rangeEnd = 0.0; // seconds, used when useRange
    std::vector<ExportRange> ranges; // when not empty: one file per range and channel (overrides useRange)
    NameConflict conflict = NameConflict::overwrite;
};

// Cycle-marker ranges of the active marker track (or the first marker track), by position.
// Names come from the marker description («Ciclo N» when empty) and are made unique.
std::vector<ExportRange> cycleMarkerRanges(AudioEngine&);
// «base.ext» itself or the first «base 2.ext», «base 3.ext», … that does not exist yet.
juce::File uniqueFile(const juce::File&);
// Every file the job would write, in order (channel by channel, range by range).
std::vector<juce::File> plannedTargets(AudioEngine&, const ExportJob&);
juce::File exportTarget(const ExportJob&, const juce::String& channelName, const juce::String& rangeName = {});
juce::Result exportChannels(AudioEngine&, const ExportJob&, const std::function<bool(double)>& progress, juce::StringArray& written);
}

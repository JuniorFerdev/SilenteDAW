#pragma once
#include "AudioFileFormats.h"
#include "../AudioEngine.h"

namespace silente::io
{
// «Exportar mezcla de audio» (Cubase manual, p. 1527): the Stereo Out mix and/or any number of
// channels, one file per channel. A channel is exported soloed, so it keeps its inserts, EQ,
// fader and pan, and the group/FX channels it feeds stay audible (solo through buses).
struct ExportJob
{
    juce::File folder;
    juce::String baseName;
    EncodeSettings settings;
    bool mix = true;
    juce::StringArray trackIds;
    bool useRange = false;          // only between the left and right locators
    double rangeStart = 0.0, rangeEnd = 0.0; // seconds, used when useRange
};

juce::File exportTarget(const ExportJob&, const juce::String& channelName);
juce::Result exportChannels(AudioEngine&, const ExportJob&, const std::function<bool(double)>& progress, juce::StringArray& written);
}

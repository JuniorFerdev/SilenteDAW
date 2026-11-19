#pragma once
#include <juce_graphics/juce_graphics.h>
#include "VideoEvents.h"
#include <vector>

namespace silente::domain
{
struct Marker
{
    juce::String id { juce::Uuid().toString() }, description;
    int number = 1;
    double position = 0.0, end = 0.0;
    bool cycle = false;
    juce::Colour colour {}; // transparent follows the marker-track colour
    bool operator==(const Marker&) const = default;
};

// Non-DSP project data shared by persistence, commands and the track views.
struct TrackData
{
    juce::String parentFolder;
    bool folderCollapsed = false;
    int rulerFormat = 0; // bars/beats, seconds, timecode, samples, user frames
    double rulerOffset = 0.0;
    std::vector<Marker> markers;
    std::vector<VideoEvent> videoEvents; // video tracks only
};

Marker* findMarker(TrackData&, const juce::String& id);
const Marker* findMarker(const TrackData&, const juce::String& id);
Marker makeMarker(const TrackData&, double position, bool cycle = false, double end = 0.0);
bool validMarker(const Marker&);
}

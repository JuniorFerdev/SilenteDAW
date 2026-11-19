#pragma once
#include <juce_core/juce_core.h>
#include <vector>

namespace silente::domain
{
// A picture event on a video track (Cubase: video event). `path` is the movie or the still image;
// `start`/`length` place it on the project timeline in seconds and `offset` is the source time
// shown at `start`. A still image shows the same picture for its whole length.
struct VideoEvent
{
    juce::String id { juce::Uuid().toString() }, path, name;
    double start = 0.0, length = 0.0, offset = 0.0;
    bool still = false;
    int width = 0, height = 0;
    double frameRate = 0.0; // 0 = unknown or still image
    bool operator==(const VideoEvent&) const = default;
};

// Default length of a still image placed on a video track, in seconds.
inline constexpr double defaultStillLength = 10.0;

bool validVideoEvent(const VideoEvent&);
// The event that shows its picture at `position` (the last one in the list wins where events
// overlap, as audio events do); nullptr when the position falls outside every event.
const VideoEvent* videoEventAt(const std::vector<VideoEvent>&, double position);
// Source time of the picture at project `position` (0 for still images), snapped to the frame
// grid when the frame rate is known, so a viewer can reuse the frame it already decoded.
double videoSourceTime(const VideoEvent&, double position);
}

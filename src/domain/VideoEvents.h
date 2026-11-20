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
    double sourceLength = 0.0; // movie duration in source seconds (0 = unknown: offset + length)
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

// Editing (Cubase: move an event, size its start or end). Pure functions on one event; the
// results are always valid events. Times in project seconds.
inline constexpr double minimumVideoLength = 0.04; // one frame at 25 fps
enum class VideoEdit { move, trimStart, trimEnd };
// Source seconds available after `offset` (stills: unlimited; movies: sourceLength, or the
// current end of the event when the source length is unknown).
double videoSourceLimit(const VideoEvent&);
// `move`: start at `position` (>= 0). `trimStart`: the start goes to `position` keeping the end
// fixed; the source offset follows, so the picture under the cursor does not change (offset >= 0).
// `trimEnd`: the end goes to `position`, within the source. The length never drops below
// minimumVideoLength (or the current length when it is already shorter).
VideoEvent editedVideoEvent(const VideoEvent&, VideoEdit, double position);
VideoEvent* findVideoEvent(std::vector<VideoEvent>&, const juce::String& id);
}

#include "VideoEvents.h"
#include <cmath>

namespace silente::domain
{
bool validVideoEvent(const VideoEvent& event)
{
    return event.path.isNotEmpty() && std::isfinite(event.start) && std::isfinite(event.length) && std::isfinite(event.offset)
        && std::isfinite(event.frameRate) && event.start >= 0.0 && event.length > 0.0 && event.offset >= 0.0 && event.frameRate >= 0.0;
}

const VideoEvent* videoEventAt(const std::vector<VideoEvent>& events, double position)
{
    for (auto it = events.rbegin(); it != events.rend(); ++it)
        if (position >= it->start && position < it->start + it->length) return &*it;
    return nullptr;
}

double videoSourceTime(const VideoEvent& event, double position)
{
    if (event.still) return 0.0;
    const double source = juce::jlimit(event.offset, event.offset + event.length, event.offset + position - event.start);
    if (event.frameRate <= 0.0) return source;
    // Frame k covers [k/fps, (k+1)/fps): pick the frame on screen and return its start.
    return std::floor(source * event.frameRate + 1.0e-6) / event.frameRate;
}
}

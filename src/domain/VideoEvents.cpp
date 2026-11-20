#include "VideoEvents.h"
#include <cmath>
#include <limits>

namespace silente::domain
{
bool validVideoEvent(const VideoEvent& event)
{
    return event.path.isNotEmpty() && std::isfinite(event.start) && std::isfinite(event.length) && std::isfinite(event.offset)
        && std::isfinite(event.frameRate) && std::isfinite(event.sourceLength) && event.sourceLength >= 0.0 && event.start >= 0.0 && event.length > 0.0 && event.offset >= 0.0 && event.frameRate >= 0.0;
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

double videoSourceLimit(const VideoEvent& event)
{
    if (event.still) return std::numeric_limits<double>::max();
    return event.sourceLength > 0.0 ? juce::jmax(event.sourceLength, event.offset + event.length) : event.offset + event.length;
}

VideoEvent editedVideoEvent(const VideoEvent& event, VideoEdit edit, double position)
{
    auto result = event;
    if (! std::isfinite(position)) return result;
    const double minimum = juce::jmin(minimumVideoLength, event.length), end = event.start + event.length;
    switch (edit)
    {
        case VideoEdit::move:
            result.start = juce::jmax(0.0, position);
            break;
        case VideoEdit::trimStart:
        {   // Earliest start: the project origin and, for movies, the first source frame.
            const double earliest = event.still ? 0.0 : juce::jmax(0.0, event.start - event.offset);
            const double start = juce::jlimit(earliest, juce::jmax(earliest, end - minimum), position);
            if (! event.still) result.offset = juce::jmax(0.0, event.offset + start - event.start);
            result.start = start; result.length = end - start;
            break;
        }
        case VideoEdit::trimEnd:
        {
            const double latest = event.still ? std::numeric_limits<double>::max() : event.start + videoSourceLimit(event) - event.offset;
            result.length = juce::jlimit(minimum, juce::jmax(minimum, latest - event.start), position - event.start);
            break;
        }
    }
    return result;
}

VideoEvent* findVideoEvent(std::vector<VideoEvent>& events, const juce::String& id)
{
    for (auto& event : events) if (event.id == id) return &event;
    return nullptr;
}
}

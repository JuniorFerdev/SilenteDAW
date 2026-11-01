#include "RecordingModes.h"

namespace silente::audio
{
void cutRange(std::vector<AudioClip>& clips, double from, double to)
{
    if (to <= from) return;
    constexpr double edge = 1.0e-9;
    std::vector<AudioClip> result;
    result.reserve(clips.size() + 1);
    for (auto& clip : clips)
    {
        const double start = clip.startSeconds, end = clip.startSeconds + clip.durationSeconds;
        if (end <= from + edge || start >= to - edge) { result.push_back(std::move(clip)); continue; }
        if (start < from - edge)
        {
            AudioClip left = clip;
            left.durationSeconds = from - start;
            left.fadeOut = juce::jmin(left.fadeOut, left.durationSeconds);
            result.push_back(std::move(left));
        }
        if (end > to + edge)
        {
            AudioClip right = clip;
            if (start < from - edge) right.id = juce::Uuid().toString();
            right.offsetSeconds += to - start;
            right.startSeconds = to;
            right.durationSeconds = end - to;
            right.fadeIn = juce::jmin(clip.fadeIn, right.durationSeconds);
            result.push_back(std::move(right));
        }
    }
    clips = std::move(result);
}

void placeTakes(std::vector<AudioClip>& clips, std::vector<AudioClip> takes, AudioRecordMode mode)
{
    if (takes.empty()) return;
    double from = takes.front().startSeconds, to = from;
    for (const auto& take : takes)
    {
        from = juce::jmin(from, take.startSeconds);
        to = juce::jmax(to, take.startSeconds + take.durationSeconds);
    }
    if (mode != AudioRecordMode::keepHistory) cutRange(clips, from, to);
    if (mode == AudioRecordMode::replace)
    {
        // Each later lap replaces what the earlier laps left under it.
        std::vector<AudioClip> kept;
        for (auto& take : takes)
        {
            cutRange(kept, take.startSeconds, take.startSeconds + take.durationSeconds);
            kept.push_back(std::move(take));
        }
        takes = std::move(kept);
    }
    // Later clips play on top (see AudioEngine::renderSegment), so the last take is heard.
    for (auto& take : takes) clips.push_back(std::move(take));
}
}

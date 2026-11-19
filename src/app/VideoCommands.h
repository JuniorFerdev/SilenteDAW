#pragma once
#include "../AudioEngine.h"
#include "../io/VideoFrames.h"
#include <optional>

namespace silente::app
{
// Video track commands (Cubase Pro 15, «Importar archivos de video»: the picture becomes an event
// on a video track; extracting its audio stays a separate flow). Message thread only.

// Track that receives imported pictures: the selected track when it is a video track, otherwise
// the first video track of the project; nullptr when there is none.
std::shared_ptr<Track> videoTrackFor(const AudioEngine&, int selectedIndex);

// Places `file` (described by `info`, see io::probeVisualMedia) at `start` seconds on the video
// track `trackId`, under modelLock. Movies keep their duration; still images get
// domain::defaultStillLength. Refused while recording or when the track is not a video track.
juce::Result addVideoEvent(AudioEngine&, const juce::String& trackId, const juce::File& file,
                           const io::VisualMediaInfo& info, double start, juce::String* createdId = nullptr);

// The picture to show at project `position`: the topmost video track with an event there.
struct VideoFrameTarget
{
    juce::String trackId, eventId, path;
    double sourceSeconds = 0.0;
    bool still = false;
    bool operator==(const VideoFrameTarget&) const = default;
};
// Reads the track list without modelLock (message thread, which is the only writer).
std::optional<VideoFrameTarget> videoFrameAt(const AudioEngine&, double position);
}

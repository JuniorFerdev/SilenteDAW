#include "VideoCommands.h"
#include <cmath>

namespace silente::app
{
std::shared_ptr<Track> videoTrackFor(const AudioEngine& engine, int selectedIndex)
{
    if (juce::isPositiveAndBelow(selectedIndex, (int) engine.tracks.size()) && engine.tracks[(size_t) selectedIndex]->kind == TrackKind::video)
        return engine.tracks[(size_t) selectedIndex];
    for (const auto& track : engine.tracks)
        if (track->kind == TrackKind::video) return track;
    return nullptr;
}

juce::Result addVideoEvent(AudioEngine& engine, const juce::String& trackId, const juce::File& file,
                           const io::VisualMediaInfo& info, double start, juce::String* createdId)
{
    if (engine.recording.load()) return juce::Result::fail(i18n::tr("Detén la grabación antes de importar archivos."));
    domain::VideoEvent event;
    event.path = file.getFullPathName(); event.name = file.getFileName();
    event.start = std::isfinite(start) ? juce::jmax(0.0, start) : 0.0;
    event.still = info.still;
    event.length = info.still ? domain::defaultStillLength : info.duration;
    event.width = info.width; event.height = info.height; event.frameRate = info.still ? 0.0 : info.frameRate;
    if (! domain::validVideoEvent(event)) return juce::Result::fail(i18n::tr("El video no tiene duración válida: ") + file.getFileName());
    const juce::ScopedLock lock(engine.modelLock);
    auto track = engine.findTrack(trackId);
    if (! track || track->kind != TrackKind::video) return juce::Result::fail(i18n::tr("Selecciona o añade una pista de video."));
    if (createdId != nullptr) *createdId = event.id;
    track->data.videoEvents.push_back(std::move(event));
    return juce::Result::ok();
}

std::optional<VideoFrameTarget> videoFrameAt(const AudioEngine& engine, double position)
{
    for (const auto& track : engine.tracks)
    {
        if (track->kind != TrackKind::video) continue;
        if (const auto* event = domain::videoEventAt(track->data.videoEvents, position))
            return VideoFrameTarget { track->id, event->id, event->path, domain::videoSourceTime(*event, position), event->still };
    }
    return std::nullopt;
}
}

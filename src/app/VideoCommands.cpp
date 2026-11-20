#include "VideoCommands.h"
#include <algorithm>
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
    event.sourceLength = info.still ? 0.0 : info.duration;
    if (! domain::validVideoEvent(event)) return juce::Result::fail(i18n::tr("El video no tiene duración válida: ") + file.getFileName());
    const juce::ScopedLock lock(engine.modelLock);
    auto track = engine.findTrack(trackId);
    if (! track || track->kind != TrackKind::video) return juce::Result::fail(i18n::tr("Selecciona o añade una pista de video."));
    if (createdId != nullptr) *createdId = event.id;
    track->data.videoEvents.push_back(std::move(event));
    return juce::Result::ok();
}

namespace
{
std::shared_ptr<Track> videoTrack(const AudioEngine& engine, const juce::String& trackId)
{
    for (const auto& track : engine.tracks)
        if (track->id == trackId) return track->kind == TrackKind::video ? track : nullptr;
    return nullptr;
}
}

std::optional<domain::VideoEvent> previewVideoEdit(const AudioEngine& engine, const juce::String& trackId, const juce::String& eventId,
                                                   domain::VideoEdit edit, double position)
{
    auto track = videoTrack(engine, trackId);
    if (! track) return std::nullopt;
    if (const auto* event = domain::findVideoEvent(track->data.videoEvents, eventId)) return domain::editedVideoEvent(*event, edit, position);
    return std::nullopt;
}

juce::Result editVideoEvent(AudioEngine& engine, const juce::String& trackId, const juce::String& eventId, domain::VideoEdit edit, double position)
{
    if (engine.recording.load()) return juce::Result::fail(i18n::tr("Detén la grabación antes de editar eventos."));
    const juce::ScopedLock lock(engine.modelLock);
    auto track = videoTrack(engine, trackId);
    if (! track) return juce::Result::fail(i18n::tr("Selecciona o añade una pista de video."));
    auto& events = track->data.videoEvents;
    const auto it = std::find_if(events.begin(), events.end(), [&](const auto& event) { return event.id == eventId; });
    if (it == events.end()) return juce::Result::fail(i18n::tr("El evento de video ya no existe."));
    auto edited = domain::editedVideoEvent(*it, edit, position);
    if (edit == domain::VideoEdit::move) { events.erase(it); events.push_back(std::move(edited)); }
    else *it = std::move(edited);
    return juce::Result::ok();
}

juce::Result deleteVideoEvent(AudioEngine& engine, const juce::String& trackId, const juce::String& eventId)
{
    if (engine.recording.load()) return juce::Result::fail(i18n::tr("Detén la grabación antes de editar eventos."));
    const juce::ScopedLock lock(engine.modelLock);
    auto track = videoTrack(engine, trackId);
    if (! track) return juce::Result::fail(i18n::tr("Selecciona o añade una pista de video."));
    auto& events = track->data.videoEvents;
    const auto it = std::find_if(events.begin(), events.end(), [&](const auto& event) { return event.id == eventId; });
    if (it == events.end()) return juce::Result::fail(i18n::tr("El evento de video ya no existe."));
    events.erase(it);
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

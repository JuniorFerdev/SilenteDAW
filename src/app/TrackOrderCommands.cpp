#include "TrackOrderCommands.h"

namespace silente::app
{
std::optional<domain::TrackOrder> planTrackMove(const AudioEngine& engine, const TrackMoveRequest& request)
{
    const auto order = engine.trackOrder();
    const int index = domain::indexOf(order, request.track);
    if (index < 0) return std::nullopt;
    std::optional<domain::TrackOrder> next;
    switch (request.move)
    {
        case TrackMove::up: next = domain::moveUp(order, index); break;
        case TrackMove::down: next = domain::moveDown(order, index); break;
        case TrackMove::outOfFolder: next = domain::moveOutOfFolder(order, index); break;
        case TrackMove::intoFolder: next = domain::moveIntoFolder(order, index, request.target); break;
        case TrackMove::drop: next = domain::dropOn(order, index, domain::indexOf(order, request.target), request.zone); break;
    }
    if (! next || *next == order) return std::nullopt;
    // Marker and ruler tracks stay at top level (project-wide tracks; their headers are not nested).
    const auto track = engine.findTrack(request.track);
    if (track && (track->kind == TrackKind::marker || track->kind == TrackKind::ruler)
        && (*next)[(size_t) domain::indexOf(*next, request.track)].parent.isNotEmpty()) return std::nullopt;
    return next;
}

juce::Result moveTrack(AudioEngine& engine, const TrackMoveRequest& request)
{
    const auto next = planTrackMove(engine, request);
    if (! next) return juce::Result::fail(i18n::tr("No se puede mover la pista ahí."));
    return engine.applyTrackOrder(*next);
}

bool setFolderCollapsed(AudioEngine& engine, const juce::String& folderId, bool collapsed)
{
    const juce::ScopedLock lock(engine.modelLock);
    auto track = engine.findTrack(folderId);
    if (! track || track->kind != TrackKind::folder || track->data.folderCollapsed == collapsed) return false;
    track->data.folderCollapsed = collapsed;
    return true;
}
}

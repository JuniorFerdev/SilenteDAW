#pragma once
#include "../AudioEngine.h"

namespace silente::app
{
// Track list commands (message thread): Mover arriba/abajo, Sacar de carpeta, Meter en carpeta,
// drag and drop and folding. They work on track ids; plan* only computes the new order (nullopt
// = refused or nothing would change) so the caller can record the undo step before applying it.
// Marker and ruler tracks can be reordered but stay at top level.
enum class TrackMove { up, down, outOfFolder, intoFolder, drop };

struct TrackMoveRequest
{
    TrackMove move = TrackMove::up;
    juce::String track, target; // target: folder (intoFolder) or row under the pointer (drop; empty = end)
    domain::DropZone zone = domain::DropZone::before;
};

std::optional<domain::TrackOrder> planTrackMove(const AudioEngine&, const TrackMoveRequest&);
juce::Result moveTrack(AudioEngine&, const TrackMoveRequest&); // plan + apply
// Fold or unfold a folder track. Not an undo step (view state), but saved with the project.
bool setFolderCollapsed(AudioEngine&, const juce::String& folderId, bool collapsed);
}

#pragma once
#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

namespace silente::domain
{
// Track list hierarchy (folder tracks, Cubase manual: «Pistas de carpeta»). The project keeps one
// flat list in pre-order: a folder is followed by everything inside it, so a list without folders
// is exactly the old flat order. `parent` is the id of the enclosing folder track (empty = top level).
struct TrackNode
{
    juce::String id, parent;
    bool folder = false, collapsed = false;
    bool operator==(const TrackNode&) const = default;
};
using TrackOrder = std::vector<TrackNode>;

// Where a dragged track lands relative to the row under the pointer.
enum class DropZone { before, after, into };

int indexOf(const TrackOrder&, const juce::String& id);
int depthOf(const TrackOrder&, int index);                      // 0 = top level
bool isInside(const TrackOrder&, int index, const juce::String& ancestorId); // strict descendant
int subtreeEnd(const TrackOrder&, int index);                   // one past its last descendant
bool hiddenByCollapse(const TrackOrder&, int index);            // an enclosing folder is folded
std::vector<bool> hiddenByCollapse(const TrackOrder&);         // the same for every row, one pass (pre-order)
std::vector<int> depths(const TrackOrder&);                     // depthOf for every row, one pass (pre-order)
std::vector<int> childrenOf(const TrackOrder&, const juce::String& parent); // direct children, in order

// Parents exist, are folders, come first, no cycles, and every folder's content is contiguous.
bool isValid(const TrackOrder&);
// Repairs a loaded or hand-edited list: unknown, non-folder or cyclic parents become top level and
// each folder's content is gathered right after it, keeping the relative order. Identity when valid.
TrackOrder normalised(TrackOrder);

// Moves the track at `index` with everything inside it so it becomes child number `position` of
// `parent` (counted without the moved block; clamped). Refused (nullopt) when the parent is not a
// folder, is the track itself or lies inside it (no cycles).
std::optional<TrackOrder> moveInto(const TrackOrder&, int index, const juce::String& parent, int position);
// Swap with the previous / next sibling inside the same folder (nullopt at the edge).
std::optional<TrackOrder> moveUp(const TrackOrder&, int index);
std::optional<TrackOrder> moveDown(const TrackOrder&, int index);
// Out of its folder: placed right after that folder, one level up (nullopt at top level).
std::optional<TrackOrder> moveOutOfFolder(const TrackOrder&, int index);
// Last child of `folderId`.
std::optional<TrackOrder> moveIntoFolder(const TrackOrder&, int index, const juce::String& folderId);
// Drag and drop on the row `target`: before / after it (same folder; after an open folder with
// content means its first child) or into it (folders only, as last child). target < 0 = end of list.
std::optional<TrackOrder> dropOn(const TrackOrder&, int index, int target, DropZone);
// Removing a folder: its content moves one level up into the folder's place (nothing is lost).
TrackOrder withoutTrack(const TrackOrder&, int index);
}

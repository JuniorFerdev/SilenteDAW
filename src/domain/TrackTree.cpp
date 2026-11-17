#include "TrackTree.h"
#include <algorithm>
#include <functional>
#include <map>

namespace silente::domain
{
int indexOf(const TrackOrder& order, const juce::String& id)
{
    if (id.isEmpty()) return -1;
    for (int i = 0; i < (int) order.size(); ++i) if (order[(size_t) i].id == id) return i;
    return -1;
}

int depthOf(const TrackOrder& order, int index)
{
    int depth = 0;
    auto parent = juce::isPositiveAndBelow(index, (int) order.size()) ? order[(size_t) index].parent : juce::String();
    for (int guard = 0; parent.isNotEmpty() && guard <= (int) order.size(); ++guard)
    {
        const int p = indexOf(order, parent);
        if (p < 0) break;
        ++depth;
        parent = order[(size_t) p].parent;
    }
    return depth;
}

bool isInside(const TrackOrder& order, int index, const juce::String& ancestorId)
{
    if (ancestorId.isEmpty() || ! juce::isPositiveAndBelow(index, (int) order.size())) return false;
    auto parent = order[(size_t) index].parent;
    for (int guard = 0; parent.isNotEmpty() && guard <= (int) order.size(); ++guard)
    {
        if (parent == ancestorId) return true;
        const int p = indexOf(order, parent);
        if (p < 0) return false;
        parent = order[(size_t) p].parent;
    }
    return false;
}

int subtreeEnd(const TrackOrder& order, int index)
{
    if (! juce::isPositiveAndBelow(index, (int) order.size())) return index;
    int end = index + 1;
    while (end < (int) order.size() && isInside(order, end, order[(size_t) index].id)) ++end;
    return end;
}

bool hiddenByCollapse(const TrackOrder& order, int index)
{
    if (! juce::isPositiveAndBelow(index, (int) order.size())) return false;
    auto parent = order[(size_t) index].parent;
    for (int guard = 0; parent.isNotEmpty() && guard <= (int) order.size(); ++guard)
    {
        const int p = indexOf(order, parent);
        if (p < 0) return false;
        if (order[(size_t) p].collapsed) return true;
        parent = order[(size_t) p].parent;
    }
    return false;
}

std::vector<bool> hiddenByCollapse(const TrackOrder& order)
{
    std::vector<bool> hidden(order.size(), false);
    std::map<juce::String, int> seen;
    for (int i = 0; i < (int) order.size(); ++i)
    {
        const auto& node = order[(size_t) i];
        if (const auto p = seen.find(node.parent); p != seen.end())
            hidden[(size_t) i] = order[(size_t) p->second].collapsed || hidden[(size_t) p->second];
        else if (node.parent.isNotEmpty()) hidden[(size_t) i] = hiddenByCollapse(order, i); // not pre-order: slow path
        seen.emplace(node.id, i);
    }
    return hidden;
}

std::vector<int> depths(const TrackOrder& order)
{
    std::vector<int> depth(order.size(), 0);
    std::map<juce::String, int> seen;
    for (int i = 0; i < (int) order.size(); ++i)
    {
        const auto& node = order[(size_t) i];
        if (const auto p = seen.find(node.parent); p != seen.end()) depth[(size_t) i] = depth[(size_t) p->second] + 1;
        else if (node.parent.isNotEmpty()) depth[(size_t) i] = depthOf(order, i);
        seen.emplace(node.id, i);
    }
    return depth;
}

std::vector<int> childrenOf(const TrackOrder& order, const juce::String& parent)
{
    std::vector<int> result;
    for (int i = 0; i < (int) order.size(); ++i) if (order[(size_t) i].parent == parent) result.push_back(i);
    return result;
}

bool isValid(const TrackOrder& order)
{
    return normalised(order) == order;
}

TrackOrder normalised(TrackOrder order)
{
    const int n = (int) order.size();
    std::map<juce::String, int> first;
    for (int i = 0; i < n; ++i) first.emplace(order[(size_t) i].id, i);
    for (auto& node : order) if (! node.folder) node.collapsed = false;
    // Parents must be existing folders other than the track itself.
    for (auto& node : order)
    {
        if (node.parent.isEmpty()) continue;
        const auto found = first.find(node.parent);
        if (found == first.end() || node.parent == node.id || ! order[(size_t) found->second].folder) node.parent.clear();
    }
    // Break cycles: a chain longer than the list, or one that comes back, goes to top level.
    for (int i = 0; i < n; ++i)
    {
        auto parent = order[(size_t) i].parent;
        for (int steps = 0; parent.isNotEmpty(); ++steps)
        {
            if (parent == order[(size_t) i].id || steps > n) { order[(size_t) i].parent.clear(); break; }
            parent = order[(size_t) first[parent]].parent;
        }
    }
    // Pre-order rebuild keeping the relative order of siblings.
    TrackOrder result;
    result.reserve(order.size());
    std::vector<bool> emitted((size_t) n, false);
    std::function<void(const juce::String&)> emitChildren = [&](const juce::String& parent)
    {
        for (int i = 0; i < n; ++i)
            if (! emitted[(size_t) i] && order[(size_t) i].parent == parent)
            {
                emitted[(size_t) i] = true;
                result.push_back(order[(size_t) i]);
                if (order[(size_t) i].folder && first[order[(size_t) i].id] == i) emitChildren(order[(size_t) i].id);
            }
    };
    emitChildren({});
    for (int i = 0; i < n; ++i) if (! emitted[(size_t) i]) { order[(size_t) i].parent.clear(); result.push_back(order[(size_t) i]); }
    return result;
}

std::optional<TrackOrder> moveInto(const TrackOrder& order, int index, const juce::String& parent, int position)
{
    if (! juce::isPositiveAndBelow(index, (int) order.size())) return std::nullopt;
    const auto& moved = order[(size_t) index];
    if (parent.isNotEmpty())
    {
        const int p = indexOf(order, parent);
        if (p < 0 || ! order[(size_t) p].folder || parent == moved.id || isInside(order, p, moved.id)) return std::nullopt;
    }
    const int end = subtreeEnd(order, index);
    TrackOrder block(order.begin() + index, order.begin() + end), rest;
    rest.insert(rest.end(), order.begin(), order.begin() + index);
    rest.insert(rest.end(), order.begin() + end, order.end());
    block.front().parent = parent;
    const auto siblings = childrenOf(rest, parent);
    position = juce::jlimit(0, (int) siblings.size(), position);
    int at = (int) rest.size();
    if (position < (int) siblings.size()) at = siblings[(size_t) position];
    else if (parent.isNotEmpty()) at = subtreeEnd(rest, indexOf(rest, parent));
    rest.insert(rest.begin() + at, block.begin(), block.end());
    return rest;
}

namespace
{
int siblingPosition(const TrackOrder& order, int index)
{
    const auto siblings = childrenOf(order, order[(size_t) index].parent);
    return (int) std::distance(siblings.begin(), std::find(siblings.begin(), siblings.end(), index));
}
}

std::optional<TrackOrder> moveUp(const TrackOrder& order, int index)
{
    if (! juce::isPositiveAndBelow(index, (int) order.size())) return std::nullopt;
    const int position = siblingPosition(order, index);
    if (position <= 0) return std::nullopt;
    return moveInto(order, index, order[(size_t) index].parent, position - 1);
}

std::optional<TrackOrder> moveDown(const TrackOrder& order, int index)
{
    if (! juce::isPositiveAndBelow(index, (int) order.size())) return std::nullopt;
    const int position = siblingPosition(order, index);
    if (position + 1 >= (int) childrenOf(order, order[(size_t) index].parent).size()) return std::nullopt;
    return moveInto(order, index, order[(size_t) index].parent, position + 1);
}

std::optional<TrackOrder> moveOutOfFolder(const TrackOrder& order, int index)
{
    if (! juce::isPositiveAndBelow(index, (int) order.size())) return std::nullopt;
    const int folder = indexOf(order, order[(size_t) index].parent);
    if (folder < 0) return std::nullopt;
    const auto& grand = order[(size_t) folder].parent;
    // The folder keeps its place among the siblings (the moved block is not one of them).
    return moveInto(order, index, grand, siblingPosition(order, folder) + 1);
}

std::optional<TrackOrder> moveIntoFolder(const TrackOrder& order, int index, const juce::String& folderId)
{
    if (folderId.isEmpty() || ! juce::isPositiveAndBelow(index, (int) order.size()) || order[(size_t) index].parent == folderId) return std::nullopt;
    return moveInto(order, index, folderId, (int) order.size());
}

std::optional<TrackOrder> dropOn(const TrackOrder& order, int index, int target, DropZone zone)
{
    if (! juce::isPositiveAndBelow(index, (int) order.size())) return std::nullopt;
    if (target < 0 || target >= (int) order.size()) return moveInto(order, index, {}, (int) order.size());
    const auto& node = order[(size_t) target];
    if (target == index || isInside(order, target, order[(size_t) index].id)) return std::nullopt;
    if (zone == DropZone::into) return node.folder ? moveInto(order, index, node.id, (int) order.size()) : std::nullopt;
    if (zone == DropZone::after && node.folder && ! node.collapsed && subtreeEnd(order, target) > target + 1)
        return moveInto(order, index, node.id, 0);
    // Position of the target among its siblings once the moved block is out of the list.
    int position = 0;
    for (int s : childrenOf(order, node.parent))
    {
        if (s == target) break;
        if (s != index) ++position;
    }
    return moveInto(order, index, node.parent, zone == DropZone::after ? position + 1 : position);
}

TrackOrder withoutTrack(const TrackOrder& order, int index)
{
    if (! juce::isPositiveAndBelow(index, (int) order.size())) return order;
    TrackOrder result = order;
    const auto removed = result[(size_t) index];
    for (auto& node : result) if (node.parent == removed.id) node.parent = removed.parent;
    result.erase(result.begin() + index);
    return result;
}
}

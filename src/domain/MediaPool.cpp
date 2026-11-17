#include "MediaPool.h"
#include <algorithm>
#include <map>

namespace silente::domain
{
std::vector<MediaEntry> buildPool(const std::vector<MediaClipRef>& clips,
                                  const std::function<bool(const juce::String&)>& exists,
                                  const std::set<juce::String>& replaced)
{
    std::vector<MediaEntry> pool;
    std::map<juce::String, size_t> byPath;
    for (const auto& clip : clips)
    {
        size_t index = pool.size();
        const auto found = clip.path.isEmpty() ? byPath.end() : byPath.find(clip.path);
        if (found != byPath.end()) index = found->second;
        else
        {
            MediaEntry entry;
            entry.path = clip.path; entry.name = clip.name;
            pool.push_back(std::move(entry));
            if (clip.path.isNotEmpty()) byPath[clip.path] = index;
        }
        auto& entry = pool[index];
        if (clip.lengthSamples > entry.lengthSamples)
        { entry.lengthSamples = clip.lengthSamples; entry.channels = clip.channels; entry.sampleRate = clip.sampleRate; }
        ++entry.uses;
        entry.clips.push_back(clip);
    }
    for (auto& entry : pool)
    {
        entry.seconds = entry.sampleRate > 0.0 ? (double) entry.lengthSamples / entry.sampleRate : 0.0;
        const bool there = entry.path.isEmpty() || (exists && exists(entry.path));
        entry.state = ! there ? MediaState::missing : replaced.count(entry.path) > 0 ? MediaState::replaced : MediaState::present;
    }
    return pool;
}

void sortPool(std::vector<MediaEntry>& pool, MediaColumn column, bool ascending)
{
    const auto less = [column](const MediaEntry& a, const MediaEntry& b)
    {
        switch (column)
        {
            case MediaColumn::name: return a.name.compareNatural(b.name, false) < 0;
            case MediaColumn::path: return a.path.compareNatural(b.path, false) < 0;
            case MediaColumn::duration: return a.seconds < b.seconds;
            case MediaColumn::uses: return a.uses < b.uses;
            case MediaColumn::state: return (int) a.state < (int) b.state;
        }
        return false;
    };
    std::stable_sort(pool.begin(), pool.end(), [&](const MediaEntry& a, const MediaEntry& b) { return ascending ? less(a, b) : less(b, a); });
}

int reassignPath(std::vector<MediaClipRef>& clips, const juce::String& fromPath, const juce::String& toPath)
{
    int changed = 0;
    for (auto& clip : clips)
        if (clip.path == fromPath) { clip.path = toPath; ++changed; }
    return changed;
}
}

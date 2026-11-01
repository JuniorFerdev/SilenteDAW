#pragma once
#include "../../AudioEngine.h"
#include <set>

namespace silente::ui
{
// Track list visibility (Cubase manual pp. 69–71, 83–85): tracks hidden one by one in the
// Visibility tab and track types hidden with «Ajustar filtro de tipo de pista».
struct TrackFilter
{
    std::set<juce::String> hiddenTracks;
    std::set<int> hiddenKinds;

    bool shows(const Track& t) const { return hiddenTracks.count(t.id) == 0 && hiddenKinds.count((int) t.kind) == 0; }
    bool filtering() const { return ! hiddenKinds.empty(); }

    std::vector<int> visible(const std::vector<std::shared_ptr<Track>>& tracks) const
    {
        std::vector<int> result;
        for (int i = 0; i < (int) tracks.size(); ++i) if (shows(*tracks[(size_t) i])) result.push_back(i);
        return result;
    }

    // «Buscar pistas»: every track whose name contains all the typed words, hidden ones included
    // (tracks hidden by the type filter are left out, as in Cubase).
    std::vector<int> search(const std::vector<std::shared_ptr<Track>>& tracks, const juce::String& query) const
    {
        std::vector<int> result;
        const auto words = juce::StringArray::fromTokens(query.trim(), " ", "");
        for (int i = 0; i < (int) tracks.size(); ++i)
        {
            const auto& t = *tracks[(size_t) i];
            if (hiddenKinds.count((int) t.kind)) continue;
            bool all = true;
            for (const auto& w : words) if (! t.name.containsIgnoreCase(w)) all = false;
            if (all) result.push_back(i);
        }
        return result;
    }

    // Picking a found track makes it visible again.
    void reveal(const Track& t) { hiddenTracks.erase(t.id); }

    // Visibility tab, Shift-click: show only this track.
    void showOnly(const std::vector<std::shared_ptr<Track>>& tracks, const Track& keep)
    {
        hiddenTracks.clear();
        for (const auto& t : tracks) if (t.get() != &keep) hiddenTracks.insert(t->id);
    }
};
}

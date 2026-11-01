#pragma once
#include "TrackFilter.h"
#include "../theme/Drawing.h"
#include "../../i18n/Translations.h"
#include <functional>

namespace silente::ui
{
// «Buscar pistas» selector (manual p. 70): a search field and the matching tracks; it updates as
// you type and picking a track selects it (and shows it again if it was hidden).
class TrackFinder final : public juce::Component
{
public:
    std::function<void(int)> onPick;
    TrackFinder(AudioEngine& e, const TrackFilter& f) : engine(e), filter(f)
    {
        addAndMakeVisible(field);
        field.setTextToShowWhenEmpty(i18n::tr("Nombre de la pista"), theme::muted);
        field.onTextChange = [this] { update(); };
        field.onReturnKey = [this] { if (! matches.empty() && onPick) onPick(matches.front()); };
        setSize(280, 300);
        update();
    }
    void visibilityChanged() override { if (isShowing()) field.grabKeyboardFocus(); }
    void resized() override { field.setBounds(getLocalBounds().reduced(8).removeFromTop(26)); }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(theme::panel);
        int y = 42;
        for (int i : matches)
        {
            if (y + 22 > getHeight()) break;
            const auto& t = *engine.tracks[(size_t) i];
            g.setColour(t.colour); g.fillRect(10, y + 4, 4, 14);
            theme::text(g, t.name + (filter.hiddenTracks.count(t.id) ? i18n::tr("  (oculta)") : juce::String()), { 20, y, getWidth() - 28, 22 }, 12, theme::ink);
            y += 22;
        }
        if (matches.empty()) theme::text(g, i18n::tr("Ninguna pista coincide."), { 10, 42, getWidth() - 20, 22 }, 11, theme::muted);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const int row = (e.y - 42) / 22;
        if (e.y >= 42 && juce::isPositiveAndBelow(row, (int) matches.size()) && onPick) onPick(matches[(size_t) row]);
    }

private:
    AudioEngine& engine;
    const TrackFilter& filter;
    juce::TextEditor field;
    std::vector<int> matches;
    void update() { matches = filter.search(engine.tracks, field.getText()); repaint(); }
};
}

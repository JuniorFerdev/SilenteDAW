#pragma once
#include "../theme/SilenteLookAndFeel.h"
#include "../theme/Drawing.h"
#include "../../i18n/Translations.h"
#include <functional>

namespace silente::ui
{
// «Diálogo Historial de ediciones» (manual p. 114): every edit with its time and state. The line
// marks the current point; clicking a row undoes or redoes everything up to it.
class EditHistoryPanel final : public juce::Component, private juce::Timer
{
public:
    struct Entry { juce::String action; juce::Time time; bool done; };
    std::function<std::vector<Entry>()> entries;
    std::function<void(int doneCount)> goTo;
    EditHistoryPanel() { setSize(520, 460); startTimerHz(4); }
    void paint(juce::Graphics& g) override
    {
        using namespace theme;
        g.fillAll(panel);
        text(g, i18n::tr("Acción"), { 16, 8, 250, 20 }, 11, muted); text(g, i18n::tr("Tiempo"), { 280, 8, 120, 20 }, 11, muted); text(g, i18n::tr("Estado"), { 410, 8, 100, 20 }, 11, muted);
        const auto list = entries ? entries() : std::vector<Entry>();
        int y = 32;
        const auto row = [&](const juce::String& action, const juce::String& time, const juce::String& state, bool done)
        {
            text(g, action, { 16, y, 260, rowHeight }, 12, done ? ink : muted);
            text(g, time, { 280, y, 120, rowHeight }, 11, done ? ink : muted);
            text(g, state, { 410, y, 100, rowHeight }, 11, done ? mint : muted);
            y += rowHeight;
        };
        row(i18n::tr("Estado inicial"), {}, {}, true);
        int doneCount = 0;
        for (const auto& e : list) if (e.done) ++doneCount;
        int index = 0;
        for (const auto& e : list)
        {
            if (index++ == doneCount) { g.setColour(accent); g.fillRect(10, y - 1, getWidth() - 20, 2); }
            row(e.action, e.time.formatted("%H:%M:%S"), e.done ? i18n::tr("Hecho") : i18n::tr("Deshecho"), e.done);
        }
        if (doneCount == (int) list.size()) { g.setColour(accent); g.fillRect(10, y - 1, getWidth() - 20, 2); }
        text(g, i18n::tr("Haz clic en una fila para deshacer o rehacer hasta ese punto."), { 16, getHeight() - 26, getWidth() - 32, 20 }, 10, muted);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const int r = (e.y - 32) / rowHeight;
        if (e.y >= 32 && goTo && entries && r <= (int) entries().size()) goTo(r);
        repaint();
    }

private:
    static constexpr int rowHeight = 22;
    void timerCallback() override { repaint(); }
};
}

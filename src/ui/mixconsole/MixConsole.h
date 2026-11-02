#pragma once
#include "../../AudioEngine.h"
#include "../../PluginManager.h"
#include "../theme/SilenteLookAndFeel.h"
#include <array>
#include <functional>
#include <set>

namespace silente::ui
{
// What the console needs from the project window: undo, dirty flag, selection, the channel
// settings window and the transport shortcuts (Space, C…) while the console has focus.
struct MixConsoleHooks
{
    std::function<void()> beforeEdit, changed;
    std::function<void(int)> select, openChannelSettings;
    std::function<int()> selected;
    std::function<bool(const juce::KeyPress&)> key;
};

// MixConsole window content (Cubase habit: F3). Toolbar, left zone with channel visibility and
// left/right zones, a legend of the channel racks, and the channel strips: racks (routing,
// inserts, EQ) above the fader section (pan, M/S/R/monitor, «e», fader, meter with peak hold
// and clip). Visibility, zones and rack layout belong to this window only.
class MixConsole final : public juce::Component, private juce::Timer
{
public:
    enum Rack { routing, inserts, equaliser, sends, rackCount };
    static constexpr int stripWidth = 98, legendWidth = 78, toolbarHeight = 40, leftZoneWidth = 230;
    static constexpr int minStripWidth = 64, maxStripWidth = 240;

    // compact: the lower-zone MixConsole of the project window (no left zone, racks folded).
    MixConsole(AudioEngine&, PluginManager&, MixConsoleHooks, bool compact = false);
    ~MixConsole() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

    bool isExpanded(Rack r) const { return expanded[(size_t) r]; }
    bool isCompact() const { return compact; }
    static int rackHeight(Rack);
    int racksHeight() const;
    void resetClips();
    int visibleStripCount() const { return (int) strips.size(); }
    // Channel width (Cubase: drag the right edge of a channel; ⌥ resizes all of them).
    void setStripWidth(int trackIndex, int width, bool all); // trackIndex -1 = output channel

private:
    class Strip;
    class LeftZone;
    class Legend;
    AudioEngine& engine;
    PluginManager& plugins;
    MixConsoleHooks hooks;
    theme::SilenteLookAndFeel look;
    std::array<bool, rackCount> expanded { true, true, true, true };
    std::set<juce::String> hidden, pinnedLeft, pinnedRight;
    std::set<int> hiddenKinds;
    juce::TextButton leftZoneButton, typesButton, clipsButton;
    std::array<juce::TextButton, rackCount> rackButtons;
    juce::TextEditor search;
    std::unique_ptr<LeftZone> leftZone;
    std::unique_ptr<Legend> legend;
    juce::Viewport scroller;
    juce::Component scrolled, leftArea, rightArea;
    std::vector<std::unique_ptr<Strip>> strips;
    juce::StringArray layoutKey;
    bool showLeftZone = true, compact = false;
    int toolbar = toolbarHeight, outputWidth = stripWidth;
    int widthOf(const Strip&) const;

    bool passesFilter(const Track&) const;
    void rebuild();
    juce::StringArray currentLayoutKey() const;
    void layoutStrips();
    void timerCallback() override;
    friend class LeftZone;
};

class MixConsoleWindow final : public juce::DocumentWindow
{
public:
    MixConsoleWindow(AudioEngine&, PluginManager&, MixConsoleHooks);
    void closeButtonPressed() override { setVisible(false); }
};
}

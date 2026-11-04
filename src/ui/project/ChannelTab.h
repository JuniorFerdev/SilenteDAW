#pragma once
#include "../../AudioEngine.h"
#include "../../PluginManager.h"
#include "../plugins/PluginSlotButton.h"
#include <array>
#include <functional>

namespace silente::ui
{
// Project window «Canal» tab (Cubase manual, p. 76): the channel of the selected track at the
// far left of the window — name, inserts, fader with meter, mute and solo.
class ChannelTab final : public juce::Component, private juce::ScrollBar::Listener
{
public:
    std::function<void()> beforeEdit, changed;
    std::function<void(int)> openSettings;
    ChannelTab(AudioEngine&, PluginManager&);
    void setTrack(int index);
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void scrollBarMoved(juce::ScrollBar*, double start) override;
    AudioEngine& engine;
    PluginManager& plugins;
    int index = -1;
    std::shared_ptr<Track> track;
    std::array<ui::PluginSlotButton, 4> inserts;
    juce::TextButton mute { "M" }, solo { "S" }, edit { "e" }, arm, monitor;
    juce::Slider fader;
    juce::ScrollBar sendScroll { true };
    int firstSend = 0;
    juce::Label volume;
    juce::Rectangle<int> meterArea, nameArea, eqArea, insertsHeader, sendsHeader, sendsArea, bottomName, scaleArea;
    bool syncing = false, metersActive = false;
    juce::String lastShown;
};
}

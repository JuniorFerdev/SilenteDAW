#pragma once
#include "../../audio/TapTempo.h"
#include "../theme/SilenteLookAndFeel.h"
#include <functional>

namespace silente::ui
{
// Tempo calculator (Cubase "Tap Tempo"): tap with Space or by clicking the pad while the
// music plays; the BPM follows the median of the recent taps. Backspace starts over.
class TapTempoPanel final : public juce::Component, private juce::Timer
{
public:
    std::function<void(double)> onApply;
    TapTempoPanel();
    ~TapTempoPanel() override { setLookAndFeel(nullptr); }
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void visibilityChanged() override;

private:
    theme::SilenteLookAndFeel look;
    audio::TapTempo taps;
    double factor = 1.0;
    float flash = 0.0f;
    juce::TextButton half { juce::String::fromUTF8("÷2") }, twice { juce::String::fromUTF8("×2") }, reset, apply;
    juce::Rectangle<int> pad;
    void tap();
    double shownBpm() const { return juce::jlimit(20.0, 300.0, taps.bpm() * factor); }
    void timerCallback() override;
};
}

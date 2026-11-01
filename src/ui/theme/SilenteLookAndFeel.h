#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Colours.h"

namespace silente::ui::theme
{
class SilenteLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SilenteLookAndFeel();
    juce::Font getTextButtonFont(juce::TextButton&, int height) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    // Cubase-style controls: rectangular grey fader caps on a dark groove, thin horizontal
    // sliders with a white dot, dark knobs with a grey value ring.
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float position, float minPos, float maxPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float position, float startAngle, float endAngle, juce::Slider&) override;
    int getSliderThumbRadius(juce::Slider&) override;
};
}

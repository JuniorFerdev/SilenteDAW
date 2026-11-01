#pragma once
#include "Colours.h"

namespace silente::ui::theme
{
void font(juce::Graphics&, float size, bool bold = false);
void text(juce::Graphics&, const juce::String&, juce::Rectangle<int>, float size = 13,
          juce::Colour colour = ink, juce::Justification = juce::Justification::centredLeft);
void logo(juce::Graphics&, juce::Rectangle<float>);
}

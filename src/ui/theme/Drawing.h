#pragma once
#include "Colours.h"

namespace silente::ui::theme
{
void font(juce::Graphics&, float size, bool bold = false);
void text(juce::Graphics&, const juce::String&, juce::Rectangle<int>, float size = 13,
          juce::Colour colour = ink, juce::Justification = juce::Justification::centredLeft);
// Width of a line of plain text (cached).
float textWidth(const juce::String&, float size);
void logo(juce::Graphics&, juce::Rectangle<float>);
}

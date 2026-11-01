#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>

namespace silente::ui::theme
{
// UI only: the caller owns the project command, undo and persistence.
void showTrackColourPicker(juce::Rectangle<int> screenAnchor, juce::Colour initial,
                           std::function<void(juce::Colour)> apply);
}

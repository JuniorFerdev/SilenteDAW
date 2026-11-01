#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace silente::i18n
{
// Only static controls and tooltips; never project titles, track names or editor input.
void translateControls(juce::Component&);
}

#pragma once
#include <juce_core/juce_core.h>

namespace silente::app
{
// One directory for global UI preferences; diagnostics supply an isolated directory.
juce::File preferencesDirectory();
void usePreferencesDirectory(const juce::File&);
float loadInterfaceScale();
juce::Result saveInterfaceScale(float);
}

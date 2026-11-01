#pragma once
#include <juce_core/juce_core.h>

namespace silente
{
// Runs offline, never opens an audio device. Returns the number of failed cases.
int runEngineTests(const juce::File& outputDir);
}

#pragma once
#include <juce_core/juce_core.h>

namespace silente
{
// Import/export and format conversion checks (MP3, M4A, FLAC, video audio, channel export).
// Offline; returns the number of failed cases.
int runIoTests(const juce::File& outputDir);
}

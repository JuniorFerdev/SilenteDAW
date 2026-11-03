#pragma once
#include <juce_core/juce_core.h>

namespace silente::diagnostics
{
// Titles of every native window of this process (including windows a plugin opens by itself,
// such as a licence or demo notice). For the plugin probes.
juce::StringArray nativeWindowTitles();
}

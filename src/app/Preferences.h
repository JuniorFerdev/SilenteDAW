#pragma once
#include <juce_core/juce_core.h>

namespace silente::app
{
// One directory for global UI preferences; diagnostics supply an isolated directory.
juce::File preferencesDirectory();
void usePreferencesDirectory(const juce::File&);
float loadInterfaceScale();
juce::Result saveInterfaceScale(float);
bool showToolboxOnRightClick();
juce::Result saveShowToolboxOnRightClick(bool);
// Extra folder scanned for VST3 and AU besides the standard macOS plug-in folders.
// Returns juce::File() when none is chosen or it no longer exists.
juce::File pluginSearchFolder();
juce::Result savePluginSearchFolder(const juce::File&);
}

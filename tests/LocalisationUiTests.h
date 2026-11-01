#pragma once
#include <juce_core/juce_core.h>
namespace silente
{
class MainComponent; class AudioEngine;
int runLanguageSwitchChecks(MainComponent&, AudioEngine&, const juce::File&);
}

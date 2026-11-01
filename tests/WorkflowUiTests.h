#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace silente
{
class MainComponent;class AudioEngine;class PluginManager;
void runWorkflowChecks(MainComponent&,AudioEngine&,PluginManager&,const juce::File& output,const juce::File& optionalVstFixture,std::function<void(int)> complete);
}

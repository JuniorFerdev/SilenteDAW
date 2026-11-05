#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
namespace silente
{
class MainComponent;class AudioEngine;
void runMarkerUiChecks(MainComponent&,AudioEngine&,const juce::File&,const std::function<void(bool,const juce::String&)>&);
}

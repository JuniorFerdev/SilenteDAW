#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
namespace silente
{
class MainComponent;class AudioEngine;
// Etapa 7 (pistas03): folder tracks and reordering in the project window track list.
void runFolderUiChecks(MainComponent&,AudioEngine&,const juce::File&,const std::function<void(bool,const juce::String&)>&);
}

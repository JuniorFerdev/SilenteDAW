#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
namespace silente
{
class MainComponent;class AudioEngine;
// Video track: import a movie and a still image, the Video Player follows the project cursor,
// undo/redo, save/reopen and a missing file. Uses a synthetic H.264 movie with real pictures.
void runVideoUiChecks(MainComponent&,AudioEngine&,const juce::File&,const std::function<void(bool,const juce::String&)>&);
}

#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
namespace silente
{
class MainComponent;class AudioEngine;
// Pool de medios: abrir con el atajo, ordenar, doble clic selecciona clips, reasignar con deshacer.
void runPoolUiChecks(MainComponent&,AudioEngine&,const juce::File&,const std::function<void(bool,const juce::String&)>&);
}

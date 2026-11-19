#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
namespace silente
{
class MainComponent;class AudioEngine;
// Panel «Automatización» (F6): se abre con el atajo, modo por pista, Mostrar abre la curva, Suspender lectura/escritura.
void runAutomationUiChecks(MainComponent&,AudioEngine&,const std::function<void(bool,const juce::String&)>&);
}

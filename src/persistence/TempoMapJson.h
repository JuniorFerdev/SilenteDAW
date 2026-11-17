#pragma once
#include "../domain/TempoMap.h"
#include <juce_core/juce_core.h>

namespace silente::persistence
{
// Project JSON property "tempoMap": { "tempos": [{beat,bpm,ramp}], "signatures": [{bar,numerator,denominator}] }.
// Absent in older projects: they read back as an empty map (single tempo, 4/4).
juce::var writeTempoMap(const domain::TempoMap&);
domain::TempoMap readTempoMap(const juce::var&);
}

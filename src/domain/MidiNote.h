#pragma once
#include <juce_core/juce_core.h>

namespace silente::domain
{
// One note of a MIDI or instrument track, in musical time (beats at the project tempo).
// Project data only: no DSP state. The engine reads it under modelLock.
struct MidiNote
{
    juce::String id { juce::Uuid().toString() };
    int pitch = 60, velocity = 100;
    double startBeats = 0.0, lengthBeats = 1.0;
};
}

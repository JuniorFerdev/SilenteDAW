#pragma once
#include "../AudioEngine.h"
#include <vector>

namespace silente::audio
{
// Event editing of the project window tools (Cubase manual, «Herramientas» and Audio menu).

// The event that follows `index` on the track: the nearest one starting at or after it.
int nextEvent(const std::vector<AudioClip>& clips, int index);

// Herramienta Pegar (4): joins the event with the next one. Two pieces of the same recording that
// still touch (after Dividir) become one event again. Anything else is consolidated into a new
// event whose audio is the two events as they sound (fades, gain, the top event over the lower);
// that event has no file yet (see writeEventFile). Cubase builds an audio part in that case.
juce::Result glueEvents(std::vector<AudioClip>& clips, int index);

// Fundido cruzado (X): overlapping events get a fade-out and a fade-in across the overlap and both
// sound there. Events that only touch are first overlapped by `length` seconds around the joint,
// using the audio each event has beyond its edge.
juce::Result crossfadeEvents(std::vector<AudioClip>& clips, int index, double length = 0.01);

// Writes the audio of an event without a file (32-bit float WAV) into `folder` and points the
// event at it, so the project can be saved.
juce::Result writeEventFile(AudioClip&, const juce::File& folder);
}

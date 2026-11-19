#pragma once
#include <juce_graphics/juce_graphics.h>
#include <vector>

namespace silente::testing
{
// Synthetic movie with real pictures (H.264 in QuickTime, written with AVFoundation): second k
// is filled with perSecond[k]; the top quarter of every frame is white, so a test can tell an
// upright picture from a flipped one. One second per colour.
juce::Result writeColourMovie(const juce::File&, const std::vector<juce::Colour>& perSecond, int fps = 10, int width = 160, int height = 96);
}

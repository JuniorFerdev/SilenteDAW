#pragma once
#include "../domain/TrackData.h"

namespace silente::persistence
{
juce::var writeTrackData(const domain::TrackData&);
domain::TrackData readTrackData(const juce::var&);
}

#pragma once
#include "../domain/TrackData.h"

namespace silente::persistence
{
// `projectFolder` (optional) lets video events also keep their path relative to the project, so
// a moved project folder still finds its pictures. Fields absent in older projects take defaults.
juce::var writeTrackData(const domain::TrackData&, const juce::File& projectFolder = {});
domain::TrackData readTrackData(const juce::var&, const juce::File& projectFolder = {});
}

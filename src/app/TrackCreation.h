#pragma once
#include "../AudioEngine.h"
#include <optional>
namespace silente::app
{
struct TrackCreation
{
    TrackKind kind=TrackKind::audio;
    juce::String name,output;
    int count=1,inputLeft=0,inputRight=1,rulerFormat=0;
    std::optional<juce::PluginDescription> plugin;
};
juce::Result validateTrackCreation(const AudioEngine&,const TrackCreation&);
juce::Result createTracks(AudioEngine&,const TrackCreation&,std::vector<std::shared_ptr<Track>>& created);
}

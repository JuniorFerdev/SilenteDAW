#pragma once
#include "../../domain/TrackData.h"
#include <optional>

namespace silente::ui
{
struct TimelineView
{
    juce::Rectangle<int> lane;
    double scale=75.0, scroll=0.0;
    float x(double seconds) const { return (float)(lane.getX()+(seconds-scroll)*scale); }
};
struct MarkerHit { juce::String id; int edge=0; }; // 0 body, 1 start, 2 end
juce::Rectangle<float> markerBounds(const domain::Marker&,const TimelineView&);
std::optional<MarkerHit> hitMarker(const domain::TrackData&,const TimelineView&,juce::Point<float>);
void paintMarkers(juce::Graphics&,const domain::TrackData&,const TimelineView&,juce::Colour,const juce::String& selected);
void paintRulerTrack(juce::Graphics&,const domain::TrackData&,const TimelineView&,double tempo,double rate);
juce::String timelinePosition(double seconds,int format,double tempo,double rate);
std::optional<double> parseTimelinePosition(const juce::String&,int format,double tempo,double rate);
juce::String rulerFormatName(int format);
}

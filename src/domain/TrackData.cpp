#include "TrackData.h"
#include <algorithm>
#include <cmath>

namespace silente::domain
{
Marker* findMarker(TrackData& data,const juce::String& id)
{
    const auto it=std::find_if(data.markers.begin(),data.markers.end(),[&](const Marker& marker){return marker.id==id;});
    return it==data.markers.end()?nullptr:&*it;
}
const Marker* findMarker(const TrackData& data,const juce::String& id)
{
    const auto it=std::find_if(data.markers.begin(),data.markers.end(),[&](const Marker& marker){return marker.id==id;});
    return it==data.markers.end()?nullptr:&*it;
}
bool validMarker(const Marker& marker)
{
    return marker.id.isNotEmpty()&&marker.number>0&&std::isfinite(marker.position)&&marker.position>=0.0
        &&std::isfinite(marker.end)&&(!marker.cycle||marker.end>marker.position);
}
Marker makeMarker(const TrackData& data,double position,bool cycle,double end)
{
    Marker result;result.position=position;result.end=cycle?end:position;result.cycle=cycle;
    while(std::any_of(data.markers.begin(),data.markers.end(),[&](const auto& marker){return marker.cycle==cycle&&marker.number==result.number;}))++result.number;
    return result;
}
}

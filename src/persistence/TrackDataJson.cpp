#include "TrackDataJson.h"
#include <cmath>
#include <set>

namespace silente::persistence
{
namespace
{
void set(juce::var& object,const char* name,juce::var value){object.getDynamicObject()->setProperty(name,std::move(value));}
double finite(const juce::var& object,const char* name,double fallback)
{
    const auto value=(double)object.getProperty(name,fallback);return std::isfinite(value)?value:fallback;
}
}
juce::var writeTrackData(const domain::TrackData& data)
{
    juce::var object(new juce::DynamicObject());
    set(object,"parentFolder",data.parentFolder);set(object,"folderCollapsed",data.folderCollapsed);
    set(object,"rulerFormat",data.rulerFormat);set(object,"rulerOffset",data.rulerOffset);
    juce::Array<juce::var> markers;
    for(const auto& marker:data.markers)
    {
        juce::var value(new juce::DynamicObject());set(value,"id",marker.id);set(value,"number",marker.number);
        set(value,"description",marker.description);set(value,"position",marker.position);set(value,"end",marker.end);
        set(value,"cycle",marker.cycle);set(value,"colour",marker.colour.toString());markers.add(value);
    }
    set(object,"markers",markers);return object;
}
domain::TrackData readTrackData(const juce::var& object)
{
    domain::TrackData data;
    data.parentFolder=object.getProperty("parentFolder",{}).toString();data.folderCollapsed=(bool)object.getProperty("folderCollapsed",false);
    data.rulerFormat=juce::jlimit(0,4,(int)object.getProperty("rulerFormat",0));data.rulerOffset=finite(object,"rulerOffset",0.0);
    std::set<juce::String> ids;std::set<int> points,cycles;
    const auto markerValues=object.getProperty("markers",{});
    if(const auto* markers=markerValues.getArray())for(const auto& value:*markers)
    {
        domain::Marker marker;marker.id=value.getProperty("id",marker.id).toString();
        if(marker.id.isEmpty()||ids.count(marker.id))marker.id=juce::Uuid().toString();
        marker.position=juce::jmax(0.0,finite(value,"position",0.0));marker.cycle=(bool)value.getProperty("cycle",false);
        marker.end=finite(value,"end",marker.position);marker.description=value.getProperty("description",{}).toString();
        if(!marker.cycle)marker.end=marker.position;
        marker.colour=juce::Colour::fromString(value.getProperty("colour","00000000").toString());
        marker.number=(int)value.getProperty("number",1);auto& numbers=marker.cycle?cycles:points;
        if(marker.number<1||numbers.count(marker.number)){marker.number=1;while(numbers.count(marker.number))++marker.number;}
        if(!domain::validMarker(marker))continue;
        ids.insert(marker.id);numbers.insert(marker.number);data.markers.push_back(std::move(marker));
    }
    return data;
}
}

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
juce::var writeTrackData(const domain::TrackData& data,const juce::File& projectFolder)
{
    juce::var object(new juce::DynamicObject());
    // Folder fields only when used: projects without folders carry no folder data.
    if(data.parentFolder.isNotEmpty())set(object,"parentFolder",data.parentFolder);if(data.folderCollapsed)set(object,"folderCollapsed",true);
    set(object,"rulerFormat",data.rulerFormat);set(object,"rulerOffset",data.rulerOffset);
    juce::Array<juce::var> markers;
    for(const auto& marker:data.markers)
    {
        juce::var value(new juce::DynamicObject());set(value,"id",marker.id);set(value,"number",marker.number);
        set(value,"description",marker.description);set(value,"position",marker.position);set(value,"end",marker.end);
        set(value,"cycle",marker.cycle);set(value,"colour",marker.colour.toString());markers.add(value);
    }
    set(object,"markers",markers);
    if(!data.videoEvents.empty())
    {   // Only video tracks carry this field; older Silente versions ignore it.
        juce::Array<juce::var> events;
        for(const auto& event:data.videoEvents)
        {
            juce::var value(new juce::DynamicObject());set(value,"id",event.id);set(value,"path",event.path);set(value,"name",event.name);
            if(projectFolder!=juce::File()&&juce::File::isAbsolutePath(event.path))set(value,"relativePath",juce::File(event.path).getRelativePathFrom(projectFolder));
            set(value,"start",event.start);set(value,"length",event.length);set(value,"offset",event.offset);set(value,"still",event.still);
            set(value,"width",event.width);set(value,"height",event.height);set(value,"frameRate",event.frameRate);
            if(event.sourceLength>0.0)set(value,"sourceLength",event.sourceLength);events.add(value);
        }
        set(object,"videoEvents",events);
    }
    return object;
}
domain::TrackData readTrackData(const juce::var& object,const juce::File& projectFolder)
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
    std::set<juce::String> eventIds;
    const auto videoValues=object.getProperty("videoEvents",{});
    if(const auto* events=videoValues.getArray())for(const auto& value:*events)
    {
        domain::VideoEvent event;event.id=value.getProperty("id",event.id).toString();
        if(event.id.isEmpty()||eventIds.count(event.id))event.id=juce::Uuid().toString();
        event.path=value.getProperty("path",{}).toString();event.name=value.getProperty("name",{}).toString();
        // A project moved with its media: the relative path wins when the absolute one is gone.
        const auto relative=value.getProperty("relativePath",{}).toString();
        if(relative.isNotEmpty()&&projectFolder!=juce::File()&&!juce::File::isAbsolutePath(relative)&&(!juce::File::isAbsolutePath(event.path)||!juce::File(event.path).exists()))
            if(const auto moved=projectFolder.getChildFile(relative);moved.exists()||event.path.isEmpty())event.path=moved.getFullPathName();
        event.start=finite(value,"start",0.0);event.length=finite(value,"length",0.0);event.offset=finite(value,"offset",0.0);
        event.still=(bool)value.getProperty("still",false);event.frameRate=finite(value,"frameRate",0.0);
        event.sourceLength=juce::jmax(0.0,finite(value,"sourceLength",0.0)); // older projects: unknown
        event.width=juce::jmax(0,(int)value.getProperty("width",0));event.height=juce::jmax(0,(int)value.getProperty("height",0));
        if(event.name.isEmpty())event.name=juce::File::createFileWithoutCheckingPath(event.path).getFileName();
        if(!domain::validVideoEvent(event))continue;
        eventIds.insert(event.id);data.videoEvents.push_back(std::move(event));
    }
    return data;
}
}

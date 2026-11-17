#include "TimelineTracks.h"
#include "SnapGrid.h"
#include "../../i18n/Translations.h"
#include "../theme/Drawing.h"
#include <cmath>

namespace silente::ui
{
using namespace theme;
juce::Rectangle<float> markerBounds(const domain::Marker& marker,const TimelineView& view)
{
    const auto y=(float)view.lane.getY(),h=(float)view.lane.getHeight();
    if(marker.cycle)return {view.x(marker.position),y+3,juce::jmax(2.0f,view.x(marker.end)-view.x(marker.position)),h*.43f};
    return {view.x(marker.position),y+h*.52f,juce::jlimit(35.0f,180.0f,28.0f+textWidth(marker.description,11)),h*.44f};
}
std::optional<MarkerHit> hitMarker(const domain::TrackData& data,const TimelineView& view,juce::Point<float> point)
{
    for(auto it=data.markers.rbegin();it!=data.markers.rend();++it)
    {
        auto r=markerBounds(*it,view);
        if(r.expanded(3,2).contains(point))return MarkerHit{it->id,it->cycle?(std::abs(point.x-r.getX())<7?1:std::abs(point.x-r.getRight())<7?2:0):0};
    }
    return {};
}
void paintMarkers(juce::Graphics& g,const domain::TrackData& data,const TimelineView& view,juce::Colour trackColour,const juce::String& selected)
{
    for(const auto& marker:data.markers)
    {
        auto r=markerBounds(marker,view);if(!g.getClipBounds().toFloat().intersects(r))continue;
        const bool chosen=marker.id==selected;const auto colour=marker.colour.isTransparent()?trackColour:marker.colour;
        g.setColour(colour.withAlpha(chosen?1.0f:.82f));
        if(marker.cycle)
        {
            g.fillRect(r);g.setColour(chosen?ink:colour.brighter(.3f));g.drawRect(r,chosen?1.5f:1.0f);
            g.drawVerticalLine((int)r.getX(),r.getY(),(float)view.lane.getBottom()-2);g.drawVerticalLine((int)r.getRight(),r.getY(),(float)view.lane.getBottom()-2);
        }
        else
        {
            juce::Path flag;flag.startNewSubPath(r.getX(),r.getBottom());flag.lineTo(r.getX(),r.getY());flag.lineTo(r.getRight()-6,r.getY());flag.lineTo(r.getRight(),r.getCentreY());flag.lineTo(r.getRight()-6,r.getBottom()-2);flag.lineTo(r.getX()+1,r.getBottom()-2);flag.closeSubPath();g.fillPath(flag);
            g.setColour(chosen?ink:colour.brighter(.3f));g.strokePath(flag,juce::PathStrokeType(chosen?1.5f:1.0f));
        }
        text(g,(marker.cycle?juce::String("[")+juce::String(marker.number)+"]":juce::String(marker.number))+" "+marker.description,r.toNearestInt().reduced(5,0),11,activeInk);
    }
}
juce::String rulerFormatName(int format)
{
    switch(format){case 1:return i18n::tr("Segundos");case 2:return i18n::tr("Código de tiempo");case 3:return i18n::tr("Muestras");case 4:return i18n::tr("Fotogramas (60 fps)");default:return i18n::tr("Compases+Tiempos");}
}
juce::String timelinePosition(double seconds,int format,double tempo,double rate)
{
    return timelinePosition(seconds,format,domain::TempoView{juce::jlimit(domain::minimumBpm,domain::maximumBpm,tempo),nullptr},rate);
}
std::optional<double> parseTimelinePosition(const juce::String& input,int format,double tempo,double rate)
{
    return parseTimelinePosition(input,format,domain::TempoView{juce::jlimit(domain::minimumBpm,domain::maximumBpm,tempo),nullptr},rate);
}
juce::String timelinePosition(double seconds,int format,const domain::TempoView& tempo,double rate)
{
    if(format==4)return juce::String((juce::int64)std::llround(juce::jmax(0.0,seconds)*60));
    return SnapGrid::formatTime(seconds,(TimeFormat)juce::jlimit(0,3,format),tempo,rate);
}
std::optional<double> parseTimelinePosition(const juce::String& input,int format,const domain::TempoView& tempo,double rate)
{
    auto s=input.trim();if(s.isEmpty()||s.containsAnyOf("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-"))return {};
    if(!s.containsOnly("0123456789.,: \t"))return {};
    const auto parts=juce::StringArray::fromTokens(s.replace(",","."),format==0?". ":":","");
    auto values=parts;values.removeEmptyStrings();if(values.isEmpty())return {};
    double seconds=0;
    if(format==0)
    {
        if(values.size()>4)return {};
        const double bar=values[0].getDoubleValue(),beat=values.size()>1?values[1].getDoubleValue():1,six=values.size()>2?values[2].getDoubleValue():1,tick=values.size()>3?values[3].getDoubleValue():0;
        // Beats count in the unit of the bar's signature (eighths in 6/8): up to its numerator.
        const auto signature=tempo.signatureAtBar((int)std::floor(bar-1));
        if(bar<1||beat<1||beat>signature.numerator||six<1||six>4||tick<0||tick>=120)return {};
        seconds=tempo.secondsAt(tempo.beatsFromBarsBeats(bar,beat,six,tick));
    }
    else if(format==3||format==4)
    {if(!s.containsOnly("0123456789"))return {};seconds=s.getDoubleValue()/(format==3?juce::jmax(1.0,rate):60);}
    else if(values.size()==1)seconds=values[0].getDoubleValue();
    else
    {
        if(values.size()!=(format==2?4:3))return {};
        seconds=values[0].getDoubleValue()*3600+values[1].getDoubleValue()*60+values[2].getDoubleValue();
        if(values[1].getDoubleValue()>=60||values[2].getDoubleValue()>=60)return {};
        if(format==2){if(values[3].getDoubleValue()>=30)return {};seconds+=values[3].getDoubleValue()/30;}
    }
    return std::isfinite(seconds)&&seconds>=0?std::optional<double>(seconds):std::nullopt;
}
void paintRulerTrack(juce::Graphics& g,const domain::TrackData& data,const TimelineView& view,const domain::TempoView& tempo,double rate)
{
    g.setColour(panel.brighter(.06f));g.fillRect(view.lane);const double beat=60/tempo.bpm;
    if(data.rulerFormat==0&&!tempo.isConstant())
    {   // Musical ruler with the tempo map: labels on beat multiples placed in time.
        const double secondsPerBeat=60/tempo.bpmAt(tempo.beatsAt(view.scroll+data.rulerOffset));double stepBeats=1;while(stepBeats*secondsPerBeat*view.scale<70)stepBeats*=2;
        const double last=view.scroll+view.lane.getWidth()/view.scale+data.rulerOffset;
        for(double b=std::floor(tempo.beatsAt(juce::jmax(0.0,view.scroll+data.rulerOffset))/stepBeats)*stepBeats;tempo.secondsAt(b)<last;b+=stepBeats)
        {
            const double at=tempo.secondsAt(b);const int x=(int)view.x(at-data.rulerOffset);if(x<view.lane.getX())continue;
            g.setColour(muted);g.drawVerticalLine(x,(float)view.lane.getY()+4,(float)view.lane.getBottom()-4);
            text(g,timelinePosition(at,data.rulerFormat,tempo,rate),{x+4,view.lane.getY()+5,120,20},10,ink);
        }
        return;
    }
    double step=data.rulerFormat==0?beat:1.0;while(step*view.scale<70)step*=2;
    const double from=std::floor((view.scroll+data.rulerOffset)/step)*step;
    for(double at=from;at<view.scroll+view.lane.getWidth()/view.scale+data.rulerOffset;at+=step)
    {
        const int x=(int)view.x(at-data.rulerOffset);if(x<view.lane.getX())continue;
        g.setColour(muted);g.drawVerticalLine(x,(float)view.lane.getY()+4,(float)view.lane.getBottom()-4);
        text(g,timelinePosition(at,data.rulerFormat,tempo,rate),{x+4,view.lane.getY()+5,120,20},10,ink);
        if(step*view.scale>100)for(int k=1;k<4;++k)g.drawVerticalLine((int)(x+step*view.scale*k/4),(float)view.lane.getBottom()-10,(float)view.lane.getBottom()-4);
    }
}
}

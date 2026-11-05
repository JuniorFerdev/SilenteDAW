#include "i18n/Translations.h"
#include "MainComponent.h"
#include "i18n/UiTranslation.h"
#include "ui/theme/Drawing.h"
#include "ui/theme/SilenteLookAndFeel.h"
#include "ui/theme/ToolIcons.h"
#include "ui/theme/TrackColourPicker.h"
#include "ui/plugins/PluginSlotButton.h"
#include "ui/audio/AudioSettingsPanel.h"
#include "ui/settings/SettingsWorkspace.h"
#include "app/Preferences.h"
#include "ui/metronome/MetronomePanel.h"
#include "ui/metronome/TapTempoPanel.h"
#include "ui/mixconsole/MixConsole.h"
#include "ui/project/ChannelTab.h"
#include "ui/project/TrackFinder.h"
#include "ui/project/SnapGrid.h"
#include "ui/project/ZoneSplitter.h"
#include "ui/project/FadeEditor.h"
#include "ui/project/ProjectToolbox.h"
#include "audio/FadeCurves.h"
#include "audio/RangeEdit.h"
#include "audio/EventEdit.h"
#include "app/KeyCommands.h"
#include "ui/keys/KeyCommandsPanel.h"
#include "ui/project/EditHistoryPanel.h"
#include <map>
#include "ui/io/BackgroundJob.h"
#include "ui/io/ConversionPanel.h"
#include "ui/io/ExportDialog.h"
#include "io/AudioFileFormats.h"
#include "io/VideoAudio.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <numeric>
#include <optional>
#include <set>

namespace silente
{
namespace
{
using namespace silente::ui::theme;
juce::String kindLabel(TrackKind kind)
{
    switch(kind){case TrackKind::instrument:return i18n::tr("INSTRUMENTO");case TrackKind::midi:return i18n::tr("MIDI");case TrackKind::folder:return i18n::tr("CARPETA");case TrackKind::fx:return i18n::tr("FX");case TrackKind::group:return i18n::tr("GRUPO");default:return i18n::tr("AUDIO");}
}
juce::String kindDescription(TrackKind kind)
{
    switch(kind){case TrackKind::instrument:return i18n::tr("Instrumento MIDI");case TrackKind::midi:return i18n::tr("Pista MIDI");case TrackKind::folder:return i18n::tr("Pista de carpeta");case TrackKind::fx:return i18n::tr("Canal de efectos");case TrackKind::group:return i18n::tr("Canal de grupo");default:return i18n::tr("Audio estéreo");}
}
juce::String gainLabel(float value) { return value < 0.0001f ? juce::String::fromUTF8("−∞ dB") : juce::String(juce::Decibels::gainToDecibels(value), 1) + " dB"; }
void meter(juce::Graphics& g, juce::Rectangle<float> r, float level)
{
    g.setColour(bg); g.fillRoundedRectangle(r, 2);
    const auto norm = juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(juce::jmax(0.00001f, level)) + 60.0f) / 60.0f);
    auto fill = r.removeFromBottom(r.getHeight() * norm);
    g.setColour(level > 1.0f ? coral : (level > .8f ? meterWarning : meterColour)); g.fillRoundedRectangle(fill, 2);
}


class Arrangement final : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    explicit Arrangement(AudioEngine& e) : engine(e) { setComponentID("arrangement"); setWantsKeyboardFocus(true); setMouseCursor(juce::MouseCursor::NormalCursor); }
    AudioEngine& engine;
    std::function<void(int)> selectTrack;
    std::function<void(int, juce::Rectangle<int>)> editColour;
    std::function<void()> beforeEdit, changed, openPiano;
    std::function<void(int)> openChannel;
    std::function<void(const juce::String&)> warn;
    std::function<void()> eventColour, rangeMenu;
    std::function<void(double,int,bool)> moveRange; // seconds, rows, copy (⌥)
    bool rangeMoving=false;double moveFrom=0,moveDelta=0;int moveFromRow=0,moveRows=0;
    bool insideRange(double at,int row)const{return hasRange&&at>=rangeStart&&at<=rangeEnd&&row>=rangeFirstRow&&row<=rangeLastRow&&rangeEnd>rangeStart;}
    // Selección de rango (herramienta 2, manual pp. 295–305): a time span over consecutive rows.
    bool hasRange=false,rangeDragging=false;double rangeStart=0,rangeEnd=0,rangeAnchor=0;int rangeFirstRow=0,rangeLastRow=0,rangeAnchorRow=0;
    int rowAt(int y)const{return (y-ruler)/lane+firstTrack;}
    std::vector<int> rangeTracks()const{auto v=visibleTracks();std::vector<int> out;for(int r=rangeFirstRow;r<=rangeLastRow;++r)if(r>=0&&r<(int)v.size())out.push_back(v[(size_t)r]);return out;}
    void updateRange(double at,int row)
    {
        const int rows=juce::jmax(1,(int)visibleTracks().size());row=juce::jlimit(0,rows-1,row);
        rangeStart=juce::jmin(rangeAnchor,at);rangeEnd=juce::jmax(rangeAnchor,at);rangeFirstRow=juce::jmin(rangeAnchorRow,row);rangeLastRow=juce::jmax(rangeAnchorRow,row);
    }
    void setRange(double from,double to,int firstRow,int lastRow){hasRange=true;rangeAnchor=from;rangeAnchorRow=firstRow;updateRange(to,lastRow);repaint();}
    std::function<void()> trackFilterMenu, findTracks;
    static juce::Rectangle<int> filterButton() { return { header-62, 4, 24, 22 }; }
    static juce::Rectangle<int> searchButton() { return { header-32, 4, 24, 22 }; }
    std::function<void(const juce::StringArray&, int, double)> import;
    int selectedTrack = 0, selectedClip = -1;
    ui::SnapGrid snapGrid;
    bool& snap = snapGrid.enabled;
    int tool = 1; // 1 select, 3 scissors, 5 erase, 8 draw MIDI
    ui::TrackFilter filter;
    std::set<juce::String>& hiddenTracks = filter.hiddenTracks;
    double pixelsPerSecond = 75, scrollSeconds = 0;
    int firstTrack = 0;
    static constexpr int header = 250, ruler = 30, minLane = 40, maxLane = 240;
    // Track height (Cubase: vertical zoom) and waveform zoom of the event display.
    int lane = 60; float waveZoom = 1.0f;
    double xToSeconds(float x) const { return juce::jmax(0.0, scrollSeconds + (x - header)/pixelsPerSecond); }
    float secondsToX(double s) const { return (float)(header + (s-scrollSeconds)*pixelsPerSecond); }
    // Snap with the project window's Ajustar settings; the dragged event is not its own magnet.
    double snapped(double s,int ignoreTrack=-1,int ignoreClip=-1) const
    {
        std::vector<double> edges;
        if(snapGrid.usesEvents())for(int t=0;t<(int)engine.tracks.size();++t)for(int c=0;c<(int)engine.tracks[(size_t)t]->clips.size();++c)
            if(!(t==ignoreTrack&&c==ignoreClip)){const auto& clip=engine.tracks[(size_t)t]->clips[(size_t)c];edges.push_back(clip.startSeconds);edges.push_back(clip.startSeconds+clip.durationSeconds);}
        return snapGrid.snap(s,engine.tempo.load(),pixelsPerSecond,edges,engine.positionSeconds.load());
    }
    std::function<void()> rulerMenu;
    std::function<void(int)> chooseTool;
    std::function<void(const juce::String&)> contextAction;
    std::function<void(const juce::String&,const juce::String&,juce::Rectangle<int>)> paintEventColour;
    std::vector<int> visibleTracks() const { return filter.visible(engine.tracks); }
    int visualRow(int i) const {auto v=visibleTracks();auto it=std::find(v.begin(),v.end(),i);return it==v.end()?-999:(int)std::distance(v.begin(),it)-firstTrack;}
    int trackAt(int y) const { auto v=visibleTracks();int n=(y-ruler)/lane+firstTrack;return y>=ruler&&n>=0&&n<(int)v.size()?v[(size_t)n]:-1; }
    void setTool(int value){tool=value;setMouseCursor(value==3||value==8?juce::MouseCursor::CrosshairCursor:value==6?juce::MouseCursor::CrosshairCursor:value==9?juce::MouseCursor::PointingHandCursor:juce::MouseCursor::NormalCursor);repaint();}
    void zoom(double factor) { pixelsPerSecond = juce::jlimit(12.0, 480.0, pixelsPerSecond*factor); repaint(); }
    void paint(juce::Graphics& g) override
    {
        // Model containers are only mutated on the message thread (under modelLock, which the audio
        // thread try-locks). Message-thread reads such as paint must NOT take the lock: holding it
        // while drawing made the audio callback drop blocks (crackling playback).
        g.fillAll(bg); g.setColour(eventArea); g.fillRect(header,ruler,getWidth()-header,getHeight()-ruler); 
        g.setColour(panel); g.fillRect(0,0,header,getHeight());
        g.setColour(rulerBackground); g.fillRect(header,0,getWidth()-header,ruler);
        const double beat = 60.0/engine.tempo.load(), bar=beat*4;
        const int startBeat=(int)std::floor(scrollSeconds/beat), endBeat=(int)std::ceil(xToSeconds((float)getWidth())/beat);
        if(snapGrid.format==ui::TimeFormat::barsBeats)
        {
        for (int b=startBeat; b<=endBeat; ++b)
        {
            const auto x=secondsToX(b*beat); if(x<header)continue;
            const bool isBar=b%4==0;
            g.setColour(isBar ? gridBar.withAlpha(.65f) : gridBeat); g.drawVerticalLine((int)x,ruler,(float)getHeight());
            g.setColour(isBar ? ink : muted); g.drawVerticalLine((int)x,isBar?11.0f:20.0f,(float)ruler);
            if(isBar)text(g,juce::String(b/4+1),{(int)x+5,11,55,ruler-11},11,ink);
            else if(beat*pixelsPerSecond>36)text(g,juce::String(b%4+1),{(int)x+4,16,22,14},9,muted);
            if(beat*pixelsPerSecond>32)for(int sub=1;sub<4;++sub)
            { int sx=(int)(x+sub*beat*pixelsPerSecond/4);g.setColour(gridBeat.withAlpha(.45f));g.drawVerticalLine(sx,ruler,(float)getHeight());g.setColour(muted.withAlpha(.6f));g.drawVerticalLine(sx,26,(float)ruler); }
        }
        }
        else
        {   // Seconds, timecode or samples: labels every 1, 2, 5, 10, 30 or 60 s, at least 90 px apart.
            double step=1.0;for(double candidate:{1.0,2.0,5.0,10.0,30.0,60.0,120.0}){step=candidate;if(candidate*pixelsPerSecond>=90.0)break;}
            for(double t=std::floor(scrollSeconds/step)*step;t<=xToSeconds((float)getWidth());t+=step)
            {
                const auto x=secondsToX(t);if(x<header)continue;
                g.setColour(gridBar.withAlpha(.65f));g.drawVerticalLine((int)x,ruler,(float)getHeight());g.setColour(ink);g.drawVerticalLine((int)x,11.0f,(float)ruler);
                text(g,ui::SnapGrid::formatTime(t,snapGrid.format,engine.tempo.load(),engine.currentSampleRate()),{(int)x+5,11,110,ruler-11},10,ink);
            }
        }
        const float loopX=secondsToX(engine.loopStart),loopW=secondsToX(engine.loopEnd)-loopX;
        { juce::Graphics::ScopedSaveState clipped(g);g.reduceClipRegion(header,0,getWidth()-header,ruler);
          g.setColour(cycleColour.withAlpha(engine.looping?.8f:.3f));g.fillRect(loopX,0.0f,loopW,10.0f);
          g.setColour(cycleColour.brighter(.25f));juce::Path handles;handles.addTriangle(loopX,0,loopX+8,0,loopX,10);handles.addTriangle(loopX+loopW,0,loopX+loopW-8,0,loopX+loopW,10);g.fillPath(handles); }
        // Global track controls (manual p. 69): visible/total, track type filter, find tracks.
        text(g,juce::String((int)visibleTracks().size())+" / "+juce::String((int)engine.tracks.size()),{34,0,70,ruler},11,muted);
        if(filter.filtering()){g.setColour(accent.withAlpha(.35f));g.fillRoundedRectangle(filterButton().toFloat(),3);}
        drawToolIcon(g,ToolIcon::filter,filterButton().toFloat().reduced(4),filter.filtering()?ink:muted);
        drawToolIcon(g,ToolIcon::search,searchButton().toFloat().reduced(4),muted);
        auto visible=visibleTracks();const auto dirtyArea=g.getClipBounds();
        for (int rowIndex=firstTrack;rowIndex<(int)visible.size();++rowIndex)
        {
            int i=visible[(size_t)rowIndex];const int y=ruler+(rowIndex-firstTrack)*lane; if (y>getHeight()) break;
            if(!dirtyArea.intersects(juce::Rectangle<int>(0,y,getWidth(),lane)))continue; // only rows in the area being repainted
            auto& t=*engine.tracks[(size_t)i]; const auto colour=t.colour;
            g.setColour(i==selectedTrack ? selectedTrackColour() : panel); g.fillRect(0,y,header,lane);
            if (i==selectedTrack) { g.setColour(colour.withAlpha(.035f)); g.fillRect(header,y,getWidth()-header,lane); }
            // Cubase track header: colour strip with type icon and number; row 1 M · S · name · volume;
            // row 2 record · monitor · R · W · e; thin level meter at the right edge.
            if(dirtyArea.getX()<header)
            {
            g.setColour(colour); g.fillRect(0,y,strip,lane-1);
            trackIcon(g,t.kind,{4.0f,(float)y+6,15.0f,15.0f},eventInk);
            text(g,juce::String(i+1),{18,y+5,strip-20,18},10,eventInk,juce::Justification::centredRight);
            const bool chosen=i==selectedTrack;const auto headerInk=chosen?activeInk:ink;
            if(t.kind!=TrackKind::folder){chip(g,"M",{46,y+5,22,20},t.mute,muteColour);chip(g,"S",{70,y+5,22,20},t.solo,soloColour);}
            text(g,t.name,{98,y+4,header-232,22},13,headerInk);
            if(t.kind!=TrackKind::folder)
            {
                const auto v=volumeRect(y);const float db=juce::Decibels::gainToDecibels(t.gain.load(),-100.0f);const float p=volumeProportion(db);
                g.setColour(chosen?activeInk.withAlpha(.5f):muted);juce::Path tri;tri.addTriangle((float)v.getX()-14,(float)v.getBottom()-2,(float)v.getX()-4,(float)v.getBottom()-2,(float)v.getX()-4,(float)v.getY()+3);g.fillPath(tri);
                g.setColour(chosen?activeInk.withAlpha(.35f):raised);g.fillRoundedRectangle(v.toFloat().withSizeKeepingCentre((float)v.getWidth(),3.0f),1.5f);
                g.setColour(chosen?activeInk:ink.withAlpha(.85f));g.fillRoundedRectangle(v.toFloat().withWidth((float)v.getWidth()*p).withSizeKeepingCentre((float)v.getWidth()*p,3.0f).withX((float)v.getX()),1.5f);
                g.setColour(chosen?activeInk:juce::Colours::white);g.fillEllipse((float)v.getX()+(float)v.getWidth()*p-5.0f,(float)v.getCentreY()-5.0f,10.0f,10.0f);
                text(g,db<=-60.0f?juce::String::fromUTF8("-∞"):juce::String(db,2)+" dB",{v.getRight()+6,y+4,52,22},10,headerInk,juce::Justification::centredRight);
            }
            // Second row only when the track is tall enough (Cubase hides it on small tracks).
            if(lane>=56&&t.hasEvents()){chip(g,juce::String::fromUTF8("●"),{98,y+32,22,20},t.armed,coral);chip(g,juce::String::fromUTF8("◉"),{122,y+32,22,20},t.monitor,meterWarning);}
            if(lane>=56&&t.kind!=TrackKind::folder){chip(g,"R",{150,y+32,22,20},false,mint);chip(g,"W",{174,y+32,22,20},false,coral);chip(g,"e",{202,y+32,22,20},false,accent);}
            meter(g,{(float)header-7.0f,(float)y+4,3.0f,(float)lane-9},juce::jmax(t.peakL.load(),t.peakR.load()));
            }
            g.setColour(line.withAlpha(.65f)); g.drawHorizontalLine(y+lane-1,0,(float)getWidth());
            if(dirtyArea.getRight()<=header)continue; // nothing of the event lane is being repainted
            juce::Graphics::ScopedSaveState state(g); g.reduceClipRegion(header,y,getWidth()-header,lane);
            if (t.isMidi && !t.notes.empty())
            {
                double end=4; for (auto& n:t.notes) end=juce::jmax(end,n.startBeats+n.lengthBeats);
                auto r=juce::Rectangle<float>(secondsToX(0),(float)y+8, (float)(end*beat*pixelsPerSecond),lane-16.0f);
                g.setColour(colour.withAlpha(.82f));g.fillRoundedRectangle(r,2);g.setColour(colour.brighter(.25f));g.drawRoundedRectangle(r.reduced(.5f),2,1);
                text(g,t.name + juce::String::fromUTF8(" · MIDI"),r.removeFromTop(22).toNearestInt().reduced(8,0),11,eventInk);
                for (auto& n:t.notes) { float x=secondsToX(n.startBeats*beat); float ny=(float)y+35+(84-n.pitch)*.75f; ny=juce::jlimit((float)y+34,juce::jmax((float)y+34,(float)y+lane-13),ny); g.setColour(eventInk.withAlpha(.85f));g.fillRoundedRectangle(x,ny,juce::jmax(3.0f,(float)(n.lengthBeats*beat*pixelsPerSecond)-2),3,1); }
            }
            for(int c=0;c<(int)t.clips.size();++c)
            {
                const auto& clip=t.clips[(size_t)c]; auto r=clipRect(i,clip); if (!r.intersects(dirtyArea.toFloat().expanded(2.0f))) continue;
                const bool selected=i==selectedTrack&&c==selectedClip;
                const auto tone=clip.colour.isTransparent()?colour:clip.colour; // own event colour overrides the track's
                const juce::Graphics::ScopedSaveState faded(g);if(clip.muted)g.setOpacity(.35f); // muted events look switched off
                g.setColour(selected ? tone.brighter(.12f) : tone.withAlpha(.82f));g.fillRoundedRectangle(r,2);
                g.setColour(selected ? ink : tone.brighter(.3f));g.drawRoundedRectangle(r.reduced(.5f),2,selected?1.5f:1);
                g.setColour(tone.brighter(.1f));g.fillRoundedRectangle(r.withHeight(22),2);
                text(g,clip.name,r.withHeight(22).reduced(9,0).toNearestInt(),11,eventInk);
                drawWave(g,clip,r.withTrimmedTop(24).reduced(2,5),eventInk);
                if(clip.audio)drawFades(g,clip,r,selected);
                if(clip.crossfade&&clip.fadeIn>0.0)
                {   // Crossfade: the two fade lines across the overlap.
                    const float x0=r.getX(),x1=r.getX()+(float)(clip.fadeIn*pixelsPerSecond);g.setColour(ink.withAlpha(.9f));
                    g.drawLine(x0,r.getBottom()-2,juce::jmax(x0+1,x1),r.getY()+2,1.4f);g.drawLine(x0,r.getY()+2,juce::jmax(x0+1,x1),r.getBottom()-2,1.4f);
                }
                if(selected) { g.setColour(ink.withAlpha(.65f));g.fillRoundedRectangle(r.getRight()-5,r.getCentreY()-8,2,16,1); }
            }
            drawRecording(g,t,y);
        }
        if(hasRange)
        {   // Range highlight over its rows (a zero-length range is the range cursor).
            const float x0=secondsToX(rangeStart),x1=juce::jmax(x0+1.0f,secondsToX(rangeEnd));
            for(int row=juce::jmax(rangeFirstRow,firstTrack);row<=rangeLastRow;++row)
            {
                const float y=(float)(ruler+(row-firstTrack)*lane);if(y>getHeight())break;
                const juce::Rectangle<float> area(juce::jmax((float)header,x0),y,x1-juce::jmax((float)header,x0),(float)lane);if(area.getWidth()<=0)continue;
                g.setColour(ink.withAlpha(.16f));g.fillRect(area);g.setColour(accent);g.drawVerticalLine((int)x0,y,y+lane);g.drawVerticalLine((int)x1,y,y+lane);
            }
            if(rangeMoving&&(std::abs(moveDelta)>1e-9||moveRows!=0))
            {   // Where the content will land.
                const float gx=secondsToX(rangeStart+moveDelta),gw=juce::jmax(1.0f,(float)((rangeEnd-rangeStart)*pixelsPerSecond));
                const float gy=(float)(ruler+(rangeFirstRow+moveRows-firstTrack)*lane),gh=(float)((rangeLastRow-rangeFirstRow+1)*lane);
                g.setColour(accent.withAlpha(.18f));g.fillRect(gx,gy,gw,gh);g.setColour(accent);g.drawRect(gx,gy,gw,gh,1.5f);
            }
        }
        if(engine.tracks.empty()) { text(g,i18n::tr("Tu próxima idea empieza aquí"),{header+30,100,getWidth()-header-60,32},23,ink,juce::Justification::centred); text(g,i18n::tr("Arrastra audio o MIDI, o añade una pista."),{header+30,139,getWidth()-header-60,25},14,muted,juce::Justification::centred); }
        float px=secondsToX(engine.positionSeconds); if(px>=header&&px<getWidth()) { g.setColour(ink.withAlpha(.85f));g.drawVerticalLine((int)px,0,(float)getHeight()); juce::Path p;p.addTriangle(px-5,4,px+5,4,px,11);g.fillPath(p); }
        g.setColour(line);g.drawVerticalLine(header,0,(float)getHeight());
        if(dragOver){g.setColour(accent.withAlpha(.1f));g.fillAll();g.setColour(accent);g.drawRect(getLocalBounds().reduced(2),2);}
        juce::ignoreUnused(bar);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        grabKeyboardFocus(); dragClip=-1; rulerDragging=false; locatorDrag=0; const int i=trackAt(e.y);
        if(e.y<ruler)
        {
            if(e.mods.isPopupMenu()&&e.x>=header){if(rulerMenu)rulerMenu();return;}
            if(filterButton().contains(e.getPosition()))
            {   // Cmd-click resets the type filter; click opens it.
                if(e.mods.isCommandDown()){filter.hiddenKinds.clear();firstTrack=0;repaint();}
                else if(trackFilterMenu)trackFilterMenu();
                return;
            }
            if(searchButton().contains(e.getPosition())){if(findTracks)findTracks();return;}
            if(e.x>=header)
            {
                const float lx=secondsToX(engine.loopStart),rx=secondsToX(engine.loopEnd);
                locatorDrag=e.y<13 && std::abs(e.x-lx)<10 ? 1 : e.y<13 && std::abs(e.x-rx)<10 ? 2 : 0;
                if(locatorDrag){if(beforeEdit)beforeEdit();}
                else if(e.mods.isLeftButtonDown()){rulerDragging=true;engine.seek(xToSeconds(e.position.x));}
            }
            repaint();return;
        }
        if(e.mods.isPopupMenu()&&e.x>=header)
        {
            const bool showOnRightClick=app::showToolboxOnRightClick();
            if(ui::opensProjectToolbox(showOnRightClick,e.mods))
            {
                juce::CallOutBox::launchAsynchronously(std::make_unique<ui::ProjectToolbox>(tool,[safe=juce::Component::SafePointer<Arrangement>(this)](int value){if(safe&&safe->chooseTool)safe->chooseTool(value);},showOnRightClick),localAreaToGlobal(juce::Rectangle<int>(e.x,e.y,1,1)),nullptr);
                return;
            }
        }
        const juce::ScopedLock lock(engine.modelLock); if(i<0||i>=(int)engine.tracks.size())return;
        selectedTrack=i; selectedClip=-1; if(selectTrack)selectTrack(i); auto& t=*engine.tracks[(size_t)i];
        if(e.x<header)
        {
            const int dy=e.y-(ruler+visualRow(i)*lane);
            if(e.x<strip && !e.mods.isPopupMenu()) { if(editColour)editColour(i,localAreaToGlobal(juce::Rectangle<int>{0,ruler+visualRow(i)*lane,strip,lane}));repaint();return; }
            if(!e.mods.isPopupMenu()&&t.kind!=TrackKind::folder)
            {
                const auto v=volumeRect(ruler+visualRow(i)*lane).expanded(6,4);
                if(v.contains(e.getPosition())){if(beforeEdit)beforeEdit();volumeDrag=i;if(e.getNumberOfClicks()>1)t.gain=1.0f;else setVolumeFrom(t,e.x);if(changed)changed();repaint();return;}
                if(dy>=5&&dy<25){if(e.x>=46&&e.x<68){if(beforeEdit)beforeEdit();t.mute=!t.mute;if(changed)changed();}else if(e.x>=70&&e.x<92){if(beforeEdit)beforeEdit();t.solo=!t.solo;if(changed)changed();}}
                else if(lane>=56&&dy>=32&&dy<52)
                {
                    if(e.x>=98&&e.x<120&&t.hasEvents()){if(beforeEdit)beforeEdit();t.armed=!t.armed;if(changed)changed();}
                    else if(e.x>=122&&e.x<144&&t.hasEvents()){if(beforeEdit)beforeEdit();t.monitor=!t.monitor;if(changed)changed();}
                    else if(e.x>=202&&e.x<224&&openChannel){repaint();openChannel(i);return;}
                }
            }
            if(e.mods.isPopupMenu())trackMenu(i); repaint();return;
        }
        if(tool==2&&e.x>=header)
        {   // Range: click and drag; Mayús extends the current range; right click opens its menu.
            if(e.mods.isPopupMenu()){if(hasRange&&rangeMenu)rangeMenu();return;}
            const double at=snapped(xToSeconds((float)e.x));const int row=rowAt(e.y);
            if(!e.mods.isShiftDown()&&insideRange(xToSeconds((float)e.x),row))
            {   // Drag the range to move its content; with ⌥ the content is copied.
                rangeMoving=true;moveFrom=xToSeconds((float)e.x);moveFromRow=row;moveDelta=0;moveRows=0;return;
            }
            if(e.mods.isShiftDown()&&hasRange){rangeAnchor=at<(rangeStart+rangeEnd)*0.5?rangeEnd:rangeStart;rangeAnchorRow=row<(rangeFirstRow+rangeLastRow+1)/2?rangeLastRow:rangeFirstRow;}
            else{rangeAnchor=at;rangeAnchorRow=row;}
            hasRange=rangeDragging=true;selectedClip=-1;dragClip=-1;updateRange(at,row);repaint();return;
        }
        for(int c=(int)t.clips.size()-1;c>=0;--c) if(clipRect(i,t.clips[(size_t)c]).contains(e.position)) { selectedClip=c;dragClip=c;dragOrigin=t.clips[(size_t)c];dragStart=e.position;resizing=e.x>clipRect(i,dragOrigin).getRight()-9;if(!e.mods.isPopupMenu()&&tool==1&&beforeEdit)beforeEdit();
            fadeDrag=0;if(tool==1&&!e.mods.isPopupMenu()&&dragOrigin.audio){const auto cr=clipRect(i,dragOrigin);const auto hit=[&](juce::Rectangle<float> h){return h.expanded(3).contains(e.position);};
                if(hit(fadeInHandle(dragOrigin,cr)))fadeDrag=1;else if(hit(fadeOutHandle(dragOrigin,cr)))fadeDrag=2;else if(hit(volumeHandle(cr)))fadeDrag=3;if(fadeDrag)resizing=false;}
            break; }
        if(e.mods.isPopupMenu())clipMenu();
        else if(tool==13)
        {
            dragClip=-1;
            if(selectedClip>=0)
            {
                const auto trackId=t.id,clipId=t.clips[(size_t)selectedClip].id;
                const auto anchor=localAreaToGlobal(juce::Rectangle<int>(e.x,e.y,1,1));
                juce::MessageManager::callAsync([safe=juce::Component::SafePointer<Arrangement>(this),trackId,clipId,anchor]{if(safe&&safe->paintEventColour)safe->paintEventColour(trackId,clipId,anchor);});
            }
        }
        else if(tool==3&&selectedClip>=0){engine.seek(snapped(xToSeconds((float)e.x)));splitSelected();dragClip=-1;}
        else if(tool==5&&selectedClip>=0){deleteSelected();dragClip=-1;}
        else if(tool==4&&selectedClip>=0){dragClip=-1;juce::MessageManager::callAsync([safe=juce::Component::SafePointer<Arrangement>(this)]{if(safe)safe->glueSelected();});} // after mouseDown releases the model lock
        else if(tool==7&&selectedClip>=0){dragClip=-1;if(beforeEdit)beforeEdit();auto& c=t.clips[(size_t)selectedClip];c.muted=!c.muted;if(changed)changed();}
        else if(tool==6){dragClip=-1;zoomAt((float)e.x,e.mods.isAltDown()?1.0/1.6:1.6);}
        else if(tool==9){dragClip=-1;playTool=true;engine.seek(xToSeconds((float)e.x));engine.play();}
        else if(tool==8&&t.isMidi){if(beforeEdit)beforeEdit();MidiNote n;n.startBeats=snapped(xToSeconds((float)e.x))*engine.tempo/60;n.pitch=60;n.lengthBeats=1;t.notes.push_back(n);if(changed)changed();if(openPiano)openPiano();}
        else if(selectedClip<0)engine.seek(snapped(xToSeconds((float)e.x)));
        repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if(rangeDragging){updateRange(snapped(xToSeconds(e.position.x)),rowAt(e.y));repaint();return;}
        if(rangeMoving)
        {
            moveDelta=juce::jmax(0.0,snapped(rangeStart+xToSeconds(e.position.x)-moveFrom))-rangeStart;
            const int rows=(int)visibleTracks().size();moveRows=juce::jlimit(-rangeFirstRow,juce::jmax(0,rows-1-rangeLastRow),rowAt(e.y)-moveFromRow);
            setMouseCursor(e.mods.isAltDown()?juce::MouseCursor::CopyingCursor:juce::MouseCursor::DraggingHandCursor);repaint();return;
        }
        if(volumeDrag>=0&&volumeDrag<(int)engine.tracks.size()){setVolumeFrom(*engine.tracks[(size_t)volumeDrag],e.x);if(changed)changed();repaint();return;}
        if(rulerDragging)
        {
            // The transport follows the pointer precisely; editing snap does not quantise scrubbing.
            engine.seek(xToSeconds(e.position.x));repaint();return;
        }
        if(locatorDrag)
        {
            const double at=snapped(xToSeconds((float)e.x)),minimum=60.0/engine.tempo.load()/4;
            if(locatorDrag==1)engine.loopStart=juce::jlimit(0.0,juce::jmax(0.0,engine.loopEnd.load()-minimum),at);
            else engine.loopEnd=juce::jmax(engine.loopStart.load()+minimum,at);
            if(changed)changed();repaint();return;
        }
        if(dragClip<0||e.mods.isPopupMenu())return; const juce::ScopedLock lock(engine.modelLock);
        if(selectedTrack>=(int)engine.tracks.size())return;auto& clips=engine.tracks[(size_t)selectedTrack]->clips;if(dragClip>=(int)clips.size())return;
        auto& c=clips[(size_t)dragClip];const double delta=(e.x-dragStart.x)/pixelsPerSecond;
        if(fadeDrag)
        {   // Event handles: fade-in and fade-out length, event volume (−∞ … +6 dB).
            if(fadeDrag==1){c.fadeIn=juce::jlimit(0.0,juce::jmax(0.0,c.durationSeconds-c.fadeOut),dragOrigin.fadeIn+delta);if(c.fadeIn<=0.0)c.crossfade=false;}
            else if(fadeDrag==2)c.fadeOut=juce::jlimit(0.0,juce::jmax(0.0,c.durationSeconds-c.fadeIn),dragOrigin.fadeOut-delta);
            else{const float db=juce::jlimit(-60.0f,6.0f,juce::Decibels::gainToDecibels(dragOrigin.gain,-60.0f)-(e.position.y-dragStart.y)*0.2f);c.gain=db<=-60.0f?0.0f:juce::Decibels::decibelsToGain(db);}
            if(changed)changed();repaint();return;
        }
        if(resizing) { const double available=c.audio ? (double)c.audio->getNumSamples()/c.sourceRate-c.offsetSeconds : c.durationSeconds; c.durationSeconds=juce::jlimit(.02,juce::jmax(.02,available),snapped(dragOrigin.durationSeconds+delta)); }
        else c.startSeconds=juce::jmax(0.0,snapped(dragOrigin.startSeconds+delta,selectedTrack,dragClip));
        if(changed)changed();repaint();
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if(rangeMoving){rangeMoving=false;setTool(tool);if((std::abs(moveDelta)>1e-9||moveRows!=0)&&moveRange)moveRange(moveDelta,moveRows,e.mods.isAltDown());repaint();}
        dragClip=-1;locatorDrag=0;rulerDragging=false;volumeDrag=-1;fadeDrag=0;rangeDragging=false;if(playTool){playTool=false;engine.pause();}
    }
    int fadeDrag=0; // 1 fade-in handle, 2 fade-out handle, 3 volume handle
    // Event handles (manual p. 397): fade triangles at the top corners, volume at the bottom left.
    juce::Rectangle<float> fadeInHandle(const AudioClip& c,juce::Rectangle<float> r)const{return {juce::jlimit(r.getX(),juce::jmax(r.getX(),r.getRight()-10.0f),r.getX()+(float)(c.fadeIn*pixelsPerSecond)-5.0f),r.getY()+23.0f,10.0f,8.0f};}
    juce::Rectangle<float> fadeOutHandle(const AudioClip& c,juce::Rectangle<float> r)const{return {juce::jlimit(r.getX(),juce::jmax(r.getX(),r.getRight()-10.0f),r.getRight()-(float)(c.fadeOut*pixelsPerSecond)-5.0f),r.getY()+23.0f,10.0f,8.0f};}
    juce::Rectangle<float> volumeHandle(juce::Rectangle<float> r)const{return {r.getX()+4.0f,r.getBottom()-14.0f,10.0f,10.0f};}
    // Cubase draws the take being recorded as a red event that grows with its waveform.
    void drawRecording(juce::Graphics& g,const Track& t,int y)
    {
        if(!engine.recording.load()||!t.armed.load()||!t.hasEvents())return;
        const double start=engine.recordLapStart.load();if(start<0.0)return;
        const double rate=engine.currentSampleRate();const auto offset=engine.recordLapOffset.load();
        const double length=(double)(engine.recordCaptured.load()-offset)/rate;
        const juce::Rectangle<float> r(secondsToX(start),(float)y+8,juce::jmax(2.0f,(float)(length*pixelsPerSecond)),lane-16.0f);
        g.setColour(coral.withAlpha(.55f));g.fillRoundedRectangle(r,2);g.setColour(coral);g.drawRoundedRectangle(r.reduced(.5f),2,1.2f);
        text(g,i18n::tr("Grabando…"),r.withHeight(22).reduced(8,0).toNearestInt(),11,eventInk);
        const float* minMax=nullptr;int count=0,samplesPerPeak=1;
        if(!engine.recordingOverview(t,minMax,count,samplesPerPeak)||!minMax||count<=0||samplesPerPeak<=0)return;
        const auto w=r.withTrimmedTop(24).reduced(0,5);const float half=w.getHeight()*.5f,scale=w.getHeight()*.48f*waveZoom;
        const double peaksPerPixel=rate/samplesPerPeak/pixelsPerSecond;const int first=(int)(offset/samplesPerPeak);
        g.setColour(eventInk.withAlpha(.85f));
        const auto dirty=g.getClipBounds();
        for(int x=juce::jmax(header,(int)w.getX(),dirty.getX()-1);x<juce::jmin(getWidth(),(int)w.getRight(),dirty.getRight()+1);++x)
        {
            // The first pixel is rounded down from a fractional event position.
            // It may precede the first peak; never index before the overview.
            const int i0=juce::jmax(0,first+(int)((x-w.getX())*peaksPerPixel)),i1=juce::jmax(i0+1,first+(int)((x+1-w.getX())*peaksPerPixel));if(i0>=count)break;
            float lo=0,hi=0;for(int i=i0;i<juce::jmin(i1,count);++i){lo=juce::jmin(lo,minMax[2*i]);hi=juce::jmax(hi,minMax[2*i+1]);}
            g.drawVerticalLine(x,w.getCentreY()-juce::jmin(half,hi*scale),w.getCentreY()+juce::jmin(half,-lo*scale)+1);
        }
    }
    void drawFades(juce::Graphics& g,const AudioClip& c,juce::Rectangle<float> r,bool selected)
    {
        const auto area=r.withTrimmedTop(22.0f);
        for(int side=0;side<2;++side)
        {
            const double length=side==0?c.fadeIn:c.fadeOut;if(length<=0.0005||(side==0&&c.crossfade))continue;
            const float w=juce::jmin(area.getWidth(),(float)(length*pixelsPerSecond));if(w<2.0f)continue;
            const float x0=side==0?area.getX():area.getRight()-w;juce::Path curve,shade;shade.startNewSubPath(x0,area.getY());
            for(int k=0;k<=(int)w;k+=2)
            {
                const double t=(double)k/w;const float gain=audio::fadeGain(side==0?c.fadeInShape:c.fadeOutShape,side==0?t:1.0-t);
                const juce::Point<float> p(x0+(float)k,area.getBottom()-gain*area.getHeight());
                if(k==0)curve.startNewSubPath(p);else curve.lineTo(p);shade.lineTo(p);
            }
            shade.lineTo(x0+w,area.getY());shade.closeSubPath();
            g.setColour(juce::Colours::black.withAlpha(.28f));g.fillPath(shade);g.setColour(ink.withAlpha(.9f));g.strokePath(curve,juce::PathStrokeType(1.2f));
        }
        if(!selected)return;
        g.setColour(ink);
        for(auto h:{fadeInHandle(c,r),fadeOutHandle(c,r)}){juce::Path t;t.addTriangle(h.getX(),h.getY(),h.getRight(),h.getY(),h.getCentreX(),h.getBottom());g.fillPath(t);}
        const auto v=volumeHandle(r);g.setColour(ink.withAlpha(.9f));g.fillRect(v);g.setColour(line);g.drawRect(v);
        const float db=juce::Decibels::gainToDecibels(c.gain,-60.0f);
        text(g,c.gain<=0.0f?juce::String::fromUTF8("-∞ dB"):juce::String(db,1)+" dB",juce::Rectangle<float>(v.getRight()+4,v.getY()-3,60,16).toNearestInt(),9.5f,eventInk);
    }
    bool playTool=false; // Herramienta reproducir: plays while the button is held
    // Timer repaint while nothing else changed: the cursor strips, the header meters and, when
    // recording, the lanes (the red event grows). Everything else repaints only when it changes.
    int lastCursorX=-100;bool metersActive=false;
    void liveRepaint()
    {
        const int x=(int)secondsToX(engine.positionSeconds.load());
        if(x!=lastCursorX){repaint(lastCursorX-6,0,13,getHeight());repaint(x-6,0,13,getHeight());lastCursorX=x;}
        bool active=false;for(const auto& t:engine.tracks)active|=t->peakL.load()>1e-6f||t->peakR.load()>1e-6f;
        if(active||metersActive)repaint(header-8,ruler,6,getHeight()-ruler);
        metersActive=active;
        if(engine.recording.load())repaint(header,ruler,getWidth()-header,getHeight()-ruler);
    }
    void zoomAt(float x,double factor)
    {   // Herramienta zoom: the clicked time stays under the pointer.
        const double at=xToSeconds(x);zoom(factor);scrollSeconds=juce::jmax(0.0,at-(x-header)/pixelsPerSecond);repaint();
    }
    // Pegar (4) and Fundido cruzado (X) work on a copy of the track's events, so the new audio
    // file of a consolidated event is written without holding the model lock.
    void editEvents(const std::function<juce::Result(std::vector<AudioClip>&,int)>& operation)
    {
        if(selectedTrack<0||selectedTrack>=(int)engine.tracks.size()){if(warn)warn(i18n::tr("Selecciona un evento."));return;}
        auto track=engine.tracks[(size_t)selectedTrack];auto clips=track->clips;
        if(selectedClip<0||selectedClip>=(int)clips.size()){if(warn)warn(i18n::tr("Selecciona un evento."));return;}
        if(auto r=operation(clips,selectedClip);r.failed()){if(warn)warn(r.getErrorMessage());return;}
        for(auto& c:clips)if(c.file==juce::File()&&c.audio)if(auto r=audio::writeEventFile(c,engine.audioFolder());r.failed()){if(warn)warn(r.getErrorMessage());return;}
        if(beforeEdit)beforeEdit();
        {const juce::ScopedLock lock(engine.modelLock);track->clips=std::move(clips);}
        selectedClip=juce::jmin(selectedClip,(int)track->clips.size()-1);if(changed)changed();repaint();
    }
    void glueSelected(){editEvents([](std::vector<AudioClip>& c,int i){return audio::glueEvents(c,i);});}
    void crossfadeSelected(){editEvents([](std::vector<AudioClip>& c,int i){return audio::crossfadeEvents(c,i);});}
    int volumeDrag=-1;
    void setVolumeFrom(Track& t,int x){const auto v=volumeRect(0);const float db=volumeFromProportion((float)(x-v.getX())/(float)v.getWidth());t.gain=db<=-60.0f?0.0f:juce::Decibels::decibelsToGain(db);}
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        const juce::ScopedLock lock(engine.modelLock);const int i=trackAt(e.y);if(i<0||i>=(int)engine.tracks.size())return;
        if(tool==2&&e.x>=header)
        {   // Double click: the range covers the event (or the gap between two events).
            const auto& clips=engine.tracks[(size_t)i]->clips;const double at=xToSeconds((float)e.x);double from=0.0,to=engine.duration();
            for(int c=(int)clips.size()-1;c>=0;--c){const auto& k=clips[(size_t)c];if(at>=k.startSeconds&&at<k.startSeconds+k.durationSeconds){from=k.startSeconds;to=k.startSeconds+k.durationSeconds;break;}
                if(k.startSeconds+k.durationSeconds<=at)from=juce::jmax(from,k.startSeconds+k.durationSeconds);if(k.startSeconds>at)to=juce::jmin(to,k.startSeconds);}
            setRange(from,to,rowAt(e.y),rowAt(e.y));return;
        }
        if(engine.tracks[(size_t)i]->isMidi&&openPiano)openPiano();
    }
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w) override
    {
        if(e.mods.isCommandDown()||e.mods.isCtrlDown())zoom(w.deltaY>0?1.15:1/1.15);
        else if(e.mods.isShiftDown()||std::abs(w.deltaX)>.01f)scrollSeconds=juce::jmax(0.0,scrollSeconds-(w.deltaX!=0?w.deltaX:w.deltaY)*8);
        else { const juce::ScopedLock lock(engine.modelLock);const int visible=juce::jmax(1,(getHeight()-ruler)/lane);firstTrack=juce::jlimit(0,juce::jmax(0,(int)visibleTracks().size()-visible),firstTrack+(w.deltaY>0?-1:1)); }repaint();
    }
    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
    void fileDragEnter(const juce::StringArray&,int,int) override {dragOver=true;repaint();}
    void fileDragExit(const juce::StringArray&) override {dragOver=false;repaint();}
    void filesDropped(const juce::StringArray& files,int x,int y) override {dragOver=false;int i=trackAt(y);if(i<0||i>=(int)engine.tracks.size())i=-1;if(import)import(files,i,snapped(xToSeconds((float)x)));repaint();}
    void deleteSelected()
    {
        const juce::ScopedLock lock(engine.modelLock);if(selectedTrack<0||selectedTrack>=(int)engine.tracks.size())return;auto& clips=engine.tracks[(size_t)selectedTrack]->clips;
        if(selectedClip>=0&&selectedClip<(int)clips.size()){if(beforeEdit)beforeEdit();clips.erase(clips.begin()+selectedClip);selectedClip=-1;if(changed)changed();repaint();}
    }
    void duplicateSelected()
    {
        const juce::ScopedLock lock(engine.modelLock);if(selectedTrack<0||selectedTrack>=(int)engine.tracks.size())return;auto& clips=engine.tracks[(size_t)selectedTrack]->clips;
        if(selectedClip>=0&&selectedClip<(int)clips.size()){if(beforeEdit)beforeEdit();auto c=clips[(size_t)selectedClip];c.id=juce::Uuid().toString();c.startSeconds+=c.durationSeconds;clips.push_back(c);selectedClip=(int)clips.size()-1;if(changed)changed();repaint();}
    }
    void splitSelected()
    {
        const juce::ScopedLock lock(engine.modelLock);if(selectedTrack<0||selectedTrack>=(int)engine.tracks.size())return;auto& clips=engine.tracks[(size_t)selectedTrack]->clips;
        if(selectedClip>=0&&selectedClip<(int)clips.size()){auto c=clips[(size_t)selectedClip];double at=engine.positionSeconds-c.startSeconds;if(at<=.01||at>=c.durationSeconds-.01)return;if(beforeEdit)beforeEdit();clips[(size_t)selectedClip].durationSeconds=at;c.id=juce::Uuid().toString();c.startSeconds+=at;c.offsetSeconds+=at;c.durationSeconds-=at;clips.push_back(c);if(changed)changed();repaint();}
    }
private:
    static juce::Colour selectedTrackColour() { return silente::ui::theme::selectedTrack; }
    int locatorDrag=0; bool rulerDragging=false;
    int dragClip=-1; bool resizing=false,dragOver=false;juce::Point<float> dragStart;AudioClip dragOrigin;
    juce::Rectangle<float> clipRect(int i,const AudioClip& c)const {return {secondsToX(c.startSeconds),(float)(ruler+visualRow(i)*lane+8),juce::jmax(4.0f,(float)(c.durationSeconds*pixelsPerSecond)),lane-16.0f};}
    // Track list buttons as in Cubase: grey when off, filled with their colour and dark text when on.
    static constexpr int strip = 40;
    static juce::Rectangle<int> volumeRect(int y) { return { header-140, y+8, 74, 14 }; }
    static float volumeProportion(float db) { return db <= -60.0f ? 0.0f : juce::jlimit(0.0f,1.0f,std::pow((db+60.0f)/66.0f,1.6f)); }
    static float volumeFromProportion(float p) { return p <= 0.001f ? -100.0f : std::pow(juce::jlimit(0.0f,1.0f,p),1.0f/1.6f)*66.0f-60.0f; }
    // Track type icons drawn in the colour strip (audio waveform, keys, MIDI, folder, FX, group).
    static void trackIcon(juce::Graphics& g,TrackKind kind,juce::Rectangle<float> r,juce::Colour c)
    {
        g.setColour(c);juce::Path p;const float x=r.getX(),y=r.getY(),w=r.getWidth(),h=r.getHeight();
        switch(kind)
        {
            case TrackKind::audio: p.startNewSubPath(x,y+h*.5f);for(int k=1;k<=8;++k)p.lineTo(x+w*k/8.0f,y+h*(k%2?(k%4==1?.1f:.25f):(k%4==2?.9f:.75f)));g.strokePath(p,juce::PathStrokeType(1.4f));break;
            case TrackKind::instrument: case TrackKind::midi: for(int k=0;k<3;++k)g.fillRect(x+k*w/3.0f+.5f,y+h*.15f,w/3.0f-1.5f,h*.75f);if(kind==TrackKind::midi){g.setColour(c.withAlpha(.0f));}break;
            case TrackKind::folder: p.startNewSubPath(x,y+h*.2f);p.lineTo(x+w*.4f,y+h*.2f);p.lineTo(x+w*.5f,y+h*.35f);p.lineTo(x+w,y+h*.35f);p.lineTo(x+w,y+h*.9f);p.lineTo(x,y+h*.9f);p.closeSubPath();g.fillPath(p);break;
            case TrackKind::fx: g.setFont(juce::Font(juce::FontOptions(h*.75f,juce::Font::bold)));g.drawText("fx",r,juce::Justification::centred);break;
            case TrackKind::group: for(int k=0;k<3;++k)g.drawLine(x,y+h*(.2f+k*.3f),x+w*.55f,y+h*.5f,1.3f);g.drawLine(x+w*.55f,y+h*.5f,x+w,y+h*.5f,1.6f);break;
        }
    }
    static void chip(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> r,bool on,juce::Colour colour) {g.setColour(on?colour:raised);g.fillRoundedRectangle(r.toFloat(),3);g.setColour(line);g.drawRoundedRectangle(r.toFloat().reduced(.5f),3,1);text(g,s,r,10,on?activeInk:ink,juce::Justification::centred);}
    void drawWave(juce::Graphics& g,const AudioClip& c,juce::Rectangle<float> r,juce::Colour colour)
    {
        if(!c.audio||c.audio->getNumSamples()<1)return;const auto dirty=g.getClipBounds();
        const int x0=juce::jmax(header,(int)r.getX(),dirty.getX()-2),x1=juce::jmin(getWidth(),(int)r.getRight(),dirty.getRight()+2); // only the strip being repainted
        const float* data=c.audio->getReadPointer(0);const int size=c.audio->getNumSamples();g.setColour(colour.withAlpha(.8f));
        for(int x=x0;x<x1;x+=2){double rel=(x-secondsToX(c.startSeconds))/pixelsPerSecond;const float env=c.gain*audio::fadeEnvelope(rel,c.durationSeconds,c.fadeIn,c.fadeInShape,c.fadeOut,c.fadeOutShape);const int a=juce::jlimit(0,size-1,(int)((c.offsetSeconds+rel)*c.sourceRate));const int b=juce::jlimit(a+1,size,(int)((c.offsetSeconds+rel+2/pixelsPerSecond)*c.sourceRate));float lo=0,hi=0;const int step=juce::jmax(1,(b-a)/32);for(int n=a;n<b;n+=step){lo=juce::jmin(lo,data[n]);hi=juce::jmax(hi,data[n]);}
            // Waveform zoom enlarges the drawing only; peaks beyond the event are clipped at its edges.
            const float half=r.getHeight()*.5f,scale=r.getHeight()*.48f*waveZoom*env;g.drawVerticalLine(x,r.getCentreY()-juce::jmin(half,hi*scale),r.getCentreY()+juce::jmin(half,-lo*scale)+1);}
    }
    void clipMenu() {juce::PopupMenu m;m.addItem(1,i18n::tr("Dividir en el cursor"),selectedClip>=0);m.addItem(2,i18n::tr("Duplicar"),selectedClip>=0);m.addItem(3,i18n::tr("Eliminar"),selectedClip>=0);
        m.addSeparator();m.addItem(4,i18n::tr("Pegar con el evento siguiente"),selectedClip>=0);m.addItem(5,i18n::tr("Fundido cruzado  X"),selectedClip>=0);m.addItem(6,i18n::tr("Enmudecer evento"),selectedClip>=0);m.addItem(7,i18n::tr("Color de evento…"),selectedClip>=0);
        m.addSeparator();m.addItem(8,i18n::tr("Localizadores a la selección  P"),selectedClip>=0);m.addItem(9,i18n::tr("Fijar localizador izquierdo en el cursor"));m.addItem(10,i18n::tr("Fijar localizador derecho en el cursor"));
        m.addSeparator();m.addItem(11,i18n::tr("Copiar  ⌘C"),selectedClip>=0);m.addItem(12,i18n::tr("Cortar  ⌘X"),selectedClip>=0);m.addItem(13,i18n::tr("Pegar  ⌘V"));
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe=juce::Component::SafePointer<Arrangement>(this)](int r){if(!safe)return;if(r==1)safe->splitSelected();if(r==2)safe->duplicateSelected();if(r==3)safe->deleteSelected();if(r==4)safe->glueSelected();if(r==5)safe->crossfadeSelected();if(r==7&&safe->eventColour)safe->eventColour();
            if(r>=8&&r<=13&&safe->contextAction){const char* ids[]{"locatorsToSelection","setLeft","setRight","copy","cut","paste"};safe->contextAction(ids[r-8]);}
            if(r==6&&safe->selectedTrack>=0&&safe->selectedTrack<(int)safe->engine.tracks.size()){auto& clips=safe->engine.tracks[(size_t)safe->selectedTrack]->clips;if(safe->selectedClip>=0&&safe->selectedClip<(int)clips.size()){if(safe->beforeEdit)safe->beforeEdit();clips[(size_t)safe->selectedClip].muted=!clips[(size_t)safe->selectedClip].muted;if(safe->changed)safe->changed();safe->repaint();}}});}
    void trackMenu(int i)
    {
        juce::PopupMenu m;m.addItem(1,i18n::tr("Renombrar pista…"));m.addItem(3,i18n::tr("Color de pista…"));m.addSeparator();m.addItem(2,i18n::tr("Eliminar pista"));m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe=juce::Component::SafePointer<Arrangement>(this),i](int r){if(!safe)return;if(r==3){if(safe->editColour)safe->editColour(i,safe->localAreaToGlobal(juce::Rectangle<int>{0,ruler+safe->visualRow(i)*safe->lane,25,safe->lane}));return;}if(r==2){if(safe->beforeEdit)safe->beforeEdit();safe->engine.removeTrack(i);safe->selectedTrack=juce::jmax(0,i-1);if(safe->selectTrack)safe->selectTrack(safe->selectedTrack);if(safe->changed)safe->changed();safe->repaint();}if(r==1){auto* alert=new juce::AlertWindow(i18n::tr("Nombre de la pista"),"",juce::MessageBoxIconType::NoIcon);alert->addTextEditor("name",safe->engine.tracks[(size_t)i]->name);alert->addButton(i18n::tr("Guardar"),1,juce::KeyPress(juce::KeyPress::returnKey));alert->addButton(i18n::tr("Cancelar"),0,juce::KeyPress(juce::KeyPress::escapeKey));alert->enterModalState(true,juce::ModalCallbackFunction::create([safe,alert,i](int result){if(safe&&result==1){const juce::ScopedLock lock(safe->engine.modelLock);if(i<(int)safe->engine.tracks.size()){if(safe->beforeEdit)safe->beforeEdit();safe->engine.tracks[(size_t)i]->name=alert->getTextEditorContents("name");if(safe->changed)safe->changed();}}}),true);}});
    }
};

class PianoRoll final : public juce::Component
{
public:
    explicit PianoRoll(AudioEngine& e):engine(e){setWantsKeyboardFocus(true);}
    AudioEngine& engine;int track=0,selected=-1,topPitch=79;double pixelsPerBeat=64,scrollBeats=0;bool snap=true;
    std::function<void()> beforeEdit,changed;
    void paint(juce::Graphics& g)override
    {
        g.fillAll(bg);if(track<0||track>=(int)engine.tracks.size()||!engine.tracks[(size_t)track]->isMidi){text(g,i18n::tr("Selecciona una pista de instrumento"),getLocalBounds().withTrimmedBottom(25),17,muted,juce::Justification::centred);text(g,i18n::tr("Añade notas con doble clic · arrastra para mover · borde derecho para duración"),getLocalBounds().removeFromBottom(45),12,muted,juce::Justification::centred);return;}
        for(int y=24,p=topPitch;y<getHeight();y+=row,--p){const bool black=p%12==1||p%12==3||p%12==6||p%12==8||p%12==10;g.setColour(black?bg:panel);g.fillRect(52,y,getWidth()-52,row);g.setColour(black?raised:pianoWhite);g.fillRect(0,y,50,row-1);if(p%12==0)text(g,juce::MidiMessage::getMidiNoteName(p,true,true,3),{5,y,43,row},9,black?ink:bg);g.setColour(line.withAlpha(.3f));g.drawHorizontalLine(y,52,(float)getWidth());}
        for(int b=(int)scrollBeats;b<scrollBeats+(getWidth()-52)/pixelsPerBeat+1;++b){int x=(int)beatX(b);g.setColour(b%4==0?line.brighter(.1f):line.withAlpha(.5f));g.drawVerticalLine(x,24,(float)getHeight());if(b%4==0)text(g,juce::String(b/4+1),{x+4,0,40,24},10,muted);if(pixelsPerBeat>40)for(int sub=1;sub<4;++sub){g.setColour(line.withAlpha(.18f));g.drawVerticalLine((int)(x+sub*pixelsPerBeat/4),24,(float)getHeight());}}
        auto& t=*engine.tracks[(size_t)track];for(int i=0;i<(int)t.notes.size();++i){auto r=noteRect(t.notes[(size_t)i]);g.setColour(i==selected?ink:t.colour.withAlpha(.75f));g.fillRoundedRectangle(r.reduced(1),2);if(r.getWidth()>38)text(g,juce::MidiMessage::getMidiNoteName(t.notes[(size_t)i].pitch,true,true,3),r.toNearestInt().reduced(4,0),9,bg);}
        float x=beatX(engine.positionSeconds*engine.tempo/60);if(x>=52){g.setColour(ink.withAlpha(.6f));g.drawVerticalLine((int)x,24,(float)getHeight());}
    }
    void mouseDown(const juce::MouseEvent& e)override
    {
        grabKeyboardFocus();selected=-1;const juce::ScopedLock lock(engine.modelLock);if(!valid())return;
        if(e.x<52){previewPitch=juce::jlimit(0,127,topPitch-(e.y-24)/row);engine.keyboardState.noteOn(1,previewPitch,.75f);return;}
        auto& notes=engine.tracks[(size_t)track]->notes;for(int i=(int)notes.size()-1;i>=0;--i)if(noteRect(notes[(size_t)i]).contains(e.position)){selected=i;original=notes[(size_t)i];dragStart=e.position;resize=e.x>noteRect(original).getRight()-8;if(beforeEdit)beforeEdit();if(e.mods.isPopupMenu())deleteSelected();break;}
        repaint();
    }
    void mouseDoubleClick(const juce::MouseEvent& e)override
    {
        const juce::ScopedLock lock(engine.modelLock);if(!valid()||e.x<52||e.y<24)return;if(beforeEdit)beforeEdit();MidiNote n;n.pitch=juce::jlimit(0,127,topPitch-(e.y-24)/row);n.startBeats=quantize(scrollBeats+(e.x-52)/pixelsPerBeat);n.lengthBeats=1;engine.tracks[(size_t)track]->notes.push_back(n);selected=(int)engine.tracks[(size_t)track]->notes.size()-1;if(changed)changed();repaint();
    }
    void mouseDrag(const juce::MouseEvent& e)override
    {
        const juce::ScopedLock lock(engine.modelLock);if(!valid()||selected<0)return;auto& notes=engine.tracks[(size_t)track]->notes;if(selected>=(int)notes.size())return;auto& n=notes[(size_t)selected];double delta=(e.x-dragStart.x)/pixelsPerBeat;
        if(resize)n.lengthBeats=juce::jmax(.0625,quantize(original.lengthBeats+delta));else{n.startBeats=juce::jmax(0.0,quantize(original.startBeats+delta));n.pitch=juce::jlimit(0,127,original.pitch-(int)std::round((e.y-dragStart.y)/(float)row));}if(changed)changed();repaint();
    }
    void mouseUp(const juce::MouseEvent&)override{if(previewPitch>=0){engine.keyboardState.noteOff(1,previewPitch,0);previewPitch=-1;}}
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w)override{if(e.mods.isCommandDown())pixelsPerBeat=juce::jlimit(20.0,240.0,pixelsPerBeat*(w.deltaY>0?1.15:1/1.15));else if(e.mods.isShiftDown()||std::abs(w.deltaX)>.01)scrollBeats=juce::jmax(0.0,scrollBeats-(w.deltaX!=0?w.deltaX:w.deltaY)*8);else topPitch=juce::jlimit(24,127,topPitch+(w.deltaY>0?2:-2));repaint();}
    void deleteSelected(){const juce::ScopedLock lock(engine.modelLock);if(!valid())return;auto& n=engine.tracks[(size_t)track]->notes;if(selected>=0&&selected<(int)n.size()){if(beforeEdit)beforeEdit();n.erase(n.begin()+selected);selected=-1;if(changed)changed();repaint();}}
    bool keyPressed(const juce::KeyPress& k)override{if(k==juce::KeyPress::deleteKey||k==juce::KeyPress::backspaceKey){deleteSelected();return true;}return false;}
private:
    static constexpr int row=12;MidiNote original;juce::Point<float> dragStart;bool resize=false;int previewPitch=-1;
    bool valid()const{return track>=0&&track<(int)engine.tracks.size()&&engine.tracks[(size_t)track]->isMidi;}
    double quantize(double b)const{return snap?std::round(b*4)/4:b;}
    float beatX(double b)const{return (float)(52+(b-scrollBeats)*pixelsPerBeat);}
    juce::Rectangle<float> noteRect(const MidiNote& n)const{return {beatX(n.startBeats),(float)(24+(topPitch-n.pitch)*row),(float)(n.lengthBeats*pixelsPerBeat),(float)row};}
};

class MixerStrip final : public juce::Component
{
public:
    MixerStrip(AudioEngine& e,PluginManager& p,int i):engine(e),plugins(p),index(i)
    {
        addAndMakeVisible(fader);fader.setSliderStyle(juce::Slider::LinearVertical);fader.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);fader.setRange(-60,6,.1);fader.setDoubleClickReturnValue(true,0);fader.setTooltip(i18n::tr("Nivel del canal. Doble clic: 0 dB."));
        fader.onValueChange=[this]{auto value=juce::Decibels::decibelsToGain((float)fader.getValue(),-60.0f);if(auto t=get())t->gain=value;else if(index<0)engine.masterGain=value;if(changed)changed();repaint();};
        addAndMakeVisible(pan);pan.setSliderStyle(juce::Slider::LinearHorizontal);pan.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);pan.setRange(-1,1,.01);pan.setDoubleClickReturnValue(true,0);pan.setTooltip(i18n::tr("Panorama. Doble clic: centro."));pan.onValueChange=[this]{if(auto t=get())t->pan=(float)pan.getValue();if(changed)changed();};
        for(auto* b:{&mute,&solo,&insert})addAndMakeVisible(*b);
        for(int k=0;k<2;++k){auto& slot=effectSlots[(size_t)k];addAndMakeVisible(slot);slot.onClick=[this,k]{plugins.showEffectEditor(index,k,&effectSlots[(size_t)k]);};slot.onOptions=[this,k]{plugins.showEffectSlot(index,k,&effectSlots[(size_t)k]);};}mute.setButtonText("M");solo.setButtonText("S");insert.setButtonText(i18n::tr("+ Insertar"));mute.setClickingTogglesState(true);solo.setClickingTogglesState(true);
        mute.onClick=[this]{if(auto t=get())t->mute=mute.getToggleState();if(changed)changed();};solo.onClick=[this]{if(auto t=get())t->solo=solo.getToggleState();if(changed)changed();};insert.onClick=[this]{plugins.showEffectSelector(index,-1,&insert);};if(index<0){for(auto& slot:effectSlots)slot.setVisible(false);mute.setVisible(false);solo.setVisible(false);insert.setVisible(false);pan.setVisible(false);}refresh();
    }
    std::function<void()> changed;std::function<void(int)> select;
    void refresh()
    {
        if(auto t=get())
        {
            
            if(!fader.isMouseButtonDown())fader.setValue(juce::Decibels::gainToDecibels(t->gain.load(),-60.0f),juce::dontSendNotification);
            if(!pan.isMouseButtonDown())pan.setValue(t->pan,juce::dontSendNotification);
            mute.setToggleState(t->mute,juce::dontSendNotification);solo.setToggleState(t->solo,juce::dontSendNotification);
            for(int k=0;k<2;++k){const int slot=effectSlotIndex(*t,k);effectSlots[(size_t)k].setButtonText(slot>=0?t->plugins[(size_t)slot]->description.name:juce::String(k+1)+"  +");effectSlots[(size_t)k].setAlpha(slot>=0&&t->plugins[(size_t)slot]->bypassed?.5f:1.0f);}
        }
        else if(index<0&&!fader.isMouseButtonDown())fader.setValue(juce::Decibels::gainToDecibels(engine.masterGain.load(),-60.0f),juce::dontSendNotification);
        repaint();
    }
    void paint(juce::Graphics& g)override
    {
        g.fillAll(index<0?masterPanel:panel);auto t=get();auto c=t?t->colour:accent;g.setColour(c);g.fillRect(0,0,getWidth(),3);g.setColour(line);g.drawVerticalLine(getWidth()-1,0,(float)getHeight());text(g,t?t->name:i18n::tr("STEREO OUT"),{11,10,getWidth()-22,22},12);
        text(g,index<0?i18n::tr("MASTER"):t?kindLabel(t->kind):juce::String(),{11,31,getWidth()-22,16},9,muted);
        
        float level=t?t->gain.load():engine.masterGain.load();text(g,gainLabel(level),{10,getHeight()-28,getWidth()-20,23},11,ink,juce::Justification::centred);
        auto m=juce::Rectangle<float>((float)getWidth()-31,120,5,(float)getHeight()-157);meter(g,m,t?t->peakL.load():engine.masterPeakL.load());meter(g,m.translated(8,0),t?t->peakR.load():engine.masterPeakR.load());
        text(g,"0",{getWidth()-50,137,16,12},8,muted);text(g,juce::String::fromUTF8("−∞"),{getWidth()-51,getHeight()-50,20,12},8,muted);
    }
    void resized()override {for(int k=0;k<2;++k)effectSlots[(size_t)k].setBounds(10,52+k*25,getWidth()-20,22);insert.setBounds(10,104,getWidth()-20,24);mute.setBounds(10,132,26,23);solo.setBounds(40,132,26,23);pan.setBounds(9,159,63,20);fader.setBounds(15,179,52,juce::jmax(30,getHeight()-209));}
    void mouseDown(const juce::MouseEvent& e)override {if(select&&index>=0)select(index);}
private:
    AudioEngine& engine;PluginManager& plugins;int index;juce::Slider fader,pan;juce::TextButton mute,solo,insert;std::array<ui::PluginSlotButton,2>effectSlots;
    std::shared_ptr<Track> get(){return index>=0&&index<(int)engine.tracks.size()?engine.tracks[(size_t)index]:nullptr;}
};

class Mixer final : public juce::Component
{
public:
    Mixer(AudioEngine& e,PluginManager& p):engine(e),plugins(p){addAndMakeVisible(view);view.setViewedComponent(&content,false);view.setScrollBarsShown(false,true);view.setScrollBarThickness(8);}
    void refresh(){std::vector<int> channels;for(int i=0;i<(int)engine.tracks.size();++i)if(engine.tracks[(size_t)i]->kind!=TrackKind::folder)channels.push_back(i);channels.push_back(-1);if(channels!=shown){strips.clear();shown=channels;count=(int)engine.tracks.size();for(int i:channels){auto strip=std::make_unique<MixerStrip>(engine,plugins,i);strip->changed=[this]{if(changed)changed();};strip->select=[this](int i){if(select)select(i);};content.addAndMakeVisible(*strip);strips.push_back(std::move(strip));}resized();}for(auto& s:strips)s->refresh();}
    void resized()override{view.setBounds(getLocalBounds());content.setSize(juce::jmax(getWidth()-8,(count+1)*126),juce::jmax(250,getHeight()-10));int x=0;for(auto& s:strips){s->setBounds(x,0,126,content.getHeight());x+=126;}}
    std::function<void()> changed;std::function<void(int)> select;
private:AudioEngine& engine;PluginManager& plugins;juce::Viewport view;juce::Component content;std::vector<std::unique_ptr<MixerStrip>>strips;std::vector<int> shown;int count=-1;
};

class UtilityWindow final : public juce::DocumentWindow
{
public:
    UtilityWindow(const juce::String& title,juce::Component* content,int width,int height)
        :juce::DocumentWindow(title,panel,juce::DocumentWindow::closeButton)
    {setUsingNativeTitleBar(true);setContentOwned(content,true);setResizable(true,true);setResizeLimits(440,200,2000,1400);centreWithSize(width,height);setVisible(true);}
    void closeButtonPressed()override{setVisible(false);}
};

class VisibilityPanel final : public juce::Component
{
public:
    explicit VisibilityPanel(Arrangement& a):arrangement(a){}
    void paint(juce::Graphics& g)override{g.fillAll(panel);const juce::ScopedLock lock(arrangement.engine.modelLock);text(g,i18n::tr("Mostrar pistas en el proyecto"),{12,8,getWidth()-24,24},11,muted);for(int i=0;i<(int)arrangement.engine.tracks.size();++i){auto& t=*arrangement.engine.tracks[(size_t)i];int y=40+i*30;bool visible=!arrangement.hiddenTracks.count(t.id);g.setColour(visible?t.colour:line);g.fillRoundedRectangle(13.0f,y+5.0f,16,16,3);if(visible)text(g,juce::String::fromUTF8("✓"),{13,y+4,16,17},12,bg,juce::Justification::centred);text(g,t.name,{38,y,getWidth()-48,26},12,visible?ink:muted);}}
    void mouseDown(const juce::MouseEvent& e)override{int i=(e.y-40)/30;const juce::ScopedLock lock(arrangement.engine.modelLock);if(e.y<40||i<0||i>=(int)arrangement.engine.tracks.size())return;auto id=arrangement.engine.tracks[(size_t)i]->id;if(e.mods.isShiftDown())arrangement.filter.showOnly(arrangement.engine.tracks,*arrangement.engine.tracks[(size_t)i]);else if(arrangement.hiddenTracks.count(id))arrangement.hiddenTracks.erase(id);else arrangement.hiddenTracks.insert(id);arrangement.firstTrack=0;arrangement.repaint();repaint();}
private:Arrangement& arrangement;
};

class RightRack final : public juce::Component
{
public:
    RightRack(AudioEngine& e,PluginManager& p):engine(e),plugins(p)
    {
        for(auto* b:{&vsti,&media,&meters,&scan,&add,&import,&example,&reset})addAndMakeVisible(b);
        vsti.onClick=[this]{setTab(0);};media.onClick=[this]{setTab(1);};meters.onClick=[this]{setTab(2);};
        for(auto* b:{&vsti,&media,&meters})b->setClickingTogglesState(true);
        scan.onClick=[this]{if(plugins.isScanning())plugins.stopScan();else plugins.scanPlugins();};add.onClick=[this]{if(addPlugin)addPlugin();};import.onClick=[this]{if(importFiles)importFiles();};example.onClick=[this]{if(loadDemo)loadDemo();};reset.onClick=[this]{heldL=heldR=0;engine.clippedSamples=0;repaint();};setTab(0);
    }
    std::function<void()> addPlugin,importFiles,loadDemo;std::function<void(int)> select;
    void setTab(int i){tab=i;vsti.setToggleState(i==0,juce::dontSendNotification);media.setToggleState(i==1,juce::dontSendNotification);meters.setToggleState(i==2,juce::dontSendNotification);scan.setVisible(i==0);add.setVisible(i==0);import.setVisible(i==1);example.setVisible(i==1);reset.setVisible(i==2);repaint();}
    void tick()
    {   // Only the meter tab and the scan progress change by themselves.
        heldL=juce::jmax(heldL,engine.masterPeakL.load());heldR=juce::jmax(heldR,engine.masterPeakR.load());const bool scanning=plugins.isScanning();
        scan.setButtonText(scanning?i18n::tr("Detener búsqueda"):i18n::tr("Buscar plugins…"));
        if(isShowing()&&((tab==2&&(engine.playing.load()||engine.masterPeakL.load()>1e-6f||engine.masterPeakR.load()>1e-6f))||scanning||scanning!=wasScanning))repaint();
        wasScanning=scanning;
    }
    bool wasScanning=false;
    void resized()override{int third=(getWidth()-12)/3;vsti.setBounds(4,3,third,25);media.setBounds(6+third,3,third,25);meters.setBounds(8+2*third,3,third,25);scan.setBounds(11,getHeight()-78,getWidth()-22,28);add.setBounds(11,getHeight()-43,getWidth()-22,28);import.setBounds(11,75,getWidth()-22,30);example.setBounds(11,113,getWidth()-22,30);reset.setBounds(17,getHeight()-50,getWidth()-34,28);}
    void paint(juce::Graphics& g)override
    {
        g.fillAll(panel);g.setColour(line);g.drawHorizontalLine(32,0,(float)getWidth());
        if(tab==0){text(g,i18n::tr("RACK DE INSTRUMENTOS"),{12,43,getWidth()-24,23},10,muted);int y=78;bool any=false;for(int i=0;i<(int)engine.tracks.size();++i){auto& t=*engine.tracks[(size_t)i];if(!t.isMidi)continue;any=true;g.setColour(raised);g.fillRoundedRectangle(10.0f,(float)y,getWidth()-20.0f,60,4);g.setColour(t.colour);g.fillRect(10,y,3,60);text(g,t.name,{20,y+5,getWidth()-40,22},12);juce::String name=i18n::tr("Sintetizador interno");for(auto& slot:t.plugins)if(slot->description.isInstrument){name=slot->description.name;break;}text(g,name,{20,y+29,getWidth()-50,21},10,t.colour);text(g,"e",{getWidth()-31,y+31,15,18},12,ink,juce::Justification::centred);y+=68;if(y>getHeight()-175)break;}if(!any){font(g,12);g.setColour(muted);g.drawFittedText(i18n::tr("Añade una pista de instrumento para tocar y editar MIDI."),{14,91,getWidth()-28,78},juce::Justification::topLeft,4);}font(g,10);g.setColour(muted);g.drawFittedText(plugins.status(),{13,getHeight()-144,getWidth()-26,54},juce::Justification::topLeft,3);}
        else if(tab==1){text(g,i18n::tr("MEDIOS DEL PROYECTO"),{12,43,getWidth()-24,23},10,muted);int y=165;std::set<juce::String> seen;for(auto& t:engine.tracks)for(auto& clip:t->clips)if(seen.insert(clip.file.getFullPathName()).second){g.setColour(line);g.drawHorizontalLine(y+44,12,(float)getWidth()-12);text(g,clip.name,{12,y,getWidth()-24,23},11);text(g,juce::String(clip.durationSeconds,2)+juce::String::fromUTF8(" s · ")+juce::String(clip.sourceRate/1000,1)+" kHz",{12,y+22,getWidth()-24,18},9,muted);y+=51;if(y>getHeight()-60)break;}if(seen.empty())text(g,i18n::tr("Arrastra audio al proyecto"),{12,174,getWidth()-24,25},11,muted);}
        else{const int height=juce::jmax(100,getHeight()-230);int x=getWidth()/2-37;auto l=engine.masterPeakL.load(),r=engine.masterPeakR.load();text(g,i18n::tr("STEREO OUT"),{10,47,getWidth()-20,25},12,ink,juce::Justification::centred);meter(g,{(float)x,94,22,(float)height},l);meter(g,{(float)x+39,94,22,(float)height},r);for(int db=0;db>=-60;db-=6){int y=94+(int)(height*(-db/60.0));text(g,juce::String(db),{x-28,y-6,22,12},9,muted,juce::Justification::centredRight);g.setColour(line);g.drawHorizontalLine(y,(float)x+24,(float)x+37);}text(g,"L",{x,95+height,22,20},10,muted,juce::Justification::centred);text(g,"R",{x+39,95+height,22,20},10,muted,juce::Justification::centred);text(g,i18n::tr("Pico ")+gainLabel(juce::jmax(heldL,heldR)),{12,height+123,getWidth()-24,21},11,juce::jmax(heldL,heldR)>1?coral:mint,juce::Justification::centred);text(g,i18n::tr("Clips: ")+juce::String(engine.clippedSamples.load()),{12,height+147,getWidth()-24,20},10,muted,juce::Justification::centred);}
    }
    void mouseDown(const juce::MouseEvent& e)override{if(tab!=0)return;const juce::ScopedLock lock(engine.modelLock);int row=(e.y-78)/68;if(e.y<78)return;for(int i=0;i<(int)engine.tracks.size();++i){auto& t=*engine.tracks[(size_t)i];if(!t.isMidi)continue;if(row--==0){if(select)select(i);for(int k=0;k<(int)t.plugins.size();++k)if(t.plugins[(size_t)k]->description.isInstrument){plugins.showEditor(i,k);return;}plugins.showInstrumentSelector(i,this);return;}}}
private:AudioEngine& engine;PluginManager& plugins;int tab=0;float heldL=0,heldR=0;juce::TextButton vsti{i18n::tr("VSTi")},media{i18n::tr("Media")},meters{i18n::tr("Meter")},scan{i18n::tr("Buscar plugins…")},add{i18n::tr("+ Instrumento / efecto")},import{i18n::tr("Importar archivo…")},example{i18n::tr("Sesión de ejemplo")},reset{i18n::tr("Restablecer picos")};
};

class ChannelPanel final : public juce::Component
{
public:
    ChannelPanel(AudioEngine& e,PluginManager& p,int i):engine(e),plugins(p),index(i)
    {
        for(auto* s:{&gain,&pan,&low,&mid,&high}){addAndMakeVisible(s);s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s->setTextBoxStyle(juce::Slider::TextBoxBelow,false,78,22);s->setDoubleClickReturnValue(true,0);}
        gain.setRange(-60,6,.1);pan.setRange(-100,100,1);for(auto* s:{&low,&mid,&high}){s->setRange(-18,18,.1);s->setTextValueSuffix(" dB");}gain.setTextValueSuffix(" dB");
        for(int k=0;k<6;++k){auto& b=inserts[(size_t)k];addAndMakeVisible(b);b.onClick=[this,k]{plugins.showEffectEditor(index,k,&inserts[(size_t)k]);};b.onOptions=[this,k]{plugins.showEffectSlot(index,k,&inserts[(size_t)k]);};auto& bypass=bypasses[(size_t)k];addAndMakeVisible(bypass);bypass.setButtonText(juce::String::fromUTF8("Ø"));bypass.setTooltip(i18n::tr("Bypass del inserto"));bypass.setClickingTogglesState(true);bypass.onClick=[this,k]{auto t=get();if(t){const int slot=effectSlotIndex(*t,k);if(slot>=0)plugins.toggleBypass(index,slot);}};}
        gain.onValueChange=[this]{if(auto t=get())t->gain=juce::Decibels::decibelsToGain((float)gain.getValue(),-60.0f);if(changed)changed();};pan.onValueChange=[this]{if(auto t=get())t->pan=(float)pan.getValue()/100;if(changed)changed();};low.onValueChange=[this]{if(auto t=get())t->lowGainDb=(float)low.getValue();if(changed)changed();};mid.onValueChange=[this]{if(auto t=get())t->midGainDb=(float)mid.getValue();if(changed)changed();};high.onValueChange=[this]{if(auto t=get())t->highGainDb=(float)high.getValue();if(changed)changed();};refresh();
    }
    std::function<void()> changed;
    void refresh(){auto t=get();if(!t)return;if(!gain.isMouseButtonDown())gain.setValue(juce::Decibels::gainToDecibels(t->gain.load(),-60.0f),juce::dontSendNotification);if(!pan.isMouseButtonDown())pan.setValue(t->pan*100,juce::dontSendNotification);if(!low.isMouseButtonDown())low.setValue(t->lowGainDb,juce::dontSendNotification);if(!mid.isMouseButtonDown())mid.setValue(t->midGainDb,juce::dontSendNotification);if(!high.isMouseButtonDown())high.setValue(t->highGainDb,juce::dontSendNotification);const juce::ScopedLock lock(engine.modelLock);for(int k=0;k<6;++k){const int slot=effectSlotIndex(*t,k);bool exists=slot>=0;inserts[(size_t)k].setButtonText(exists?t->plugins[(size_t)slot]->description.name:i18n::tr("+ Añadir inserto"));bypasses[(size_t)k].setEnabled(exists);bypasses[(size_t)k].setToggleState(exists&&t->plugins[(size_t)slot]->bypassed,juce::dontSendNotification);}repaint();}
    void resized()override{gain.setBounds(22,110,90,122);pan.setBounds(122,110,90,122);low.setBounds(250,110,90,122);mid.setBounds(350,110,90,122);high.setBounds(450,110,90,122);for(int k=0;k<6;++k){inserts[(size_t)k].setBounds(580,62+k*36,getWidth()-634,29);bypasses[(size_t)k].setBounds(getWidth()-47,62+k*36,30,29);}}
    void paint(juce::Graphics& g)override{g.fillAll(panel);auto t=get();text(g,t?t->name:i18n::tr("Canal eliminado"),{20,12,getWidth()-40,30},19);text(g,i18n::tr("Stereo In  →  Canal  →  Stereo Out"),{20,47,450,22},11,muted);text(g,i18n::tr("NIVEL"),{22,86,90,20},10,muted,juce::Justification::centred);text(g,i18n::tr("PAN"),{122,86,90,20},10,muted,juce::Justification::centred);text(g,i18n::tr("GRAVES"),{250,86,90,20},10,muted,juce::Justification::centred);text(g,i18n::tr("MEDIOS"),{350,86,90,20},10,muted,juce::Justification::centred);text(g,i18n::tr("AGUDOS"),{450,86,90,20},10,muted,juce::Justification::centred);text(g,i18n::tr("INSERTOS"),{580,31,180,24},10,muted);if(t){meter(g,{27,260,220,7},0);g.setColour(mint);g.fillRect(27,260,(int)(220*juce::jlimit(0.0f,1.0f,t->peakL.load())),7);}text(g,i18n::tr("EQ de tres bandas · insertos en serie · salida estéreo"),{22,292,520,25},11,muted);}
private:AudioEngine& engine;PluginManager& plugins;int index;juce::Slider gain,pan,low,mid,high;std::array<ui::PluginSlotButton,6>inserts;std::array<juce::TextButton,6>bypasses;std::shared_ptr<Track>get(){const juce::ScopedLock lock(engine.modelLock);return index>=0&&index<(int)engine.tracks.size()?engine.tracks[(size_t)index]:nullptr;}
};

}

struct MainComponent::Impl : private juce::ScrollBar::Listener
{
    MainComponent& owner;
    AudioEngine& engine;
    PluginManager& plugins;
    SilenteLookAndFeel look;
    Arrangement arrangement;
    PianoRoll piano;
    Mixer mixer;
    VisibilityPanel visibility;
    RightRack rightRack;
    juce::TextButton play { juce::String::fromUTF8("▶") }, stop { juce::String::fromUTF8("■") }, record { juce::String::fromUTF8("●") }, rewind { juce::String::fromUTF8("|‹") }, loop { i18n::tr("Ciclo") }, click { i18n::tr("Click") };
    juce::TextButton importButton { i18n::tr("Importar") }, exportButton, settings { i18n::tr("Audio") }, addTrackButton { "+" };
    juce::TextButton mixerTab { i18n::tr("Mezclador") }, pianoTab { i18n::tr("Piano roll") }, undoButton { juce::String::fromUTF8("↶") }, redoButton { juce::String::fromUTF8("↷") }, splitButton { i18n::tr("Dividir") }, duplicateButton { i18n::tr("Duplicar") }, snapButton { i18n::tr("Rejilla 1/16") }, zoomOut { juce::String::fromUTF8("−") }, zoomIn { "+" };
    juce::TextButton mute { i18n::tr("Mute") }, solo { i18n::tr("Solo") }, arm { i18n::tr("Grabar") }, monitor { i18n::tr("Monitor") }, scan { i18n::tr("Buscar plugins") }, browser { i18n::tr("Instrumentos y efectos") }, demo { i18n::tr("Abrir sesión de ejemplo") };
    juce::TextButton selectTool{i18n::tr("Selección")},cutTool{i18n::tr("Dividir")},eraseTool{i18n::tr("Borrador")},drawTool{i18n::tr("Dibujar")},trackColour{i18n::tr("Color de pista")};
    juce::TextButton glueTool,zoomTool,muteTool,playTool,rangeTool;
    juce::TextButton inspectorTab{i18n::tr("Inspector")},visibilityTab{i18n::tr("Visibilidad")},channelButton{"e"};
    juce::ScrollBar timelineScroll{false};
    juce::TextButton languageButton{juce::String::fromUTF8("Español")};
    std::array<ui::PluginSlotButton,4> inserts;
    ui::PluginSlotButton instrument;
    juce::TextButton inputRouting;
    juce::Slider tempo, gain, pan, low, mid, high, master;
    juce::Label projectTitle, transportTime, status;
    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<UtilityWindow> channelWindow, settingsWindow, tapWindow;
    std::unique_ptr<ui::MixConsoleWindow> mixConsoleWindow;
    std::unique_ptr<UtilityWindow> exportWindow, conversionWindow, historyWindow;
    void showHistory()
    {
        if(!historyWindow)
        {
            auto* panel=new ui::EditHistoryPanel();
            panel->entries=[this]
            {
                std::vector<ui::EditHistoryPanel::Entry> list;
                for(const auto& s:undoStack)list.push_back({s.action,s.time,true});
                for(auto it=redoStack.rbegin();it!=redoStack.rend();++it)list.push_back({it->action,it->time,false});
                return list;
            };
            panel->goTo=[this](int done){while((int)undoStack.size()>done&&!undoStack.empty())undo(false);while((int)undoStack.size()<done&&!redoStack.empty())undo(true);refresh();};
            historyWindow=std::make_unique<UtilityWindow>(i18n::tr("Silente · Historial de ediciones"),panel,520,460);historyWindow->setLookAndFeel(&look);
        }
        historyWindow->setVisible(true);historyWindow->toFront(true);
    }
    void showKeyCommands()
    {
        showSettingsPage(ui::SettingsWorkspace::Page::keys);
    }
    // Project window zones and lines (Cubase manual, «Ventana de proyecto», pp. 55–77).
    std::unique_ptr<ui::ChannelTab> channelTab;
    std::unique_ptr<ui::MixConsole> lowerConsole; // «MixConsole en la zona inferior» (manual p. 89)
    juce::TextButton snapTypeButton,gridButton;
    juce::TextButton punchInButton,punchOutButton;
    // Waveform zoom (top) and track height (bottom) at the right edge of the event display, and
    // the draggable borders of the zones; sizes are kept in layout.xml.
    juce::Slider waveZoomSlider{juce::Slider::LinearVertical,juce::Slider::NoTextBox},trackHeightSlider{juce::Slider::LinearVertical,juce::Slider::NoTextBox};
    ui::ZoneSplitter channelSplit{true},inspectorSplit{true},rightSplit{true},lowerSplit{false};
    int channelWidth=132,inspectorWidth=196,rightWidth=0,lowerHeight=0,dragBase=0;
    juce::TextButton allMuteOff{"M"},allSoloOff{"S"},hubButton,mixConsoleButton,zoneChannelButton,zoneLeftButton,zoneLowerButton,zoneRightButton,layoutButton;
    juce::TextButton trackTabButton{i18n::tr("Pista")},editorTabButton{i18n::tr("Editor")};
    bool showChannel=true,showLeft=true,showLower=true,showRight=true,showStatusLine=true,showInfoLine=true,showTransport=true,editorInspector=false;
    juce::Rectangle<int> channelRect,statusRect,infoRect,transportRect;
    std::vector<std::pair<juce::Rectangle<int>,int>> statusFields;
    juce::int64 freeBytes=0;double freeBytesTime=-10.0;
    static juce::File layoutFile(){return MainComponent::settingsFolder().getChildFile("layout.xml");}
    void loadLayout()
    {
        auto xml=juce::XmlDocument::parse(layoutFile());if(!xml||!xml->hasTagName("PROJECTWINDOW"))return;
        showChannel=xml->getBoolAttribute("channel",showChannel);showLeft=xml->getBoolAttribute("left",showLeft);showLower=xml->getBoolAttribute("lower",showLower);showRight=xml->getBoolAttribute("right",showRight);
        showStatusLine=xml->getBoolAttribute("statusLine",showStatusLine);showInfoLine=xml->getBoolAttribute("infoLine",showInfoLine);showTransport=xml->getBoolAttribute("transport",showTransport);
        channelWidth=juce::jlimit(110,320,xml->getIntAttribute("channelWidth",channelWidth));inspectorWidth=juce::jlimit(170,420,xml->getIntAttribute("inspectorWidth",inspectorWidth));
        rightWidth=xml->getIntAttribute("rightWidth",0);lowerHeight=xml->getIntAttribute("lowerHeight",0);
        arrangement.lane=juce::jlimit(Arrangement::minLane,Arrangement::maxLane,xml->getIntAttribute("trackHeight",arrangement.lane));
        arrangement.waveZoom=(float)juce::jlimit(1.0,16.0,xml->getDoubleAttribute("waveZoom",1.0));
    }
    void setTrackHeight(int value)
    {   // Keeps the top visible track in place while the rows grow or shrink.
        arrangement.lane=juce::jlimit(Arrangement::minLane,Arrangement::maxLane,value);trackHeightSlider.setValue(arrangement.lane,juce::dontSendNotification);arrangement.repaint();
    }
    void saveLayout()
    {
        juce::XmlElement xml("PROJECTWINDOW");xml.setAttribute("channel",showChannel);xml.setAttribute("left",showLeft);xml.setAttribute("lower",showLower);xml.setAttribute("right",showRight);
        xml.setAttribute("statusLine",showStatusLine);xml.setAttribute("infoLine",showInfoLine);xml.setAttribute("transport",showTransport);
        xml.setAttribute("channelWidth",channelWidth);xml.setAttribute("inspectorWidth",inspectorWidth);xml.setAttribute("rightWidth",rightWidth);xml.setAttribute("lowerHeight",lowerHeight);
        xml.setAttribute("trackHeight",arrangement.lane);xml.setAttribute("waveZoom",(double)arrangement.waveZoom);
        layoutFile().getParentDirectory().createDirectory();xml.writeTo(layoutFile());
    }
    void toggleZone(bool& flag){flag=!flag;saveLayout();owner.resized();owner.repaint();}
    bool inspectorShown()const{return showLeft&&!visibilityVisible&&!editorInspector;}
    void statusClick(juce::Point<int> p)
    {
        for(auto& [area,action]:statusFields)if(area.contains(p)&&action==1){owner.showAudioSettings();return;}
        for(auto& [area,action]:statusFields)if(area.contains(p)&&action==2){recordFormatMenu().showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(owner.localAreaToGlobal(area)),[this](int r){if(r)transportMenuItem(r);});return;}
        if(inspectorShown())for(size_t n=0;n<insp.sections.size();++n)if(insp.sections[n].contains(p)){sectionOpen[n]=!sectionOpen[n];owner.resized();owner.repaint();return;}
    }
    juce::ShapeButton clickOptions{"metronome-options",muted,ink,accent};juce::TextButton tapButton{"TAP"};
    std::function<void()> afterSave;
    bool visibilityVisible=false;
    bool dirty = false, pianoVisible = false, syncing = false, closing = false;
    int selected = 0;
    juce::String notice { i18n::tr("Listo. Pulsa Espacio para escuchar la sesión.") };
    juce::StringArray recentFiles;
    juce::Rectangle<int> inspectorRect, browserRect, lowerRect, centreRect;

    struct TrackState
    {
        std::shared_ptr<Track> track;
        juce::String name; juce::Colour colour;
        std::vector<AudioClip> clips; std::vector<MidiNote> notes;
        float gain, pan, low, mid, high; bool mute, solo, armed, monitor;int inputLeft,inputRight;
        juce::String output; struct Send { juce::String target; float level; bool pre, on; }; std::array<Send,Track::sendCount> sends;
    };
    struct Snapshot { std::vector<TrackState> tracks; double tempo, loopStart, loopEnd; float master; bool looping; juce::String action; juce::Time time; };
    std::deque<Snapshot> undoStack, redoStack;

    Impl(MainComponent& o, AudioEngine& e, PluginManager& p)
        : owner(o), engine(e), plugins(p), arrangement(e), piano(e), mixer(e,p), visibility(arrangement),rightRack(e,p),
          keyboard(e.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard), tooltips(&o,700)
    {
        owner.setLookAndFeel(&look); owner.setOpaque(true); owner.setWantsKeyboardFocus(true);
        for(auto* c : std::initializer_list<juce::Component*> { &arrangement,&piano,&mixer,&play,&stop,&record,&rewind,&loop,&click,&importButton,&exportButton,&settings,&addTrackButton,&mixerTab,&pianoTab,&undoButton,&redoButton,&splitButton,&duplicateButton,&snapButton,&zoomOut,&zoomIn,&mute,&solo,&arm,&monitor,&scan,&browser,&demo,&tempo,&gain,&pan,&low,&mid,&high,&master,&projectTitle,&transportTime,&status,&keyboard }) owner.addAndMakeVisible(c);
        for(auto* c:std::initializer_list<juce::Component*>{&visibility,&rightRack,&selectTool,&cutTool,&eraseTool,&drawTool,&glueTool,&zoomTool,&muteTool,&playTool,&rangeTool,&trackColour,&inspectorTab,&visibilityTab,&channelButton,&timelineScroll})owner.addAndMakeVisible(c);
        visibility.setVisible(false);inspectorTab.setToggleState(true,juce::dontSendNotification);
        inspectorTab.onClick=[this]{showVisibility(false);};visibilityTab.onClick=[this]{showVisibility(true);};
        for(auto* b:{&inspectorTab,&visibilityTab,&selectTool,&cutTool,&eraseTool,&drawTool,&glueTool,&zoomTool,&muteTool,&playTool,&rangeTool})b->setClickingTogglesState(true);
        auto configure=[this](juce::TextButton& button,ToolIcon icon,int tool,const juce::String& tooltip){setButtonIcon(button,icon);button.setTooltip(tooltip);button.onClick=[this,tool]{setTool(tool);};};
        configure(selectTool,ToolIcon::select,1,i18n::tr("Selección de objetos · 1"));configure(cutTool,ToolIcon::scissors,3,i18n::tr("Dividir eventos · 3"));configure(eraseTool,ToolIcon::erase,5,i18n::tr("Borrar eventos · 5"));configure(drawTool,ToolIcon::pencil,8,i18n::tr("Dibujar notas MIDI · 8"));
        configure(glueTool,ToolIcon::glue,4,i18n::tr("Pegar: une el evento con el siguiente · 4"));configure(zoomTool,ToolIcon::zoomTool,6,i18n::tr("Zoom: clic acerca, ⌥clic aleja · 6"));
        configure(muteTool,ToolIcon::muteTool,7,i18n::tr("Enmudecer eventos · 7"));configure(playTool,ToolIcon::playTool,9,i18n::tr("Reproducir desde el clic mientras se mantiene · 9"));
        configure(rangeTool,ToolIcon::rangeTool,2,i18n::tr("Selección de rango · 2"));rangeTool.setComponentID("tool-range");glueTool.setComponentID("tool-glue");zoomTool.setComponentID("tool-zoom");muteTool.setComponentID("tool-mute");playTool.setComponentID("tool-play");setTool(1);
        for(auto [button,icon]:std::initializer_list<std::pair<juce::TextButton*,ToolIcon>>{{&rewind,ToolIcon::rewind},{&stop,ToolIcon::stop},{&play,ToolIcon::play},{&record,ToolIcon::record},{&loop,ToolIcon::cycle},{&click,ToolIcon::click},{&undoButton,ToolIcon::undo},{&redoButton,ToolIcon::redo},{&trackColour,ToolIcon::palette}})setButtonIcon(*button,icon);
        trackColour.setTooltip(i18n::tr("Seleccionar color para pistas o eventos seleccionados"));trackColour.onClick=[this]{if(currentTrack())editTrackColour(selected,trackColour.getScreenBounds());else error(i18n::tr("Selecciona una pista para elegir su color."));};
        channelButton.setTooltip(i18n::tr("Editar configuración de canal"));channelButton.onClick=[this]{if(currentTrack())openChannelSettings(selected);};
        arrangement.editColour=[this](int i,juce::Rectangle<int> bounds){editTrackColour(i,bounds);};
        rightRack.select=[this](int i){select(i);};rightRack.addPlugin=[this]{plugins.showInstrumentSelector();};rightRack.importFiles=[this]{owner.importFiles();};rightRack.loadDemo=[this]{demo.triggerClick();};
        timelineScroll.addListener(this);
        owner.addAndMakeVisible(clickOptions);owner.addAndMakeVisible(tapButton);
        loadLayout();loadTransport();registerKeyActions();
        for(auto* c:std::initializer_list<juce::Component*>{&waveZoomSlider,&trackHeightSlider,&channelSplit,&inspectorSplit,&rightSplit,&lowerSplit})owner.addAndMakeVisible(c);
        waveZoomSlider.setRange(1.0,16.0,0.01);waveZoomSlider.setSkewFactorFromMidPoint(4.0);waveZoomSlider.setValue(arrangement.waveZoom,juce::dontSendNotification);
        waveZoomSlider.setTooltip(i18n::tr("Zoom de forma de onda"));waveZoomSlider.setComponentID("wave-zoom");waveZoomSlider.setDoubleClickReturnValue(true,1.0);
        waveZoomSlider.onValueChange=[this]{arrangement.waveZoom=(float)waveZoomSlider.getValue();arrangement.repaint();};waveZoomSlider.onDragEnd=[this]{saveLayout();};
        trackHeightSlider.setRange(Arrangement::minLane,Arrangement::maxLane,1.0);trackHeightSlider.setValue(arrangement.lane,juce::dontSendNotification);
        trackHeightSlider.setTooltip(i18n::tr("Altura de pistas  ⇧H / ⇧G"));trackHeightSlider.setComponentID("track-height");trackHeightSlider.setDoubleClickReturnValue(true,60.0);
        trackHeightSlider.onValueChange=[this]{setTrackHeight((int)trackHeightSlider.getValue());};trackHeightSlider.onDragEnd=[this]{saveLayout();};
        const auto splitter=[this](ui::ZoneSplitter& z,int& size,int sign,std::function<int()> current,int lo,std::function<int()> hi)
        {
            z.onStart=[this,current]{dragBase=current();};
            z.onDrag=[this,&size,sign,lo,hi](int offset){size=juce::jlimit(lo,juce::jmax(lo,hi()),dragBase+sign*offset);owner.resized();};
            z.onEnd=[this]{saveLayout();};
        };
        splitter(channelSplit,channelWidth,1,[this]{return channelRect.getWidth();},110,[this]{return juce::jmin(320,owner.getWidth()-inspectorRect.getWidth()-browserRect.getWidth()-400);});
        splitter(inspectorSplit,inspectorWidth,1,[this]{return inspectorRect.getWidth();},170,[this]{return juce::jmin(420,owner.getWidth()-channelRect.getWidth()-browserRect.getWidth()-400);});
        splitter(rightSplit,rightWidth,-1,[this]{return browserRect.getWidth();},160,[this]{return juce::jmin(480,owner.getWidth()-channelRect.getWidth()-inspectorRect.getWidth()-400);});
        splitter(lowerSplit,lowerHeight,-1,[this]{return lowerRect.getHeight();},150,[this]{return juce::jmax(150,channelRect.getHeight()-140);});
        channelSplit.setComponentID("split-channel");inspectorSplit.setComponentID("split-left");rightSplit.setComponentID("split-right");lowerSplit.setComponentID("split-lower");
        channelTab=std::make_unique<ui::ChannelTab>(engine,plugins);owner.addAndMakeVisible(*channelTab);
        lowerConsole=std::make_unique<ui::MixConsole>(engine,plugins,consoleHooks(),true);owner.addAndMakeVisible(*lowerConsole);
        channelTab->beforeEdit=[this]{pushUndo(i18n::tr("Cambio en el canal"));};channelTab->openSettings=[this](int i){openChannelSettings(i);};channelTab->changed=[this]{markDirty();};
        for(auto* b:{&punchInButton,&punchOutButton,&hubButton,&allMuteOff,&allSoloOff,&mixConsoleButton,&zoneChannelButton,&zoneLeftButton,&zoneLowerButton,&zoneRightButton,&layoutButton,&trackTabButton,&editorTabButton})owner.addAndMakeVisible(b);
        allMuteOff.setTooltip(i18n::tr("Desactivar todos los estados de enmudecido"));allSoloOff.setTooltip(i18n::tr("Desactivar todos los estados de solo"));
        allMuteOff.setColour(juce::TextButton::buttonOnColourId,muteColour);allSoloOff.setColour(juce::TextButton::buttonOnColourId,soloColour);
        allMuteOff.onClick=[this]{pushUndo(i18n::tr("Desactivar enmudecidos"));for(auto& t:engine.tracks)t->mute=false;markDirty();refresh();};
        allSoloOff.onClick=[this]{pushUndo(i18n::tr("Desactivar solos"));for(auto& t:engine.tracks)t->solo=false;markDirty();refresh();};
        setButtonIcon(punchInButton,ToolIcon::punchIn);punchInButton.setTooltip(i18n::tr("Activar punch in · I"));punchInButton.setComponentID("punch-in");punchInButton.onClick=[this]{keyActions["punchIn"]();};
        setButtonIcon(punchOutButton,ToolIcon::punchOut);punchOutButton.setTooltip(i18n::tr("Activar punch out · O"));punchOutButton.setComponentID("punch-out");punchOutButton.onClick=[this]{keyActions["punchOut"]();};
        setButtonIcon(exportButton,ToolIcon::exportAudio);exportButton.setTooltip(i18n::tr("Exportar mezcla de audio"));
        setButtonIcon(hubButton,ToolIcon::hub);hubButton.setTooltip(i18n::tr("Hub: nuevo, abrir o guardar proyecto"));hubButton.setComponentID("hub");
        hubButton.onClick=[this]{juce::PopupMenu menu;menu.addItem(1,i18n::tr("Nuevo proyecto"));menu.addItem(2,i18n::tr("Abrir proyecto"));menu.addItem(3,i18n::tr("Guardar"));
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&hubButton),[this](int r){if(r==1)owner.newProject();else if(r==2)owner.openProject();else if(r==3)owner.saveProject();});};
        setButtonIcon(mixConsoleButton,ToolIcon::mixer);mixConsoleButton.setTooltip(i18n::tr("Abrir MixConsole · F3"));mixConsoleButton.onClick=[this]{showMixConsole();};
        zoneChannelButton.setComponentID("zone-channel");zoneLeftButton.setComponentID("zone-left");zoneLowerButton.setComponentID("zone-lower");zoneRightButton.setComponentID("zone-right");
        setButtonIcon(zoneChannelButton,ToolIcon::zoneChannel);zoneChannelButton.setTooltip(i18n::tr("Mostrar/Ocultar canal"));zoneChannelButton.onClick=[this]{toggleZone(showChannel);};
        setButtonIcon(zoneLeftButton,ToolIcon::zoneLeft);zoneLeftButton.setTooltip(i18n::tr("Mostrar/Ocultar zona izquierda"));zoneLeftButton.onClick=[this]{toggleZone(showLeft);};
        setButtonIcon(zoneLowerButton,ToolIcon::zoneLower);zoneLowerButton.setTooltip(i18n::tr("Mostrar/Ocultar zona inferior"));zoneLowerButton.onClick=[this]{toggleZone(showLower);};
        setButtonIcon(zoneRightButton,ToolIcon::zoneRight);zoneRightButton.setTooltip(i18n::tr("Mostrar/Ocultar zona derecha"));zoneRightButton.onClick=[this]{toggleZone(showRight);};
        setButtonIcon(layoutButton,ToolIcon::layout);layoutButton.setTooltip(i18n::tr("Configurar disposición de ventanas"));
        layoutButton.onClick=[this]
        {
            juce::PopupMenu menu;menu.addSectionHeader(i18n::tr("Configurar disposición de ventanas"));
            menu.addItem(1,i18n::tr("Canal"),true,showChannel);menu.addItem(2,i18n::tr("Zona izquierda"),true,showLeft);menu.addItem(3,i18n::tr("Zona inferior"),true,showLower);menu.addItem(4,i18n::tr("Zona derecha"),true,showRight);
            menu.addSeparator();menu.addItem(5,i18n::tr("Línea de estado"),true,showStatusLine);menu.addItem(6,i18n::tr("Línea de información"),true,showInfoLine);menu.addItem(7,i18n::tr("Barra de transporte"),true,showTransport);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&layoutButton),[safe=juce::Component::SafePointer<MainComponent>(&owner)](int result)
            {
                if(!safe||result==0)return;auto& self=*safe->ui;
                bool* flags[]{&self.showChannel,&self.showLeft,&self.showLower,&self.showRight,&self.showStatusLine,&self.showInfoLine,&self.showTransport};
                self.toggleZone(*flags[result-1]);
            });
        };
        trackTabButton.setTooltip(i18n::tr("Inspector de la pista seleccionada"));editorTabButton.setTooltip(i18n::tr("Inspector del editor de la zona inferior"));
        trackTabButton.onClick=[this]{editorInspector=false;owner.resized();owner.repaint();};editorTabButton.onClick=[this]{editorInspector=true;owner.resized();owner.repaint();};
        {juce::Path arrow;arrow.addTriangle(0,0,8,0,4,5);clickOptions.setShape(arrow,false,true,false);}
        clickOptions.setTooltip(i18n::tr("Ajustes del metrónomo"));clickOptions.onClick=[this]{showMetronomeSettings();};
        tapButton.setTooltip(i18n::tr("Calculadora de tempo: marca el pulso con Espacio"));tapButton.onClick=[this]{showTapTempo();};
        tempo.setIncDecButtonsMode(juce::Slider::incDecButtonsDraggable_Vertical);
        owner.addAndMakeVisible(languageButton);languageButton.setTooltip(i18n::tr("Idioma de la interfaz"));
        languageButton.onClick=[this]
        {
            juce::PopupMenu menu;menu.addItem(1,juce::String::fromUTF8("Español"),true,i18n::language()==i18n::Language::spanish);menu.addItem(2,"English",true,i18n::language()==i18n::Language::english);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&languageButton),[](int result){if(result==1)i18n::setLanguage(i18n::Language::spanish);if(result==2)i18n::setLanguage(i18n::Language::english);});
        };
        for(auto& b:inserts)owner.addAndMakeVisible(b);
        for(auto* b : { &play,&loop,&click,&mixerTab,&pianoTab,&snapButton,&mute,&solo,&arm,&monitor }) b->setClickingTogglesState(true);
        record.setColour(juce::TextButton::textColourOffId,coral); play.setColour(juce::TextButton::textColourOffId,mint); play.setColour(juce::TextButton::textColourOnId,activeInk);
        // Cubase state colours: cycle purple, play green, mute yellow, solo red, record red, monitor orange.
        play.setColour(juce::TextButton::buttonOnColourId,mint);loop.setColour(juce::TextButton::buttonOnColourId,cycleColour);
        mute.setColour(juce::TextButton::buttonOnColourId,muteColour);solo.setColour(juce::TextButton::buttonOnColourId,soloColour);
        mute.setButtonText("M");solo.setButtonText("S");arm.setButtonText(juce::String::fromUTF8("●"));monitor.setButtonText(juce::String::fromUTF8("◉"));
        mute.setTooltip(i18n::tr("Silenciar"));solo.setTooltip(i18n::tr("Solo"));arm.setTooltip(i18n::tr("Activar grabación"));monitor.setTooltip(i18n::tr("Monitorizar"));
        arm.setColour(juce::TextButton::buttonOnColourId,coral);monitor.setColour(juce::TextButton::buttonOnColourId,meterWarning);
        mixerTab.setToggleState(true,juce::dontSendNotification); snapButton.setToggleState(true,juce::dontSendNotification);
        piano.setVisible(false); keyboard.setVisible(false);
        projectTitle.setFont(juce::Font(juce::FontOptions(17,juce::Font::bold))); projectTitle.setEditable(false,true,false);projectTitle.setTooltip(i18n::tr("Doble clic para cambiar el nombre de la sesión."));
        projectTitle.onTextChange=[this]{if(syncing)return;engine.projectName=projectTitle.getText().trim();markDirty();};
        transportTime.setFont(juce::Font(juce::FontOptions(19)));transportTime.setJustificationType(juce::Justification::centred);
        status.setFont(juce::Font(juce::FontOptions(11))); status.setColour(juce::Label::textColourId,muted);
        tempo.setSliderStyle(juce::Slider::IncDecButtons);tempo.setTextBoxStyle(juce::Slider::TextBoxLeft,false,53,30);tempo.setRange(20,300,.1);tempo.setValue(engine.tempo,juce::dontSendNotification);tempo.setTooltip(i18n::tr("Tempo de la sesión, en BPM. El audio conserva su duración; MIDI sigue el tempo."));
        tempo.onValueChange=[this]{if(syncing)return;engine.tempo=tempo.getValue();markDirty();};
        setupSlider(gain,-60,6,.1,juce::Slider::LinearHorizontal," dB",0);
        setupSlider(pan,-100,100,1,juce::Slider::LinearHorizontal,"",0);
        setupSlider(low,-18,18,.1,juce::Slider::RotaryHorizontalVerticalDrag," dB",0);
        setupSlider(mid,-18,18,.1,juce::Slider::RotaryHorizontalVerticalDrag," dB",0);
        setupSlider(high,-18,18,.1,juce::Slider::RotaryHorizontalVerticalDrag," dB",0);
        setupSlider(master,-60,6,.1,juce::Slider::LinearHorizontal," dB",0);
        gain.onValueChange=[this]{if(!syncing){if(auto t=currentTrack())t->gain=juce::Decibels::decibelsToGain((float)gain.getValue(),-60.0f);markDirty();}};
        pan.onValueChange=[this]{if(!syncing){if(auto t=currentTrack())t->pan=(float)pan.getValue()/100;markDirty();}};
        low.onValueChange=[this]{if(!syncing){if(auto t=currentTrack())t->lowGainDb=(float)low.getValue();markDirty();}};
        mid.onValueChange=[this]{if(!syncing){if(auto t=currentTrack())t->midGainDb=(float)mid.getValue();markDirty();}};
        high.onValueChange=[this]{if(!syncing){if(auto t=currentTrack())t->highGainDb=(float)high.getValue();markDirty();}};
        master.onValueChange=[this]{if(!syncing){engine.masterGain=juce::Decibels::decibelsToGain((float)master.getValue(),-60.0f);markDirty();}};
        for(auto* s:{&gain,&pan,&low,&mid,&high,&master})s->onDragStart=[this]{pushUndo();};
        play.onClick=[this]{togglePlay();};stop.onClick=[this]{stopTransport();};rewind.onClick=[this]{engine.seek(0);arrangement.scrollSeconds=0;};
        record.onClick=[this]{toggleRecord();}; loop.onClick=[this]{engine.looping=loop.getToggleState();markDirty();};click.onClick=[this]{engine.metronome=click.getToggleState();markDirty();};
        importButton.onClick=[this]{owner.importFiles();};exportButton.onClick=[this]{owner.exportAudio();};settings.onClick=[this]{owner.showAudioSettings();};
        addTrackButton.setTooltip(i18n::tr("Añadir pista"));addTrackButton.setComponentID("add-track");
        addTrackButton.onClick=[this]
        {
            juce::PopupMenu menu;
            menu.addSectionHeader(i18n::tr("Añadir pista"));
            menu.addItem(1,i18n::tr("Audio"));menu.addItem(2,i18n::tr("Instrumento…"));menu.addItem(3,i18n::tr("MIDI"));
            menu.addSeparator();menu.addItem(4,i18n::tr("Carpeta"));menu.addItem(5,i18n::tr("FX (efectos)"));menu.addItem(6,i18n::tr("Grupo"));
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addTrackButton),[safe=juce::Component::SafePointer<MainComponent>(&owner)](int result)
            {
                if(!safe||result==0)return;auto& self=*safe->ui;
                if(result==2){self.plugins.showInstrumentSelector(-1,&self.addTrackButton);return;}
                static constexpr TrackKind kinds[]{TrackKind::audio,TrackKind::instrument,TrackKind::midi,TrackKind::folder,TrackKind::fx,TrackKind::group};
                safe->addTrack(kinds[result-1]);
            });
        };
        mixerTab.onClick=[this]{setPiano(false);};pianoTab.onClick=[this]{setPiano(true);};
        undoButton.onClick=[this]{undo(false);};redoButton.onClick=[this]{undo(true);};splitButton.onClick=[this]{arrangement.splitSelected();};duplicateButton.onClick=[this]{if(pianoVisible)duplicateNote();else arrangement.duplicateSelected();};
        // Ajustar act./desact., Tipo de ajuste and Tipo de rejilla (manual pp. 65, 108–112).
        setButtonIcon(snapButton,ToolIcon::snap);snapButton.setTooltip(i18n::tr("Ajustar act./desact."));
        snapButton.onClick=[this]{arrangement.snap=piano.snap=snapButton.getToggleState();};
        owner.addAndMakeVisible(snapTypeButton);owner.addAndMakeVisible(gridButton);
        snapTypeButton.setTooltip(i18n::tr("Tipo de ajuste"));gridButton.setTooltip(i18n::tr("Tipo de rejilla"));
        snapTypeButton.onClick=[this]
        {
            juce::PopupMenu menu;menu.addSectionHeader(i18n::tr("Tipo de ajuste"));
            for(int n=0;n<6;++n)menu.addItem(n+1,snapTypeName((ui::SnapType)n),true,(int)arrangement.snapGrid.type==n);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&snapTypeButton),[safe=juce::Component::SafePointer<MainComponent>(&owner)](int r){if(safe&&r>0){safe->ui->arrangement.snapGrid.type=(ui::SnapType)(r-1);safe->ui->refresh();}});
        };
        gridButton.onClick=[this]
        {
            juce::PopupMenu menu;menu.addSectionHeader(i18n::tr("Tipo de rejilla"));
            const bool musical=arrangement.snapGrid.format==ui::TimeFormat::barsBeats;
            for(int n=0;n<9;++n){const bool isMusical=n<=5;if(isMusical==musical)menu.addItem(n+1,gridName((ui::GridType)n),true,(int)arrangement.snapGrid.grid==n);}
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&gridButton),[safe=juce::Component::SafePointer<MainComponent>(&owner)](int r){if(safe&&r>0){safe->ui->arrangement.snapGrid.grid=(ui::GridType)(r-1);safe->ui->refresh();}});
        };
        arrangement.rulerMenu=[this]
        {
            juce::PopupMenu menu;menu.addSectionHeader(i18n::tr("Formato de tiempo de regla"));
            const juce::String names[]{i18n::tr("Compases+Tiempos"),i18n::tr("Segundos"),i18n::tr("Código de tiempo"),i18n::tr("Muestras")};
            for(int n=0;n<4;++n)menu.addItem(n+1,names[n],true,(int)arrangement.snapGrid.format==n);
            menu.showMenuAsync(juce::PopupMenu::Options(),[safe=juce::Component::SafePointer<MainComponent>(&owner)](int r)
            {
                if(!safe||r<=0)return;auto& g=safe->ui->arrangement.snapGrid;g.format=(ui::TimeFormat)(r-1);
                // The grid type list follows the time format (musical vs. time-based).
                const bool musical=g.format==ui::TimeFormat::barsBeats;if(musical!=((int)g.grid<=5))g.grid=musical?ui::GridType::sixteenth:ui::GridType::ms100;
                safe->ui->refresh();
            });
        };zoomOut.onClick=[this]{arrangement.zoom(1/1.25);};zoomIn.onClick=[this]{arrangement.zoom(1.25);};
        mute.onClick=[this]{pushUndo();if(auto t=currentTrack())t->mute=mute.getToggleState();markDirty();};solo.onClick=[this]{pushUndo();if(auto t=currentTrack())t->solo=solo.getToggleState();markDirty();};
        arm.onClick=[this]{pushUndo();if(auto t=currentTrack())t->armed=arm.getToggleState();markDirty();};monitor.onClick=[this]{pushUndo();if(auto t=currentTrack())t->monitor=monitor.getToggleState();markDirty();};
        scan.onClick=[this]{if(plugins.isScanning())plugins.stopScan();else plugins.scanPlugins();};browser.onClick=[this]{owner.showPlugins();};
        demo.onClick=[this]{confirmDiscard([this]{plugins.closeEditors();engine.createDemo();selected=0;arrangement.selectedClip=-1;arrangement.firstTrack=0;arrangement.scrollSeconds=0;undoStack.clear();redoStack.clear();dirty=true;notice=i18n::tr("Sesión de ejemplo: audio generado y notas MIDI editables.");refresh();});};
        for(int i=0;i<(int)inserts.size();++i)
        {
            inserts[(size_t)i].onClick=[this,i]{plugins.showEffectEditor(selected,i,&inserts[(size_t)i]);};
            inserts[(size_t)i].onOptions=[this,i]{plugins.showEffectSlot(selected,i,&inserts[(size_t)i]);};
        }
        owner.addAndMakeVisible(inputRouting);inputRouting.setComponentID("input-routing");
        inputRouting.onClick=[this]
        {
            auto track=currentTrack();if(!track||track->kind!=TrackKind::audio)return;const auto names=engine.devices.inputNames();juce::PopupMenu menu;
            for(int n=0;n<DeviceSettings::channelLimit;++n)menu.addItem(n+1,i18n::tr("Mono ")+juce::String(n+1)+(n<names.size()?" · "+names[n]:juce::String()),true,track->inputLeft==n&&track->inputRight==n);
            menu.addSeparator();for(int n=0;n<DeviceSettings::channelLimit;n+=2)menu.addItem(101+n,i18n::tr("Estéreo ")+juce::String(n+1)+"–"+juce::String(n+2),true,track->inputLeft==n&&track->inputRight==n+1);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&inputRouting),[safe=juce::Component::SafePointer<MainComponent>(&owner),track](int result)
            {
                if(!safe||result==0)return;auto& self=*safe->ui;const juce::ScopedLock lock(self.engine.modelLock);
                if(self.engine.recording||std::find(self.engine.tracks.begin(),self.engine.tracks.end(),track)==self.engine.tracks.end())return;
                self.pushUndo();track->inputLeft=result>=101?result-101:result-1;track->inputRight=result>=101?track->inputLeft+1:track->inputLeft;self.markDirty();self.refresh();
            });
        };
        owner.addAndMakeVisible(instrument);instrument.setComponentID("instrument-slot");
        instrument.onClick=[this]{plugins.showInstrument(selected,&instrument);};
        instrument.onOptions=[this]{plugins.showInstrumentSelector(selected,&instrument);};
        arrangement.selectTrack=[this](int i){select(i);};arrangement.beforeEdit=[this]{pushUndo(i18n::tr("Editar evento o pista"));};arrangement.changed=[this]{markDirty();};arrangement.openPiano=[this]{setPiano(true);};
        arrangement.chooseTool=[this](int value){setTool(value);};
        arrangement.contextAction=[this](const juce::String& id){owner.runCommand(id);};
        arrangement.paintEventColour=[this](const juce::String& trackId,const juce::String& clipId,juce::Rectangle<int> anchor){editEventColour(trackId,clipId,anchor);};
        arrangement.trackFilterMenu=[this]
        {
            juce::PopupMenu menu;menu.addSectionHeader(i18n::tr("Ajustar filtro de tipo de pista"));
            const std::pair<TrackKind,juce::String> kinds[]{{TrackKind::audio,i18n::tr("Audio")},{TrackKind::instrument,i18n::tr("Instrumento")},{TrackKind::midi,i18n::tr("MIDI")},{TrackKind::folder,i18n::tr("Carpeta")},{TrackKind::fx,i18n::tr("FX")},{TrackKind::group,i18n::tr("Grupo")}};
            for(const auto& [kind,label]:kinds)menu.addItem((int)kind+1,label,true,arrangement.filter.hiddenKinds.count((int)kind)==0);
            menu.addSeparator();menu.addItem(100,i18n::tr("Mostrar todos los tipos  (⌘ clic)"),arrangement.filter.filtering());
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(arrangement.localAreaToGlobal(Arrangement::filterButton())),[safe=juce::Component::SafePointer<MainComponent>(&owner)](int result)
            {
                if(!safe||result==0)return;auto& f=safe->ui->arrangement.filter;
                if(result==100)f.hiddenKinds.clear();else if(!f.hiddenKinds.erase(result-1))f.hiddenKinds.insert(result-1);
                safe->ui->arrangement.firstTrack=0;safe->ui->refresh();
            });
        };
        arrangement.findTracks=[this]
        {
            auto finder=std::make_unique<ui::TrackFinder>(engine,arrangement.filter);
            finder->onPick=[safe=juce::Component::SafePointer<MainComponent>(&owner)](int index)
            {
                if(!safe)return;auto& self=*safe->ui;if(!juce::isPositiveAndBelow(index,(int)self.engine.tracks.size()))return;
                self.arrangement.filter.reveal(*self.engine.tracks[(size_t)index]);self.select(index);
                const auto v=self.arrangement.visibleTracks();const auto it=std::find(v.begin(),v.end(),index);
                if(it!=v.end())self.arrangement.firstTrack=juce::jmax(0,(int)std::distance(v.begin(),it)-1);
                if(auto* box=safe->findParentComponentOfClass<juce::CallOutBox>())box->dismiss();
                juce::ModalComponentManager::getInstance()->cancelAllModalComponents();
                self.refresh();
            };
            juce::CallOutBox::launchAsynchronously(std::move(finder),arrangement.localAreaToGlobal(Arrangement::searchButton()),nullptr);
        };arrangement.openChannel=[this](int i){openChannelSettings(i);};arrangement.warn=[this](const juce::String& m){error(m);};arrangement.eventColour=[this]{colourMenu();};arrangement.moveRange=[this](double d,int rows,bool copy){moveRangeContent(d,rows,copy);};arrangement.rangeMenu=[this]{rangeMenu().showMenuAsync(juce::PopupMenu::Options(),[safe=juce::Component::SafePointer<MainComponent>(&owner)](int r){if(safe&&r)safe->ui->rangeMenuItem(r);});};arrangement.import=[this](const juce::StringArray& f,int i,double s){importPaths(f,i,s);};
        piano.beforeEdit=[this]{pushUndo();};piano.changed=[this]{markDirty();};mixer.changed=[this]{markDirty();};mixer.select=[this](int i){select(i);};
        plugins.onChange=[this]{markDirty();refresh();};
        plugins.onTrackCreated=[this](int index){select(index);};
        keyboard.setAvailableRange(24,108);keyboard.setLowestVisibleKey(48);keyboard.setKeyWidth(20);keyboard.setVelocity(.8f,true);
        keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId,keyboardWhite);keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId,bg);keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,accent.withAlpha(.7f));keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,accent.withAlpha(.3f));
        play.setTooltip(i18n::tr("Reproducir / pausar · Espacio"));stop.setTooltip(i18n::tr("Detener y volver al inicio"));record.setTooltip(i18n::tr("Grabar en las pistas armadas. El audio usa la entrada seleccionada en Audio."));
        rewind.setTooltip(i18n::tr("Volver al inicio"));loop.setTooltip(i18n::tr("Repetir entre los localizadores. Menú contextual de Ciclo para ajustar."));loop.onStateChange=[this]{if(loop.isMouseButtonDown()&&juce::ModifierKeys::getCurrentModifiers().isPopupMenu())showLoopDialog();};
        click.setTooltip(i18n::tr("Metrónomo · C"));undoButton.setTooltip(i18n::tr("Deshacer edición de pistas, clips o notas · ⌘Z"));redoButton.setTooltip(i18n::tr("Rehacer · ⌘⇧Z"));splitButton.setTooltip(i18n::tr("Dividir el clip de audio seleccionado en el cursor · S"));duplicateButton.setTooltip(i18n::tr("Duplicar clip o nota · ⌘D"));
        refresh();
    }
    ~Impl(){timelineScroll.removeListener(this);settingsWindow.reset();channelWindow.reset();plugins.onChange={};plugins.onTrackCreated={};owner.setLookAndFeel(nullptr);}

    void scrollBarMoved(juce::ScrollBar*,double start) override {arrangement.scrollSeconds=start;arrangement.repaint();}
    void setTool(int value)
    {
        arrangement.setTool(value);
        selectTool.setToggleState(value==1,juce::dontSendNotification);cutTool.setToggleState(value==3,juce::dontSendNotification);
        eraseTool.setToggleState(value==5,juce::dontSendNotification);drawTool.setToggleState(value==8,juce::dontSendNotification);
        glueTool.setToggleState(value==4,juce::dontSendNotification);zoomTool.setToggleState(value==6,juce::dontSendNotification);muteTool.setToggleState(value==7,juce::dontSendNotification);playTool.setToggleState(value==9,juce::dontSendNotification);rangeTool.setToggleState(value==2,juce::dontSendNotification);
        trackColour.setToggleState(value==13,juce::dontSendNotification);
    }
    void showVisibility(bool show)
    {
        visibilityVisible=show;inspectorTab.setToggleState(!show,juce::dontSendNotification);visibilityTab.setToggleState(show,juce::dontSendNotification);
        owner.resized();owner.repaint();
    }
    // «Seleccionar color para pistas o eventos seleccionados» (manual pp. 115–118): the selected
    // event when there is one, otherwise the selected track; plus the two reset options.
    void editEventColour(const juce::String& trackId,const juce::String& clipId,juce::Rectangle<int> anchor)
    {
        auto track=engine.findTrack(trackId);if(!track)return;
        const auto clip=std::find_if(track->clips.begin(),track->clips.end(),[&](const AudioClip& c){return c.id==clipId;});if(clip==track->clips.end())return;
        const auto initial=clip->colour.isTransparent()?track->colour:clip->colour;
        showTrackColourPicker(anchor,initial,[safe=juce::Component::SafePointer<MainComponent>(&owner),trackId,clipId](juce::Colour colour)
        {
            if(!safe)return;auto& self=*safe->ui;auto target=self.engine.findTrack(trackId);if(!target)return;
            const auto event=std::find_if(target->clips.begin(),target->clips.end(),[&](const AudioClip& c){return c.id==clipId;});if(event==target->clips.end())return;
            if(event->colour==colour)return;
            self.pushUndo(i18n::tr("Color de evento"));
            {const juce::ScopedLock lock(self.engine.modelLock);event->colour=colour;}
            self.markDirty();self.refresh();
        },i18n::tr("Color de evento"));
    }
    void colourMenu()
    {
        if(auto* clip=selectedClipPtr())
        {   // Event colour, from the event's context menu (the toolbar button colours the track).
            juce::PopupMenu menu;menu.addSectionHeader(i18n::tr("Colorear eventos seleccionados"));menu.addItem(1,i18n::tr("Elegir color de evento…"));menu.addItem(2,i18n::tr("Ajustar color de evento a pista"),!clip->colour.isTransparent());
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&trackColour),[safe=juce::Component::SafePointer<MainComponent>(&owner)](int r)
            {
                if(!safe||r==0)return;auto& self=*safe->ui;auto t=self.currentTrack();const int index=self.arrangement.selectedClip;
                if(!t||!juce::isPositiveAndBelow(index,(int)t->clips.size()))return;
                if(r==2){self.pushUndo(i18n::tr("Color de evento"));t->clips[(size_t)index].colour={};self.markDirty();self.refresh();return;}
                const auto initial=t->clips[(size_t)index].colour.isTransparent()?t->colour:t->clips[(size_t)index].colour;
                showTrackColourPicker(self.trackColour.getScreenBounds(),initial,[safe,t,index](juce::Colour colour)
                {
                    if(!safe||!juce::isPositiveAndBelow(index,(int)t->clips.size()))return;auto& me=*safe->ui;
                    me.pushUndo(i18n::tr("Color de evento"));{const juce::ScopedLock lock(me.engine.modelLock);t->clips[(size_t)index].colour=colour;}me.markDirty();me.refresh();
                });
            });
            return;
        }
        juce::PopupMenu menu;menu.addSectionHeader(i18n::tr("Colorear pistas seleccionadas"));menu.addItem(1,i18n::tr("Elegir color de pista…"));menu.addItem(2,i18n::tr("Ajustar color de pista a por defecto"),currentTrack()!=nullptr);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&trackColour),[safe=juce::Component::SafePointer<MainComponent>(&owner)](int r)
        {
            if(!safe||r==0)return;auto& self=*safe->ui;
            if(r==1){self.editTrackColour(self.selected,self.trackColour.getScreenBounds());return;}
            if(auto t=self.currentTrack()){self.pushUndo(i18n::tr("Color de pista"));static constexpr juce::uint32 defaults[]{0xffa994ff,0xff65d6c5,0xffffba72,0xff6eabef,0xffe391bf,0xffbdd27a};t->colour=juce::Colour(defaults[(size_t)self.selected%6]);self.markDirty();self.refresh();}
        });
    }
    void editTrackColour(int index,juce::Rectangle<int> anchor)
    {
        juce::String id;juce::Colour initial;
        {const juce::ScopedLock lock(engine.modelLock);if(index<0||index>=(int)engine.tracks.size())return;id=engine.tracks[(size_t)index]->id;initial=engine.tracks[(size_t)index]->colour;}
        showTrackColourPicker(anchor,initial,[safe=juce::Component::SafePointer<MainComponent>(&owner),id](juce::Colour colour)
        {
            if(!safe)return;auto& self=*safe->ui;const juce::ScopedLock lock(self.engine.modelLock);
            for(auto& t:self.engine.tracks)if(t->id==id){if(t->colour==colour)return;self.pushUndo(i18n::tr("Color de pista"));t->colour=colour;self.markDirty();self.refresh();return;}
        });
    }
    void setupSlider(juce::Slider& s,double a,double b,double step,juce::Slider::SliderStyle style,const juce::String& suffix,double reset)
    {s.setSliderStyle(style);s.setRange(a,b,step);s.setTextBoxStyle(style==juce::Slider::RotaryHorizontalVerticalDrag?juce::Slider::TextBoxBelow:juce::Slider::TextBoxRight,false,style==juce::Slider::RotaryHorizontalVerticalDrag?55:64,20);s.setTextValueSuffix(suffix);s.setDoubleClickReturnValue(true,reset);}
    std::shared_ptr<Track> currentTrack()const {return selected>=0&&selected<(int)engine.tracks.size()?engine.tracks[(size_t)selected]:nullptr;}
    void select(int i){selected=i;arrangement.selectedTrack=i;piano.track=i;piano.selected=-1;syncInspector();owner.repaint();}
    void markDirty(){dirty=true;owner.repaint();}
    Snapshot snapshot()
    {
        const juce::ScopedLock lock(engine.modelLock);Snapshot s;s.tempo=engine.tempo;s.master=engine.masterGain;s.loopStart=engine.loopStart;s.loopEnd=engine.loopEnd;s.looping=engine.looping;
        for(auto& t:engine.tracks)
        {
            TrackState state{t,t->name,t->colour,t->clips,t->notes,t->gain,t->pan,t->lowGainDb,t->midGainDb,t->highGainDb,t->mute,t->solo,t->armed,t->monitor,t->inputLeft,t->inputRight,t->output,{}};
            for(size_t k=0;k<state.sends.size();++k)state.sends[k]={t->sends[k].target,t->sends[k].levelDb.load(),t->sends[k].preFader.load(),t->sends[k].enabled.load()};
            s.tracks.push_back(std::move(state));
        }
        return s;
    }
    void restore(const Snapshot& s)
    {
        const juce::ScopedLock lock(engine.modelLock);engine.panic();engine.tracks.clear();for(auto& a:s.tracks){auto& t=*a.track;t.name=a.name;t.colour=a.colour;t.clips=a.clips;t.notes=a.notes;t.gain=a.gain;t.pan=a.pan;t.lowGainDb=a.low;t.midGainDb=a.mid;t.highGainDb=a.high;t.mute=a.mute;t.solo=a.solo;t.armed=a.armed;t.monitor=a.monitor;t.inputLeft=a.inputLeft;t.inputRight=a.inputRight;t.output=a.output;for(size_t k=0;k<a.sends.size();++k){t.sends[k].target=a.sends[k].target;t.sends[k].levelDb=a.sends[k].level;t.sends[k].preFader=a.sends[k].pre;t.sends[k].enabled=a.sends[k].on;}engine.tracks.push_back(a.track);}engine.tempo=s.tempo;engine.masterGain=s.master;engine.loopStart=s.loopStart;engine.loopEnd=s.loopEnd;engine.looping=s.looping;selected=juce::jlimit(0,juce::jmax(0,(int)engine.tracks.size()-1),selected);arrangement.selectedClip=-1;piano.selected=-1;markDirty();refresh();
    }
    // Each undo step stores the state before an edit plus the edit's name and time (Historial de ediciones).
    void pushUndo(const juce::String& action={}){auto s=snapshot();s.action=action.isEmpty()?i18n::tr("Edición"):action;s.time=juce::Time::getCurrentTime();undoStack.push_back(std::move(s));if(undoStack.size()>100)undoStack.pop_front();redoStack.clear();}
    void undo(bool redo){auto& from=redo?redoStack:undoStack;auto& to=redo?undoStack:redoStack;if(from.empty())return;to.push_back(snapshot());to.back().action=from.back().action;to.back().time=from.back().time;auto s=std::move(from.back());from.pop_back();restore(s);notice=redo?i18n::tr("Edición restaurada."):i18n::tr("Edición deshecha.");}
    void duplicateNote(){const juce::ScopedLock lock(engine.modelLock);auto t=currentTrack();if(!t||piano.selected<0||piano.selected>=(int)t->notes.size())return;pushUndo();auto n=t->notes[(size_t)piano.selected];n.id=juce::Uuid().toString();n.startBeats+=n.lengthBeats;t->notes.push_back(n);piano.selected=(int)t->notes.size()-1;markDirty();}
    void setPiano(bool show){pianoVisible=show;piano.setVisible(show);keyboard.setVisible(show);mixer.setVisible(!show);mixerTab.setToggleState(!show,juce::dontSendNotification);pianoTab.setToggleState(show,juce::dontSendNotification);owner.resized();owner.repaint();}
    // Transport options of the Transporte menu (manual pp. 311–331), saved per user.
    int startMode=0; // 0 cursor, 1 cycle start, 2 selection, 3 selection or cycle
    bool returnToStart=false,usePreRoll=false,usePostRoll=false,autoScroll=true,stationary=false;
    double preRollBars=1.0,postRollBars=1.0,playStartedAt=0.0;
    // «Modos de grabación» (manual p. 351): Record while recording stops (0) or re-records (1);
    // Record in stop mode starts at the cursor (0), left locator/punch in (1) or selection (2).
    int recordRestartMode=0,recordStartMode=0;bool stopAfterPunchOut=false;double recordStartedAt=0.0;
    double standardFadeLength=0.5;int standardFadeShape=0; // «Aplicar fundido estándar» (Por defecto in the fade editor)
    static juce::File transportFile(){return MainComponent::settingsFolder().getChildFile("transport.xml");}
    void loadTransport(){if(auto x=juce::XmlDocument::parse(transportFile())){startMode=juce::jlimit(0,3,x->getIntAttribute("startMode"));returnToStart=x->getBoolAttribute("returnToStart");usePreRoll=x->getBoolAttribute("preRoll");usePostRoll=x->getBoolAttribute("postRoll");autoScroll=x->getBoolAttribute("autoScroll",true);stationary=x->getBoolAttribute("stationary");preRollBars=x->getDoubleAttribute("preRollBars",1.0);postRollBars=x->getDoubleAttribute("postRollBars",1.0);
        recordRestartMode=juce::jlimit(0,1,x->getIntAttribute("recordRestart"));recordStartMode=juce::jlimit(0,2,x->getIntAttribute("recordStart"));stopAfterPunchOut=x->getBoolAttribute("stopAfterPunchOut");
        standardFadeLength=juce::jlimit(0.001,30.0,x->getDoubleAttribute("standardFade",0.5));standardFadeShape=juce::jlimit(0,(int)audio::FadeShape::count-1,x->getIntAttribute("standardFadeShape"));}}
    void saveTransport(){juce::XmlElement x("TRANSPORT");x.setAttribute("startMode",startMode);x.setAttribute("returnToStart",returnToStart);x.setAttribute("preRoll",usePreRoll);x.setAttribute("postRoll",usePostRoll);x.setAttribute("autoScroll",autoScroll);x.setAttribute("stationary",stationary);x.setAttribute("preRollBars",preRollBars);x.setAttribute("postRollBars",postRollBars);x.setAttribute("recordRestart",recordRestartMode);x.setAttribute("recordStart",recordStartMode);x.setAttribute("stopAfterPunchOut",stopAfterPunchOut);x.setAttribute("standardFade",standardFadeLength);x.setAttribute("standardFadeShape",standardFadeShape);transportFile().getParentDirectory().createDirectory();x.writeTo(transportFile());}
    double startPosition()
    {
        auto* sel=selectedClipPtr();
        switch(startMode){case 1:return engine.loopStart.load();case 2:return sel?sel->startSeconds:engine.positionSeconds.load();case 3:return sel?sel->startSeconds:engine.loopStart.load();default:return engine.positionSeconds.load();}
    }
    void startPlayback()
    {
        const double from=startPosition();playStartedAt=from;
        engine.seek(juce::jmax(0.0,from-(usePreRoll?preRollBars*barSeconds():0.0)));
        // Punch in active and armed tracks: playback switches to recording at the punch-in point.
        if(engine.punchIn.load()&&anyArmed()&&!engine.recording.load())
        {
            recordStartedAt=engine.loopStart.load();
            if(auto r=engine.beginRecording(recordFolder(),{-1.0,false});r.wasOk()){notice=i18n::tr("Punch in: grabará al llegar al localizador izquierdo.");return;}
            else error(r.getErrorMessage());
        }
        engine.play();
    }
    bool anyArmed(){const juce::ScopedLock lock(engine.modelLock);for(auto& t:engine.tracks)if(t->armed.load()&&t->hasEvents())return true;return false;}
    juce::File recordFolder()
    {   // Cubase records into the «Audio» folder of the project.
        return engine.audioFolder();
    }
    void finishRecording(const juce::String& message)
    {
        pushUndo(i18n::tr("Grabar"));auto r=engine.endRecording();if(r.failed())error(r.getErrorMessage());markDirty();notice=message;refresh();
    }
    void togglePlay(){if(engine.playing){engine.pause();if(returnToStart)engine.seek(playStartedAt);}else startPlayback();refresh();}
    void stopTransport()
    {
        // Cubase: Stop while playing stops (and returns to the start position if asked);
        // Stop while already stopped moves the cursor back to where playback started.
        if(engine.recording)finishRecording(i18n::tr("Grabación finalizada."));
        const bool wasPlaying=engine.playing.load();engine.pause();
        if(!wasPlaying||returnToStart)engine.seek(playStartedAt);
        refresh();
    }
    void playRange(double from,double until)
    {
        const double post=usePostRoll?postRollBars*barSeconds():0.0,pre=usePreRoll?preRollBars*barSeconds():0.0;
        playStartedAt=from;engine.playRange(juce::jmax(0.0,from-pre),until+post);refresh();
    }
    juce::PopupMenu transportMenu()
    {
        juce::PopupMenu m;
        m.addItem(100,i18n::tr("Inicio  Intro"));m.addItem(101,i18n::tr("Detener  Num 0"));m.addItem(102,i18n::tr("Iniciar/Detener  Espacio"));
        m.addItem(103,i18n::tr("Ciclo  Num /"),true,engine.looping.load());m.addItem(104,i18n::tr("Grabar  Num *"),true,engine.recording.load());
        m.addItem(105,i18n::tr("Rebobinar  Num −"));m.addItem(106,i18n::tr("Avanzar  Num +"));m.addItem(107,i18n::tr("Introducir posición del cursor del proyecto  ⇧P"));
        m.addItem(108,i18n::tr("Ir al inicio del proyecto  Num ."));m.addItem(109,i18n::tr("Ir al final del proyecto"));
        juce::PopupMenu mode;const juce::String modes[]{i18n::tr("Inicio desde posición del cursor de proyecto"),i18n::tr("Inicio desde inicio de ciclo"),i18n::tr("Inicio desde selección"),i18n::tr("Inicio desde inicio de selección o ciclo")};
        for(int n=0;n<4;++n)mode.addItem(110+n,modes[n],true,startMode==n);
        mode.addSeparator();mode.addItem(114,i18n::tr("Volver a la posición de inicio al parar"),true,returnToStart);m.addSubMenu(i18n::tr("Modo de inicio"),mode);
        juce::PopupMenu loc;loc.addItem(120,i18n::tr("Ir a la posición del localizador izquierdo  Num 1"));loc.addItem(121,i18n::tr("Ir a la posición del localizador derecho  Num 2"));
        loc.addItem(122,i18n::tr("Establecer localizador izquierdo a posición del cursor del proyecto  ⌘Num 1"));loc.addItem(123,i18n::tr("Establecer localizador derecho a posición del cursor del proyecto  ⌘Num 2"));
        loc.addItem(124,i18n::tr("Introducir posición del localizador izquierdo  ⇧L"));loc.addItem(125,i18n::tr("Introducir posición del localizador derecho  ⇧R"));loc.addItem(126,i18n::tr("Introducir duración de rango de localizadores  ⇧D"));
        loc.addItem(127,i18n::tr("Establecer localizadores a rango de selección  P"),selectedClipPtr()!=nullptr);loc.addItem(128,i18n::tr("Intercambiar posiciones de localizadores izquierdo y derecho"));m.addSubMenu(i18n::tr("Localizadores"),loc);
        juce::PopupMenu cursor;cursor.addItem(130,i18n::tr("Ir al inicio de la selección  L"),selectedClipPtr()!=nullptr);cursor.addItem(131,i18n::tr("Ir al final de la selección"),selectedClipPtr()!=nullptr);
        cursor.addItem(132,i18n::tr("Ir al evento siguiente  N"));cursor.addItem(133,i18n::tr("Ir al evento anterior  B"));m.addSubMenu(i18n::tr("Establecer posición del cursor del proyecto"),cursor);
        juce::PopupMenu range;const bool sel=selectedClipPtr()!=nullptr;
        range.addItem(140,i18n::tr("Reproducir desde el inicio de la selección"),sel);range.addItem(141,i18n::tr("Reproducir desde el final de la selección"),sel);
        range.addItem(142,i18n::tr("Reproducir hasta el inicio de la selección"),sel);range.addItem(143,i18n::tr("Reproducir hasta el final de la selección"),sel);
        range.addItem(144,i18n::tr("Reproducir selección  ⌥Espacio"),sel);range.addItem(145,i18n::tr("Reproducir selección en bucle  ⌥P"),sel);m.addSubMenu(i18n::tr("Reproducir rango del proyecto"),range);
        juce::PopupMenu roll;roll.addItem(150,i18n::tr("Usar pre-roll"),true,usePreRoll);roll.addItem(151,i18n::tr("Usar post-roll"),true,usePostRoll);
        roll.addSeparator();for(int n=0;n<4;++n){const double bars[]{0.5,1,2,4};roll.addItem(152+n,i18n::tr("Duración de pre-roll/post-roll: ")+juce::String(bars[n],1)+i18n::tr(" compás(es)"),true,std::abs(preRollBars-bars[n])<1e-9);}
        m.addSubMenu(i18n::tr("Pre-roll y post-roll"),roll);
        m.addSeparator();m.addItem(164,i18n::tr("Activar punch in  I"),true,engine.punchIn.load());m.addItem(165,i18n::tr("Activar punch out  O"),true,engine.punchOut.load());
        juce::PopupMenu rec;rec.addItem(170,i18n::tr("Punch in/out"),true,recordRestartMode==0);rec.addItem(171,i18n::tr("Regrabar"),true,recordRestartMode==1);rec.addSeparator();
        rec.addItem(172,i18n::tr("Iniciar grabación en la posición del cursor del proyecto"),true,recordStartMode==0);rec.addItem(173,i18n::tr("Iniciar grabación en el localizador izquierdo/posición de punch in"),true,recordStartMode==1);
        rec.addItem(174,i18n::tr("Iniciar grabación en la selección"),true,recordStartMode==2);rec.addSeparator();rec.addItem(175,i18n::tr("Parar después de pinchado de salida automático"),true,stopAfterPunchOut);
        m.addSubMenu(i18n::tr("Modos de grabación"),rec);
        juce::PopupMenu audioMode;const juce::String modeNames[]{i18n::tr("Mantener historial"),i18n::tr("Ciclo historial + reemplazar"),i18n::tr("Reemplazar")};
        for(int n=0;n<3;++n)audioMode.addItem(180+n,modeNames[n],true,engine.audioRecordMode.load()==n);m.addSubMenu(i18n::tr("Modo grabación de audio"),audioMode);
        m.addSubMenu(i18n::tr("Formato de grabación"),recordFormatMenu());
        m.addSeparator();m.addItem(160,i18n::tr("Activar metrónomo  C"),true,engine.metronome.load());m.addItem(161,i18n::tr("Configuración del metrónomo…"));
        m.addItem(162,i18n::tr("Desplazamiento auto.  F"),true,autoScroll);m.addItem(163,i18n::tr("Cursor estacionario  ⌥C"),true,stationary);
        return m;
    }
    // Audio › Fundidos (Cubase manual pp. 398–411) on the selected event.
    juce::PopupMenu audioMenu()
    {
        auto* c=selectedClipPtr();const bool has=c&&c->audio;
        juce::PopupMenu fades;
        fades.addItem(200,i18n::tr("Abrir editor(es) de fundido"),has);fades.addSeparator();
        fades.addItem(201,i18n::tr("Aplicar fundido de entrada estándar"),has);fades.addItem(202,i18n::tr("Aplicar fundido de salida estándar"),has);fades.addSeparator();
        fades.addItem(203,i18n::tr("Eliminar fundido de entrada"),has&&c->fadeIn>0.0);fades.addItem(204,i18n::tr("Eliminar fundido de salida"),has&&c->fadeOut>0.0);
        fades.addItem(205,i18n::tr("Eliminar fundidos"),has);fades.addSeparator();fades.addItem(206,i18n::tr("Fundido cruzado  X"),has);
        juce::PopupMenu m;m.addSubMenu(i18n::tr("Fundidos"),fades);
        m.addSeparator();m.addItem(207,i18n::tr("Pegar con el evento siguiente"),has);m.addItem(208,i18n::tr("Enmudecer evento"),has,has&&c->muted);
        return m;
    }
    void audioMenuItem(int id)
    {
        auto t=currentTrack();auto* c=selectedClipPtr();if(!t||!c){error(i18n::tr("Selecciona un evento."));return;}
        const auto change=[&](const juce::String& name,const std::function<void(AudioClip&)>& f){pushUndo(name);{const juce::ScopedLock lock(engine.modelLock);f(*c);}markDirty();arrangement.repaint();refresh();};
        switch(id)
        {
            case 200:
            {
                ui::FadeEditor::Settings s{c->fadeIn,c->fadeOut,c->fadeInShape,c->fadeOutShape};const auto id0=c->id;
                ui::showFadeEditor(owner,s,c->durationSeconds,[safe=juce::Component::SafePointer<MainComponent>(&owner),t,id0](ui::FadeEditor::Settings v)
                {
                    if(!safe)return;auto& self=*safe->ui;
                    for(auto& clip:t->clips)if(clip.id==id0){self.pushUndo(i18n::tr("Fundidos"));{const juce::ScopedLock lock(self.engine.modelLock);clip.fadeIn=v.fadeIn;clip.fadeOut=v.fadeOut;clip.fadeInShape=v.inShape;clip.fadeOutShape=v.outShape;}self.markDirty();self.arrangement.repaint();return;}
                },[safe=juce::Component::SafePointer<MainComponent>(&owner)](ui::FadeEditor::Settings v){if(!safe)return;auto& self=*safe->ui;self.standardFadeLength=juce::jmax(0.001,v.fadeIn);self.standardFadeShape=v.inShape;self.saveTransport();});
                break;
            }
            case 201:change(i18n::tr("Fundido de entrada"),[this](AudioClip& e){e.fadeIn=juce::jmin(standardFadeLength,juce::jmax(0.0,e.durationSeconds-e.fadeOut));e.fadeInShape=standardFadeShape;e.crossfade=false;});break;
            case 202:change(i18n::tr("Fundido de salida"),[this](AudioClip& e){e.fadeOut=juce::jmin(standardFadeLength,juce::jmax(0.0,e.durationSeconds-e.fadeIn));e.fadeOutShape=standardFadeShape;});break;
            case 203:change(i18n::tr("Eliminar fundido de entrada"),[](AudioClip& e){e.fadeIn=0.0;e.crossfade=false;});break;
            case 204:change(i18n::tr("Eliminar fundido de salida"),[](AudioClip& e){e.fadeOut=0.0;});break;
            case 205:change(i18n::tr("Eliminar fundidos"),[](AudioClip& e){e.fadeIn=e.fadeOut=0.0;e.crossfade=false;});break;
            case 206:arrangement.crossfadeSelected();break;
            case 207:arrangement.glueSelected();break;
            case 208:change(i18n::tr("Enmudecer evento"),[](AudioClip& e){e.muted=!e.muted;});break;
            default:break;
        }
    }
    bool rangeActive()const{return arrangement.hasRange&&arrangement.tool==2;}
    // Range clipboard: one content per covered track, top to bottom (Cubase: cut/copy/paste a range).
    std::vector<audio::RangeContent> rangeClipboard;double rangeClipboardStart=0.0,rangeClipboardLength=0.0;
    void copyRangeToClipboard()
    {
        const audio::TimeRange r{arrangement.rangeStart,arrangement.rangeEnd};rangeClipboard.clear();
        for(int i:arrangement.rangeTracks())if(i>=0&&i<(int)engine.tracks.size()){const auto& t=*engine.tracks[(size_t)i];rangeClipboard.push_back(audio::copyRange(t.clips,t.notes,r,engine.tempo.load()));}
        rangeClipboardStart=r.start;rangeClipboardLength=r.length();clipboard.reset();
    }
    void pasteRangeClipboard(bool atOrigin)
    {   // Pastes at the cursor (⌥V: where it came from) on the selected track and the ones below.
        const auto v=arrangement.visibleTracks();auto it=std::find(v.begin(),v.end(),selected);if(it==v.end()){error(i18n::tr("Selecciona una pista para pegar."));return;}
        const double at=atOrigin?rangeClipboardStart:engine.positionSeconds.load();int row=(int)std::distance(v.begin(),it);
        pushUndo(i18n::tr("Pegar rango"));
        {const juce::ScopedLock lock(engine.modelLock);for(const auto& content:rangeClipboard){if(row>=(int)v.size())break;auto& t=*engine.tracks[(size_t)v[(size_t)row++]];audio::pasteContent(t.clips,t.notes,content,at,engine.tempo.load());}}
        arrangement.setRange(at,at+rangeClipboardLength,(int)std::distance(v.begin(),it),juce::jmin((int)v.size()-1,(int)std::distance(v.begin(),it)+(int)rangeClipboard.size()-1));
        markDirty();refresh();
    }
    void moveRangeContent(double delta,int rows,bool copy)
    {   // All contents are taken first, then removed (unless copying), then placed, so overlapping moves work.
        const audio::TimeRange r{arrangement.rangeStart,arrangement.rangeEnd};const auto v=arrangement.visibleTracks();const auto tracks=arrangement.rangeTracks();
        pushUndo(copy?i18n::tr("Copiar rango"):i18n::tr("Mover rango"));
        {
            const juce::ScopedLock lock(engine.modelLock);const double bpm=engine.tempo.load();std::vector<audio::RangeContent> contents;
            for(int i:tracks)contents.push_back(audio::copyRange(engine.tracks[(size_t)i]->clips,engine.tracks[(size_t)i]->notes,r,bpm));
            if(!copy)for(int i:tracks)audio::deleteRange(engine.tracks[(size_t)i]->clips,engine.tracks[(size_t)i]->notes,r,bpm);
            for(size_t k=0;k<tracks.size();++k){const int row=arrangement.rangeFirstRow+(int)k+rows;if(row<0||row>=(int)v.size())continue;auto& t=*engine.tracks[(size_t)v[(size_t)row]];audio::pasteContent(t.clips,t.notes,contents[k],r.start+delta,bpm);}
        }
        arrangement.rangeStart+=delta;arrangement.rangeEnd+=delta;arrangement.rangeFirstRow+=rows;arrangement.rangeLastRow+=rows;arrangement.selectedClip=-1;
        markDirty();arrangement.repaint();refresh();
    }
    // Range edits on every track the range covers, as one undo step.
    void rangeEdit(const juce::String& name,const std::function<void(std::vector<AudioClip>&,std::vector<MidiNote>&,audio::TimeRange,double)>& op,bool needsLength=true)
    {
        if(!arrangement.hasRange){error(i18n::tr("Selecciona un rango con la herramienta Selección de rango (2)."));return;}
        const audio::TimeRange r{arrangement.rangeStart,arrangement.rangeEnd};
        if(needsLength&&r.length()<=1e-9){error(i18n::tr("El rango está vacío."));return;}
        const auto tracks=arrangement.rangeTracks();if(tracks.empty())return;
        pushUndo(name);
        {const juce::ScopedLock lock(engine.modelLock);for(int i:tracks)if(i>=0&&i<(int)engine.tracks.size()){auto& t=*engine.tracks[(size_t)i];op(t.clips,t.notes,r,engine.tempo.load());}}
        arrangement.selectedClip=-1;markDirty();arrangement.repaint();refresh();
    }
    juce::PopupMenu rangeMenu()
    {   // Edición › Rango (and the range's context menu).
        juce::PopupMenu m;const bool has=arrangement.hasRange;
        m.addItem(71,i18n::tr("Dividir  ⇧X"),has);m.addItem(72,i18n::tr("Truncar"),has);m.addItem(73,i18n::tr("Suprimir intervalo de tiempo  ⇧Retroceso"),has);
        m.addItem(74,i18n::tr("Insertar silencio  ⇧⌘E"),has);m.addItem(75,i18n::tr("Ajustar fundidos al rango  A"),has);m.addSeparator();
        m.addItem(76,i18n::tr("Suprimir  Retroceso"),has);m.addItem(77,i18n::tr("Duplicar  ⌘D"),has);
        return m;
    }
    void rangeMenuItem(int id)
    {
        const char* ids[]{"splitRange","truncateRange","deleteTime","insertSilence","adjustFades","delete","duplicate"};
        if(id>=71&&id<=77){if(id>=76&&arrangement.tool!=2)setTool(2);if(auto a=keyActions.find(ids[id-71]);a!=keyActions.end())a->second();}
    }
    juce::PopupMenu recordFormatMenu()
    {
        juce::PopupMenu f;const int bits=engine.recordBitDepth.load();
        f.addItem(185,i18n::tr("16 bit"),true,bits==16);f.addItem(186,i18n::tr("24 bit"),true,bits==24);f.addItem(187,i18n::tr("32 bit flotante"),true,bits==32);return f;
    }
    void transportMenuItem(int id)
    {
        auto run=[this](const char* c){if(auto a=keyActions.find(c);a!=keyActions.end())a->second();};
        auto* sel=selectedClipPtr();
        switch(id)
        {
            case 100:if(!engine.playing)startPlayback();break;case 101:stopTransport();break;case 102:togglePlay();break;case 103:run("cycle");break;case 104:run("record");break;
            case 105:run("rewind");break;case 106:run("forward");break;case 107:run("enterPosition");break;case 108:run("goStart");break;case 109:engine.seek(engine.duration());break;
            case 110:case 111:case 112:case 113:startMode=id-110;saveTransport();break;case 114:returnToStart=!returnToStart;saveTransport();break;
            case 120:run("goLeft");break;case 121:run("goRight");break;case 122:run("setLeft");break;case 123:run("setRight");break;case 124:run("enterLeft");break;case 125:run("enterRight");break;case 126:run("enterRange");break;
            case 127:run("locatorsToSelection");break;case 128:{pushUndo(i18n::tr("Intercambiar localizadores"));const double a=engine.loopStart.load();engine.loopStart=engine.loopEnd.load();engine.loopEnd=a;markDirty();break;}
            case 130:run("selectionStart");break;case 131:if(sel)engine.seek(sel->startSeconds+sel->durationSeconds);break;case 132:run("nextEvent");break;case 133:run("previousEvent");break;
            case 140:if(sel){playStartedAt=sel->startSeconds;engine.seek(sel->startSeconds);engine.play();}break;
            case 141:if(sel){const double e=sel->startSeconds+sel->durationSeconds;playStartedAt=e;engine.seek(e);engine.play();}break;
            case 142:if(sel){playStartedAt=juce::jmax(0.0,sel->startSeconds-2.0);engine.playRange(playStartedAt,sel->startSeconds);}break;
            case 143:if(sel){const double e=sel->startSeconds+sel->durationSeconds;playStartedAt=juce::jmax(0.0,e-2.0);engine.playRange(playStartedAt,e);}break;
            case 144:run("playSelection");break;case 145:run("loopSelection");break;
            case 150:usePreRoll=!usePreRoll;saveTransport();break;case 151:usePostRoll=!usePostRoll;saveTransport();break;
            case 152:case 153:case 154:case 155:{const double bars[]{0.5,1,2,4};preRollBars=postRollBars=bars[id-152];saveTransport();break;}
            case 164:run("punchIn");break;case 165:run("punchOut");break;
            case 170:case 171:recordRestartMode=id-170;saveTransport();break;case 172:case 173:case 174:recordStartMode=id-172;saveTransport();break;
            case 175:stopAfterPunchOut=!stopAfterPunchOut;saveTransport();break;
            case 180:case 181:case 182:engine.audioRecordMode=id-180;markDirty();break;
            case 185:case 186:case 187:if(engine.recording.load())error(i18n::tr("Detén la grabación antes de cambiar el formato."));else{engine.recordBitDepth=id==185?16:id==186?24:32;markDirty();}break;
            case 160:run("metronome");break;case 161:showMetronomeSettings();break;case 162:run("autoScroll");break;case 163:run("stationary");break;
            default:break;
        }
        refresh();
    }
    void toggleRecord()
    {
        if(engine.recording)
        {
            if(recordRestartMode==0){finishRecording(i18n::tr("Grabación finalizada."));return;}
            engine.cancelRecording();beginRecord(true);return; // Regrabar: same start, takes discarded
        }
        beginRecord(false);
    }
    void beginRecord(bool again)
    {
        {const juce::ScopedLock lock(engine.modelLock);bool armedAny=false;for(auto& t:engine.tracks)armedAny|=t->armed.load();if(!armedAny){if(auto t=currentTrack();t&&t->hasEvents())t->armed=true;else{engine.addAudioTrack(i18n::tr("Grabación"))->armed=true;selected=(int)engine.tracks.size()-1;}}}
        RecordOptions options;
        if(!engine.playing.load()||again)
        {
            auto* sel=selectedClipPtr();
            const double at=again?recordStartedAt:recordStartMode==1?engine.loopStart.load():recordStartMode==2&&sel?sel->startSeconds:engine.positionSeconds.load();
            recordStartedAt=playStartedAt=at;
            // Pre-roll: the cursor starts earlier and the input is written from `at` (no count-in).
            if(usePreRoll){engine.seek(juce::jmax(0.0,at-preRollBars*barSeconds()));options.captureFrom=at;options.countIn=false;}else engine.seek(at);
        }
        else{options.countIn=false;recordStartedAt=engine.positionSeconds.load();}
        auto r=engine.beginRecording(recordFolder(),options);if(r.failed())error(r.getErrorMessage());else{markDirty();notice=i18n::tr("Grabando pistas armadas…");}refresh();
    }
    void showLoopDialog()
    {
        auto* a=new juce::AlertWindow(i18n::tr("Región de ciclo"),i18n::tr("Posiciones en compases; 1 equivale al inicio."),juce::MessageBoxIconType::NoIcon);double bar=240.0/engine.tempo;
        a->addTextEditor("start",juce::String(engine.loopStart/bar+1,2),i18n::tr("Inicio"));a->addTextEditor("end",juce::String(engine.loopEnd/bar+1,2),i18n::tr("Final"));a->addButton(i18n::tr("Aplicar"),1);a->addButton(i18n::tr("Cancelar"),0);
        a->enterModalState(true,juce::ModalCallbackFunction::create([safe=juce::Component::SafePointer<MainComponent>(&owner),a](int result){if(!safe||result!=1)return;auto& self=*safe->ui;double bar=240.0/self.engine.tempo;double start=juce::jmax(0.0,(a->getTextEditorContents("start").getDoubleValue()-1)*bar);double end=(a->getTextEditorContents("end").getDoubleValue()-1)*bar;if(end>start+.01){self.engine.loopStart=start;self.engine.loopEnd=end;self.markDirty();}}),true);
    }
    void error(const juce::String& message){notice=message;juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Silente",message,i18n::tr("Entendido"),&owner);}
    void syncInspector()
    {
        syncing=true;auto t=currentTrack();for(auto* c:std::initializer_list<juce::Component*>{&gain,&pan,&low,&mid,&high,&mute,&solo,&arm,&monitor})c->setEnabled(t!=nullptr);
        if(t){const bool signal=t->kind!=TrackKind::folder;for(auto* c:std::initializer_list<juce::Component*>{&gain,&pan,&low,&mid,&high,&mute,&solo})c->setEnabled(signal);arm.setEnabled(t->hasEvents());monitor.setEnabled(t->hasEvents());}
        if(t){if(!gain.isMouseButtonDown())gain.setValue(juce::Decibels::gainToDecibels(t->gain.load(),-60.0f),juce::dontSendNotification);if(!pan.isMouseButtonDown())pan.setValue(t->pan*100,juce::dontSendNotification);if(!low.isMouseButtonDown())low.setValue(t->lowGainDb,juce::dontSendNotification);if(!mid.isMouseButtonDown())mid.setValue(t->midGainDb,juce::dontSendNotification);if(!high.isMouseButtonDown())high.setValue(t->highGainDb,juce::dontSendNotification);mute.setToggleState(t->mute,juce::dontSendNotification);solo.setToggleState(t->solo,juce::dontSendNotification);arm.setToggleState(t->armed,juce::dontSendNotification);monitor.setToggleState(t->monitor,juce::dontSendNotification);}
        {for(int i=0;i<(int)inserts.size();++i){inserts[(size_t)i].setEnabled(t!=nullptr);const int slot=t?effectSlotIndex(*t,i):-1;if(slot>=0){auto& p=*t->plugins[(size_t)slot];inserts[(size_t)i].setButtonText(juce::String(i+1)+"  "+p.description.name);inserts[(size_t)i].setTooltip(p.error.isNotEmpty()?p.error:i18n::tr("Abrir editor del plugin"));}else{inserts[(size_t)i].setButtonText(i18n::tr("+  Añadir plugin"));inserts[(size_t)i].setTooltip(i18n::tr("VST3 / Audio Units: instrumentos y efectos"));}}}
        inputRouting.setVisible(inspectorShown()&&sectionOpen[0]&&!inputRouting.getBounds().isEmpty()&&t&&t->kind==TrackKind::audio);inputRouting.setEnabled(t&&t->kind==TrackKind::audio&&!engine.recording);
        if(t)inputRouting.setButtonText(t->inputLeft==t->inputRight?i18n::tr("Mono ")+juce::String(t->inputLeft+1):i18n::tr("Estéreo ")+juce::String(t->inputLeft+1)+"–"+juce::String(t->inputRight+1));
        instrument.setVisible(inspectorShown()&&sectionOpen[0]&&!instrument.getBounds().isEmpty()&&t&&t->isMidi);instrument.setEnabled(t&&t->isMidi);
        const int instrumentIndex=t?instrumentSlotIndex(*t):-1;
        instrument.setButtonText(instrumentIndex>=0?t->plugins[(size_t)instrumentIndex]->description.name:i18n::tr("Seleccionar instrumento"));
        instrument.setTooltip(i18n::tr("Clic: abrir instrumento. Flecha: seleccionar o sustituir."));
        if(!master.isMouseButtonDown())master.setValue(juce::Decibels::gainToDecibels(engine.masterGain.load(),-60.0f),juce::dontSendNotification);syncing=false;
    }
    void refresh(bool fromTimer=false)
    {
        syncing=true;if(!projectTitle.isBeingEdited())projectTitle.setText(engine.projectName,juce::dontSendNotification);if(!tempo.isMouseButtonDown())tempo.setValue(engine.tempo,juce::dontSendNotification);syncing=false;
        play.setToggleState(engine.playing,juce::dontSendNotification);play.setButtonText(engine.playing?juce::String::fromUTF8("Ⅱ"):juce::String::fromUTF8("▶"));loop.setToggleState(engine.looping,juce::dontSendNotification);click.setToggleState(engine.metronome,juce::dontSendNotification);
        record.setColour(juce::TextButton::buttonColourId,engine.capturing?coral:engine.recording?coral.withAlpha(.3f):raised);
        if(engine.recording.load())arrangement.repaint(); // the red event grows
        punchInButton.setToggleState(engine.punchIn.load(),juce::dontSendNotification);punchOutButton.setToggleState(engine.punchOut.load(),juce::dontSendNotification);
        if(engine.recording.load()&&engine.punchedOut.exchange(false))
        {   // Punch out passed: the take is closed, playback continues (or stops after the post-roll).
            finishRecording(i18n::tr("Punch out: grabación finalizada."));
            if(stopAfterPunchOut){const double until=engine.loopEnd.load()+(usePostRoll?postRollBars*barSeconds():0.0);if(engine.positionSeconds.load()>=until)engine.pause();else engine.stopAt=until;}
        }scan.setButtonText(plugins.isScanning()?i18n::tr("Detener búsqueda"):i18n::tr("Buscar plugins"));
        double time=engine.positionSeconds;const int mins=(int)time/60,secs=(int)time%60,ms=(int)(time*1000)%1000;transportTime.setText(juce::String(mins).paddedLeft('0',2)+":"+juce::String(secs).paddedLeft('0',2)+"."+juce::String(ms).paddedLeft('0',3),juce::dontSendNotification);
        {const auto countIn=engine.countInSamples.load();status.setText(countIn>0?i18n::tr("Precuenta… ")+juce::String((int)std::ceil(countIn/engine.currentSampleRate()*engine.tempo.load()/60.0)):notice,juce::dontSendNotification);}rightRack.tick();trackColour.setEnabled(currentTrack()!=nullptr);if(channelWindow)if(auto* panel=dynamic_cast<ChannelPanel*>(channelWindow->getContentComponent()))panel->refresh();
        const double view=juce::jmax(1.0,(arrangement.getWidth()-Arrangement::header)/arrangement.pixelsPerSecond);timelineScroll.setRangeLimits(0,juce::jmax(engine.duration()+8,arrangement.scrollSeconds+view));timelineScroll.setCurrentRange(arrangement.scrollSeconds,view,juce::dontSendNotification);
        {bool anyMute=false,anySolo=false;for(auto& t:engine.tracks){anyMute|=t->mute.load();anySolo|=t->solo.load();}allMuteOff.setToggleState(anyMute,juce::dontSendNotification);allSoloOff.setToggleState(anySolo,juce::dontSendNotification);}
        if(channelTab)channelTab->setTrack(selected);
        if(engine.playing.load()&&autoScroll&&!arrangement.isMouseButtonDown())
        {   // Desplazamiento auto.: page scroll, or the cursor stays centred (Cursor estacionario).
            const double view=juce::jmax(1.0,(arrangement.getWidth()-Arrangement::header)/arrangement.pixelsPerSecond),pos=engine.positionSeconds.load();
            if(stationary)arrangement.scrollSeconds=juce::jmax(0.0,pos-view*0.5);
            else if(pos>arrangement.scrollSeconds+view*0.98||pos<arrangement.scrollSeconds)arrangement.scrollSeconds=juce::jmax(0.0,pos-view*0.02);
        }
        snapTypeButton.setButtonText(snapTypeName(arrangement.snapGrid.type));gridButton.setButtonText(gridName(arrangement.snapGrid.grid));
        if(auto* window=owner.findParentComponentOfClass<juce::DocumentWindow>())
        {   // As in Cubase, the window title carries the project name; «*» marks unsaved changes.
            juce::String title="Silente - "+engine.projectName+(dirty?" *":"");
#if defined(__x86_64__)
            title<<" · Intel (Rosetta)";
#endif
            if(window->getName()!=title)window->setName(title);
        }
        undoButton.setEnabled(!undoStack.empty());redoButton.setEnabled(!redoStack.empty());arrangement.selectedTrack=selected;piano.track=selected;syncInspector();if(mixer.isShowing())mixer.refresh();
        // Calls from edits repaint everything; the 24 Hz timer repaints the whole window only when
        // something visible changed, otherwise just the cursor, meters and transport.
        const auto signature=viewSignature();
        if(!fromTimer||signature!=lastSignature){lastSignature=signature;arrangement.repaint();piano.repaint();owner.repaint();return;}
        arrangement.liveRepaint();if(piano.isShowing()&&engine.playing.load())piano.repaint();
        if(!transportRect.isEmpty())
        {   // Only what moves by itself: the musical position box and the beat lights (the time label repaints itself).
            const int ty=transportRect.getY(),cx=owner.getWidth()/2;
            owner.repaint(cx-300,ty+10,150,26);owner.repaint(cx+516,ty+20,52,10);
        }
    }
    juce::uint64 lastSignature=0;
    juce::uint64 viewSignature()const
    {   // Everything the window draws, except the playback position and the meters.
        juce::uint64 h=1469598103934665603ull;const auto mix=[&h](double v){juce::uint64 bits;std::memcpy(&bits,&v,sizeof bits);h=(h^bits)*1099511628211ull;};
        mix(engine.tracks.size());mix(selected);mix(arrangement.selectedClip);mix(arrangement.tool);mix(dirty);mix(undoStack.size());mix(redoStack.size());
        mix(engine.looping.load());mix(engine.loopStart.load());mix(engine.loopEnd.load());mix(engine.punchIn.load());mix(engine.punchOut.load());mix(engine.tempo.load());
        mix(engine.metronome.load());mix(engine.recording.load());mix(engine.playing.load());mix(engine.capturing.load());mix(engine.recordBitDepth.load());
        mix(arrangement.scrollSeconds);mix(arrangement.pixelsPerSecond);mix(arrangement.lane);mix(arrangement.waveZoom);mix(arrangement.firstTrack);
        mix(arrangement.hasRange);mix(arrangement.rangeStart);mix(arrangement.rangeEnd);mix(arrangement.rangeFirstRow);mix(arrangement.rangeLastRow);
        mix((double)notice.hashCode64());mix(owner.getWidth());mix(owner.getHeight());
        mix(engine.currentSampleRate());mix(engine.currentBlockSize());
        if(auto* device=engine.deviceManager.getCurrentAudioDevice()){mix((double)device->getName().hashCode64());mix(device->getActiveInputChannels().countNumberOfSetBits());mix(device->getActiveOutputChannels().countNumberOfSetBits());}
        for(const auto& t:engine.tracks)
        {
            mix(t->mute.load());mix(t->solo.load());mix(t->armed.load());mix(t->monitor.load());mix(t->gain.load());mix(t->pan.load());mix((double)t->colour.getARGB());
            mix((double)t->name.hashCode64());mix(t->clips.size());mix(t->notes.size());mix(t->plugins.size());mix(t->stripWidth);
            for(const auto& c:t->clips){mix(c.startSeconds);mix(c.durationSeconds);mix(c.offsetSeconds);mix(c.gain);mix(c.fadeIn);mix(c.fadeOut);mix(c.muted);mix((double)c.colour.getARGB());}
        }
        return h;
    }
    void layout()
    {
        const int w=owner.getWidth(),h=owner.getHeight();
        const int left=showLeft?inspectorWidth:0,channelW=showChannel?channelWidth:0,right=showRight?(rightWidth>0?rightWidth:(w>=1250?212:180)):0;
        int top=41;statusRect=infoRect={};
        if(showStatusLine){statusRect={0,top,w,24};top+=24;}
        if(showInfoLine){infoRect={0,top,w,22};top+=22;}
        const int bottom=(showTransport?47:0)+23;
        transportRect=showTransport?juce::Rectangle<int>(0,h-bottom,w,47):juce::Rectangle<int>();
        channelRect={0,top,channelW,h-top-bottom};inspectorRect={channelW,top,left,h-top-bottom};
        browserRect={w-right,top,right,h-top-bottom};centreRect={channelW+left,top,w-channelW-left-right,h-top-bottom};
        lowerRect=showLower?centreRect.removeFromBottom(lowerHeight>0?juce::jlimit(150,juce::jmax(150,centreRect.getHeight()-140),lowerHeight):juce::jlimit(280,380,(int)(h*0.4))):juce::Rectangle<int>();
        // Toolbar in the order of the manual (pp. 58–66); zone controls on the right.
        undoButton.setBounds(50,7,28,28);redoButton.setBounds(80,7,28,28);
        allMuteOff.setBounds(120,8,26,26);allSoloOff.setBounds(148,8,26,26);mixConsoleButton.setBounds(186,7,30,28);
        // Tools in Cubase's order: 1 selección, 3 dividir, 4 pegar, 5 borrar, 6 zoom, 7 enmudecer, 8 dibujar, 9 reproducir.
        {int x=228;for(auto* b:{&selectTool,&rangeTool,&cutTool,&glueTool,&eraseTool,&zoomTool,&muteTool,&drawTool,&playTool}){b->setBounds(x,7,30,28);x+=32;}}
        trackColour.setBounds(526,7,30,28);snapButton.setBounds(568,7,30,28);snapTypeButton.setBounds(600,8,96,26);gridButton.setBounds(698,8,104,26);zoomOut.setBounds(814,8,28,26);zoomIn.setBounds(844,8,28,26);
        hubButton.setBounds(8,7,30,28);exportButton.setBounds(w-214,7,30,28);zoneChannelButton.setBounds(w-170,7,30,28);zoneLeftButton.setBounds(w-138,7,30,28);
        zoneLowerButton.setBounds(w-106,7,30,28);zoneRightButton.setBounds(w-74,7,30,28);layoutButton.setBounds(w-38,7,30,28);
        addTrackButton.setBounds(centreRect.getX()+4,centreRect.getY()+4,24,22);
        splitButton.setVisible(false);duplicateButton.setVisible(false);scan.setVisible(false);browser.setVisible(false);demo.setVisible(false);master.setVisible(false);
        const int dx=inspectorRect.getX();
        const int tabW=(left-10)/2;
        inspectorTab.setBounds(dx+3,top+3,tabW,25);visibilityTab.setBounds(dx+6+tabW,top+3,tabW,25);
        trackTabButton.setBounds(dx+3,inspectorRect.getBottom()-28,tabW,25);editorTabButton.setBounds(dx+6+tabW,inspectorRect.getBottom()-28,tabW,25);
        visibility.setBounds(inspectorRect.withTrimmedTop(31).withTrimmedBottom(31));rightRack.setBounds(browserRect);channelTab->setBounds(channelRect);
        auto timeline=centreRect;auto zoomStrip=timeline.removeFromRight(16);timelineScroll.setBounds(timeline.removeFromBottom(12).withTrimmedLeft(Arrangement::header));arrangement.setBounds(timeline);
        waveZoomSlider.setBounds(zoomStrip.getX(),zoomStrip.getY()+Arrangement::ruler,16,juce::jmin(90,zoomStrip.getHeight()/2-Arrangement::ruler));
        trackHeightSlider.setBounds(zoomStrip.getX(),zoomStrip.getBottom()-12-juce::jmin(110,zoomStrip.getHeight()/2),16,juce::jmin(110,zoomStrip.getHeight()/2));
        channelSplit.setBounds(channelRect.getRight()-3,channelRect.getY(),6,channelRect.getHeight());channelSplit.setVisible(showChannel);
        inspectorSplit.setBounds(inspectorRect.getRight()-3,inspectorRect.getY(),6,inspectorRect.getHeight());inspectorSplit.setVisible(showLeft);
        rightSplit.setBounds(browserRect.getX()-3,browserRect.getY(),6,browserRect.getHeight());rightSplit.setVisible(showRight);
        lowerSplit.setBounds(lowerRect.getX(),lowerRect.getY()-3,lowerRect.getWidth(),6);lowerSplit.setVisible(showLower);
        for(auto* z:{&channelSplit,&inspectorSplit,&rightSplit,&lowerSplit})z->toFront(false);
        mixerTab.setButtonText(i18n::tr("MixConsole"));pianoTab.setButtonText(i18n::tr("Editor"));
        if(showLower)
        {
            mixerTab.setBounds(lowerRect.getX()+5,lowerRect.getBottom()-28,101,25);pianoTab.setBounds(lowerRect.getX()+109,lowerRect.getBottom()-28,101,25);
            auto lowerContent=lowerRect.withTrimmedBottom(31);mixer.setBounds(lowerContent);lowerConsole->setBounds(lowerContent);piano.setBounds(lowerContent.withTrimmedBottom(46));keyboard.setBounds(lowerContent.removeFromBottom(44));
        }
        layoutInspector(dx,top,left,inspectorRect.getBottom()-32);
        // Transport bar as in Cubase: locators left; cycle, stop, play, record centred; position,
        // tempo with Tap and the metronome to the right.
        const int y=transportRect.getY()+9,cx=w/2;
        rewind.setBounds(cx-132,y+2,30,29);loop.setBounds(cx-94,y+2,35,29);stop.setBounds(cx-55,y+2,34,29);play.setBounds(cx-17,y+2,40,29);record.setBounds(cx+27,y+2,34,29);
        punchInButton.setBounds(10,y+2,28,26);punchOutButton.setBounds(318,y+2,28,26);
        click.setBounds(cx+72,y+2,35,29);clickOptions.setBounds(cx+109,y+13,9,7);
        transportTime.setBounds(cx+128,y-1,150,35);transportTime.setFont(juce::Font(juce::FontOptions(19)));
        tempo.setBounds(cx+288,y+2,92,29);tapButton.setBounds(cx+418,y+4,44,25);status.setBounds(12,h-23,w-330,23);
        applyZones();
    }
    // Inspector laid out like Cubase's (manual pp. 78–82): colour name bar with «e», M · S · ● · ◉,
    // thin volume and pan rows, then folding sections «▼ Enrutado / Inserciones / Ecualizadores».
    struct InspectorAreas { juce::Rectangle<int> nameBar,volumeIcon,panIcon,output,eqLabels; std::array<juce::Rectangle<int>,3> sections; } insp;
    std::array<bool,3> sectionOpen{true,true,true};
    std::vector<juce::Component*> inspectorOverflow; // controls that did not fit in the zone height
    void layoutInspector(int dx,int top,int left,int bottom)
    {
        inspectorOverflow.clear();insp={};
        int y=top+34;const int w=left-16;
        const auto place=[&](juce::Component& c,juce::Rectangle<int> r){c.setBounds(r);if(r.getBottom()>bottom)inspectorOverflow.push_back(&c);};
        insp.nameBar={dx+8,y,w-32,24};place(channelButton,{dx+left-36,y,28,24});y+=30;
        int x=dx+8;for(auto* b:{&mute,&solo,&arm,&monitor}){place(*b,{x,y,28,22});x+=31;}y+=28;
        insp.volumeIcon={dx+8,y,16,22};place(gain,{dx+26,y,left-34,22});y+=25;
        insp.panIcon={dx+8,y,16,22};place(pan,{dx+26,y,left-34,22});y+=30;
        const bool midi=currentTrack()&&currentTrack()->isMidi;
        insp.sections[0]={dx+6,y,w+4,20};y+=24;
        if(sectionOpen[0]){place(midi?instrument:inputRouting,{dx+10,y,w-4,22});(midi?inputRouting:instrument).setBounds({});y+=26;insp.output={dx+10,y,w-4,20};y+=26;}
        else{instrument.setBounds({});inputRouting.setBounds({});}
        insp.sections[1]={dx+6,y,w+4,20};y+=24;
        if(sectionOpen[1]){for(auto& b:inserts){place(b,{dx+10,y,w-4,22});y+=25;}y+=4;}
        else for(auto& b:inserts)b.setBounds({});
        insp.sections[2]={dx+6,y,w+4,20};y+=24;
        if(sectionOpen[2]){const int k=(w)/3;place(low,{dx+8,y,k,58});place(mid,{dx+8+k,y,k,58});place(high,{dx+8+2*k,y,k,58});insp.eqLabels={dx+8,y+58,3*k,14};y+=76;}
        else{low.setBounds({});mid.setBounds({});high.setBounds({});}
        for(auto& r:insp.sections)if(r.getBottom()>bottom)r={};
        if(insp.eqLabels.getBottom()>bottom)insp.eqLabels={};if(insp.output.getBottom()>bottom)insp.output={};
    }
    void applyZones()
    {
        const bool shown=inspectorShown();
        const auto fits=[this](juce::Component* c){return !c->getBounds().isEmpty()&&std::find(inspectorOverflow.begin(),inspectorOverflow.end(),c)==inspectorOverflow.end();};
        for(auto* c:std::initializer_list<juce::Component*>{&gain,&pan,&mute,&solo,&arm,&monitor,&channelButton,&low,&mid,&high})c->setVisible(shown&&fits(c));
        for(auto& b:inserts)b.setVisible(shown&&fits(&b));
        if(!shown){instrument.setVisible(false);inputRouting.setVisible(false);}
        for(auto* c:std::initializer_list<juce::Component*>{&inspectorTab,&visibilityTab,&trackTabButton,&editorTabButton})c->setVisible(showLeft);
        visibility.setVisible(showLeft&&visibilityVisible);rightRack.setVisible(showRight);channelTab->setVisible(showChannel);
        mixerTab.setVisible(showLower);pianoTab.setVisible(showLower);mixer.setVisible(false);lowerConsole->setVisible(showLower&&!pianoVisible);piano.setVisible(showLower&&pianoVisible);keyboard.setVisible(showLower&&pianoVisible);
        for(auto* c:std::initializer_list<juce::Component*>{&rewind,&stop,&play,&record,&loop,&click,&clickOptions,&tapButton,&transportTime,&tempo})c->setVisible(showTransport);
        // Language, audio setup and import live in the menus (Idioma, Estudio, Archivo), as in Cubase.
        for(auto* c:std::initializer_list<juce::Component*>{&languageButton,&settings,&importButton,&projectTitle})c->setVisible(false);
        zoneChannelButton.setToggleState(showChannel,juce::dontSendNotification);zoneLeftButton.setToggleState(showLeft,juce::dontSendNotification);
        zoneLowerButton.setToggleState(showLower,juce::dontSendNotification);zoneRightButton.setToggleState(showRight,juce::dontSendNotification);
        trackTabButton.setToggleState(!editorInspector,juce::dontSendNotification);editorTabButton.setToggleState(editorInspector,juce::dontSendNotification);
    }
    // Inspector section header bar, as in Cubase («▼ Inserciones», «▼ Enrutado»…).
    static void section(juce::Graphics& g,const juce::String& title,juce::Rectangle<int> r,bool open=true)
    {
        g.setColour(raised);g.fillRoundedRectangle(r.toFloat(),2.5f);g.setColour(line);g.drawRoundedRectangle(r.toFloat().reduced(.5f),2.5f,1);
        juce::Path arrow;const float ax=(float)r.getX()+7,ay=(float)r.getY()+5;
        if(open)arrow.addTriangle(ax,ay+2,ax+8,ay+2,ax+4,ay+8);else arrow.addTriangle(ax+1,ay,ax+1,ay+9,ax+7,ay+4.5f);g.setColour(ink);g.fillPath(arrow);
        text(g,title,r.withTrimmedLeft(22),11,ink);
    }
    static juce::String snapTypeName(ui::SnapType t)
    {
        switch(t){case ui::SnapType::grid:return i18n::tr("Rejilla");case ui::SnapType::events:return i18n::tr("Eventos");case ui::SnapType::cursor:return i18n::tr("Cursor");
            case ui::SnapType::gridCursor:return i18n::tr("Rejilla + Cursor");case ui::SnapType::eventsCursor:return i18n::tr("Eventos + Cursor");case ui::SnapType::eventsGridCursor:return i18n::tr("Eventos + Rejilla + Cursor");}
        return {};
    }
    static juce::String gridName(ui::GridType t)
    {
        switch(t){case ui::GridType::bar:return i18n::tr("Compás");case ui::GridType::beat:return i18n::tr("Tiempo");case ui::GridType::eighth:return "1/8";case ui::GridType::sixteenth:return "1/16";
            case ui::GridType::thirtySecond:return "1/32";case ui::GridType::adaptToZoom:return i18n::tr("Adaptar a zoom");case ui::GridType::ms10:return "10 ms";case ui::GridType::ms100:return "100 ms";case ui::GridType::ms1000:return "1000 ms";}
        return {};
    }
    void paintStatusLine(juce::Graphics& g)
    {
        // Línea de estado (manual p. 73): connection state, record time, format, pan law, buffer.
        statusFields.clear();if(statusRect.isEmpty())return;
        auto* device=engine.deviceManager.getCurrentAudioDevice();
        const bool ins=device&&device->getActiveInputChannels().countNumberOfSetBits()>0,outs=device&&device->getActiveOutputChannels().countNumberOfSetBits()>0;
        const double rate=engine.currentSampleRate(),now=juce::Time::getMillisecondCounterHiRes()*.001;
        if(now-freeBytesTime>5.0){freeBytes=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getBytesFreeOnVolume();freeBytesTime=now;}
        const double seconds=(double)freeBytes/(rate*3.0*2.0);
        const juce::String recordTime=juce::String((int)(seconds/3600.0))+i18n::tr(" horas ")+juce::String((int)std::fmod(seconds/60.0,60.0))+" min";
        struct Field{juce::String label,value;juce::Colour colour;int action;};
        const Field fields[]{{i18n::tr("Entradas audio"),ins?i18n::tr("Conectadas"):i18n::tr("No conectadas"),ins?ink:coral,1},{i18n::tr("Salidas audio"),outs?i18n::tr("Conectadas"):i18n::tr("No conectadas"),outs?ink:coral,1},
            {i18n::tr("Tiempo de grabación máx."),recordTime,ink,0},{i18n::tr("Formato de grabación"),juce::String(rate/1000.0,1)+" kHz - "+(engine.recordBitDepth.load()==32?i18n::tr("32 bit flotante"):juce::String(engine.recordBitDepth.load())+" bit"),ink,2},
            {i18n::tr("Pan law de proyecto"),"-3 dB",ink,0},{i18n::tr("Tamaño de búfer"),juce::String(engine.currentBlockSize()),ink,1}};
        int x=8;
        for(const auto& field:fields)
        {
            const int lw=(int)textWidth(field.label,10.0f)+10,vw=(int)textWidth(field.value,10.0f)+12;
            const juce::Rectangle<int> box(x,statusRect.getY()+3,lw+vw,statusRect.getHeight()-6);if(box.getRight()>owner.getWidth()-8)break;
            g.setColour(raised.withAlpha(.6f));g.fillRoundedRectangle(box.toFloat(),3.0f);
            text(g,field.label,box.withWidth(lw).withTrimmedLeft(6),10,muted);text(g,field.value,box.withTrimmedLeft(lw),10,field.colour);
            statusFields.push_back({box,field.action});x=box.getRight()+6;
        }
    }
    void paint(juce::Graphics& g)
    {
        g.fillAll(bg);const int w=owner.getWidth(),h=owner.getHeight(),top=inspectorRect.getY();
        g.setColour(panel);g.fillRect(0,0,w,41);for(auto r:{statusRect,infoRect,channelRect,inspectorRect,browserRect,lowerRect})g.fillRect(r);g.fillRect(0,transportRect.isEmpty()?h-23:transportRect.getY(),w,h);
        g.setColour(line.brighter(.35f));for(int x:{44,114,180,222,520,562,808,w-222,w-178})g.drawVerticalLine(x,10.0f,32.0f);
        g.setColour(line);g.drawHorizontalLine(40,0,(float)w);g.drawHorizontalLine(h-25,0,(float)w);
        if(!statusRect.isEmpty())g.drawHorizontalLine(statusRect.getBottom()-1,0,(float)w);
        if(!infoRect.isEmpty())g.drawHorizontalLine(infoRect.getBottom()-1,0,(float)w);
        if(!transportRect.isEmpty())g.drawHorizontalLine(transportRect.getY(),0,(float)w);
        if(showChannel)g.drawVerticalLine(channelRect.getRight(),(float)top,(float)channelRect.getBottom());
        if(showLeft)g.drawVerticalLine(inspectorRect.getRight(),(float)top,(float)inspectorRect.getBottom());
        if(showRight)g.drawVerticalLine(browserRect.getX(),(float)top,(float)browserRect.getBottom());
        if(showLower)g.drawHorizontalLine(lowerRect.getY(),(float)lowerRect.getX(),(float)lowerRect.getRight());
        // Each zone is drawn only when it lies in the area being repainted.
        const auto dirty=g.getClipBounds();
        if(dirty.intersects(statusRect))paintStatusLine(g);
        if(!infoRect.isEmpty()&&dirty.intersects(infoRect))
        {   // Línea de información (manual p. 74): the selected event, or «Sin objeto seleccionado».
            juce::String info=i18n::tr("Sin objeto seleccionado");
            if(auto t=currentTrack();t&&arrangement.selectedClip>=0&&arrangement.selectedClip<(int)t->clips.size())
            {
                const auto& c=t->clips[(size_t)arrangement.selectedClip];
                info=c.name+"   ·   "+i18n::tr("Inicio ")+juce::String(c.startSeconds,3)+" s   ·   "+i18n::tr("Duración ")+juce::String(c.durationSeconds,3)+" s   ·   "+i18n::tr("Ganancia ")+juce::String(juce::Decibels::gainToDecibels(c.gain),1)+" dB";
            }
            if(arrangement.hasRange&&arrangement.tool==2)
                info=i18n::tr("Inicio del rango ")+juce::String(arrangement.rangeStart,3)+" s   ·   "+i18n::tr("Fin del rango ")+juce::String(arrangement.rangeEnd,3)+" s   ·   "+i18n::tr("Duración del rango ")+juce::String(arrangement.rangeEnd-arrangement.rangeStart,3)+" s";
            text(g,info,infoRect,10,muted,juce::Justification::centred);
        }
        if(inspectorShown()&&dirty.intersects(inspectorRect))
        {
            auto t=currentTrack();
            if(t){g.setColour(t->colour);g.fillRoundedRectangle(insp.nameBar.toFloat(),3);text(g,t->name,insp.nameBar.reduced(8,0),12,activeInk);}
            // Small volume and pan icons, as at the start of Cubase's inspector rows.
            {juce::Path tri;const auto v=insp.volumeIcon.toFloat();tri.addTriangle(v.getX(),v.getCentreY()+5,v.getRight()-2,v.getCentreY()+5,v.getRight()-2,v.getCentreY()-5);g.setColour(muted);g.fillPath(tri);
             const auto p=insp.panIcon.toFloat();g.drawLine(p.getX(),p.getCentreY(),p.getRight()-2,p.getCentreY(),1.4f);g.fillEllipse(p.getCentreX()-3,p.getCentreY()-3,6,6);}
            const juce::String names[]{t&&t->isMidi?i18n::tr("Instrumento"):i18n::tr("Enrutado"),i18n::tr("Inserciones"),i18n::tr("Ecualizadores")};
            for(int n=0;n<3;++n)if(!insp.sections[(size_t)n].isEmpty())section(g,names[n],insp.sections[(size_t)n],sectionOpen[(size_t)n]);
            if(!insp.output.isEmpty()&&t){g.setColour(bg);g.fillRoundedRectangle(insp.output.toFloat(),3);text(g,juce::String::fromUTF8("⇥  ")+(t->output.isEmpty()?juce::String("Stereo Out"):(engine.findTrack(t->output)?engine.findTrack(t->output)->name:juce::String("Stereo Out"))),insp.output.reduced(8,0),11,ink);}
            if(!insp.eqLabels.isEmpty()){const int k=insp.eqLabels.getWidth()/3;const juce::String bands[]{i18n::tr("GRAVES"),i18n::tr("MEDIOS"),i18n::tr("AGUDOS")};for(int n=0;n<3;++n)text(g,bands[n],insp.eqLabels.withX(insp.eqLabels.getX()+n*k).withWidth(k),8,muted,juce::Justification::centred);}
        }
        else if(showLeft&&editorInspector&&!visibilityVisible&&dirty.intersects(inspectorRect))
        {
            auto area=inspectorRect.reduced(12).withTrimmedTop(36);
            if(showLower&&pianoVisible&&currentTrack()&&currentTrack()->isMidi){auto t=currentTrack();text(g,i18n::tr("Editor de teclas"),area.removeFromTop(22),12,ink);text(g,t->name,area.removeFromTop(20),11,t->colour);text(g,juce::String((int)t->notes.size())+i18n::tr(" notas"),area.removeFromTop(20),10,muted);}
            else{font(g,11);g.setColour(muted);g.drawFittedText(i18n::tr("El inspector del editor solo contiene información si hay un editor abierto en la zona inferior."),area.withHeight(60),juce::Justification::topLeft,4);}
        }
        if(showTransport&&dirty.intersects(transportRect))
        {
            const int ty=transportRect.getY();
            const int cx=w/2;
            {   // Musical position (bars. beats. sixteenths. ticks), next to the transport buttons.
                const double beats=engine.positionSeconds.load()*engine.tempo.load()/60.0;const int b=(int)std::floor(beats);
                const double sixteenths=(beats-b)*4.0;const int six=(int)std::floor(sixteenths),ticks=(int)std::floor((sixteenths-six)*120.0);
                const juce::Rectangle<int> box(cx-300,ty+10,150,26);g.setColour(bg);g.fillRoundedRectangle(box.toFloat(),3);
                text(g,juce::String(b/4+1)+".  "+juce::String(b%4+1)+".  "+juce::String(six+1)+".  "+juce::String(ticks).paddedLeft('0',3),box.reduced(10,0),13,ink,juce::Justification::centredRight);
            }
            text(g,"BPM",{cx+383,ty+13,34,25},10,muted);text(g,"4 / 4",{cx+468,ty+13,44,25},12,ink);
            const double bar=240.0/engine.tempo.load();
            // Left and right locators, as the boxes at the left of Cubase's transport bar.
            for(int k=0;k<2;++k){const juce::Rectangle<int> box(44+k*136,ty+10,130,26);g.setColour(bg);g.fillRoundedRectangle(box.toFloat(),3);text(g,(k==0?"L   ":"R   ")+juce::String((k==0?engine.loopStart.load():engine.loopEnd.load())/bar+1,2),box.reduced(10,0),12,k==0?ink:ink,juce::Justification::centredLeft);}
            {   // Beat lights: the bar's beats, the first one accented, lit while the transport runs.
                const double beats=engine.positionSeconds.load()*engine.tempo.load()/60.0;const int current=(int)std::floor(beats)%4;const float fade=1.0f-(float)(beats-std::floor(beats));
                if(w/2+570<w)for(int b=0;b<4;++b){const bool lit=engine.playing.load()&&b==current;auto colour=b==0?coral:mint;g.setColour(lit?colour.withAlpha(.35f+.65f*fade):raised);g.fillEllipse((float)(w/2+518)+b*12.0f,(float)ty+22.0f,b==0?8.0f:7.0f,b==0?8.0f:7.0f);} }
        }
        text(g,engine.recording?i18n::tr("● REC"):engine.playing?i18n::tr("● PLAY"):i18n::tr("● LISTO"),{w-112,h-24,100,23},10,engine.recording?coral:engine.playing?mint:muted,juce::Justification::centredRight);
    }
    void confirmDiscard(std::function<void()> action)
    {
        if(!dirty){action();return;}
        auto options=juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon).withTitle(i18n::tr("Guardar los cambios de la sesión")).withMessage(juce::String::fromUTF8("«")+engine.projectName+i18n::tr("» tiene cambios sin guardar.")).withButton(i18n::tr("Guardar")).withButton(i18n::tr("Descartar")).withButton(i18n::tr("Cancelar")).withAssociatedComponent(&owner);
        juce::AlertWindow::showAsync(options,[safe=juce::Component::SafePointer<MainComponent>(&owner),action=std::move(action)](int result)mutable{if(!safe)return;if(result==1)safe->ui->save(std::move(action));else if(result==2)action();});
    }
    void save(std::function<void()> completion={})
    {
        if(engine.recording){error(i18n::tr("Detén la grabación antes de guardar la sesión."));return;}
        if(engine.projectFile!=juce::File()) {auto r=engine.saveProject(engine.projectFile);if(r.failed()){error(r.getErrorMessage());return;}dirty=false;notice=i18n::tr("Sesión guardada: ")+engine.projectFile.getFileName();refresh();if(completion)completion();return;}
        afterSave=std::move(completion);chooser=std::make_unique<juce::FileChooser>(i18n::tr("Guardar sesión de Silente"),juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(engine.projectName+".silente"),"*.silente",true);
        chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe=juce::Component::SafePointer<MainComponent>(&owner)](const juce::FileChooser& fc){if(!safe)return;auto& self=*safe->ui;auto f=fc.getResult();if(f==juce::File()){self.afterSave={};return;}if(!f.hasFileExtension("silente"))f=f.withFileExtension("silente");self.engine.projectName=f.getFileNameWithoutExtension();auto r=self.engine.saveProject(f);if(r.failed()){self.afterSave={};self.error(r.getErrorMessage());return;}self.dirty=false;self.notice=i18n::tr("Sesión guardada: ")+f.getFileName();self.refresh();auto done=std::move(self.afterSave);self.afterSave={};if(done)done();});
    }
    void open()
    {
        confirmDiscard([this]{chooser=std::make_unique<juce::FileChooser>(i18n::tr("Abrir sesión de Silente"),juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.silente;*.json",true);chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<MainComponent>(&owner)](const juce::FileChooser& fc){if(!safe||fc.getResult()==juce::File())return;auto& self=*safe->ui;self.plugins.closeEditors();self.stopTransport();auto r=self.engine.loadProject(fc.getResult());if(r.failed()){self.error(r.getErrorMessage());return;}self.dirty=false;self.undoStack.clear();self.redoStack.clear();self.selected=0;self.arrangement.selectedClip=-1;self.arrangement.scrollSeconds=0;self.arrangement.firstTrack=0;self.notice=i18n::tr("Sesión abierta: ")+fc.getResult().getFileName();self.refresh();if(self.engine.lastWarning.isNotEmpty())self.error(self.engine.lastWarning);});});
    }
    void importPaths(const juce::StringArray& input,int trackIndex=-1,double at=0)
    {
        if(engine.recording){error(i18n::tr("Detén la grabación antes de importar archivos."));return;}
        // Videos: extract their audio track first (worker thread), then import the result.
        juce::StringArray paths,videos;for(auto& path:input)(io::isVideoFile(juce::File(path))?videos:paths).add(path);
        if(!videos.isEmpty())
        {
            auto extracted=std::make_shared<juce::StringArray>();
            ui::BackgroundJob::start(i18n::tr("Extrayendo audio del video…"),[videos,extracted](const std::function<bool(double)>& progress)
            {
                const auto dir=juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("Silente/Audio de video");dir.createDirectory();
                for(int n=0;n<videos.size();++n)
                {
                    const juce::File video(videos[n]);const auto out=dir.getNonexistentChildFile(video.getFileNameWithoutExtension(),".m4a",false);
                    if(auto result=io::extractAudioTrack(video,out);result.failed())return result;
                    extracted->add(out.getFullPathName());if(!progress((n+1.0)/videos.size()))return juce::Result::fail("cancel");
                }
                return juce::Result::ok();
            },[safe=juce::Component::SafePointer<MainComponent>(&owner),extracted,trackIndex,at](const juce::Result& result)
            {
                if(!safe)return;if(result.failed()&&result.getErrorMessage()!="cancel")safe->ui->error(result.getErrorMessage());
                if(!extracted->isEmpty())safe->ui->importPaths(*extracted,trackIndex,at);
            });
            if(paths.isEmpty())return;
        }
        pushUndo();int count=0;juce::StringArray errors;
        for(auto& path:paths){juce::File f(path);if(f.isDirectory())continue;juce::Result r=juce::Result::ok();if(f.hasFileExtension("mid;midi"))r=engine.importMidi(f);else if(f.hasFileExtension("silente")){errors.add(i18n::tr("Abre las sesiones con Archivo → Abrir."));continue;}else r=engine.importAudio(f,trackIndex,at);if(r.failed())errors.add(f.getFileName()+": "+r.getErrorMessage());else{++count;recentFiles.insert(0,f.getFileName());}}
        if(count){dirty=true;notice=juce::String(count)+i18n::tr(" archivo(s) importado(s).");if(trackIndex>=0)select(trackIndex);else{const juce::ScopedLock lock(engine.modelLock);select(juce::jmax(0,(int)engine.tracks.size()-1));}}refresh();if(!errors.isEmpty())error(errors.joinIntoString("\n"));
    }
    void importChooser()
    {
        chooser=std::make_unique<juce::FileChooser>(i18n::tr("Importar audio o MIDI"),juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),io::readableWildcard(engine.formatManager)+";*.mid;*.midi",true);chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::canSelectMultipleItems,[safe=juce::Component::SafePointer<MainComponent>(&owner)](const juce::FileChooser& fc){if(!safe)return;juce::StringArray paths;for(auto& f:fc.getResults())paths.add(f.getFullPathName());if(!paths.isEmpty())safe->ui->importPaths(paths);});
    }
    void exportChooser()
    {
        // «Exportar mezcla de audio»: mix and/or separate channels in WAV, AIFF, FLAC, MP3 or M4A.
        if(engine.recording){error(i18n::tr("Detén la grabación antes de exportar."));return;}
        exportWindow=std::make_unique<UtilityWindow>(i18n::tr("Silente · Exportar mezcla de audio"),new ui::ExportDialog(engine,selected),820,520);exportWindow->setLookAndFeel(&look);
    }
    void showConversion(bool video)
    {
        if(!conversionWindow){conversionWindow=std::make_unique<UtilityWindow>(i18n::tr("Silente · Conversión de formatos"),new ui::ConversionPanel(engine.formatManager),860,540);conversionWindow->setLookAndFeel(&look);}
        conversionWindow->setVisible(true);conversionWindow->toFront(true);
        if(video)if(auto* panel=dynamic_cast<ui::ConversionPanel*>(conversionWindow->getContentComponent()))panel->addFiles(true);
    }
    void openChannelSettings(int index)
    {
        if(index<0||index>=(int)engine.tracks.size())return;
        auto* content=new ChannelPanel(engine,plugins,index);content->changed=[this]{markDirty();};
        channelWindow=std::make_unique<UtilityWindow>(i18n::tr("Silente · Configuración de canal"),content,900,350);channelWindow->setLookAndFeel(&look);
    }
    ui::MixConsoleHooks consoleHooks()
    {
        ui::MixConsoleHooks hooks;
        hooks.beforeEdit=[this]{pushUndo(i18n::tr("Cambio en MixConsole"));};hooks.changed=[this]{markDirty();};
        hooks.select=[this](int i){select(i);};hooks.selected=[this]{return selected;};
        hooks.openChannelSettings=[this](int i){openChannelSettings(i);};
        hooks.key=[this](const juce::KeyPress& k){return owner.keyPressed(k);};
        return hooks;
    }
    void showMixConsole()
    {
        if(!mixConsoleWindow)mixConsoleWindow=std::make_unique<ui::MixConsoleWindow>(engine,plugins,consoleHooks());
        mixConsoleWindow->setVisible(true);mixConsoleWindow->toFront(true);
    }
    // Key commands (Cubase manual pp. 1609–1628): command id → action. Ids come from app::keyCommands().
    std::map<juce::String,std::function<void()>> keyActions;
    std::optional<std::pair<AudioClip,int>> clipboard; // copied event and its source track
    std::shared_ptr<Track> selectedTrackPtr(){return currentTrack();}
    AudioClip* selectedClipPtr(){auto t=currentTrack();if(!t||!juce::isPositiveAndBelow(arrangement.selectedClip,(int)t->clips.size()))return nullptr;return &t->clips[(size_t)arrangement.selectedClip];}
    double barSeconds()const{return 240.0/juce::jlimit(20.0,400.0,engine.tempo.load());}
    void selectVisible(int offset,bool absolute)
    {
        const auto v=arrangement.visibleTracks();if(v.empty())return;
        int at=(int)(std::find(v.begin(),v.end(),selected)-v.begin());if(at>=(int)v.size())at=0;
        at=absolute?(offset<0?0:(int)v.size()-1):juce::jlimit(0,(int)v.size()-1,at+offset);
        select(v[(size_t)at]);arrangement.selectedClip=-1;
        const int rows=juce::jmax(1,(arrangement.getHeight()-Arrangement::ruler)/arrangement.lane);
        if(at<arrangement.firstTrack)arrangement.firstTrack=at;else if(at>=arrangement.firstTrack+rows)arrangement.firstTrack=at-rows+1;
        refresh();
    }
    void locateEvent(bool next)
    {
        // N / B: next or previous event start or end on the visible tracks.
        const double now=engine.positionSeconds.load();double best=next?1.0e12:-1.0;
        for(int i:arrangement.visibleTracks())for(const auto& c:engine.tracks[(size_t)i]->clips)for(double edge:{c.startSeconds,c.startSeconds+c.durationSeconds})
            if(next?(edge>now+1.0e-4&&edge<best):(edge<now-1.0e-4&&edge>best))best=edge;
        if(next?best<1.0e11:best>=0.0)engine.seek(best);refresh();
    }
    void askValue(const juce::String& title,const juce::String& label,const juce::String& value,std::function<void(const juce::String&)> apply)
    {
        auto* a=new juce::AlertWindow(title,{},juce::MessageBoxIconType::NoIcon);a->addTextEditor("v",value,label);a->addButton(i18n::tr("Aplicar"),1,juce::KeyPress(juce::KeyPress::returnKey));a->addButton(i18n::tr("Cancelar"),0,juce::KeyPress(juce::KeyPress::escapeKey));
        a->enterModalState(true,juce::ModalCallbackFunction::create([a,apply](int result){if(result==1)apply(a->getTextEditorContents("v"));}),true);
    }
    double parseBars(const juce::String& text)const{auto parts=juce::StringArray::fromTokens(text.replace(",","."),". :",""); parts.removeEmptyStrings();const double bar=parts.size()>0?parts[0].getDoubleValue():1,beat=parts.size()>1?parts[1].getDoubleValue():1,six=parts.size()>2?parts[2].getDoubleValue():1;return juce::jmax(0.0,((bar-1)*4+(beat-1)+(six-1)/4.0)*60.0/engine.tempo.load());}
    void registerKeyActions()
    {
        auto& k=keyActions;
        k["startStop"]=[this]{togglePlay();};
        k["playSelection"]=[this]{if(auto* c=selectedClipPtr())playRange(c->startSeconds,c->startSeconds+c->durationSeconds);};
        k["enterRange"]=[this]{askValue(i18n::tr("Duración de rango de localizadores"),i18n::tr("Compases"),juce::String((engine.loopEnd.load()-engine.loopStart.load())/barSeconds(),2),[this](const juce::String& v){const double bars=v.replace(",",".").getDoubleValue();if(bars>0){pushUndo(i18n::tr("Rango de localizadores"));engine.loopEnd=engine.loopStart.load()+bars*barSeconds();markDirty();refresh();}});};
        k["autoScroll"]=[this]{autoScroll=!autoScroll;saveTransport();notice=autoScroll?i18n::tr("Desplazamiento automático activado"):i18n::tr("Desplazamiento automático desactivado");refresh();};
        k["stationary"]=[this]{stationary=!stationary;if(stationary)autoScroll=true;saveTransport();refresh();};k["stop"]=[this]{stopTransport();};k["record"]=[this]{toggleRecord();};
        k["cycle"]=[this]{engine.looping=!engine.looping.load();markDirty();refresh();};
        k["zoomInVertical"]=[this]{setTrackHeight((int)std::lround(arrangement.lane*1.25));saveLayout();};k["zoomOutVertical"]=[this]{setTrackHeight((int)std::lround(arrangement.lane/1.25));saveLayout();};
        k["punchIn"]=[this]{engine.punchIn=!engine.punchIn.load();markDirty();refresh();};k["punchOut"]=[this]{engine.punchOut=!engine.punchOut.load();markDirty();refresh();};
        k["metronome"]=[this]{toggleMetronome();};
        k["goStart"]=[this]{engine.seek(0);arrangement.scrollSeconds=0;refresh();};
        k["goLeft"]=[this]{engine.seek(engine.loopStart.load());refresh();};k["goRight"]=[this]{engine.seek(engine.loopEnd.load());refresh();};
        k["setLeft"]=[this]{pushUndo();engine.loopStart=engine.positionSeconds.load();if(engine.loopEnd<=engine.loopStart)engine.loopEnd=engine.loopStart+barSeconds();markDirty();refresh();};
        k["setRight"]=[this]{pushUndo();engine.loopEnd=juce::jmax(engine.loopStart.load()+0.01,engine.positionSeconds.load());markDirty();refresh();};
        k["forward"]=[this]{engine.seek(engine.positionSeconds.load()+barSeconds());refresh();};k["rewind"]=[this]{engine.seek(juce::jmax(0.0,engine.positionSeconds.load()-barSeconds()));refresh();};
        k["nextEvent"]=[this]{locateEvent(true);};k["previousEvent"]=[this]{locateEvent(false);};
        k["selectionStart"]=[this]{if(auto* c=selectedClipPtr()){engine.seek(c->startSeconds);refresh();}};
        k["locatorsToSelection"]=[this]{if(auto* c=selectedClipPtr()){pushUndo();engine.loopStart=c->startSeconds;engine.loopEnd=c->startSeconds+c->durationSeconds;markDirty();notice=i18n::tr("Localizadores en el evento seleccionado.");refresh();}};
        k["loopSelection"]=[this]{if(auto* c=selectedClipPtr()){engine.loopStart=c->startSeconds;engine.loopEnd=c->startSeconds+c->durationSeconds;engine.looping=true;engine.seek(c->startSeconds);if(!engine.playing)engine.play();refresh();}};
        k["enterLeft"]=[this]{showLoopDialog();};k["enterRight"]=[this]{showLoopDialog();};
        k["enterPosition"]=[this]{askValue(i18n::tr("Posición del cursor del proyecto"),i18n::tr("Compás. Tiempo. Semicorchea"),ui::SnapGrid::formatTime(engine.positionSeconds.load(),ui::TimeFormat::barsBeats,engine.tempo.load(),engine.currentSampleRate()).upToLastOccurrenceOf(".",false,false),[this](const juce::String& v){engine.seek(parseBars(v));refresh();});};
        k["enterTempo"]=[this]{askValue(i18n::tr("Introducir tempo"),"BPM",juce::String(engine.tempo.load(),3),[this](const juce::String& v){const double bpm=v.replace(",",".").getDoubleValue();if(bpm>=20&&bpm<=300){pushUndo();engine.tempo=bpm;markDirty();refresh();}});};
        k["tapTempo"]=[this]{showTapTempo();};
        k["undo"]=[this]{undo(false);};k["redo"]=[this]{undo(true);};
        k["delete"]=[this]{if(pianoVisible&&piano.hasKeyboardFocus(true))piano.deleteSelected();else if(rangeActive())rangeEdit(i18n::tr("Suprimir rango"),audio::deleteRange);else arrangement.deleteSelected();};
        k["duplicate"]=[this]{if(pianoVisible)duplicateNote();else if(rangeActive()){rangeEdit(i18n::tr("Duplicar rango"),audio::duplicateRange);const double len=arrangement.rangeEnd-arrangement.rangeStart;arrangement.rangeStart+=len;arrangement.rangeEnd+=len;arrangement.repaint();}else arrangement.duplicateSelected();};
        k["toolRange"]=[this]{setTool(2);};
        // Empujar bordes del rango (⌥← ⌥→ inicio, ⌥⇧← ⌥⇧→ final) by the grid value.
        const auto trim=[this](bool start,double sign)
        {
            if(!arrangement.hasRange)return;const double unit=arrangement.snapGrid.unitSeconds(engine.tempo.load(),arrangement.pixelsPerSecond);
            if(start)arrangement.rangeStart=juce::jlimit(0.0,arrangement.rangeEnd,arrangement.rangeStart+sign*unit);
            else arrangement.rangeEnd=juce::jmax(arrangement.rangeStart,arrangement.rangeEnd+sign*unit);
            arrangement.repaint();owner.repaint();
        };
        k["nudgeStartLeft"]=[trim]{trim(true,-1);};k["nudgeStartRight"]=[trim]{trim(true,1);};k["nudgeEndLeft"]=[trim]{trim(false,-1);};k["nudgeEndRight"]=[trim]{trim(false,1);};
        k["splitRange"]=[this]{rangeEdit(i18n::tr("Dividir rango"),[](std::vector<AudioClip>& c,std::vector<MidiNote>&,audio::TimeRange r,double){audio::splitRange(c,r);},false);};
        k["deleteTime"]=[this]{rangeEdit(i18n::tr("Suprimir intervalo de tiempo"),audio::deleteTime);};
        k["insertSilence"]=[this]{rangeEdit(i18n::tr("Insertar silencio"),audio::insertSilence);};
        k["truncateRange"]=[this]{rangeEdit(i18n::tr("Truncar"),[](std::vector<AudioClip>& c,std::vector<MidiNote>&,audio::TimeRange r,double){audio::truncate(c,r);});};
        k["adjustFades"]=[this]{rangeEdit(i18n::tr("Ajustar fundidos al rango"),[](std::vector<AudioClip>& c,std::vector<MidiNote>&,audio::TimeRange r,double){audio::adjustFadesToRange(c,r);});};k["repeat"]=[this]{arrangement.duplicateSelected();};
        k["splitAtCursor"]=[this]{arrangement.splitSelected();};
        k["copy"]=[this]{if(rangeActive()){copyRangeToClipboard();notice=i18n::tr("Rango copiado.");return;}if(auto* c=selectedClipPtr()){clipboard=std::make_pair(*c,selected);rangeClipboard.clear();notice=i18n::tr("Evento copiado.");}};
        k["cut"]=[this]{if(rangeActive()){copyRangeToClipboard();rangeEdit(i18n::tr("Cortar rango"),audio::deleteRange);notice=i18n::tr("Rango cortado.");return;}if(auto* c=selectedClipPtr()){clipboard=std::make_pair(*c,selected);rangeClipboard.clear();arrangement.deleteSelected();notice=i18n::tr("Evento cortado.");}};
        const auto paste=[this](bool atOrigin)
        {
            if(!rangeClipboard.empty()){pasteRangeClipboard(atOrigin);return;}
            auto t=currentTrack();if(!clipboard||!t||t->kind!=TrackKind::audio){notice=i18n::tr("Selecciona una pista de audio para pegar.");return;}
            pushUndo(i18n::tr("Pegar"));AudioClip c=clipboard->first;c.id=juce::Uuid().toString();if(!atOrigin)c.startSeconds=engine.positionSeconds.load();
            {const juce::ScopedLock lock(engine.modelLock);t->clips.push_back(c);}arrangement.selectedClip=(int)t->clips.size()-1;markDirty();refresh();
        };
        k["paste"]=[paste]{paste(false);};k["history"]=[this]{showHistory();};k["pasteOrigin"]=[paste]{paste(true);};
        k["moveToCursor"]=[this]{if(auto* c=selectedClipPtr()){pushUndo();const juce::ScopedLock lock(engine.modelLock);c->startSeconds=engine.positionSeconds.load();markDirty();refresh();}};
        k["muteTracks"]=[this]{if(auto t=currentTrack()){pushUndo(i18n::tr("Enmudecer"));t->mute=!t->mute.load();markDirty();refresh();}};
        k["soloTracks"]=[this]{if(auto t=currentTrack()){pushUndo(i18n::tr("Solo"));t->solo=!t->solo.load();markDirty();refresh();}};
        k["recordEnable"]=[this]{if(auto t=currentTrack();t&&t->hasEvents()){pushUndo(i18n::tr("Habilitar grabación"));t->armed=!t->armed.load();markDirty();refresh();}};
        k["snap"]=[this]{arrangement.snap=piano.snap=!arrangement.snap;snapButton.setToggleState(arrangement.snap,juce::dontSendNotification);refresh();};
        k["findTrack"]=[this]{if(arrangement.findTracks)arrangement.findTracks();};
        k["renameTrack"]=[this]{if(auto t=currentTrack())askValue(i18n::tr("Nombre de la pista"),{},t->name,[this,t](const juce::String& v){if(v.trim().isNotEmpty()){pushUndo();t->name=v.trim();markDirty();refresh();}});};
        k["renameEvent"]=[this]{if(auto* c=selectedClipPtr()){auto t=currentTrack();const int index=arrangement.selectedClip;askValue(i18n::tr("Nombre del evento"),{},c->name,[this,t,index](const juce::String& v){if(t&&juce::isPositiveAndBelow(index,(int)t->clips.size())&&v.trim().isNotEmpty()){pushUndo();t->clips[(size_t)index].name=v.trim();markDirty();refresh();}});}};
        k["open"]=[this]{if(auto t=currentTrack();t&&t->isMidi)setPiano(true);};k["openEditor"]=k["open"];
        k["colour"]=[this]{editTrackColour(selected,trackColour.getScreenBounds());};
        k["deleteTracks"]=[this]{if(currentTrack()){pushUndo(i18n::tr("Eliminar pista"));plugins.closeEditors();engine.removeTrack(selected);selected=juce::jmax(0,selected-1);markDirty();refresh();}};
        k["trackUp"]=[this]{selectVisible(-1,false);};k["trackDown"]=[this]{selectVisible(1,false);};k["trackTop"]=[this]{selectVisible(-1,true);};k["trackBottom"]=[this]{selectVisible(1,true);};
        const auto stepEvent=[this](int direction)
        {
            auto t=currentTrack();if(!t||t->clips.empty())return;std::vector<int> order((size_t)t->clips.size());std::iota(order.begin(),order.end(),0);
            std::sort(order.begin(),order.end(),[&t](int a,int b){return t->clips[(size_t)a].startSeconds<t->clips[(size_t)b].startSeconds;});
            int at=(int)(std::find(order.begin(),order.end(),arrangement.selectedClip)-order.begin());
            at=at>=(int)order.size()?(direction>0?0:(int)order.size()-1):juce::jlimit(0,(int)order.size()-1,at+direction);arrangement.selectedClip=order[(size_t)at];refresh();
        };
        k["eventLeft"]=[stepEvent]{stepEvent(-1);};k["eventRight"]=[stepEvent]{stepEvent(1);};
        const auto nudge=[this](double sign)
        {
            if(rangeActive())
            {   // ⌘← ⌘→ with a range: the range moves, its content stays.
                const double unit=arrangement.snapGrid.unitSeconds(engine.tempo.load(),arrangement.pixelsPerSecond),len=arrangement.rangeEnd-arrangement.rangeStart;
                arrangement.rangeStart=juce::jmax(0.0,arrangement.rangeStart+sign*unit);arrangement.rangeEnd=arrangement.rangeStart+len;arrangement.repaint();owner.repaint();return;
            }
            if(auto* c=selectedClipPtr()){pushUndo();const double unit=arrangement.snapGrid.unitSeconds(engine.tempo.load(),arrangement.pixelsPerSecond);const juce::ScopedLock lock(engine.modelLock);c->startSeconds=juce::jmax(0.0,c->startSeconds+sign*unit);markDirty();refresh();}
        };
        k["nudgeLeft"]=[nudge]{nudge(-1);};k["nudgeRight"]=[nudge]{nudge(1);};
        k["zoomIn"]=[this]{arrangement.zoom(1.25);};k["zoomOut"]=[this]{arrangement.zoom(1/1.25);};
        k["zoomFull"]=[this]{const double width=juce::jmax(100,arrangement.getWidth()-Arrangement::header);arrangement.pixelsPerSecond=juce::jlimit(12.0,480.0,width/juce::jmax(8.0,engine.duration()+2.0));arrangement.scrollSeconds=0;refresh();};
        k["zoomSelection"]=[this]{if(auto* c=selectedClipPtr()){const double width=juce::jmax(100,arrangement.getWidth()-Arrangement::header);arrangement.pixelsPerSecond=juce::jlimit(12.0,480.0,width/juce::jmax(0.25,c->durationSeconds*1.1));arrangement.scrollSeconds=juce::jmax(0.0,c->startSeconds-c->durationSeconds*0.05);refresh();}};
        k["zoneLeft"]=[this]{toggleZone(showLeft);};k["zoneChannel"]=[this]{toggleZone(showChannel);};k["zoneRight"]=[this]{toggleZone(showRight);};
        k["zoneLower"]=[this]{toggleZone(showLower);};k["zoneTransport"]=[this]{toggleZone(showTransport);};k["infoLine"]=[this]{toggleZone(showInfoLine);};
        k["toolGlue"]=[this]{setTool(4);};k["toolZoom"]=[this]{setTool(6);};k["toolMute"]=[this]{setTool(7);};k["toolPlay"]=[this]{setTool(9);};
        k["crossfade"]=[this]{arrangement.crossfadeSelected();};
        k["toolSelect"]=[this]{setTool(1);};k["toolSplit"]=[this]{setTool(3);};k["toolErase"]=[this]{setTool(5);};k["toolDraw"]=[this]{setTool(8);};
        k["toolColour"]=[this]{setTool(13);};
        k["mixConsole"]=[this]{showMixConsole();};k["mixConsoleLower"]=[this]{if(!showLower)toggleZone(showLower);setPiano(false);};
        k["audioConnections"]=[this]{owner.showAudioSettings();};k["vstInstruments"]=[this]{plugins.showInstrumentSelector(-1,nullptr);};
        k["addTrack"]=[this]{if(addTrackButton.onClick)addTrackButton.onClick();};
        k["new"]=[this]{owner.newProject();};k["openProject"]=[this]{open();};k["save"]=[this]{save();};
        k["saveAs"]=[this]{engine.projectFile=juce::File();save();};k["import"]=[this]{owner.importFiles();};
    }
    void showMetronomeSettings()
    {
        showSettingsPage(ui::SettingsWorkspace::Page::metronome);
    }
    void showTapTempo()
    {
        if(!tapWindow)
        {
            auto* panel=new ui::TapTempoPanel();
            panel->onApply=[this](double bpm){pushUndo();engine.tempo=bpm;markDirty();notice=i18n::tr("Tempo aplicado: ")+juce::String(bpm,1)+" BPM";refresh();};
            tapWindow=std::make_unique<UtilityWindow>(i18n::tr("Silente · Calculadora de tempo"),panel,420,330);tapWindow->setLookAndFeel(&look);
        }
        tapWindow->setVisible(true);tapWindow->toFront(true);if(auto* c=tapWindow->getContentComponent())c->grabKeyboardFocus();
    }
    void toggleMetronome(){engine.metronome=!engine.metronome.load();click.setToggleState(engine.metronome.load(),juce::dontSendNotification);notice=engine.metronome.load()?i18n::tr("Metrónomo activado"):i18n::tr("Metrónomo desactivado");markDirty();}
    void settingsDialog()
    {
        showSettingsPage(ui::SettingsWorkspace::Page::audio);
    }
    void showSettingsPage(ui::SettingsWorkspace::Page page)
    {
        if(!settingsWindow)
        {
            ui::SettingsHooks hooks;
            hooks.keyAvailable=[this](const juce::String& id){return keyActions.count(id)>0;};
            hooks.key=[this](const juce::KeyPress& key){return owner.keyPressed(key);};
            hooks.readScale=[] {return juce::Desktop::getInstance().getGlobalScaleFactor();};
            hooks.writeScale=[this](float value){if(owner.onInterfaceScaleChange)owner.onInterfaceScaleChange(value);};
            hooks.readTransport=[this]{return ui::TransportPreferences{startMode,returnToStart,usePreRoll,usePostRoll,autoScroll,stationary,preRollBars,postRollBars};};
            hooks.writeTransport=[this](const ui::TransportPreferences& state)
            {
                startMode=juce::jlimit(0,3,state.startMode);returnToStart=state.returnToStart;
                usePreRoll=state.preRoll;usePostRoll=state.postRoll;autoScroll=state.autoScroll;stationary=state.stationary;
                preRollBars=state.preRollBars;postRollBars=state.postRollBars;saveTransport();refresh();
            };
            settingsWindow=std::make_unique<UtilityWindow>(i18n::tr("Silente · Ajustes"),new ui::SettingsWorkspace(engine,std::move(hooks)),960,730);
            settingsWindow->setResizeLimits(760,540,2000,1600);settingsWindow->setLookAndFeel(&look);
        }
        auto* workspace=dynamic_cast<ui::SettingsWorkspace*>(settingsWindow->getContentComponent());
        workspace->setSearch({});workspace->showPage(page);
        settingsWindow->setVisible(true);settingsWindow->toFront(true);
        owner.fitSettingsToScreen();
    }
};

MainComponent::MainComponent(AudioEngine& engine,PluginManager& plugins):ui(std::make_unique<Impl>(*this,engine,plugins)){setSize(1440,900);refreshLanguage();startTimerHz(24);}
MainComponent::~MainComponent(){stopTimer();ui.reset();}
void MainComponent::paint(juce::Graphics& g){ui->paint(g);}
void MainComponent::mouseDown(const juce::MouseEvent& e){ui->statusClick(e.getPosition());}
void MainComponent::resized(){if(ui)ui->layout();}
juce::File MainComponent::settingsFolder(){return app::preferencesDirectory();}
void MainComponent::useSettingsFolder(const juce::File& folder){app::usePreferencesDirectory(folder);}
void MainComponent::timerCallback(){ui->refresh(true);}
void MainComponent::newProject(){ui->confirmDiscard([this]{ui->stopTransport();ui->plugins.closeEditors();ui->engine.clear();ui->selected=0;ui->arrangement.selectedClip=-1;ui->arrangement.scrollSeconds=0;ui->arrangement.firstTrack=0;ui->undoStack.clear();ui->redoStack.clear();ui->dirty=false;ui->notice=i18n::tr("Nueva sesión. Añade una pista o arrastra tus archivos.");ui->refresh();});}
void MainComponent::openProject(){ui->open();}
void MainComponent::saveProject(){ui->save();}
void MainComponent::importFiles(){ui->importChooser();}
void MainComponent::exportAudio(){ui->exportChooser();}
void MainComponent::showAudioSettings(){ui->settingsDialog();}
void MainComponent::showSettings(){ui->showSettingsPage(ui::SettingsWorkspace::Page::appearance);}
void MainComponent::fitSettingsToScreen()
{
    if(ui->settingsWindow)if(auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(ui->settingsWindow->getBounds()))
    {
        auto bounds=ui->settingsWindow->getBounds();const auto area=display->userArea.reduced(8);
        bounds.setSize(juce::jmin(bounds.getWidth(),area.getWidth()),juce::jmin(bounds.getHeight(),area.getHeight()));
        ui->settingsWindow->setBounds(bounds.constrainedWithin(area));
    }
}
void MainComponent::showMixer(){ui->setPiano(false);}
void MainComponent::toggleMetronome(){ui->toggleMetronome();}
void MainComponent::showMixConsole(){ui->showMixConsole();}
void MainComponent::showConversion(bool video){ui->showConversion(video);}
void MainComponent::showKeyCommands(){ui->showKeyCommands();}
juce::PopupMenu MainComponent::transportMenu(){return ui->transportMenu();}
void MainComponent::transportMenuItem(int id){ui->transportMenuItem(id);}
juce::PopupMenu MainComponent::audioMenu(){return ui->audioMenu();}
void MainComponent::audioMenuItem(int id){ui->audioMenuItem(id);}
juce::PopupMenu MainComponent::rangeMenu(){return ui->rangeMenu();}
void MainComponent::rangeMenuItem(int id){ui->rangeMenuItem(id);}
juce::StringArray MainComponent::editHistory()const{juce::StringArray names;for(const auto& s:ui->undoStack)names.add(s.action);return names;}
bool MainComponent::runCommand(const juce::String& id){if(auto a=ui->keyActions.find(id);a!=ui->keyActions.end()){a->second();return true;}return false;}
void MainComponent::showMetronomeSettings(){ui->showMetronomeSettings();}
void MainComponent::showTapTempo(){ui->showTapTempo();}
bool MainComponent::metronomeOn()const{return ui->engine.metronome.load();}
void MainComponent::addAudioTrack(){addTrack(TrackKind::audio);}
void MainComponent::addTrack(TrackKind kind){ui->pushUndo(i18n::tr("Añadir pista"));ui->engine.addTrack(kind);ui->select((int)ui->engine.tracks.size()-1);ui->markDirty();ui->refresh();}
void MainComponent::addMidiTrack(){ui->plugins.showInstrumentSelector();}
void MainComponent::refreshLanguage()
{
    i18n::translateControls(*this);
    ui->languageButton.setButtonText(i18n::language()==i18n::Language::spanish?juce::String::fromUTF8("Español"):juce::String("English"));
    ui->notice=ui->engine.tracks.empty()?i18n::tr("Nueva sesión. Añade una pista o arrastra tus archivos."):i18n::tr("Listo. Pulsa Espacio para escuchar la sesión.");
    if(ui->channelWindow){ui->channelWindow->setName(i18n::tr("Silente · Configuración de canal"));i18n::translateControls(*ui->channelWindow);}
    if(ui->settingsWindow){ui->settingsWindow->setName(i18n::tr("Silente · Ajustes"));if(auto* workspace=dynamic_cast<ui::SettingsWorkspace*>(ui->settingsWindow->getContentComponent()))workspace->refreshLanguage();}
    ui->refresh();
}
void MainComponent::showPlugins(){if(!ui->currentTrack())ui->plugins.showInstrumentSelector();else ui->plugins.showEffectSelector(ui->selected);}
void MainComponent::requestClose(std::function<void()> onReady){ui->stopTransport();ui->confirmDiscard(std::move(onReady));}
bool MainComponent::isInterestedInFileDrag(const juce::StringArray&){return true;}
void MainComponent::filesDropped(const juce::StringArray& paths,int,int){ui->importPaths(paths);}
bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if(dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent())!=nullptr)return false;
    if(key.getKeyCode()==juce::KeyPress::escapeKey){ui->engine.panic();return true;}
    if(const auto* command=app::findKeyCommand(key))
        if(auto action=ui->keyActions.find(command->id);action!=ui->keyActions.end()){action->second();return true;}
    return false;
}
}

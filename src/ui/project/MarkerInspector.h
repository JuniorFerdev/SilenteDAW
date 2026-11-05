#pragma once
#include "TimelineTracks.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace silente::ui
{
// Read-only presentation copies; edits carry track/marker UUIDs back to the coordinator.
class MarkerInspector final : public juce::Component,private juce::TableListBoxModel
{
public:
    MarkerInspector();
    void refreshLanguage();
    std::function<void(const juce::String&,const juce::String&,int)> act; // select, locate, cycle, zoom
    std::function<void(const juce::String&,const domain::Marker&)> edit;
    std::function<void(bool,bool)> add;
    std::function<void(const juce::String&)> activate;
    std::function<void(const juce::String&,const juce::String&)> rename;
    void showTrack(const juce::String& id,const juce::String& name,juce::Colour,const domain::TrackData&,const juce::String& selected,bool active,int format,double tempo,double rate);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    juce::Label name;
    juce::TextButton active,point,cycle;
    juce::TableListBox table;
    juce::String trackId,selection;
    std::vector<domain::Marker> rows;
    juce::Colour colour;
    bool syncing=false;
    int timeFormat=0;
    double bpm=120,sampleRate=48000;
    int getNumRows() override;
    void paintRowBackground(juce::Graphics&,int,int,int,bool) override;
    void paintCell(juce::Graphics&,int,int,int,int,bool) override;
    void cellClicked(int,int,const juce::MouseEvent&) override;
    void cellDoubleClicked(int,int,const juce::MouseEvent&) override;
    void selectedRowsChanged(int) override;
};

class MarkerInfoLine final : public juce::Component
{
public:
    MarkerInfoLine();
    std::function<void(const juce::String&,const domain::Marker&)> edit;
    void showMarker(const juce::String& track,const domain::Marker*,int format,double tempo,double rate);
    void paint(juce::Graphics&) override;
    void resized() override;
    void beginRename(){description.showEditor();}
private:
    juce::Label description,start,end;
    juce::String trackId;
    domain::Marker marker;
    int timeFormat=0;
    double bpm=120,sampleRate=48000;
    void commit(int field);
};
}

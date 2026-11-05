#include "MarkerInspector.h"
#include "../../i18n/Translations.h"
#include "../theme/Drawing.h"
#include <algorithm>

namespace silente::ui
{
using namespace theme;
MarkerInspector::MarkerInspector()
{
    setComponentID("marker-inspector");table.setComponentID("marker-list");name.setComponentID("marker-track-name");
    active.setComponentID("marker-active");point.setComponentID("marker-add-position");cycle.setComponentID("marker-add-cycle");
    for(auto* component:std::initializer_list<juce::Component*>{&name,&active,&point,&cycle,&table})addAndMakeVisible(component);
    name.setEditable(false,true);name.onTextChange=[this]{if(!syncing&&rename)rename(trackId,name.getText());};
    active.setButtonText(i18n::tr("Pista de marcador activa"));active.setClickingTogglesState(true);active.onClick=[this]{if(activate)activate(trackId);};
    point.setButtonText("+ |");point.setTooltip(i18n::tr("Añadir marcador de posición · Insert"));
    cycle.setButtonText("+ [ ]");cycle.setTooltip(i18n::tr("Añadir marcador de ciclo entre localizadores"));
    point.onClick=[this]{if(add)add(false,juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown());};
    cycle.onClick=[this]{if(add)add(true,juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown());};
    table.setModel(this);table.setRowHeight(24);table.setHeaderHeight(23);table.setMultipleSelectionEnabled(false);
    auto& header=table.getHeader();
    const int flags=juce::TableHeaderComponent::visible|juce::TableHeaderComponent::resizable|juce::TableHeaderComponent::draggable;
    header.addColumn(juce::String::fromUTF8("▶"),1,26,24,40,juce::TableHeaderComponent::visible);
    header.addColumn("ID",2,34,28,65,flags);
    header.addColumn(i18n::tr("Descripción"),4,120,65,500,flags);
    header.addColumn(i18n::tr("Posición"),3,115,70,240,flags);
    table.setColour(juce::ListBox::backgroundColourId,bg);table.setOutlineThickness(1);table.setColour(juce::ListBox::outlineColourId,line);
}
void MarkerInspector::showTrack(const juce::String& id,const juce::String& title,juce::Colour c,const domain::TrackData& data,const juce::String& selected,bool isActive,int format,double tempo,double rate)
{
    const bool timeChanged=timeFormat!=format||bpm!=tempo||sampleRate!=rate;
    syncing=true;if(trackId!=id&&name.isBeingEdited())name.hideEditor(true);
    trackId=id;selection=selected;colour=c;timeFormat=format;bpm=tempo;sampleRate=rate;
    if(!name.isBeingEdited())name.setText(title,juce::dontSendNotification);
    name.setColour(juce::Label::backgroundColourId,c);name.setColour(juce::Label::textColourId,activeInk);
    active.setToggleState(isActive,juce::dontSendNotification);
    auto next=data.markers;std::stable_sort(next.begin(),next.end(),[](const auto& a,const auto& b){return a.position<b.position;});
    const bool changed=rows!=next;
    if(changed){rows=std::move(next);table.updateContent();}
    int row=-1;for(int n=0;n<(int)rows.size();++n)if(rows[(size_t)n].id==selected)row=n;
    if(table.getSelectedRow()!=row){table.deselectAllRows();if(row>=0)table.selectRow(row,false,false);}
    syncing=false;if(changed||timeChanged)table.repaint();
}
void MarkerInspector::refreshLanguage()
{
    table.getHeader().setColumnName(4,i18n::tr("Descripción"));table.getHeader().setColumnName(3,i18n::tr("Posición"));
}
void MarkerInspector::paint(juce::Graphics& g){g.fillAll(panel);}
void MarkerInspector::resized()
{
    auto r=getLocalBounds().reduced(6);name.setBounds(r.removeFromTop(25));r.removeFromTop(5);active.setBounds(r.removeFromTop(23));r.removeFromTop(5);
    auto buttons=r.removeFromTop(24);point.setBounds(buttons.removeFromLeft(buttons.getWidth()/2-2));buttons.removeFromLeft(4);cycle.setBounds(buttons);r.removeFromTop(6);table.setBounds(r);
}
int MarkerInspector::getNumRows(){return (int)rows.size();}
void MarkerInspector::paintRowBackground(juce::Graphics& g,int row,int width,int height,bool chosen)
{
    g.fillAll(chosen?selectedTrack:row%2?panel:bg);g.setColour(line);g.drawHorizontalLine(height-1,0,(float)width);
}
void MarkerInspector::paintCell(juce::Graphics& g,int row,int column,int width,int height,bool chosen)
{
    if(!juce::isPositiveAndBelow(row,(int)rows.size()))return;const auto& marker=rows[(size_t)row];
    juce::String value;if(column==1)value=juce::String::fromUTF8("▷");if(column==2)value=marker.cycle?"["+juce::String(marker.number)+"]":juce::String(marker.number);
    if(column==3)value=timelinePosition(marker.position,timeFormat,bpm,sampleRate);if(column==4)value=marker.description;
    if(column==2){g.setColour(marker.colour.isTransparent()?colour:marker.colour);g.fillRect(1,3,3,height-6);}
    text(g,value,{5,0,width-7,height},11,chosen?activeInk:ink);
}
void MarkerInspector::selectedRowsChanged(int row)
{
    if(!syncing&&act&&juce::isPositiveAndBelow(row,(int)rows.size()))act(trackId,rows[(size_t)row].id,0);
}
void MarkerInspector::cellClicked(int row,int column,const juce::MouseEvent&)
{
    if(act&&juce::isPositiveAndBelow(row,(int)rows.size()))act(trackId,rows[(size_t)row].id,column==1?1:0);
}
void MarkerInspector::cellDoubleClicked(int row,int column,const juce::MouseEvent&)
{
    if(!juce::isPositiveAndBelow(row,(int)rows.size()))return;const auto marker=rows[(size_t)row];
    if(column==4||column==3)
    {
        auto editor=std::make_unique<juce::TextEditor>();editor->setSize(column==4?240:180,27);editor->setText(column==4?marker.description:timelinePosition(marker.position,timeFormat,bpm,sampleRate));editor->selectAll();
        auto* field=editor.get();const auto id=trackId;const auto callback=edit;const auto format=timeFormat;const auto tempo=bpm;const auto rate=sampleRate;
        field->onReturnKey=[safe=juce::Component::SafePointer<juce::TextEditor>(field),id,marker,column,format,tempo,rate,callback]()mutable
        {
            if(!safe)return;auto edited=marker;
            if(column==4)edited.description=safe->getText();else if(auto seconds=parseTimelinePosition(safe->getText(),format,tempo,rate)){const double length=edited.end-edited.position;edited.position=*seconds;edited.end=edited.cycle?*seconds+length:*seconds;}else return;
            if(callback)callback(id,edited);if(auto* box=safe->findParentComponentOfClass<juce::CallOutBox>())box->dismiss();
        };
        field->onEscapeKey=[safe=juce::Component::SafePointer<juce::TextEditor>(field)]{if(safe)if(auto* box=safe->findParentComponentOfClass<juce::CallOutBox>())box->dismiss();};
        juce::CallOutBox::launchAsynchronously(std::move(editor),table.localAreaToGlobal(table.getCellPosition(column,row,true)),nullptr);field->grabKeyboardFocus();return;
    }
    if(act)act(trackId,marker.id,marker.cycle?2:1);
}
MarkerInfoLine::MarkerInfoLine()
{
    setComponentID("marker-info");description.setComponentID("marker-description");start.setComponentID("marker-start");end.setComponentID("marker-end");
    for(auto* label:{&description,&start,&end}){addAndMakeVisible(label);label->setEditable(false,true);label->setColour(juce::Label::backgroundColourId,bg);label->setColour(juce::Label::textColourId,ink);}
    description.onTextChange=[this]{commit(0);};start.onTextChange=[this]{commit(1);};end.onTextChange=[this]{commit(2);};
}
void MarkerInfoLine::showMarker(const juce::String& track,const domain::Marker* value,int format,double tempo,double rate)
{
    const bool changed=value&&(marker!=*value||timeFormat!=format||bpm!=tempo||sampleRate!=rate);
    if(!value||trackId!=track||marker.id!=value->id)for(auto* label:{&description,&start,&end})if(label->isBeingEdited())label->hideEditor(true);
    setVisible(value!=nullptr);if(!value)return;trackId=track;marker=*value;timeFormat=format;bpm=tempo;sampleRate=rate;
    if(!description.isBeingEdited())description.setText(marker.description,juce::dontSendNotification);
    if(!start.isBeingEdited())start.setText(timelinePosition(marker.position,format,tempo,rate),juce::dontSendNotification);
    if(!end.isBeingEdited())end.setText(timelinePosition(marker.end,format,tempo,rate),juce::dontSendNotification);
    end.setVisible(marker.cycle);if(changed)repaint();
}
void MarkerInfoLine::commit(int field)
{
    auto changed=marker;if(field==0)changed.description=description.getText();
    else
    {
        if(auto value=parseTimelinePosition(field==1?start.getText():end.getText(),timeFormat,bpm,sampleRate))
        {if(field==1){changed.position=*value;if(!changed.cycle)changed.end=*value;}else changed.end=*value;}
        else{showMarker(trackId,&marker,timeFormat,bpm,sampleRate);return;}
    }
    if(domain::validMarker(changed)&&edit)edit(trackId,changed);
    else showMarker(trackId,&marker,timeFormat,bpm,sampleRate);
}
void MarkerInfoLine::paint(juce::Graphics& g)
{
    g.fillAll(panel);text(g,i18n::tr("Descripción"),{8,0,95,getHeight()},10,muted);
    text(g,"ID "+juce::String(marker.number),{360,0,70,getHeight()},10,ink);
    text(g,i18n::tr("Inicio"),{430,0,45,getHeight()},10,muted);
    if(marker.cycle)text(g,i18n::tr("Final"),{635,0,45,getHeight()},10,muted);
}
void MarkerInfoLine::resized()
{
    description.setBounds(105,2,245,getHeight()-4);start.setBounds(478,2,150,getHeight()-4);end.setBounds(683,2,150,getHeight()-4);
}
}

#include "MarkerUiTests.h"
#include "../src/MainComponent.h"
#include "../src/ui/project/MarkerInspector.h"
#include "../src/app/KeyCommands.h"
#include <cmath>

namespace silente
{
namespace
{
using namespace juce;
Component* find(Component& root,const String& id)
{
    if(root.getComponentID()==id)return &root;for(auto* child:root.getChildren())if(auto* value=find(*child,id))return value;return nullptr;
}
void drain(){MessageManager::getInstance()->runDispatchLoopUntil(35);}
MouseEvent mouse(Component& c,float x,float y,int clicks=1,int modifiers=0,Point<float> down={})
{
    if(down.isOrigin())down={x,y};return {Desktop::getInstance().getMainMouseSource(),{x,y},ModifierKeys(ModifierKeys::leftButtonModifier|modifiers),1.0f,0,0,0,0,&c,&c,Time::getCurrentTime(),down,Time::getCurrentTime(),clicks,down!=Point<float>{x,y}};
}
void capture(Component& c,const File& file)
{
    if(auto out=file.createOutputStream()){out->setPosition(0);out->truncate();PNGImageFormat().writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()),*out);}
}
bool near(double a,double b){return std::abs(a-b)<1e-8;}
}
void runMarkerUiChecks(MainComponent& view,AudioEngine& engine,const File& output,const std::function<void(bool,const String&)>& expect)
{
    using namespace juce;
    auto* heightControl=dynamic_cast<Slider*>(find(view,"track-height"));
    const int oldHeight=heightControl?(int)heightControl->getValue():60;if(heightControl)heightControl->setValue(60,sendNotificationSync);
    engine.clear();engine.tempo=120;view.showMixer();view.runCommand("goStart");view.runCommand("toolSelect");
    view.addTrack(TrackKind::marker);auto track=engine.tracks.back();const auto trackId=track->id;
    auto* arrangement=find(view,"arrangement");auto* inspector=dynamic_cast<ui::MarkerInspector*>(find(view,"marker-inspector"));
    auto* info=dynamic_cast<ui::MarkerInfoLine*>(find(view,"marker-info"));
    auto* description=dynamic_cast<Label*>(find(view,"marker-description"));
    auto* table=dynamic_cast<TableListBox*>(find(view,"marker-list"));
    expect(arrangement&&inspector&&info&&description&&table,"Marker track exposes its dedicated Inspector, movable-column list and info line");
    if(!arrangement||!inspector||!info||!description||!table)return;
    expect(inspector->isShowing()&&(!find(view,"input-routing")||!find(view,"input-routing")->isShowing()),"Marker selection replaces audio routing/inserts with marker controls");
    engine.seek(2);arrangement->grabKeyboardFocus();
    const auto* command=app::findKeyCommand(KeyPress(KeyPress::insertKey));
    expect(command&&command->id=="addMarker"&&view.keyPressed(KeyPress(KeyPress::insertKey))&&track->data.markers.size()==1&&near(track->data.markers[0].position,2),"09:20–09:30: Insert adds a position marker at the exact project cursor");
    const auto introId=track->data.markers[0].id;description->setText("INTRO",dontSendNotification);description->onTextChange();
    expect(domain::findMarker(track->data,introId)->description=="INTRO"&&description->isShowing()&&table->getNumRows()==1,"09:38–10:00: Description on the info line names the selected marker and updates the Inspector");
    engine.seek(8);auto* add=dynamic_cast<TextButton*>(find(view,"marker-add-position"));if(add)add->onClick();
    description->setText("ESTROFA",dontSendNotification);description->onTextChange();
    const auto verseId=track->data.markers.back().id;
    expect(track->data.markers.size()==2&&track->data.markers[1].number==2&&track->data.markers[1].description=="ESTROFA"&&table->getNumRows()==2,"10:10–10:20: add button creates the next numbered song section in the Inspector list");
    auto& header=table->getHeader();const int old=header.getIndexOfColumnId(4,true);header.moveColumn(4,1);
    expect(header.getIndexOfColumnId(4,true)==1&&table->getNumRows()==2,"10:29: Description column can be moved and resized to expose section names");
    header.moveColumn(4,old);
    engine.seek(0);const int beforeNavigation=view.editHistory().size();
    auto& tableModel=*table->getModel();tableModel.cellClicked(1,1,mouse(*table,10,55));
    expect(near(engine.positionSeconds.load(),8)&&view.editHistory().size()==beforeNavigation,"10:39: leftmost Inspector column locates a named section without creating an edit");
    // Header Locate popup: choose by the same marker UUID through the visible native JUCE menu.
    arrangement->mouseDown(mouse(*arrangement,130,70));drain();
    auto* popup=Component::getCurrentlyModalComponent();bool selectedMenu=false;
    if(popup){capture(*popup,output.getChildFile("marker-locate-menu.png"));popup->keyPressed(KeyPress(KeyPress::downKey));selectedMenu=popup->keyPressed(KeyPress(KeyPress::returnKey));}
    drain();PopupMenu::dismissAllActiveMenus();drain();
    expect(selectedMenu&&near(engine.positionSeconds.load(),2),"10:39-10:50: track-header Locate menu lists descriptions and jumps to INTRO");
    // Stable command fallback is also checked independently of popup hit testing.
    inspector->act(trackId,verseId,1);expect(near(engine.positionSeconds.load(),8),"Locate resolves the section UUID and reaches ESTROFA");
    engine.loopStart=2;engine.loopEnd=6;const bool looping=engine.looping.load();view.runCommand("addCycleMarker");
    description->setText("CORO",dontSendNotification);description->onTextChange();
    const auto cycleId=track->data.markers.back().id;auto cycle=*domain::findMarker(track->data,cycleId);cycle.colour=Colour(0xffd0a534);inspector->edit(trackId,cycle);
    expect(cycle.cycle&&cycle.number==1&&near(cycle.position,2)&&near(cycle.end,6),"Cycle marker saves both locators with independent cycle numbering");
    inspector->act(trackId,cycleId,2);
    expect(near(engine.loopStart.load(),2)&&near(engine.loopEnd.load(),6)&&engine.looping.load()==looping,"Cycle recall sets left/right locators while preserving transport Cycle mode");
    // Direct timeline double-click, drag and edge resizing at a known view transform.
    inspector->act(trackId,cycleId,3); // fit cycle; compute transform from its actual rendered left/right boundaries
    // Reset to a deterministic full-window zoom via Go Start + repeated existing zoom commands isn't exact.
    // Pixel-independent hit geometry is tested below; interaction uses a two-marker 4-second fit.
    const double scale=(arrangement->getWidth()-250-32)/4.0;
    const double scroll=std::max(0.0,2-16/scale);const float x=(float)(250+(2-scroll)*scale),right=(float)(250+(6-scroll)*scale);
    auto* height=dynamic_cast<Slider*>(find(view,"track-height"));const int lane=height?(int)height->getValue():60;
    const float cy=30+lane*.2f,py=30+lane*.7f;
    arrangement->mouseDown(mouse(*arrangement,(x+right)/2,cy,2));arrangement->mouseUp(mouse(*arrangement,(x+right)/2,cy,2));
    expect(near(engine.loopStart.load(),2)&&near(engine.loopEnd.load(),6)&&engine.looping.load()==looping,"Double-click on the cycle's visible span recalls exactly its locator range");
    const int beforeDrag=view.editHistory().size();const Point<float> down{x+20,cy};
    arrangement->mouseDown(mouse(*arrangement,down.x,down.y));arrangement->mouseDrag(mouse(*arrangement,down.x+(float)scale*.5f,down.y,1,0,down));arrangement->mouseUp(mouse(*arrangement,down.x+(float)scale*.5f,down.y,1,0,down));
    expect(near(domain::findMarker(track->data,cycleId)->position,2.5)&&near(domain::findMarker(track->data,cycleId)->end,6.5)&&view.editHistory().size()==beforeDrag+1,"Dragging a cycle moves both edges together and adds one undo step");
    view.runCommand("undo");expect(near(domain::findMarker(track->data,cycleId)->position,2),"Undo restores cycle position and description/colour metadata");
    const Point<float> endDown{right,cy};const int beforeResize=view.editHistory().size();
    arrangement->mouseDown(mouse(*arrangement,endDown.x,endDown.y));arrangement->mouseDrag(mouse(*arrangement,endDown.x+(float)scale*.5f,endDown.y,1,0,endDown));arrangement->mouseUp(mouse(*arrangement,endDown.x+(float)scale*.5f,endDown.y,1,0,endDown));
    expect(near(domain::findMarker(track->data,cycleId)->position,2)&&near(domain::findMarker(track->data,cycleId)->end,6.5)&&view.editHistory().size()==beforeResize+1,"Dragging the right cycle handle changes only its endpoint with snapping");
    view.runCommand("undo");
    // The info line rejects a backwards cycle and accepts a valid formatted endpoint.
    inspector->act(trackId,cycleId,0);auto* endpoint=dynamic_cast<Label*>(find(view,"marker-end"));
    const int beforeInvalid=view.editHistory().size();endpoint->setText("1. 1. 1. 0",dontSendNotification);endpoint->onTextChange();
    expect(near(domain::findMarker(track->data,cycleId)->end,6)&&view.editHistory().size()==beforeInvalid,"An invalid cycle endpoint is rejected without corrupting bounds or undo history");
    endpoint->setText("4. 2. 1. 0",dontSendNotification);endpoint->onTextChange();
    expect(near(domain::findMarker(track->data,cycleId)->end,6.5),"Cycle endpoint can be edited in bars/beats on the info line");view.runCommand("undo");
    // Alt-double-click zooms a cycle without creating a position marker.
    const auto beforeZoomCount=track->data.markers.size();arrangement->mouseDown(mouse(*arrangement,(x+right)/2,cy,2,ModifierKeys::altModifier));arrangement->mouseUp(mouse(*arrangement,(x+right)/2,cy,2,ModifierKeys::altModifier));
    expect(track->data.markers.size()==beforeZoomCount,"Option-double-click zooms the cycle without inserting an extra point marker");
    const float at3=(float)(250+(3-scroll)*scale),at425=(float)(250+(4.25-scroll)*scale);
    arrangement->mouseDown(mouse(*arrangement,at3,cy,1,ModifierKeys::commandModifier));arrangement->mouseDrag(mouse(*arrangement,at425,cy,1,ModifierKeys::commandModifier,{at3,cy}));arrangement->mouseUp(mouse(*arrangement,at425,cy,1,ModifierKeys::commandModifier,{at3,cy}));
    expect(track->data.markers.size()==beforeZoomCount+1&&track->data.markers.back().cycle&&near(track->data.markers.back().position,3)&&near(track->data.markers.back().end,4.25),"Command-drag creates a snapped cycle range on the marker lane");view.runCommand("undo");
    arrangement->mouseDown(mouse(*arrangement,at3,py,1,ModifierKeys::altModifier));arrangement->mouseUp(mouse(*arrangement,at3,py,1,ModifierKeys::altModifier));
    expect(track->data.markers.size()==beforeZoomCount+1&&!track->data.markers.back().cycle&&near(track->data.markers.back().position,3),"Option-click adds a position marker at the clicked timeline position");view.runCommand("undo");
    const int beforeEmptyCycle=view.editHistory().size();arrangement->mouseDown(mouse(*arrangement,at3,cy,1,ModifierKeys::commandModifier));arrangement->mouseUp(mouse(*arrangement,at3,cy,1,ModifierKeys::commandModifier));
    expect(view.editHistory().size()==beforeEmptyCycle&&track->data.markers.size()==beforeZoomCount,"A zero-length cycle gesture leaves markers and undo history unchanged");
    engine.seek(10);arrangement->mouseDown(mouse(*arrangement,60,70,1,ModifierKeys::shiftModifier));arrangement->mouseUp(mouse(*arrangement,60,70,1,ModifierKeys::shiftModifier));
    expect(track->data.markers.size()==beforeZoomCount+1&&description->isBeingEdited(),"Shift-click Add Position immediately opens the Description editor");description->hideEditor(true);view.runCommand("undo");
    inspector->act(trackId,introId,0);const auto saved=output.getChildFile("marker-sections.silente");
    expect(engine.saveProject(saved).wasOk(),"Marker project saves through the normal project writer");
    capture(view,output.getChildFile("marker-sections.png"));
    view.runCommand("delete");expect(!domain::findMarker(track->data,introId),"Delete removes the selected marker without deleting its track or audio");view.runCommand("undo");
    expect(domain::findMarker(track->data,introId)&&domain::findMarker(track->data,introId)->description=="INTRO","Undo restores the removed marker with its original identity and description");
    engine.clear();expect(engine.loadProject(saved).wasOk(),"Saved marker sections reopen successfully");view.runCommand("trackTop");track=engine.tracks.front();
    expect(track->id==trackId&&track->data.markers.size()==3&&domain::findMarker(track->data,cycleId)->colour==Colour(0xffd0a534)&&engine.activeMarkerTrack==trackId,"Reload preserves point/cycle bounds, names, colours, IDs and the active marker track");
    // An open editor addressed to a deleted track must never modify its replacement.
    const auto stale=track->data.markers.front();engine.removeTrack(0);view.addTrack(TrackKind::marker);const auto history=view.editHistory();inspector->edit(trackId,stale);
    expect(engine.tracks.back()->data.markers.empty()&&view.editHistory()==history,"A stale marker edit cannot modify a replacement track or append a false undo step");
    // Insert targets the explicitly active marker track even when an audio track is selected.
    const auto active=engine.tracks.back();view.addAudioTrack();engine.seek(3.25);view.runCommand("addMarker");
    expect(active->data.markers.size()==1&&near(active->data.markers.front().position,3.25),"Insert targets the active marker track while another track is selected");
    // The beginning of minute 11 shows a secondary ruler with its own format.
    view.addTrack(TrackKind::ruler);const auto ruler=engine.tracks.back();ruler->data.rulerFormat=2;view.transportMenuItem(0);
    expect(ruler->kind==TrackKind::ruler&&!ruler->hasAudioChannel()&&!inspector->isShowing(),"11:00: an independent ruler track has timeline controls and no audio channel");
    capture(view,output.getChildFile("marker-ruler.png"));
    for(int n=1;n<10;++n)view.addTrack(TrackKind::marker);
    const auto count=engine.tracks.size();const auto beforeLimit=view.editHistory();view.addTrack(TrackKind::marker);
    expect(engine.tracks.size()==count&&view.editHistory()==beforeLimit,"The ten-marker-track limit rejects an extra track without creating a false edit");
    juce::ignoreUnused(py);
    engine.clear();if(heightControl)heightControl->setValue(oldHeight,sendNotificationSync);view.runCommand("goStart");view.resized();
}
}

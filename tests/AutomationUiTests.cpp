#include "AutomationUiTests.h"
#include "../src/MainComponent.h"
#include "../src/ui/automation/AutomationPanel.h"
#include "../src/ui/automation/AutomationLane.h"

namespace silente
{
namespace
{
using namespace juce;
Component* find(Component& root,const String& id)
{
    if(root.getComponentID()==id)return &root;for(auto* child:root.getChildren())if(auto* value=find(*child,id))return value;return nullptr;
}
}
void runAutomationUiChecks(MainComponent& view,AudioEngine& engine,const std::function<void(bool,const String&)>& expect)
{
    engine.clear();engine.automationSuspendRead=false;engine.automationSuspendWrite=false;
    auto a=engine.addAudioTrack("Voz");auto b=engine.addAudioTrack("Guitarra");
    expect(view.runCommand("openAutomationPanel"),"automation panel: the F6 command exists");
    ui::AutomationPanel* panel=nullptr;
    for(int n=0;n<Desktop::getInstance().getNumComponents()&&!panel;++n)panel=dynamic_cast<ui::AutomationPanel*>(find(*Desktop::getInstance().getComponent(n),"automation-panel"));
    expect(panel!=nullptr&&panel->isShowing(),"automation panel: window opens");if(!panel)return;
    expect(panel->rowCount()==2&&panel->rowTrack(1)==1,"automation panel: one row per track");
    // Mode per track: combo box -> command, one undo step each.
    const auto steps=view.editHistory().size();
    if(auto* mode=dynamic_cast<ComboBox*>(find(*panel,"automation-mode-0")))mode->setSelectedId(5,sendNotificationSync);
    expect(a->automation.mode()==domain::AutomationMode::latch&&view.editHistory().size()==steps+1,"automation panel: the mode combo sets Latch with one undo step");
    panel->setMode(1,domain::AutomationMode::touch);
    expect(b->automation.mode()==domain::AutomationMode::touch&&a->automation.mode()==domain::AutomationMode::latch,"automation panel: modes are per track");
    panel->setMode(1,domain::AutomationMode::read);
    expect(b->automation.read&&!b->automation.write,"automation panel: Read");
    panel->setMode(1,domain::AutomationMode::off);
    expect(!b->automation.read&&!b->automation.write&&b->automation.empty(),"automation panel: Off clears Read and Write");
    // «Mostrar» opens the parameter's lane of that track.
    if(auto* pan=dynamic_cast<TextButton*>(find(*panel,"automation-show-pan-1")))pan->onClick();
    auto* lane=dynamic_cast<ui::AutomationLane*>(find(view,"automation-lane"));
    expect(lane!=nullptr&&lane->track==1&&lane->target==domain::AutomationTarget{domain::AutomationParameter::pan},"automation panel: Show pan selects the track and its pan lane");
    if(auto* volume=dynamic_cast<TextButton*>(find(*panel,"automation-show-volume-0")))volume->onClick();
    expect(lane&&lane->track==0&&lane->target==domain::AutomationTarget{domain::AutomationParameter::volume},"automation panel: Show volume selects the first track");
    // Cross-Over is the sixth entry of the mode combo.
    auto steps2=view.editHistory().size();
    if(auto* mode=dynamic_cast<ComboBox*>(find(*panel,"automation-mode-1")))mode->setSelectedId(6,sendNotificationSync);
    expect(b->automation.mode()==domain::AutomationMode::crossOver&&view.editHistory().size()==steps2+1,"automation panel: the mode combo sets Cross-Over with one undo step");
    // Trim follows the row's parameter: pan has none, volume does; one undo step, undo restores it.
    expect(panel->rowTarget(1)==domain::AutomationTarget{domain::AutomationParameter::pan},"automation panel: Show pan makes pan the row's parameter");
    auto* trim1=dynamic_cast<Slider*>(find(*panel,"automation-trim-1"));auto* trim0=dynamic_cast<Slider*>(find(*panel,"automation-trim-0"));
    expect(trim1&&!trim1->isEnabled()&&trim0&&trim0->isEnabled(),"automation panel: the Trim control is off for pan and on for volume");
    expect(!panel->setTrim(1,2.0)&&b->automation.volumeTrim==1.0,"automation panel: no Trim for pan");
    steps2=view.editHistory().size();
    expect(panel->setTrim(0,2.0)&&a->automation.volumeTrim==2.0&&view.editHistory().size()==steps2+1,"automation panel: Trim of volume with one undo step");
    expect(trim0&&std::abs(trim0->getValue()-6.0)<0.06,"automation panel: the Trim control shows +6 dB");
    expect(!panel->setTrim(0,2.0)&&view.editHistory().size()==steps2+1,"automation panel: an unchanged Trim adds no undo step");
    view.runCommand("undo");
    expect(engine.tracks[0]->automation.volumeTrim==1.0,"automation panel: undo restores the Trim");
    if(trim0){trim0->setValue(-6.0,sendNotificationSync);}
    expect(std::abs(engine.tracks[0]->automation.volumeTrim-Decibels::decibelsToGain(-6.0))<1e-9,"automation panel: the Trim control writes the factor");
    view.runCommand("undo");
    // Fill acts on the selected row's parameter with one undo step; undo removes it.
    engine.tracks[0]->gain=0.5f;engine.positionSeconds=2.0;panel->selectRow(0);panel->refresh();
    auto* label=dynamic_cast<Label*>(find(*panel,"automation-fill-target"));
    expect(panel->selectedRow()==0&&label&&label->getText().contains("Voz"),"automation panel: the Fill target shows the selected track and parameter");
    auto* toPunch=dynamic_cast<TextButton*>(find(*panel,"automation-fill-0"));auto* toStart=dynamic_cast<TextButton*>(find(*panel,"automation-fill-1"));
    expect(toPunch&&!toPunch->isEnabled()&&toStart&&toStart->isEnabled(),"automation panel: To punch is off without a pass, To start is on");
    steps2=view.editHistory().size();
    if(toStart)toStart->onClick();
    const auto& filled=engine.tracks[0]->automation.volume.points;
    expect(filled.size()==2&&filled[0].beat==0.0&&filled[0].value==0.5&&filled[1].value==0.5&&filled[1].beat>0.0&&view.editHistory().size()==steps2+1,"automation panel: To start fills with the control value in one undo step");
    expect(!panel->fill(domain::AutomationFill::toStart)&&view.editHistory().size()==steps2+1,"automation panel: a fill that changes nothing adds no undo step");
    view.runCommand("undo");
    expect(engine.tracks[0]->automation.volume.empty(),"automation panel: undo removes the fill");
    engine.positionSeconds=0.0;
    // Suspend read / write are global and not part of the project.
    if(auto* read=dynamic_cast<TextButton*>(find(*panel,"automation-suspend-read"))){read->setToggleState(true,dontSendNotification);read->onClick();}
    expect(engine.automationSuspendRead.load()&&!engine.automationSuspendWrite.load(),"automation panel: Suspend read sets the global flag");
    if(auto* write=dynamic_cast<TextButton*>(find(*panel,"automation-suspend-write"))){write->setToggleState(true,dontSendNotification);write->onClick();}
    expect(engine.automationSuspendWrite.load(),"automation panel: Suspend write sets the global flag");
    engine.automationSuspendRead=false;engine.automationSuspendWrite=false;panel->refresh();
}
}

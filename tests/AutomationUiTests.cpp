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
    // Suspend read / write are global and not part of the project.
    if(auto* read=dynamic_cast<TextButton*>(find(*panel,"automation-suspend-read"))){read->setToggleState(true,dontSendNotification);read->onClick();}
    expect(engine.automationSuspendRead.load()&&!engine.automationSuspendWrite.load(),"automation panel: Suspend read sets the global flag");
    if(auto* write=dynamic_cast<TextButton*>(find(*panel,"automation-suspend-write"))){write->setToggleState(true,dontSendNotification);write->onClick();}
    expect(engine.automationSuspendWrite.load(),"automation panel: Suspend write sets the global flag");
    engine.automationSuspendRead=false;engine.automationSuspendWrite=false;panel->refresh();
}
}

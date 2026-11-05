#include "ToolboxUiTests.h"
#include "../src/MainComponent.h"
#include "../src/app/Preferences.h"
#include "../src/ui/project/ProjectToolbox.h"

namespace silente
{
namespace
{
using namespace juce;
Component* find(Component& root,const String& id)
{
    if(root.getComponentID()==id)return &root;
    for(auto* child:root.getChildren())if(auto* result=find(*child,id))return result;
    return nullptr;
}
Component* desktop(const String& id)
{
    for(int n=0;n<Desktop::getInstance().getNumComponents();++n)if(auto* c=find(*Desktop::getInstance().getComponent(n),id))return c;
    return nullptr;
}
MouseEvent click(Component& c,float x,float y,int modifiers)
{
    return {Desktop::getInstance().getMainMouseSource(),{x,y},ModifierKeys(modifiers),1.0f,0,0,0,0,&c,&c,Time::getCurrentTime(),{x,y},Time::getCurrentTime(),1,false};
}
void drain(){MessageManager::getInstance()->runDispatchLoopUntil(30);}
void capture(Component& c,const File& file)
{
    if(auto out=file.createOutputStream()){out->setPosition(0);out->truncate();PNGImageFormat().writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()),*out);}
}
void closePopup(Component* c)
{
    if(c)if(auto* box=c->findParentComponentOfClass<CallOutBox>())box->dismiss();
    drain();
}
}
void runToolboxUiChecks(MainComponent& view,AudioEngine& engine,const juce::File& output,const std::function<void(bool,const juce::String&)>& expect)
{
    using namespace juce;
    const bool oldPreference=app::showToolboxOnRightClick();
    expect(app::saveShowToolboxOnRightClick(false).wasOk()&&!app::showToolboxOnRightClick(),"Toolbox preference saves and loads disabled");
    expect(app::saveShowToolboxOnRightClick(true).wasOk()&&app::showToolboxOnRightClick(),"Toolbox preference saves and loads enabled");
    bool modifiers=true;
    for(int key:{ModifierKeys::shiftModifier,ModifierKeys::altModifier,ModifierKeys::commandModifier,ModifierKeys::ctrlModifier})
        modifiers&=!ui::opensProjectToolbox(true,ModifierKeys(ModifierKeys::rightButtonModifier|key))&&ui::opensProjectToolbox(false,ModifierKeys(ModifierKeys::rightButtonModifier|key));
    expect(modifiers&&ui::opensProjectToolbox(true,ModifierKeys(ModifierKeys::rightButtonModifier))&&!ui::opensProjectToolbox(false,ModifierKeys(ModifierKeys::rightButtonModifier)),"Right-click and each keyboard modifier alternate toolbox and context menu");
    expect(ui::opensProjectToolbox(true,ModifierKeys(ModifierKeys::leftButtonModifier|ModifierKeys::ctrlModifier)),"Mac Ctrl-left-click is treated as a context-click gesture");
    int selected=0;ui::ProjectToolbox standalone(3,[&](int id){selected=id;});
    bool available=true,disabled=true;int count=0;
    for(const auto& definition:ui::projectToolDefinitions())
    {
        auto* button=dynamic_cast<TextButton*>(find(standalone,"project-toolbox-tool-"+String(definition.id)));
        if(definition.available()){++count;available&=button&&button->isEnabled()&&(bool)button->onClick;}
        else disabled&=button&&!button->isEnabled()&&!button->onClick;
    }
    expect(ui::projectToolDefinitions().size()==13&&count==10&&available&&disabled,"Toolbox lists the manual's thirteen tools; ten work and Comp/Time Warp/Line are explicitly disabled");
    auto* scissors=dynamic_cast<TextButton*>(find(standalone,"project-toolbox-tool-3"));
    expect(scissors&&scissors->getToggleState(),"Toolbox indicates the active tool");
    if(scissors&&scissors->onClick)scissors->onClick();expect(selected==3,"Toolbox dispatches the chosen tool ID");
    capture(standalone,output.getChildFile("project-toolbox.png"));

    auto* arrangement=find(view,"arrangement");if(!arrangement){expect(false,"Arrangement exists for toolbox checks");return;}
    engine.clear();view.resized();view.runCommand("toolSelect");
    // Flush earlier synthetic gestures before creating the first native popup.
    view.getTopLevelComponent()->toFront(true);drain();
    arrangement->mouseDown(click(*arrangement,330,50,ModifierKeys::rightButtonModifier));drain();
    auto* popup=desktop("project-toolbox");expect(popup!=nullptr,"Right-click opens the toolbox even with an empty project");closePopup(popup);

    auto track=engine.addAudioTrack("Toolbox fixture");track->colour=Colour(0xff138b30);
    AudioClip clip;clip.name="Colour target";clip.audio=std::make_shared<AudioBuffer<float>>(2,96000);clip.audio->clear();clip.sourceRate=48000;clip.durationSeconds=2;
    const auto clipId=clip.id;track->clips.push_back(clip);view.resized();engine.seek(0.25);
    const auto beforeHistory=view.editHistory().size();
    arrangement->mouseDown(click(*arrangement,330,50,ModifierKeys::rightButtonModifier));drain();popup=desktop("project-toolbox");
    expect(popup&&track->clips.size()==1&&view.editHistory().size()==beforeHistory&&std::abs(engine.positionSeconds.load()-0.25)<1e-9,"Opening the toolbox preserves events, cursor position and undo history");
    if(popup)if(auto* button=dynamic_cast<TextButton*>(find(*popup,"project-toolbox-tool-3")))if(button->onClick)button->onClick();drain();
    arrangement->mouseDown(click(*arrangement,330,50,ModifierKeys::leftButtonModifier));arrangement->mouseUp(click(*arrangement,330,50,0));
    expect(engine.tracks[0]->clips.size()==2,"Choosing scissors from the real popup makes the next click split audio");
    view.runCommand("undo");expect(engine.tracks[0]->clips.size()==1,"Split chosen from toolbox is undoable");

    view.runCommand("toolColour");
    const auto colourHistory=view.editHistory().size();const double colourCursor=engine.positionSeconds.load();
    arrangement->mouseDown(click(*arrangement,(float)arrangement->getWidth()-12,50,ModifierKeys::leftButtonModifier));drain();
    expect(desktop("colour-picker")==nullptr&&view.editHistory().size()==colourHistory&&std::abs(engine.positionSeconds.load()-colourCursor)<1e-9,"Colour tool on an empty area leaves the cursor and project unchanged");
    arrangement->mouseDown(click(*arrangement,330,50,ModifierKeys::leftButtonModifier));drain();
    auto* picker=desktop("colour-picker");expect(picker!=nullptr,"Colour tool opens a palette at the clicked event");
    closePopup(picker);
    expect(engine.tracks[0]->clips[0].colour.isTransparent()&&view.editHistory().size()==colourHistory,"Cancelling the colour palette does not create an edit");
    arrangement->mouseDown(click(*arrangement,330,50,ModifierKeys::leftButtonModifier));drain();picker=desktop("colour-picker");
    if(picker)
    {
        AudioClip extra=engine.tracks[0]->clips[0];extra.id=Uuid().toString();extra.colour={};
        engine.tracks[0]->clips.insert(engine.tracks[0]->clips.begin(),extra);
        capture(*picker,output.getChildFile("event-colour-tool.png"));picker->mouseDown(click(*picker,20,48,ModifierKeys::leftButtonModifier));drain();
        expect(engine.tracks[0]->clips[1].id==clipId&&!engine.tracks[0]->clips[1].colour.isTransparent()&&engine.tracks[0]->clips[0].colour.isTransparent()&&engine.tracks[0]->colour==Colour(0xff138b30),"Colour applies by event ID after reordering; track and other event colours stay intact");
        view.runCommand("undo");expect(engine.tracks[0]->clips[1].colour.isTransparent(),"Colour-tool edit is undoable");

        arrangement->mouseDown(click(*arrangement,330,50,ModifierKeys::leftButtonModifier));drain();picker=desktop("colour-picker");
        if(picker)
        {
            const auto history=view.editHistory().size();engine.tracks[0]->clips.pop_back();
            picker->mouseDown(click(*picker,20,48,ModifierKeys::leftButtonModifier));drain();
            expect(engine.tracks[0]->clips[0].colour.isTransparent()&&view.editHistory().size()==history,"Deleting the palette target cannot recolour a different event or create a false undo step");
        }
        else expect(false,"Colour palette reopens");
    }
    view.runCommand("toolSelect");
    arrangement->mouseDown(click(*arrangement,330,50,ModifierKeys::rightButtonModifier|ModifierKeys::shiftModifier));drain();
    expect(desktop("project-toolbox")==nullptr&&PopupMenu::dismissAllActiveMenus(),"Shift-right-click retains the event context menu");drain();
    app::saveShowToolboxOnRightClick(false);
    arrangement->mouseDown(click(*arrangement,330,50,ModifierKeys::rightButtonModifier));drain();
    expect(desktop("project-toolbox")==nullptr&&PopupMenu::dismissAllActiveMenus(),"Disabling the toolbox makes ordinary right-click open event actions");drain();
    arrangement->mouseDown(click(*arrangement,330,50,ModifierKeys::rightButtonModifier|ModifierKeys::shiftModifier));drain();
    popup=desktop("project-toolbox");expect(popup!=nullptr,"With toolbox disabled, Shift-right-click opens it instead");closePopup(popup);
    app::saveShowToolboxOnRightClick(oldPreference);engine.clear();view.resized();
}
}

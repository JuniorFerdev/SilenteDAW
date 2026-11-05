#include "ToolboxUiTests.h"
#include "../src/MainComponent.h"
#include "../src/app/Preferences.h"
#include "../src/app/KeyCommands.h"
#include "../src/ui/keys/KeyCommandsPanel.h"
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
bool samePixels(const Image& a,const Image& b)
{
    if(a.getBounds()!=b.getBounds())return false;
    Image::BitmapData left(a,Image::BitmapData::readOnly),right(b,Image::BitmapData::readOnly);
    for(int y=0;y<a.getHeight();++y)for(int x=0;x<a.getWidth();++x)if(left.getPixelColour(x,y)!=right.getPixelColour(x,y))return false;
    return true;
}
void navigationChecks(MainComponent& view,AudioEngine& engine,Component& arrangement,const File& output,const std::function<void(bool,const String&)>& expect)
{
    const char* buttons[]{"tool-select","tool-range","tool-split","tool-glue","tool-erase","tool-zoom","tool-mute","tool-draw","tool-play","tool-colour"};
    const auto active=[&](int index)
    {
        bool ok=true;for(int n=0;n<10;++n){auto* b=dynamic_cast<TextButton*>(find(view,buttons[n]));ok&=b&&b->getToggleState()==(n==index);}return ok;
    };
    const KeyPress next(KeyPress::F10Key),previous(KeyPress::F9Key);
    view.runCommand("toolSelect");arrangement.grabKeyboardFocus();
    const auto history=view.editHistory();const double cursor=engine.positionSeconds.load();const bool playing=engine.playing.load(),recording=engine.recording.load();
    const auto before=arrangement.createComponentSnapshot(arrangement.getLocalBounds());
    bool forward=active(0);for(int step=1;step<=10;++step)forward&=view.keyPressed(next)&&active(step%10);
    expect(forward,"F10 traverses the ten available project modes and wraps to Select");
    bool backward=active(0);for(int step=1;step<=10;++step)backward&=view.keyPressed(previous)&&active((10-step)%10);
    expect(backward,"F9 reverses the complete traversal, skips disabled modes and wraps");
    expect(history==view.editHistory()&&engine.positionSeconds.load()==cursor&&engine.playing.load()==playing&&engine.recording.load()==recording&&samePixels(before,arrangement.createComponentSnapshot(arrangement.getLocalBounds())),"Tool traversal preserves event/range drawing, cursor, transport and undo history");
    expect(ui::adjacentProjectTool(0,1)==1&&ui::adjacentProjectTool(0,-1)==13&&ui::adjacentProjectTool(9,0)==9,"Tool navigation recovers unknown modes without selecting unavailable tools");
    view.runCommand("toolPlay");expect(view.keyPressed(next)&&active(9),"F10 reaches Color after Play without inventing a direct Color shortcut");
    capture(view,output.getChildFile("tool-navigation-colour.png"));
    view.runCommand("toolSelect");
    {
        TextEditor editor;view.addAndMakeVisible(editor);editor.setBounds(20,20,140,26);editor.setText("Editar nombre",false);
        Process::makeForegroundProcess();view.getTopLevelComponent()->toFront(true);drain();editor.grabKeyboardFocus();drain();
        const bool focused=editor.hasKeyboardFocus(false),handled=view.keyPressed(next),unchanged=active(0);
        expect(focused&&!handled&&unchanged&&editor.getText()=="Editar nombre","F10 leaves the project tool unchanged while editing text (focus="+String((int)focused)+", handled="+String((int)handled)+", unchanged="+String((int)unchanged)+")");
    }
    view.runCommand("toolSelect");
    // The editor's own tool navigation is a separate pending feature.
    engine.addMidiTrack("Navigation focus fixture");view.runCommand("trackBottom");view.runCommand("openEditor");
    auto* piano=find(view,"piano-roll");auto* keyboard=find(view,"midi-keyboard");
    if(piano&&keyboard)
    {
        piano->grabKeyboardFocus();drain();const bool pianoFocused=piano->hasKeyboardFocus(true),pianoHandled=view.keyPressed(next),pianoIgnored=pianoFocused&&!pianoHandled&&active(0);
        keyboard->grabKeyboardFocus();drain();const bool keyboardFocused=keyboard->hasKeyboardFocus(true),keyboardHandled=view.keyPressed(previous),keyboardIgnored=keyboardFocused&&!keyboardHandled&&active(0);
        expect(pianoIgnored&&keyboardIgnored,"MIDI editor and keyboard focus do not redirect F9/F10 to the project tool (piano focus="+String((int)pianoFocused)+", handled="+String((int)pianoHandled)+"; keyboard focus="+String((int)keyboardFocused)+", handled="+String((int)keyboardHandled)+")");
    }
    else expect(false,"MIDI focus surfaces exist for navigation checks");
    view.showMixer();engine.removeTrack((int)engine.tracks.size()-1);view.runCommand("trackTop");view.runCommand("eventRight");arrangement.grabKeyboardFocus();
}
}
void runToolboxUiChecks(MainComponent& view,AudioEngine& engine,const juce::File& output,const std::function<void(bool,const juce::String&)>& expect)
{
    using namespace juce;
    {
        bool numericKeys=true;const char* ids[]{"toolSelect","toolRange","toolSplit","toolGlue","toolErase","toolZoom","toolMute","toolDraw","toolPlay"};
        for(int n=1;n<=9;++n){const auto* command=app::findKeyCommand(KeyPress('0'+n));numericKeys&=command&&command->id==ids[n-1];}
        expect(numericKeys,"Tool keys 1 through 9 match the installed Cubase catalog");
        const auto* drum=app::findKeyCommand(KeyPress('0'));const auto* stop=app::findKeyCommand(KeyPress(KeyPress::numberPad0));
        expect(drum&&drum->id=="toolDrumstick"&&stop&&stop->id=="stop","Main 0 is Drumstick; Num 0 remains Stop");
        const auto* previous=app::findKeyCommand(KeyPress(KeyPress::F9Key));const auto* next=app::findKeyCommand(KeyPress(KeyPress::F10Key));
        const auto* combine=app::findKeyCommand(KeyPress('1',ModifierKeys(ModifierKeys::altModifier|ModifierKeys::shiftModifier),0));
        expect(previous&&previous->id=="toolPrevious"&&next&&next->id=="toolNext"&&combine&&combine->id=="combineSelectionTools","F9, F10 and Option-Shift-1 are listed with the real Cubase commands");
        int count=0,unassigned=0;bool colour=false;
        for(const auto& command:app::keyCommands())if(command.categoryEn=="Tools")
        {
            ++count;if(command.keyCode==0)++unassigned;
            if(command.id=="toolColour")colour=command.keyCode==0&&app::describeKey(command).isNotEmpty();
        }
        expect(count==43&&unassigned==30&&colour&&app::findKeyCommand(KeyPress())==nullptr,"All 43 Tool commands are represented; unassigned commands cannot match an empty key");
        const auto history=view.editHistory().size();const auto tracks=engine.tracks.size();
        expect(!view.runCommand("toolDrumstick")&&!view.runCommand("combineSelectionTools")&&view.editHistory().size()==history&&engine.tracks.size()==tracks,"Unimplemented tool commands stay unavailable without editing the project");
        StringArray availableCommands;
        for(const auto& command:app::keyCommands())if(command.categoryEn=="Tools"&&view.runCommand(command.id))availableCommands.add(command.id);
        expect(availableCommands.size()==12,"Ten basic modes plus Previous/Next Tool are reported as available");view.runCommand("toolSelect");
        ui::KeyCommandsPanel panel([&](const String& id){return availableCommands.contains(id);});panel.setFilter("Herramientas");
        expect(panel.matchingCount()==43,"Key Commands search exposes the complete audited Tool category");
        capture(panel,output.getChildFile("tool-command-reference.png"));
    }
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
    expect(standalone.keyPressed(KeyPress(KeyPress::F10Key))&&selected==4&&standalone.keyPressed(KeyPress(KeyPress::F9Key))&&selected==3,"Toolbox F10/F9 use the same traversal as the project");
    expect(!standalone.keyPressed(KeyPress(KeyPress::F10Key,ModifierKeys(ModifierKeys::shiftModifier),0)),"Modified F10 is not captured as the default Next Tool command");

    auto* arrangement=find(view,"arrangement");if(!arrangement){expect(false,"Arrangement exists for toolbox checks");return;}
    engine.clear();view.resized();view.runCommand("toolSelect");
    // Flush earlier synthetic gestures before creating the first native popup.
    view.getTopLevelComponent()->toFront(true);drain();
    arrangement->mouseDown(click(*arrangement,330,50,ModifierKeys::rightButtonModifier));drain();
    auto* popup=desktop("project-toolbox");expect(popup!=nullptr,"Right-click opens the toolbox even with an empty project");closePopup(popup);

    auto track=engine.addAudioTrack("Toolbox fixture");track->colour=Colour(0xff138b30);
    AudioClip clip;clip.name="Colour target";clip.audio=std::make_shared<AudioBuffer<float>>(2,96000);clip.audio->clear();clip.sourceRate=48000;clip.durationSeconds=2;
    const auto clipId=clip.id;track->clips.push_back(clip);view.resized();engine.seek(0.25);
    view.runCommand("trackTop");view.runCommand("eventRight");navigationChecks(view,engine,*arrangement,output,expect);
    const auto beforeHistory=view.editHistory().size();
    arrangement->mouseDown(click(*arrangement,330,50,ModifierKeys::rightButtonModifier));drain();popup=desktop("project-toolbox");
    expect(popup&&track->clips.size()==1&&view.editHistory().size()==beforeHistory&&std::abs(engine.positionSeconds.load()-0.25)<1e-9,"Opening the toolbox preserves events, cursor position and undo history");
    if(popup)
    {
        capture(*popup,output.getChildFile("tool-navigation-popup.png"));
        const bool handled=popup->keyPressed(KeyPress(KeyPress::F10Key));drain();
        auto* range=dynamic_cast<TextButton*>(find(view,"tool-range"));
        expect(handled&&desktop("project-toolbox")==nullptr&&range&&range->getToggleState(),"F10 in the real popup selects Range and closes the toolbox");
        arrangement->grabKeyboardFocus();view.keyPressed(KeyPress(KeyPress::F9Key));
        arrangement->mouseDown(click(*arrangement,330,50,ModifierKeys::rightButtonModifier));drain();popup=desktop("project-toolbox");
        const bool previous=popup&&popup->keyPressed(KeyPress(KeyPress::F9Key));drain();
        auto* colour=dynamic_cast<TextButton*>(find(view,"tool-colour"));
        expect(previous&&desktop("project-toolbox")==nullptr&&colour&&colour->getToggleState(),"F9 in the popup wraps from Select to Color without touching events");
        view.runCommand("toolSelect");
        arrangement->mouseDown(click(*arrangement,330,50,ModifierKeys::rightButtonModifier));drain();popup=desktop("project-toolbox");
    }
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

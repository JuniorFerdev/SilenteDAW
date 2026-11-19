#include "FolderUiTests.h"
#include "../src/MainComponent.h"
#include "../src/ui/project/TrackFilter.h"

namespace silente
{
namespace
{
using namespace juce;
Component* find(Component& root,const String& id)
{
    if(root.getComponentID()==id)return &root;for(auto* child:root.getChildren())if(auto* value=find(*child,id))return value;return nullptr;
}
MouseEvent mouse(Component& c,float x,float y,Point<float> down={})
{
    if(down.isOrigin())down={x,y};
    return {Desktop::getInstance().getMainMouseSource(),{x,y},ModifierKeys(ModifierKeys::leftButtonModifier),1.0f,0,0,0,0,&c,&c,Time::getCurrentTime(),down,Time::getCurrentTime(),1,down!=Point<float>{x,y}};
}
juce::String names(const AudioEngine& engine)
{
    StringArray list;
    for(const auto& t:engine.tracks){const auto parent=engine.findTrack(t->data.parentFolder);list.add(t->name+(parent?"<"+parent->name:String()));}
    return list.joinIntoString(" ");
}
}
void runFolderUiChecks(MainComponent& view,AudioEngine& engine,const File& output,const std::function<void(bool,const String&)>& expect)
{
    auto* heightControl=dynamic_cast<Slider*>(find(view,"track-height"));
    const int oldHeight=heightControl?(int)heightControl->getValue():60;if(heightControl)heightControl->setValue(60,sendNotificationSync);
    constexpr int lane=60,ruler=30;
    engine.clear();view.showMixer();view.runCommand("toolSelect");
    auto bass=engine.addAudioTrack("Bajo");auto voice=engine.addAudioTrack("Voz");auto band=engine.addTrack(TrackKind::folder,"Banda");
    auto bus=engine.addTrack(TrackKind::group,"Bus");auto reverb=engine.addTrack(TrackKind::fx,"Rev");
    engine.setOutput(*bass,bus->id);engine.setSend(*voice,0,reverb->id);
    voice->automation.volume.points={{0.0,0.5},{4.0,1.0}};voice->automation.read=true;
    auto* arrangement=find(view,"arrangement");
    expect(arrangement!=nullptr,"folders: the project window track list is reachable");if(!arrangement)return;
    for(int n=0;n<20;++n)arrangement->mouseWheelMove(mouse(*arrangement,400,100),MouseWheelDetails{0.0f,1.0f,false,false,false});
    view.resized();arrangement->repaint();
    const auto rowY=[&](int row,float f=0.25f){return (float)(ruler+row*lane)+f*lane;};
    {
        // Cubase: a painted track also paints its row in the track list, a little darker than its colour.
        const auto paint=Colour(0xffe05050);
        bass->colour=paint;arrangement->repaint();
        const auto image=arrangement->createComponentSnapshot(arrangement->getLocalBounds());
        const auto pixel=image.getPixelAt(150,ruler+57);
        const auto expected=paint.darker(.3f);
        expect(std::abs(pixel.getRed()-expected.getRed())<=6&&std::abs(pixel.getGreen()-expected.getGreen())<=6&&std::abs(pixel.getBlue()-expected.getBlue())<=6,"A painted track paints its row in the track list, a little darker than its colour");
        bass->colour=Colour(defaultTrackColour);arrangement->repaint();
    }
    const auto drag=[&](int fromRow,float toY)
    {   // Press on the header background (not a control), drag and release over the destination.
        const Point<float> down{150.0f,rowY(fromRow,0.2f)};
        arrangement->mouseDown(mouse(*arrangement,down.x,down.y));
        arrangement->mouseDrag(mouse(*arrangement,down.x,(down.y+toY)*0.5f,down));
        arrangement->mouseDrag(mouse(*arrangement,down.x,toY,down));
        arrangement->mouseUp(mouse(*arrangement,down.x,toY,down));
    };
    // 1. Drop «Bajo» onto the middle of the folder row: it goes inside the folder.
    int history=view.editHistory().size();
    drag(0,rowY(2,0.5f));
    expect(names(engine)=="Voz Banda Bajo<Banda Bus Rev"&&view.editHistory().size()==history+1,"folders: dropping a track on a folder puts it inside (one undo step) - "+names(engine));
    // 2. Drop «Voz» at the bottom edge of the open folder row: first child.
    drag(0,rowY(1,0.9f));
    expect(names(engine)=="Banda Voz<Banda Bajo<Banda Bus Rev","folders: dropping below an open folder makes the track its first child - "+names(engine));
    // 3. Mover pista abajo / arriba keep the track inside its folder and selected.
    view.runCommand("moveTrackDown");
    expect(names(engine)=="Banda Bajo<Banda Voz<Banda Bus Rev","folders: Mover pista abajo swaps with the next track in the folder");
    view.runCommand("moveTrackDown");
    expect(names(engine)=="Banda Bajo<Banda Voz<Banda Bus Rev","folders: Mover pista abajo stops at the end of the folder");
    view.runCommand("moveTrackUp");
    expect(names(engine)=="Banda Voz<Banda Bajo<Banda Bus Rev","folders: Mover pista arriba");
    // 4. A folder cannot be dropped inside itself.
    history=view.editHistory().size();drag(0,rowY(1,0.2f));
    expect(names(engine)=="Banda Voz<Banda Bajo<Banda Bus Rev"&&view.editHistory().size()==history,"folders: dropping a folder inside itself is refused without an undo step");
    // 5. Fold and unfold with the header triangle; folded content leaves the list.
    arrangement->mouseDown(mouse(*arrangement,105,rowY(0,0.25f)));arrangement->mouseUp(mouse(*arrangement,105,rowY(0,0.25f)));
    expect(band->data.folderCollapsed&&ui::TrackFilter{}.visible(engine.tracks).size()==3,"folders: the triangle folds the folder and hides its content");
    arrangement->mouseDown(mouse(*arrangement,105,rowY(0,0.25f)));arrangement->mouseUp(mouse(*arrangement,105,rowY(0,0.25f)));
    expect(! band->data.folderCollapsed&&ui::TrackFilter{}.visible(engine.tracks).size()==5,"folders: the triangle unfolds it again");
    // 6. Folder mute from its header (M at the same place as other tracks).
    arrangement->mouseDown(mouse(*arrangement,56,rowY(0,0.25f)));arrangement->mouseUp(mouse(*arrangement,56,rowY(0,0.25f)));
    expect(band->mute.load(),"folders: M in the folder header mutes the folder");
    band->mute=false;
    // 7. Indented header: M of a track inside the folder is shifted by the indent.
    arrangement->mouseDown(mouse(*arrangement,56+12,rowY(1,0.25f)));arrangement->mouseUp(mouse(*arrangement,56+12,rowY(1,0.25f)));
    expect(voice->mute.load()&&! band->mute.load(),"folders: header controls of nested tracks follow the indent");
    voice->mute=false;
    if(auto out=output.getChildFile("folders.png").createOutputStream()){out->setPosition(0);out->truncate();PNGImageFormat().writeImageToStream(arrangement->createComponentSnapshot(arrangement->getLocalBounds()),*out);}
    // 8. Sacar de la carpeta, undo and redo through the project history.
    view.runCommand("trackTop");view.runCommand("moveTrackOutOfFolder");
    expect(names(engine)=="Banda Voz<Banda Bajo<Banda Bus Rev","folders: Sacar de la carpeta is refused for a top-level track");
    arrangement->mouseDown(mouse(*arrangement,150,rowY(1,0.2f)));arrangement->mouseUp(mouse(*arrangement,150,rowY(1,0.2f)));
    view.runCommand("moveTrackOutOfFolder");
    expect(names(engine)=="Banda Bajo<Banda Voz Bus Rev","folders: Sacar de la carpeta places the track after the folder - "+names(engine));
    view.runCommand("undo");
    expect(names(engine)=="Banda Voz<Banda Bajo<Banda Bus Rev","folders: undo restores order and folder");
    view.runCommand("redo");
    expect(names(engine)=="Banda Bajo<Banda Voz Bus Rev","folders: redo moves it out again");
    // 9. Drag the group above the folder: routing, sends and automation follow the ids.
    drag(3,rowY(0,0.1f));
    expect(names(engine)=="Bus Banda Bajo<Banda Voz Rev","folders: dragging a channel to the top of the list - "+names(engine));
    expect(bass->output==bus->id&&voice->sends[0].target==reverb->id&&voice->automation.volume.points.size()==2&&voice->automation.read,"folders: reordering keeps group output, sends and automation");
    // 10. Save and reopen keep the hierarchy.
    band->data.folderCollapsed=true;
    const auto file=output.getChildFile("folders-ui.silente");const auto before=engine.trackOrder();
    expect(engine.saveProject(file).wasOk()&&engine.loadProject(file).wasOk()&&engine.trackOrder()==before,"folders: save and reopen keep order, nesting and folding");
    file.deleteFile();
    engine.clear();view.resized();if(heightControl)heightControl->setValue(oldHeight,sendNotificationSync);
}
}

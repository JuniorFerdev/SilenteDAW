#include "AddTrackUiTests.h"
#include "../src/MainComponent.h"
#include "../src/ui/project/AddTrackDialog.h"
namespace silente
{
namespace
{
using namespace juce;
Component* find(Component& root,const String& id){if(root.getComponentID()==id)return &root;for(auto* child:root.getChildren())if(auto* result=find(*child,id))return result;return nullptr;}
ui::AddTrackDialog* dialog()
{
    for(int n=0;n<Desktop::getInstance().getNumComponents();++n)if(auto* result=dynamic_cast<ui::AddTrackDialog*>(find(*Desktop::getInstance().getComponent(n),"add-track-dialog")))return result;return nullptr;
}
void capture(Component& c,const File& file){if(auto out=file.createOutputStream()){out->setPosition(0);out->truncate();PNGImageFormat().writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()),*out);}}
}
void runAddTrackUiChecks(MainComponent& view,AudioEngine& engine,const juce::File& output,const std::function<void(bool,const juce::String&)>& expect)
{
    using namespace juce;
    engine.clear();view.runCommand("trackTop");const auto history=view.editHistory();view.runCommand("addTrack");auto* panel=dialog();
    expect(panel&&panel->isShowing()&&engine.tracks.empty()&&view.editHistory()==history,"+ and T open the icon-based Add Track dialog without creating a track or undo entry");if(!panel)return;
    const auto control=[&](const char* id){return find(*panel,id);};
    auto* name=dynamic_cast<TextEditor*>(control("add-name"));auto* count=dynamic_cast<Slider*>(control("add-count"));auto* keep=dynamic_cast<ToggleButton*>(control("add-keep-open"));
    auto* configuration=dynamic_cast<ComboBox*>(control("add-configuration"));auto* input=dynamic_cast<ComboBox*>(control("add-input"));auto* outputBox=dynamic_cast<ComboBox*>(control("add-output"));
    expect(name&&count&&keep&&configuration&&input&&outputBox,"Dialog exposes Name, Count, Keep Open, input, mono/stereo and output settings");if(!name||!count||!keep||!configuration||!input||!outputBox)return;
    const char* supported[]{"audio","instrument","midi","group","fx","marker","ruler","folder"};bool all=true;
    for(const auto* type:supported){auto* b=control(("add-type-"+String(type)).toRawUTF8());all&=b&&b->isEnabled();}
    auto* sampler=control("add-type-sampler");auto* video=control("add-type-video");
    expect(all&&sampler&&!sampler->isEnabled()&&video&&!video->isEnabled(),"All eight implemented track types are available; deferred types are visibly disabled");
    panel->updateOptions({"Input 1","Input 2","Input 3","Input 4"},{},engine.knownPlugins.getTypes(),0);
    configuration->setSelectedId(1,sendNotificationSync);input->setSelectedId(3,sendNotificationSync);name->setText(String::fromUTF8("Voz ñ"),false);count->setValue(3,dontSendNotification);keep->setToggleState(true,dontSendNotification);
    i18n::setLanguage(i18n::Language::english,false);view.refreshLanguage();
    expect(configuration->getItemText(1)=="Stereo"&&name->getText()==String::fromUTF8("Voz ñ")&&count->getValue()==3,"Language switch translates the dialog while preserving the entered name and count");
    i18n::setLanguage(i18n::Language::spanish,false);view.refreshLanguage();
    capture(*panel,output.getChildFile("add-track-audio.png"));const int before=view.editHistory().size();panel->submit();
    bool named=engine.tracks.size()==3;
    for(int n=0;n<(int)engine.tracks.size();++n){const auto& t=*engine.tracks[(size_t)n];named&=t.name==String::fromUTF8("Voz ñ ")+String(n+1).paddedLeft('0',2)&&t.inputLeft==2&&t.inputRight==2;}
    expect(named&&view.editHistory().size()==before+1&&panel->isShowing(),"A named batch creates three mono tracks on Input 3, numbered 01-03, in one undo step and keeps the dialog open");
    capture(view,output.getChildFile("add-track-named-batch.png"));
    view.runCommand("undo");expect(engine.tracks.empty(),"Undo removes the complete three-track batch");view.runCommand("redo");expect(engine.tracks.size()==3&&engine.tracks[2]->name.endsWith("03"),"Redo restores the complete batch and names");
    // Every implemented type supports multiple named tracks through the same coordinator.
    const TrackKind kinds[]{TrackKind::audio,TrackKind::instrument,TrackKind::midi,TrackKind::folder,TrackKind::fx,TrackKind::group,TrackKind::ruler,TrackKind::marker};bool batches=true;
    for(auto kind:kinds)
    {
        view.showAddTrack(kind);panel=dialog();name=dynamic_cast<TextEditor*>(find(*panel,"add-name"));count=dynamic_cast<Slider*>(find(*panel,"add-count"));
        name->setText("Batch",false);count->setValue(2,dontSendNotification);const auto size=engine.tracks.size();panel->submit();
        batches&=engine.tracks.size()==size+2&&engine.tracks[size]->kind==kind&&engine.tracks[size+1]->name=="Batch 02";
    }
    expect(batches,"Audio, Instrument, MIDI, Folder, FX, Group, Ruler and Marker each create multiple named tracks");
    view.showAddTrack(TrackKind::ruler);panel=dialog();auto* format=dynamic_cast<ComboBox*>(find(*panel,"add-ruler-format"));format->setSelectedId(3,sendNotificationSync);count=dynamic_cast<Slider*>(find(*panel,"add-count"));count->setValue(2,dontSendNotification);panel->submit();
    expect(engine.tracks.back()->data.rulerFormat==2&&engine.tracks[engine.tracks.size()-2]->data.rulerFormat==2,"Ruler batches apply the chosen independent time format to every track");
    const auto saved=output.getChildFile("track-batches.silente");const auto total=engine.tracks.size();expect(engine.saveProject(saved).wasOk()&&engine.loadProject(saved).wasOk()&&engine.tracks.size()==total,"Batch track types, names and settings persist through saving and reopening");
    view.showAddTrack(TrackKind::audio);panel=dialog();keep=dynamic_cast<ToggleButton*>(find(*panel,"add-keep-open"));keep->setToggleState(false,dontSendNotification);count=dynamic_cast<Slider*>(find(*panel,"add-count"));count->setValue(1,dontSendNotification);panel->submit();expect(!panel->isShowing(),"With Keep Dialog Open disabled, successful creation closes the window");
    view.showAddTrack(TrackKind::folder);panel=dialog();const auto beforeCancel=engine.tracks.size();const auto cancelHistory=view.editHistory();panel->keyPressed(KeyPress(KeyPress::escapeKey));
    expect(!panel->isShowing()&&engine.tracks.size()==beforeCancel&&view.editHistory()==cancelHistory,"Escape cancels without modifying tracks or undo history");
    view.showAddTrack(TrackKind::audio);panel=dialog();auto* more=dynamic_cast<TextButton*>(find(*panel,"add-more-types"));if(more)more->onClick();expect(!find(*panel,"add-type-video")->isVisible(),"More tracks folds the six additional icons");if(more)more->onClick();
    // Stable destination IDs: deleting a bus while the dialog is open must reject the batch.
    auto bus=engine.addTrack(TrackKind::group,"Destination");view.showAddTrack(TrackKind::audio);panel=dialog();outputBox=dynamic_cast<ComboBox*>(find(*panel,"add-output"));outputBox->setSelectedId(outputBox->getNumItems(),sendNotificationSync);
    engine.removeTrack((int)engine.tracks.size()-1);const auto staleSize=engine.tracks.size();const auto staleHistory=view.editHistory().size();panel->submit();
    expect(engine.tracks.size()==staleSize&&view.editHistory().size()==staleHistory&&panel->isShowing(),"A deleted output bus rejects creation without a partial batch or false undo step");
    panel->keyPressed(KeyPress(KeyPress::escapeKey));engine.clear();view.runCommand("goStart");view.resized();
}
}

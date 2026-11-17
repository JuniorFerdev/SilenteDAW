#include "WorkflowUiTests.h"
#include "SettingsUiTests.h"
#include "ToolboxUiTests.h"
#include "MarkerUiTests.h"
#include "AddTrackUiTests.h"
#include "../src/MainComponent.h"
#include "../src/ui/plugins/PluginSelector.h"
#include "../src/ui/plugins/InsertRack.h"
#include "../src/ui/project/AddTrackDialog.h"
#include "../src/ui/mixconsole/MixConsole.h"
#include "../src/ui/io/ConversionPanel.h"
#include "../src/ui/io/ExportDialog.h"
#include "../src/ui/project/TrackFilter.h"
#include "../src/ui/project/SnapGrid.h"
#include "../src/ui/project/ZoneSplitter.h"
#include "../src/ui/project/FadeEditor.h"
#include "../src/ui/project/ChannelTab.h"
#include "../src/app/KeyCommands.h"
#include "../src/ui/midi/PianoRoll.h"
#include <cmath>
#include <iostream>

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
void capture(Component& view,const File& file)
{
    const auto image=view.createComponentSnapshot(view.getLocalBounds());
    if(auto out=file.createOutputStream()){out->setPosition(0);out->truncate();PNGImageFormat().writeImageToStream(image,*out);}
}
MouseEvent mouse(Component& component,float x,float y,bool dragged=false,int extraModifiers=0)
{
    return {Desktop::getInstance().getMainMouseSource(),{x,y},ModifierKeys(ModifierKeys::leftButtonModifier|extraModifiers),1.0f,0,0,0,0,&component,&component,Time::getCurrentTime(),{350.5f,22},Time::getCurrentTime(),1,dragged};
}
class WorkflowChecks final : private Timer
{
public:
    WorkflowChecks(MainComponent& v,AudioEngine& e,PluginManager& p,const File& dir,const File& vst,std::function<void(int)> cb)
        :view(v),engine(e),manager(p),output(dir),fixture(vst),complete(std::move(cb))
    {
        output.createDirectory();initialChecks();mixConsoleChecks();trackListChecks();snapChecks();
        runToolboxUiChecks(view,engine,output,[this](bool ok,const String& label){expect(ok,label);});
        runSettingsUiChecks(view,engine,output,[this](bool ok,const String& label){expect(ok,label);});keyCommandChecks();
        runMarkerUiChecks(view,engine,output,[this](bool ok,const String& label){expect(ok,label);});
        runAddTrackUiChecks(view,engine,output,[this](bool ok,const String& label){expect(ok,label);});
        pianoRollChecks();
        engine.clear();track=engine.addAudioTrack("Workflow test");
        for(auto* format:engine.pluginFormats.getFormats())if(format->getName()=="AudioUnit")
        {
            OwnedArray<PluginDescription> found;format->findAllTypesForFile(found,"AudioUnit:Effects/aufx,lpas,appl");
            if(!found.isEmpty())apple=*found[0];
            found.clear();format->findAllTypesForFile(found,"AudioUnit:Synths/aumu,dls ,appl");
            if(!found.isEmpty())instrument=*found[0];
        }
        if(apple.name.isEmpty()){failures+="Apple AULowpass was not found\n";finish();return;}
        if(!instrument.name.isEmpty())
        {
            view.showAddTrack(TrackKind::instrument);
            for(int n=0;n<Desktop::getInstance().getNumComponents();++n)if(auto* panel=dynamic_cast<ui::AddTrackDialog*>(find(*Desktop::getInstance().getComponent(n),"add-track-dialog")))
            {
                juce::Array<PluginDescription> catalog;catalog.add(instrument);panel->updateOptions({}, {},catalog,0);
                dynamic_cast<TextEditor*>(find(*panel,"add-name"))->setText("Pianos",false);dynamic_cast<Slider*>(find(*panel,"add-count"))->setValue(2,dontSendNotification);
                dynamic_cast<ComboBox*>(find(*panel,"add-output"))->setSelectedId(1,sendNotificationSync);
                dynamic_cast<ComboBox*>(find(*panel,"add-plugin"))->setSelectedId(2,sendNotificationSync);panel->submit();break;
            }
            stage=7;
        }
        else manager.insertPlugin(0,apple);
        deadline=Time::getMillisecondCounterHiRes()+20000;startTimer(50);
    }
private:
    MainComponent& view;AudioEngine& engine;PluginManager& manager;
    File output,fixture;std::function<void(int)> complete;
    String failures,report;PluginDescription apple,vst,instrument;std::shared_ptr<Track> track;std::shared_ptr<PluginSlot> first;
    double deadline=0;int stage=0;
    void expect(bool result,const String& name){std::cout<<(result?"PASS: ":"FAIL: ")<<name.toStdString()<<std::endl;if(!result)failures+=name+"\n";else report+="PASS: "+name+"\n";}
    void pianoRollChecks()
    {
        engine.clear();engine.addMidiTrack("Piano roll test");
        auto* roll=dynamic_cast<ui::PianoRoll*>(find(view,"piano-roll"));
        expect(roll!=nullptr,"Piano roll component is reachable in the main view");if(!roll)return;
        const auto savedBefore=roll->beforeEdit,savedChanged=roll->changed;int undoSteps=0,changes=0;
        roll->beforeEdit=[&]{++undoSteps;};roll->changed=[&]{++changes;};roll->track=0;roll->selected=-1;roll->snap=true;roll->scrollBeats=0;
        const auto notes=[&]()->std::vector<MidiNote>{const ScopedLock lock(engine.modelLock);return engine.tracks[0]->notes;};
        const float y0=24.f+(roll->topPitch-60)*12+5.f;
        roll->mouseDoubleClick(mouse(*roll,52.f+2*64.f+5.f,y0));
        auto n=notes();
        expect(n.size()==1&&n[0].pitch==60&&std::abs(n[0].startBeats-2.0)<1e-9&&std::abs(n[0].lengthBeats-1.0)<1e-9&&undoSteps==1,"Piano roll: double click creates one note (pitch 60, beat 2, length 1) with one undo step");
        roll->mouseDown(mouse(*roll,52.f+2*64.f+10.f,y0));roll->mouseDrag(mouse(*roll,52.f+3*64.f+10.f,y0-24.f,true));roll->mouseUp(mouse(*roll,52.f+3*64.f+10.f,y0-24.f));
        n=notes();
        expect(n.size()==1&&std::abs(n[0].startBeats-3.0)<1e-9&&n[0].pitch==62&&std::abs(n[0].lengthBeats-1.0)<1e-9&&undoSteps==2,"Piano roll: dragging a note moves it +1 beat and +2 semitones with one undo step");
        const float right=52.f+4*64.f;
        roll->mouseDown(mouse(*roll,right-3.f,y0-24.f));roll->mouseDrag(mouse(*roll,right-3.f+32.f,y0-24.f,true));roll->mouseUp(mouse(*roll,right-3.f+32.f,y0-24.f));
        n=notes();
        expect(n.size()==1&&std::abs(n[0].lengthBeats-1.5)<1e-9&&std::abs(n[0].startBeats-3.0)<1e-9&&n[0].pitch==62&&undoSteps==3,"Piano roll: dragging the right edge changes length to 1.5 beats only");
        roll->quantizeSettings={};roll->quantizeSettings.division=4;n[0].startBeats=3.3;{const ScopedLock lock(engine.modelLock);engine.tracks[0]->notes[0].startBeats=3.3;}
        const int before=undoSteps;roll->quantizeNotes();n=notes();
        expect(n.size()==1&&std::abs(n[0].startBeats-3.0)<1e-9&&undoSteps==before+1,"Piano roll: Quantize snaps the selected note to 1/4 with one undo step");
        roll->keyPressed(KeyPress(KeyPress::deleteKey));
        expect(notes().empty()&&undoSteps==before+2&&changes>=4,"Piano roll: Delete removes the selected note with one undo step");
        pianoRollSelectionChecks(*roll,undoSteps,notes);
        roll->beforeEdit=savedBefore;roll->changed=savedChanged;
        pianoRollUndoCheck(*roll,notes);
        roll->clearSelection();engine.clear();
    }
    // Multiple selection (subetapa 1 of the MIDI editor): marquee, Shift+click, Cmd+A, group move,
    // resize, quantize and delete, each gesture with at most one undo step.
    template<typename Notes> void pianoRollSelectionChecks(ui::PianoRoll& roll,int& undoSteps,const Notes& notes)
    {
        const auto x=[&](double beat){return 52.f+(float)(beat*roll.pixelsPerBeat);};
        const auto y=[&](int pitch){return 24.f+(float)((roll.topPitch-pitch)*12)+5.f;};
        const auto setNotes=[&](std::initializer_list<std::pair<int,double>> list){const ScopedLock lock(engine.modelLock);auto& n=engine.tracks[0]->notes;n.clear();for(auto [p,b]:list){MidiNote m;m.pitch=p;m.startBeats=b;m.lengthBeats=1;n.push_back(m);}};
        setNotes({{60,0.0},{62,1.0},{64,2.0},{72,4.0}});roll.clearSelection();
        auto n=notes();const auto id=[&](int i){return n[(size_t)i].id;};
        int before=undoSteps;
        roll.mouseDown(mouse(roll,x(0.5),y(66)-3.f));roll.mouseDrag(mouse(roll,x(2.5),y(61)+1.f,true));roll.mouseUp(mouse(roll,x(2.5),y(61)+1.f));
        expect(roll.selection.size()==2&&roll.selection.contains(id(1))&&roll.selection.contains(id(2))&&undoSteps==before,"Piano roll: marquee over beats 0.5-2.5 and pitches 61-66 selects exactly two notes without an undo step");
        roll.mouseDown(mouse(roll,x(0.5),y(60),false,ModifierKeys::shiftModifier));roll.mouseUp(mouse(roll,x(0.5),y(60)));
        const bool added=roll.selection.size()==3&&roll.selection.contains(id(0));
        roll.mouseDown(mouse(roll,x(2.5),y(64),false,ModifierKeys::shiftModifier));roll.mouseUp(mouse(roll,x(2.5),y(64)));
        const bool removed=roll.selection.size()==2&&!roll.selection.contains(id(2));
        roll.mouseDown(mouse(roll,x(2.5),y(64),false,ModifierKeys::shiftModifier));roll.mouseUp(mouse(roll,x(2.5),y(64)));
        expect(added&&removed&&roll.selection.size()==3&&undoSteps==before&&notes()[0].startBeats==0.0,"Piano roll: Shift+click adds and removes notes from the selection without editing");
        roll.mouseDown(mouse(roll,x(1.5),y(62)));roll.mouseDrag(mouse(roll,x(2.5),y(62)-12.f,true));roll.mouseUp(mouse(roll,x(2.5),y(62)-12.f));
        n=notes();
        expect(undoSteps==before+1&&n[0].pitch==61&&n[1].pitch==63&&n[2].pitch==65&&n[0].startBeats==1.0&&n[1].startBeats==2.0&&n[2].startBeats==3.0&&n[3].pitch==72&&n[3].startBeats==4.0&&roll.selection.size()==3,
               "Piano roll: dragging one selected note moves the group +1 beat and +1 semitone, unselected note intact, one undo step");
        roll.mouseDown(mouse(roll,x(2.0)-3.f,y(61)));roll.mouseDrag(mouse(roll,x(2.0)-3.f+32.f,y(61),true));roll.mouseUp(mouse(roll,x(2.0)-3.f+32.f,y(61)));
        n=notes();
        expect(undoSteps==before+2&&n[0].lengthBeats==1.5&&n[1].lengthBeats==1.5&&n[2].lengthBeats==1.5&&n[3].lengthBeats==1.0&&n[0].startBeats==1.0,"Piano roll: dragging the right edge resizes the whole selection by +0.5 beat with one undo step");
        roll.mouseDown(mouse(roll,x(4.5),y(72)));roll.mouseUp(mouse(roll,x(4.5),y(72)));
        const bool single=roll.selection.size()==1&&roll.selection.contains(id(3))&&undoSteps==before+2;
        roll.keyPressed(KeyPress('a',ModifierKeys::commandModifier,0));const bool all=roll.selection.size()==4;
        roll.keyPressed(KeyPress('a',ModifierKeys::commandModifier|ModifierKeys::shiftModifier,0));
        expect(single&&all&&roll.selection.empty()&&undoSteps==before+2,"Piano roll: plain click selects only that note (no undo step), Cmd+A selects all, Cmd+Shift+A deselects");
        {const ScopedLock lock(engine.modelLock);engine.tracks[0]->notes[0].startBeats=1.2;engine.tracks[0]->notes[3].startBeats=4.3;}
        roll.quantizeSettings={};roll.quantizeSettings.division=4;
        roll.mouseDown(mouse(roll,x(4.6),y(72)));roll.mouseUp(mouse(roll,x(4.6),y(72)));roll.quantizeNotes();n=notes();
        expect(undoSteps==before+3&&n[3].startBeats==4.0&&n[0].startBeats==1.2,"Piano roll: Quantize acts only on the selected note, leaving the others, with one undo step");
        roll.mouseDown(mouse(roll,x(1.5),y(61)));roll.mouseUp(mouse(roll,x(1.5),y(61)));
        roll.mouseDown(mouse(roll,x(3.5),y(65),false,ModifierKeys::shiftModifier));roll.mouseUp(mouse(roll,x(3.5),y(65)));
        roll.keyPressed(KeyPress(KeyPress::deleteKey));n=notes();
        expect(undoSteps==before+4&&n.size()==2&&n[0].pitch==63&&n[1].pitch==72&&roll.selection.empty()&&roll.selected==-1,"Piano roll: Delete removes the two selected notes with one undo step");
    }
    // Real history: one group move is undone in one step and redone in one step.
    template<typename Notes> void pianoRollUndoCheck(ui::PianoRoll& roll,const Notes& notes)
    {
        {const ScopedLock lock(engine.modelLock);auto& n=engine.tracks[0]->notes;n.clear();for(int p:{60,64,67}){MidiNote m;m.pitch=p;m.startBeats=1;n.push_back(m);}}
        view.runCommand("selectTrack1");roll.track=0;roll.clearSelection();roll.selectAll();
        const auto x=[&](double beat){return 52.f+(float)(beat*roll.pixelsPerBeat);};const float y64=24.f+(float)((roll.topPitch-64)*12)+5.f;
        const auto historyBefore=view.editHistory();
        roll.mouseDown(mouse(roll,x(1.5),y64));roll.mouseDrag(mouse(roll,x(3.0),y64,true));roll.mouseDrag(mouse(roll,x(3.5),y64,true));roll.mouseUp(mouse(roll,x(3.5),y64));
        const auto moved=notes();
        view.runCommand("undo");const auto undone=notes();
        view.runCommand("redo");const auto redone=notes();
        const auto starts=[](const std::vector<MidiNote>& v,double b){if(v.size()!=3)return false;for(auto& m:v)if(m.startBeats!=b)return false;return true;};
        expect(starts(moved,3.0)&&starts(undone,1.0)&&starts(redone,3.0)&&roll.selection.empty(),"Piano roll: a multi-step group drag is one undo step; Undo restores all three notes and Redo reapplies it");
        ignoreUnused(historyBefore);
    }
    void keyCommandChecks()
    {
        const auto id=[](const KeyPress& k){auto* c=app::findKeyCommand(k);return c?c->id:String();};
        expect(id(KeyPress('c'))=="metronome"&&id(KeyPress('/'))=="cycle"&&id(KeyPress('*',ModifierKeys::shiftModifier,0))=="record"&&id(KeyPress('p'))=="locatorsToSelection"
               &&id(KeyPress(KeyPress::spaceKey))=="startStop"&&id(KeyPress('z',ModifierKeys::commandModifier,0))=="undo"&&id(KeyPress(KeyPress::deleteKey,ModifierKeys::shiftModifier,0))=="deleteTracks",
               "Cubase default keys map to their commands (C, /, *, P, Space, Cmd-Z, Shift-Del)");
        engine.clear();auto track=engine.addAudioTrack("Keys");
        AudioClip clip;clip.name="Clip";clip.audio=std::make_shared<AudioBuffer<float>>(2,48000);clip.audio->clear();clip.sourceRate=48000;clip.startSeconds=2.0;clip.durationSeconds=1.0;
        track->clips.push_back(clip);view.resized();
        view.runCommand("trackTop");view.runCommand("eventRight");view.runCommand("locatorsToSelection");
        expect(std::abs(engine.loopStart.load()-2.0)<1e-9&&std::abs(engine.loopEnd.load()-3.0)<1e-9,"P sets the locators to the selected event");
        view.runCommand("goLeft");expect(std::abs(engine.positionSeconds.load()-2.0)<1e-9,"Num 1 goes to the left locator");
        view.runCommand("copy");engine.seek(5.0);view.runCommand("paste");
        expect(track->clips.size()==2&&std::abs(track->clips[1].startSeconds-5.0)<1e-9,"Cmd-C / Cmd-V paste the event at the cursor");
        view.runCommand("nextEvent");expect(std::abs(engine.positionSeconds.load()-6.0)<1e-9,"N locates the next event edge");
        {   // Edit history: named steps, undo walks back through them.
            const int before=view.editHistory().size();
            view.runCommand("muteTracks");view.runCommand("soloTracks");const auto names=view.editHistory();
            expect(names.size()==before+2&&names[names.size()-2]==i18n::tr("Enmudecer")&&names[names.size()-1]==i18n::tr("Solo")&&track->mute.load()&&track->solo.load(),"edit history records named steps (Enmudecer, Solo)");
            view.runCommand("undo");expect(!track->solo.load()&&track->mute.load()&&view.editHistory().size()==before+1,"undo walks back one history step");
            view.runCommand("redo");expect(track->solo.load(),"redo re-applies the step");
        }
        const bool looping=engine.looping.load();view.runCommand("cycle");expect(engine.looping.load()!=looping,"/ toggles cycle");view.runCommand("cycle");
        engine.clear();
        recordingChecks();
    }
    void recordingChecks()
    {   // Grabación (manual pp. 347–359): I/O punch keys, record modes, format, and a punched take from the UI.
        {   // Cubase tools 4 · 6 · 7 · 9 on the toolbar, and X is Fundido cruzado.
            bool ok=true;const char* ids[]{"toolGlue","toolZoom","toolMute","toolPlay"};const char* buttons[]{"tool-glue","tool-zoom","tool-mute","tool-play"};
            for(int n=0;n<4;++n){view.runCommand(ids[n]);auto* b=dynamic_cast<TextButton*>(find(view,buttons[n]));ok=ok&&b&&b->getToggleState()&&b->isVisible()&&b->getRight()<view.getWidth()/2+300;}
            view.runCommand("toolSelect");
            expect(ok&&!dynamic_cast<TextButton*>(find(view,"tool-play"))->getToggleState(),"Glue, Zoom, Mute and Play tools on the toolbar with keys 4, 6, 7, 9");
            expect(app::findKeyCommand(juce::KeyPress('x'))&&app::findKeyCommand(juce::KeyPress('x'))->id=="crossfade","X is Crossfade");
        }
        expect(app::findKeyCommand(juce::KeyPress('i'))&&app::findKeyCommand(juce::KeyPress('i'))->id=="punchIn"&&app::findKeyCommand(juce::KeyPress('o'))->id=="punchOut","I and O are Punch In and Punch Out");
        view.transportMenuItem(182);view.transportMenuItem(187);
        expect(engine.audioRecordMode.load()==2&&engine.recordBitDepth.load()==32,"Transporte menu sets audio record mode (Reemplazar) and 32-bit float");
        view.transportMenuItem(181);view.transportMenuItem(186);
        engine.prepareForOffline(48000,128,true);
        auto track=engine.addAudioTrack("Voz");track->inputLeft=0;track->inputRight=1;track->armed=true;
        view.runCommand("punchIn");view.runCommand("punchOut");engine.loopStart=0.1;engine.loopEnd=0.2;engine.seek(0.0);
        expect(engine.punchIn.load()&&engine.punchOut.load(),"punch buttons on");
        view.transportMenuItem(100); // Inicio: playback that switches to recording at punch in
        expect(engine.recording.load()&&engine.playing.load()&&!engine.capturing.load(),"play with Punch In waits for the punch-in point");
        AudioBuffer<float> out(2,128),in(2,128);for(int c=0;c<2;++c)in.getWritePointer(c)[0]=0,juce::FloatVectorOperations::fill(in.getWritePointer(c),0.25f,128);
        for(int done=0;done<48000*3/10;done+=128){out.clear();engine.renderOfflineBlock(out,128,in);}
        capture(view,output.getChildFile("recording.png")); // the red event while the take is still open
        view.transportMenuItem(0); // refresh: the punch out closes the take
        expect(!engine.recording.load()&&engine.playing.load(),"punch out ends recording, playback continues");
        expect(track->clips.size()==1&&std::abs(track->clips[0].startSeconds-0.1)<1e-4&&std::abs(track->clips[0].durationSeconds-0.1)<1e-4,"punched take sits between the locators");
        expect(view.editHistory().size()>0&&view.editHistory()[view.editHistory().size()-1]==i18n::tr("Grabar"),"recording is an undo step named Grabar");
        view.runCommand("undo");expect(engine.tracks.size()>0&&engine.tracks.back()->clips.empty(),"undo removes the recorded take");
        view.runCommand("stop");view.runCommand("punchIn");view.runCommand("punchOut");
        {   // Three seconds of a sung-like signal from bar 2: the growing red event with its waveform.
            engine.clear();engine.prepareForOffline(48000,128,true);auto voz=engine.addAudioTrack("Voz");voz->inputLeft=0;voz->inputRight=1;voz->armed=true;
            // At the default zoom this starts at x=325.975: painting the first
            // whole pixel used to index peak -1 and crash before the allocation.
            engine.seek(1.013);const bool ok=engine.beginRecording(output.getChildFile("rec-preview"),{-1.0,false}).wasOk();
            AudioBuffer<float> o(2,128),i(2,128);double t=0;
            for(int done=0;done<48000*3;done+=128){for(int k=0;k<128;++k){const float v=(float)(0.7*std::sin(t*2*3.14159*220)*std::abs(std::sin(t*3.0)));i.setSample(0,k,v);i.setSample(1,k,v);t+=1.0/48000;}o.clear();engine.renderOfflineBlock(o,128,i);}
            view.transportMenuItem(0);capture(view,output.getChildFile("recording-live.png"));
            expect(ok&&engine.recording.load()&&std::abs(engine.recordLapStart.load()-1.013)<1e-3&&engine.recordCaptured.load()>=48000*3-128,"red event grows from a fractional pixel start without reading before the waveform");
            engine.cancelRecording();engine.pause();
        }
        engine.clear();engine.prepareForOffline(48000,128);
        fadeChecks();
    }
    void fadeChecks()
    {   // Fundidos (manual pp. 396–418): Audio › Fundidos on the selected event and the event handles.
        const auto x=[this](double oldPixel){return view.projectXForTime((oldPixel-250.0)/75.0);};
        auto track=engine.addAudioTrack("Fundidos");AudioClip clip;clip.sourceRate=48000;clip.durationSeconds=4.0;clip.fadeIn=clip.fadeOut=0.0;
        clip.audio=std::make_shared<AudioBuffer<float>>(1,48000*4);clip.audio->clear();track->clips.push_back(clip);
        view.runCommand("trackTop");view.runCommand("eventRight");
        view.audioMenuItem(201);view.audioMenuItem(202);
        expect(std::abs(track->clips[0].fadeIn-0.5)<1e-9&&std::abs(track->clips[0].fadeOut-0.5)<1e-9,"Audio > Fundidos applies the standard fade-in and fade-out");
        view.audioMenuItem(205);expect(track->clips[0].fadeIn==0.0&&track->clips[0].fadeOut==0.0,"Eliminar fundidos removes both fades");
        if(auto* arrangement=find(view,"arrangement"))
        {   // Event at 0 s on the first track: x = 250 + seconds × 75, rows 60 px under a 30 px ruler.
            arrangement->mouseDown(mouse(*arrangement,x(255),64));arrangement->mouseDrag(mouse(*arrangement,x(330),64,true));arrangement->mouseUp(mouse(*arrangement,x(330),64,true));
            expect(std::abs(track->clips[0].fadeIn-1.0)<0.02,"dragging the fade-in handle sets the fade length");
            arrangement->mouseDown(mouse(*arrangement,x(258),76));arrangement->mouseDrag(mouse(*arrangement,x(258),26,true));arrangement->mouseUp(mouse(*arrangement,x(258),26,true));
            expect(std::abs(track->clips[0].gain-Decibels::decibelsToGain(6.0f))<1e-4f,"dragging the volume handle up raises the event volume to +6 dB");
        }
        else expect(false,"arrangement exists for the fade handles");
        track->clips[0].fadeOut=1.2;track->clips[0].fadeOutShape=2;track->clips[0].gain=0.7f;
        for(int i=0;i<track->clips[0].audio->getNumSamples();++i)track->clips[0].audio->setSample(0,i,0.6f*std::sin(i*0.002f)*std::sin(i*0.0001f));
        capture(view,output.getChildFile("fades.png"));
        {
            ui::FadeEditor::Settings got;bool applied=false;
            ui::FadeEditor editor({1.0,1.2,0,2},4.0,[&](ui::FadeEditor::Settings v){got=v;applied=true;},{});editor.setSize(620,330);
            if(auto* b=dynamic_cast<TextButton*>(find(editor,"fade-out-shape-1")))b->onClick();
            if(auto* l=dynamic_cast<Slider*>(find(editor,"fade-in-length")))l->setValue(3.5,sendNotificationSync); // limited to 4.0 − 1.2
            editor.pressApply();capture(editor,output.getChildFile("fade-editor.png"));
            expect(applied&&got.outShape==1&&std::abs(got.fadeIn-2.8)<1e-9,"fade editor: curve buttons and length (fade-in + fade-out within the event)");
        }
        engine.clear();
        rangeChecks();
    }
    void rangeChecks()
    {   // Selección de rango (2): drag 1 s → 2 s on the first track, then Suprimir intervalo de tiempo.
        const auto x=[this](double oldPixel){return view.projectXForTime((oldPixel-250.0)/75.0);};
        auto track=engine.addAudioTrack("Rango");AudioClip clip;clip.sourceRate=48000;clip.durationSeconds=4.0;
        clip.audio=std::make_shared<AudioBuffer<float>>(1,48000*4);clip.audio->clear();track->clips.push_back(clip);
        view.runCommand("toolRange");auto* rangeButton=dynamic_cast<TextButton*>(find(view,"tool-range"));
        auto* arrangement=find(view,"arrangement");
        if(arrangement&&rangeButton)
        {
            arrangement->mouseDown(mouse(*arrangement,x(325),64));arrangement->mouseDrag(mouse(*arrangement,x(400),64,true));arrangement->mouseUp(mouse(*arrangement,x(400),64,true));
            capture(view,output.getChildFile("range.png"));
            view.runCommand("deleteTime");
            expect(rangeButton->getToggleState()&&track->clips.size()==2&&std::abs(track->clips[1].startSeconds-1.0)<1e-6&&std::abs(track->clips[1].offsetSeconds-2.0)<1e-6,"range tool: drag a range and Shift+Backspace deletes the time");
            view.runCommand("undo");expect(track->clips.size()==1&&std::abs(track->clips[0].durationSeconds-4.0)<1e-9,"range edit undoes in one step");
            view.runCommand("insertSilence");expect(track->clips.size()==2&&std::abs(track->clips[1].startSeconds-2.0)<1e-6,"Insert Silence moves the rest by the range length");
            // Move the range content: range 1–2 s, drag from 1.5 s to 3.5 s (x = 250 + s × 75).
            {const ScopedLock lock(engine.modelLock);track->clips.clear();track->clips.push_back(clip);}
            arrangement->mouseDown(mouse(*arrangement,x(700),64));arrangement->mouseUp(mouse(*arrangement,x(700),64)); // click outside: new empty range
            arrangement->mouseDown(mouse(*arrangement,x(325),64));arrangement->mouseDrag(mouse(*arrangement,x(400),64,true));arrangement->mouseUp(mouse(*arrangement,x(400),64,true));
            arrangement->mouseDown(mouse(*arrangement,x(362.5f),64));arrangement->mouseDrag(mouse(*arrangement,x(512.5f),64,true));arrangement->mouseUp(mouse(*arrangement,x(512.5f),64,true));
            expect(track->clips.size()==3&&std::abs(track->clips[2].startSeconds-3.0)<1e-6&&std::abs(track->clips[2].offsetSeconds-1.0)<1e-6&&std::abs(track->clips[1].startSeconds-2.0)<1e-6,"dragging the range moves its content (gap left behind, audio lands 2 s later)");
            // ⌥ drag copies: the range is now 3–4 s; copy it to 5–6 s.
            arrangement->mouseDown(mouse(*arrangement,x(512.5f),64,false,ModifierKeys::altModifier));arrangement->mouseDrag(mouse(*arrangement,x(662.5f),64,true,ModifierKeys::altModifier));arrangement->mouseUp(mouse(*arrangement,x(662.5f),64,true,ModifierKeys::altModifier));
            // Under 3–4 s there are two layers (the original event and the moved audio on top): both are copied, top stays on top.
            expect(track->clips.size()==5&&std::abs(track->clips[4].startSeconds-5.0)<1e-6&&std::abs(track->clips[4].offsetSeconds-1.0)<1e-6&&std::abs(track->clips[3].offsetSeconds-3.0)<1e-6,"Option-drag copies the range content with its layers");
            // ⌥→ moves the range start by the grid; Suprimir then keeps the first grid step of the copy.
            view.runCommand("nudgeStartRight");view.runCommand("delete");
            const auto& first=track->clips.back();
            expect(track->clips.size()==5&&std::abs(first.startSeconds-5.0)<1e-6&&first.durationSeconds>0.01&&first.durationSeconds<0.5,"Option+Right trims the range start by the grid value");
            // Copy the range and paste it at the cursor.
            view.runCommand("nudgeStartLeft"); // back to 5–6 s, which still holds the first grid step
            view.runCommand("copy");engine.seek(8.0);view.runCommand("paste");
            bool pasted=false;for(auto& c:track->clips)pasted|=std::abs(c.startSeconds-8.0)<1e-6;
            expect(pasted,"Copy and Paste with a range place its content at the cursor");
        }
        else expect(false,"range tool and arrangement exist");
        view.runCommand("toolSelect");engine.clear();
    }
    void snapChecks()
    {
        ui::SnapGrid g;const double pps=100.0; // 120 BPM: beat 0.5 s, 1/16 0.125 s
        expect(std::abs(g.snap(0.30,120,pps,{},10.0)-0.25)<1e-9,"snap grid 1/16 at 120 BPM");
        g.grid=ui::GridType::bar;expect(std::abs(g.snap(1.2,120,pps,{},10.0)-2.0)<1e-9,"snap grid bar");
        g.grid=ui::GridType::adaptToZoom;expect(std::abs(g.unitSeconds(120,pps)-0.25)<1e-9&&std::abs(g.unitSeconds(120,10.0)-2.0)<1e-9,"adapt to zoom follows the horizontal zoom");
        g.type=ui::SnapType::events;expect(std::abs(g.snap(1.33,120,pps,{1.30,4.0},0)-1.30)<1e-9&&std::abs(g.snap(1.50,120,pps,{1.30},0)-1.50)<1e-9,"events are magnetic within 10 px only");
        g.type=ui::SnapType::gridCursor;g.grid=ui::GridType::beat;expect(std::abs(g.snap(0.97,120,pps,{},0.93)-0.93)<1e-9&&std::abs(g.snap(0.70,120,pps,{},0.93)-0.5)<1e-9,"grid + cursor: cursor wins nearby, grid elsewhere");
        g.enabled=false;expect(std::abs(g.snap(0.97,120,pps,{},0.93)-0.97)<1e-9,"snap off keeps the exact position");
        expect(ui::SnapGrid::formatTime(5.25,ui::TimeFormat::barsBeats,120,48000)=="3. 3. 3. 0"&&ui::SnapGrid::formatTime(61.5,ui::TimeFormat::seconds,120,48000)=="0:01:01.500"
               &&ui::SnapGrid::formatTime(2.5,ui::TimeFormat::timecode,120,48000)=="00:00:02:15"&&ui::SnapGrid::formatTime(1.0,ui::TimeFormat::samples,120,48000)=="48000","ruler time formats");
    }
    void trackListChecks()
    {
        engine.clear();
        auto vox=engine.addAudioTrack("Voz principal");auto gtr=engine.addAudioTrack("Guitarra ritmica");engine.addMidiTrack("Piano");engine.addTrack(TrackKind::fx,"Reverb");
        ui::TrackFilter f;
        expect(f.visible(engine.tracks).size()==4,"track list shows every track by default");
        f.hiddenKinds.insert((int)TrackKind::audio);
        expect(f.visible(engine.tracks).size()==2&&f.filtering(),"track type filter hides audio tracks");
        expect(f.search(engine.tracks,"voz").empty(),"find tracks skips types hidden by the filter");
        f.hiddenKinds.clear();f.hiddenTracks.insert(vox->id);
        const auto found=f.search(engine.tracks,"VOZ princ");
        expect(found.size()==1&&engine.tracks[(size_t)found[0]]==vox,"find tracks matches words ignoring case, hidden tracks included");
        f.reveal(*vox);expect(f.visible(engine.tracks).size()==4,"picking a found track shows it again");
        f.showOnly(engine.tracks,*gtr);const auto only=f.visible(engine.tracks);
        expect(only.size()==1&&engine.tracks[(size_t)only[0]]==gtr,"Shift-click in Visibility shows only that track");
        engine.clear();
    }
    void mixConsoleChecks()
    {
        engine.clear();
        auto audio=engine.addAudioTrack("Mix audio");engine.addTrack(TrackKind::folder,"Mix folder");auto fx=engine.addTrack(TrackKind::fx,"Mix FX");engine.addMidiTrack("Mix synth");auto bus=engine.addTrack(TrackKind::group,"Mix Bus");
        expect(engine.setOutput(*audio,bus->id).wasOk()&&engine.setSend(*audio,0,fx->id).wasOk(),"routing API accepts audio-to-group output and send to FX");audio->sends[0].levelDb=-6.0f;
        engine.setSend(*audio,15,fx->id);
        int undos=0,changes=0;ui::MixConsoleHooks hooks;hooks.beforeEdit=[&undos]{++undos;};hooks.changed=[&changes]{++changes;};hooks.selected=[]{return 0;};
        ui::MixConsole console(engine,manager,hooks);console.setSize(1200,760);
        expect(console.visibleStripCount()==5,"MixConsole shows audio, FX, instrument and group channels plus Stereo Out, not the folder");
        expect(find(console,"mix-send-0-0")!=nullptr&&find(console,"mix-send-0-15")!=nullptr,"MixConsole provides sixteen send slots per channel");
        if (auto* scroll=dynamic_cast<Viewport*>(find(console,"mix-send-scroll-0")))
        {
            auto* fader=find(console,"mix-fader-0"); const auto before=fader->getBounds();
            scroll->setViewPosition(0,10000);
            auto* last=find(console,"mix-send-0-15");
            expect(last&&scroll->getViewArea().contains(last->getBounds())&&fader->getBounds()==before&&fader->getHeight()>100,"Scrolling to send 16 preserves the channel fader and reveals the last slot");
            expect(dynamic_cast<TextButton*>(last)&&dynamic_cast<TextButton*>(last)->getButtonText().contains(fx->name),"The visible last send displays its FX destination");
            if(auto* level=dynamic_cast<Slider*>(find(console,"mix-send-level-0-15")))
            {
                engine.setSend(*audio,15,fx->id);level->setValue(-18.0,sendNotificationSync);
                expect(std::abs(audio->sends[15].levelDb.load()+18.0f)<1e-4f&&changes>0,"Send 16 level edits its own model slot");
            }
            capture(console,output.getChildFile("mixconsole-sixteen-sends.png"));
        }
        else expect(false,"Send rack scrollbar exists");
        {
            ui::ChannelTab channel(engine,manager);channel.setSize(160,800);channel.setTrack(0);
            if(auto* scroll=dynamic_cast<ScrollBar*>(find(channel,"channel-send-scroll")))
            {
                scroll->setCurrentRangeStart(12,sendNotificationSync);
                expect(scroll->getCurrentRangeStart()==12&&scroll->getCurrentRangeSize()==4,"Channel column scrolls to sends 13–16 within its own rack");
                capture(channel,output.getChildFile("channel-sixteen-sends.png"));
            }
            else expect(false,"Channel send scrollbar exists");
        }
        if(auto* mute=dynamic_cast<TextButton*>(find(console,"mix-mute-0"))){mute->setToggleState(true,dontSendNotification);if(mute->onClick)mute->onClick();expect(audio->mute.load()&&undos>0&&changes>0,"MixConsole mute edits the track with undo and dirty flag");}
        else expect(false,"MixConsole mute button exists");
        if(auto* fader=dynamic_cast<Slider*>(find(console,"mix-fader-2"))){fader->setValue(-6.0,sendNotificationSync);expect(std::abs(fx->gain.load()-Decibels::decibelsToGain(-6.0f))<1e-4f,"MixConsole fader sets channel gain in dB");}
        else expect(false,"MixConsole fader exists");
        if(auto* volume=dynamic_cast<Label*>(find(console,"mix-volume-0"))){volume->setText("-12,5",sendNotificationSync);expect(std::abs(audio->gain.load()-Decibels::decibelsToGain(-12.5f))<1e-4f,"MixConsole typed volume accepts a decimal comma");}
        else expect(false,"MixConsole volume readout exists");
        capture(console,output.getChildFile("mixconsole.png"));
        {
            ui::MixConsole lower(engine,manager,hooks,true);lower.setSize(1000,260);
            auto* fader=find(lower,"mix-fader-0");
            expect(fader!=nullptr&&fader->getHeight()>40&&fader->getBottom()<=fader->getParentComponent()->getHeight(),"lower-zone MixConsole with folded racks keeps the fader inside the channel");
            if(auto* edge=dynamic_cast<ui::ZoneSplitter*>(find(lower,"mix-width-0"));edge&&fader)
            {   // Drag the right edge of channel 1 wider: only that channel grows, the next one moves.
                auto* strip=fader->getParentComponent();auto* next=find(lower,"mix-fader-1");const int width=strip->getWidth(),nextX=next?next->getParentComponent()->getX():0;
                edge->onStart();edge->onDrag(40);edge->onEnd();
                expect(strip->getWidth()==width+40&&engine.tracks[0]->stripWidth==width+40&&(!next||(next->getParentComponent()->getX()==nextX+40&&next->getParentComponent()->getWidth()==width)),"MixConsole channel width follows its edge drag; the others keep theirs");
                edge->onStart();edge->onDrag(-500);
                expect(strip->getWidth()==ui::MixConsole::minStripWidth,"MixConsole channel width stops at the minimum");
                lower.setStripWidth(0,0,true);for(auto& t:engine.tracks)t->stripWidth=0;
            }
            else expect(false,"MixConsole channel edge exists");
            capture(lower,output.getChildFile("mixconsole-lower.png"));
        }
        {
            ui::ExportDialog exportDialog(engine,0);exportDialog.setSize(820,520);capture(exportDialog,output.getChildFile("export-dialog.png"));
            ui::ConversionPanel conversion(engine.formatManager);conversion.setSize(860,540);capture(conversion,output.getChildFile("conversion.png"));
            expect(exportDialog.getWidth()==820&&conversion.getWidth()==860,"export and conversion windows build");
        }
        if(auto* search=dynamic_cast<TextEditor*>(find(console,"mix-search"))){search->setText("FX",false);if(search->onTextChange)search->onTextChange();expect(console.visibleStripCount()==2,"MixConsole search filters channels by name");}
        engine.clear();
    }
    void initialChecks()
    {
        expect(engine.tracks.empty()&&engine.positionSeconds==0&&!engine.playing,"normal startup has zero tracks and stopped transport");
        capture(view,output.getChildFile("empty.png"));
        if(auto* lower=dynamic_cast<TextButton*>(find(view,"zone-lower"));lower&&find(view,"arrangement")&&lower->onClick)
        {
            auto* arrangementView=find(view,"arrangement");const int before=arrangementView->getHeight();lower->onClick();const int hidden=arrangementView->getHeight();lower->onClick();
            expect(hidden>before&&arrangementView->getHeight()==before,"project window lower zone hides and shows from the toolbar");
            // Zone borders: drag the Inspector border and the lower zone border, then put them back.
            auto* leftSplit=dynamic_cast<ui::ZoneSplitter*>(find(view,"split-left"));auto* lowerSplit=dynamic_cast<ui::ZoneSplitter*>(find(view,"split-lower"));
            if(leftSplit&&lowerSplit&&leftSplit->isVisible()&&lowerSplit->isVisible())
            {
                const int x=arrangementView->getX(),height=arrangementView->getHeight();
                leftSplit->onStart();leftSplit->onDrag(60);lowerSplit->onStart();lowerSplit->onDrag(-50);
                expect(arrangementView->getX()==x+60&&arrangementView->getHeight()==height-50,"dragging the zone borders resizes the Inspector and the lower zone");
                leftSplit->onStart();leftSplit->onDrag(-60);lowerSplit->onStart();lowerSplit->onDrag(50); // no onEnd: the user's layout.xml is not written
                expect(arrangementView->getX()==x&&arrangementView->getHeight()==height,"zone borders return to their sizes");
            }
            else expect(false,"zone borders exist");
            auto* heightSlider=dynamic_cast<Slider*>(find(view,"track-height"));auto* waveSlider=dynamic_cast<Slider*>(find(view,"wave-zoom"));
            if(heightSlider&&waveSlider)
            {
                const double h0=heightSlider->getValue();view.runCommand("zoomInVertical");const double h1=heightSlider->getValue();view.runCommand("zoomOutVertical");
                expect(h1>h0&&std::abs(heightSlider->getValue()-h0)<=1.0,"Shift+H / Shift+G change the track height slider");
                expect(waveSlider->getMinimum()==1.0&&waveSlider->getMaximum()==16.0&&waveSlider->getRight()<=view.getWidth(),"waveform zoom slider at the right of the event display");
            }
            else expect(false,"track height and waveform zoom sliders exist");
        }
        else expect(false,"project window zone buttons exist");
        if(auto* right=dynamic_cast<TextButton*>(find(view,"zone-right"));right&&find(view,"arrangement")&&right->onClick)
        {
            auto* arrangementView=find(view,"arrangement");const int before=arrangementView->getWidth();right->onClick();const int hidden=arrangementView->getWidth();right->onClick();
            expect(hidden>before&&arrangementView->getWidth()==before,"project window right zone hides and shows from the toolbar");
        }
        auto* arrangement=find(view,"arrangement");expect(arrangement!=nullptr,"arrangement test surface exists");
        if(arrangement)
        {
            arrangement->mouseDown(mouse(*arrangement,350.5f,22)); // track header is 250 px wide
            expect(std::abs(engine.positionSeconds-100.5/75.0)<1e-8,"ruler click follows pointer without snap quantisation");
            arrangement->mouseDrag(mouse(*arrangement,565.25f,18,true));
            expect(std::abs(engine.positionSeconds-315.25/75.0)<1e-8,"ruler drag updates the transport continuously");
            arrangement->mouseDrag(mouse(*arrangement,625.75f,150,true));
            expect(std::abs(engine.positionSeconds-375.75/75.0)<1e-8,"ruler gesture continues when pointer moves below ruler");
            arrangement->mouseUp(mouse(*arrangement,625.75f,150,true));const double released=engine.positionSeconds;
            arrangement->mouseDrag(mouse(*arrangement,772,22,true));expect(engine.positionSeconds==released,"mouse release ends ruler gesture");
            arrangement->mouseDown(mouse(*arrangement,272,22));arrangement->mouseDrag(mouse(*arrangement,172,22,true));
            expect(engine.positionSeconds==0,"ruler drag clamps before project start");arrangement->mouseUp(mouse(*arrangement,172,22,true));
            engine.loopStart=1;engine.loopEnd=3;arrangement->mouseDown(mouse(*arrangement,325,5));arrangement->mouseDrag(mouse(*arrangement,400,5,true));arrangement->mouseUp(mouse(*arrangement,400,5,true));
            expect(engine.loopStart==2&&engine.loopEnd==3&&engine.positionSeconds==0,"cycle locator gesture remains independent");
        }
        KnownPluginList catalog;PluginDescription a;a.name="Test lowpass";a.manufacturerName="Silente";a.category="Filter";a.pluginFormatName="VST3";a.fileOrIdentifier="fixture1";a.uniqueId=1;a.deprecatedUid=1;
        catalog.addType(a);auto b=a;b.name="Test synth";b.isInstrument=true;b.fileOrIdentifier="fixture2";b.uniqueId=2;catalog.addType(b);
        expect(matchesPlugin(a,PluginKind::effect,"lowpass silente","VST3")&&!matchesPlugin(b,PluginKind::effect,"","")&&!matchesPlugin(a,PluginKind::effect,"","AudioUnit"),"selector separates effects, instruments, search tokens and formats");
        {
            const auto favouritesFile=output.getChildFile("test-favourites.json");favouritesFile.replaceWithText("[]");
            ui::PluginSelector selector(catalog,PluginKind::effect,"Test channel",favouritesFile);
            selector.setSize(540,540);capture(selector,output.getChildFile("selector.png"));
            bool chosen=false;selector.onChoose=[&](const auto& item){chosen=item.name==a.name;};
            if(auto* search=dynamic_cast<TextEditor*>(find(selector,"plugin-search"))){search->setText("lowpass",false);search->onTextChange();search->onReturnKey();}
            expect(chosen,"selector search and Return load the selected effect");
            if(auto* list=dynamic_cast<ListBox*>(find(selector,"plugin-results")))
                list->getModel()->listBoxItemClicked(1,mouse(*list,static_cast<float>(list->getWidth()-10),15));
            const auto saved=JSON::parse(output.getChildFile("test-favourites.json").loadFileAsString());
            expect(saved.isArray()&&saved.getArray()->contains(var(a.createIdentifierString())),"favourite star saves the plugin identity");
        }
    }
    void chooseFromSelector(const PluginDescription& description)
    {
        bool chosen=false;
        for(int i=0;i<Desktop::getInstance().getNumComponents();++i)
            if(auto* selector=dynamic_cast<ui::PluginSelector*>(find(*Desktop::getInstance().getComponent(i),"plugin-selector")))
            {capture(*selector,output.getChildFile(description.isInstrument?"instrument-selector.png":"effect-selector.png"));selector->onChoose(description);chosen=true;break;}
        expect(chosen,"host selector dispatches the chosen plugin");
    }
    void snapshotEditor(const String& name)
    {
        for(int i=0;i<Desktop::getInstance().getNumComponents();++i)
        {
            auto* component=Desktop::getInstance().getComponent(i);
            if(component->isVisible()&&component->getName().contains("Workflow test"))capture(*component,output.getChildFile(name));
        }
    }
    void timerCallback() override
    {
        if(stage==5&&Time::getMillisecondCounterHiRes()<deadline)return;
        if(stage!=5&&Time::getMillisecondCounterHiRes()>deadline){failures+="Timed out at plugin stage "+String(stage)+" | "+manager.status()+"\n";finish();return;}
        if(stage==7)
        {
            if(engine.tracks.size()<3||engine.tracks[1]->plugins.empty()||engine.tracks[2]->plugins.empty())return;
            expect(engine.tracks[1]->name=="Pianos 01"&&engine.tracks[2]->name=="Pianos 02"&&instrumentSlotIndex(*engine.tracks[1])==0&&instrumentSlotIndex(*engine.tracks[2])==0&&engine.tracks[1]->plugins[0]->processor&&engine.tracks[2]->plugins[0]->processor,"Add Track creates two named tracks with independent real AU instruments");
            manager.closeEditors();engine.removeTrack(2);engine.removeTrack(1);view.runCommand("trackTop");manager.insertPlugin(0,apple);stage=0;deadline=Time::getMillisecondCounterHiRes()+15000;return;
        }
        if(stage==0)
        {
            if(track->plugins.empty())return;
            first=track->plugins[0];expect(first->description.name=="AULowpass"&&first->processor!=nullptr,"Apple AU loads asynchronously through PluginManager");
            snapshotEditor("au-editor.png");manager.toggleBypass(0,0);expect(first->bypassed,"slot bypass enabled");manager.toggleBypass(0,0);expect(!first->bypassed,"slot bypass disabled");
            engine.prepareForOffline(48000,128);engine.play();AudioBuffer<float> block(2,128);block.clear();engine.renderOfflineBlock(block,128);engine.stop();
            expect(std::isfinite(block.getMagnitude(0,128)),"AU processes an offline block without invalid samples");
            manager.insertPlugin(0,apple);stage=1;deadline=Time::getMillisecondCounterHiRes()+15000;
        }
        else if(stage==1)
        {
            if(track->plugins.size()<2)return;
            expect(track->plugins[0]==first&&track->plugins[1]->description.name==apple.name,"effect insertion keeps serial chain order");
            manager.showEffectSelector(0,0);
            chooseFromSelector(apple);stage=3;deadline=Time::getMillisecondCounterHiRes()+15000;
        }
        else if(stage==3)
        {
            if(track->plugins[0]==first)return;
            expect(track->plugins.size()==2&&track->plugins[0]->description.name==apple.name,"selector replacement changes only the chosen slot");
            manager.removeSlot(0,0);expect(track->plugins.size()==1&&track->plugins[0]!=first,"removing a slot preserves the other effect");
            manager.removeSlot(0,0);expect(track->plugins.empty(),"removing the final slot leaves an empty effect chain");
            for(int n=0;n<Track::insertCount;++n)manager.insertPlugin(0,apple);
            stage=6;deadline=Time::getMillisecondCounterHiRes()+20000;
        }
        else if(stage==6)
        {
            if(track->plugins.size()<Track::insertCount)return;
            stopTimer(); // Popup verification pumps messages after leaving the repeating timer.
            manager.closeEditors();view.runCommand("trackTop");view.transportMenuItem(0);
            expect(track->plugins.size()==10&&std::all_of(track->plugins.begin(),track->plugins.end(),[](const auto& plugin){return plugin->processor!=nullptr;}),"Ten real Apple AU effects load independently without cancelling earlier callbacks");
            for(const String prefix:{String("inspector-inserts"),String("channel-inserts")})
            {
                auto* rack=dynamic_cast<ui::InsertRack*>(find(view,prefix));auto* last=find(view,prefix+"-slot-10");auto* scroll=dynamic_cast<Viewport*>(find(view,prefix+"-scroll"));
                if(rack)rack->revealSlot(9);
                expect(rack&&last&&scroll&&scroll->getViewArea().contains(last->getBounds()),prefix+": scroll reaches insertion slot ten");
            }
            ui::MixConsoleHooks hooks;hooks.selected=[]{return 0;};ui::MixConsole console(engine,manager,hooks);console.setSize(1200,820);
            auto* rack=dynamic_cast<ui::InsertRack*>(find(console,"mix-inserts-0"));auto* fader=find(console,"mix-fader-0");const auto bounds=fader?fader->getBounds():Rectangle<int>();
            if(rack)rack->revealSlot(9);auto* scroll=dynamic_cast<Viewport*>(find(console,"mix-inserts-0-scroll"));auto* last=find(console,"mix-inserts-0-slot-10");
            expect(rack&&scroll&&last&&scroll->getViewArea().contains(last->getBounds())&&fader&&fader->getBounds()==bounds&&fader->getHeight()>100,"MixConsole reveals insert ten while preserving fader height");
            capture(console,output.getChildFile("ten-inserts-mixconsole.png"));capture(view,output.getChildFile("ten-inserts-inspector.png"));
            if(auto* edit=dynamic_cast<TextButton*>(find(view,"channel-settings-button")))edit->onClick();
            bool settingsTen=false;
            for(int n=0;n<Desktop::getInstance().getNumComponents();++n)
            {
                auto* window=Desktop::getInstance().getComponent(n);auto* settingsRack=dynamic_cast<ui::InsertRack*>(find(*window,"settings-inserts"));
                if(settingsRack){settingsRack->revealSlot(9);auto* scroll=dynamic_cast<Viewport*>(find(*window,"settings-inserts-scroll"));auto* tenth=find(*window,"settings-inserts-slot-10");settingsTen=scroll&&tenth&&scroll->getViewArea().contains(tenth->getBounds());capture(*window,output.getChildFile("ten-inserts-settings.png"));window->setVisible(false);}
            }
            expect(settingsTen,"Channel Settings exposes all ten inserts with individual bypass buttons");
            auto* finalSlot=dynamic_cast<ui::PluginSlotButton*>(find(view,"inspector-inserts-slot-10"));if(finalSlot)finalSlot->onOptions();
            MessageManager::getInstance()->runDispatchLoopUntil(30);auto* popup=Component::getCurrentlyModalComponent();
            if(popup){for(int n=0;n<3;++n)popup->keyPressed(KeyPress(KeyPress::downKey));popup->keyPressed(KeyPress(KeyPress::returnKey));}
            MessageManager::getInstance()->runDispatchLoopUntil(30);
            expect(track->plugins[9]->bypassed.load()&&!track->plugins[0]->bypassed.load(),"Slot ten's real options menu bypasses only its own processor");
            AudioBuffer<float> block(2,128);block.clear();engine.play();engine.renderOfflineBlock(block,128);engine.stop();expect(std::isfinite(block.getMagnitude(0,128)),"Ten real AU processors render without invalid samples");
            for(int n=(int)track->plugins.size()-1;n>=0;--n)manager.removeSlot(0,n);
            if(auto xml=XmlDocument::parse(fixture))if(auto* item=xml->getChildByName("PLUGIN"))vst.loadFromXml(*item);
            if(vst.name.isEmpty()){report+="SKIP: optional installed VST3 fixture not supplied\n";finish();return;}
            manager.insertPlugin(0,vst);stage=2;deadline=Time::getMillisecondCounterHiRes()+20000;startTimer(50);
        }
        else if(stage==2)
        {
            if(track->plugins.empty())return;
            expect(track->plugins[0]->description.pluginFormatName=="VST3"&&track->plugins[0]->processor!=nullptr,"installed VST3 loads asynchronously through PluginManager");
            snapshotEditor("vst3-editor.png");
            AudioBuffer<float> block(2,128);block.clear();engine.play();engine.renderOfflineBlock(block,128);engine.stop();expect(std::isfinite(block.getMagnitude(0,128)),"VST3 processes an offline block without invalid samples");
            manager.removeSlot(0,0);
            if(instrument.name.isEmpty()){report+="SKIP: Apple instrument unavailable\n";finish();return;}
            const auto before=engine.tracks.size();manager.showInstrumentSelector();expect(engine.tracks.size()==before,"opening instrument selector creates no track");
            chooseFromSelector(instrument);stage=4;deadline=Time::getMillisecondCounterHiRes()+15000;
        }
        else if(stage==4)
        {
            if(engine.tracks.size()<2)return;
            const auto& midi=*engine.tracks[1];expect(midi.isMidi&&instrumentSlotIndex(midi)==0,"choosing an instrument creates one MIDI track with its instrument first");
            manager.showInstrumentSelector(1);chooseFromSelector(internalInstrumentDescription());
            expect(engine.tracks.size()==2&&engine.tracks[1]->plugins.empty(),"selecting the internal synth replaces the instrument without adding a track");
            manager.closeEditors();engine.clear();
            auto deleted=engine.addAudioTrack("Deleted target");engine.addAudioTrack("Surviving target");
            manager.insertPlugin(0,apple);engine.removeTrack(0);stage=5;deadline=Time::getMillisecondCounterHiRes()+500;
        }
        else if(stage==5)
        {
            expect(engine.tracks.size()==1&&engine.tracks[0]->plugins.empty(),"late plugin callback never inserts into a different track after deletion");finish();
        }
    }
    void finish()
    {
        stopTimer();manager.closeEditors();engine.clear();view.refreshLanguage();
        output.getChildFile("workflow.txt").replaceWithText(report+(failures.isEmpty()?"PASS: workflow checks completed\n":"FAIL:\n"+failures));
        auto callback=std::move(complete);const int code=failures.isEmpty()?0:1;
        MessageManager::callAsync([this,callback=std::move(callback),code]{delete this;callback(code);});
    }
};
}
void runWorkflowChecks(MainComponent& view,AudioEngine& engine,PluginManager& manager,const juce::File& output,const juce::File& fixture,std::function<void(int)> complete)
{
    new WorkflowChecks(view,engine,manager,output,fixture,std::move(complete));
}
}

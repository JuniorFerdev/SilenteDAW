#include "WorkflowUiTests.h"
#include "../src/MainComponent.h"
#include "../src/ui/plugins/PluginSelector.h"
#include "../src/ui/mixconsole/MixConsole.h"
#include "../src/ui/io/ConversionPanel.h"
#include "../src/ui/io/ExportDialog.h"
#include "../src/ui/project/TrackFilter.h"
#include "../src/ui/project/SnapGrid.h"
#include "../src/ui/project/ZoneSplitter.h"
#include "../src/ui/project/FadeEditor.h"
#include "../src/app/KeyCommands.h"
#include <cmath>

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
MouseEvent mouse(Component& component,float x,float y,bool dragged=false)
{
    return {Desktop::getInstance().getMainMouseSource(),{x,y},ModifierKeys(ModifierKeys::leftButtonModifier),1.0f,0,0,0,0,&component,&component,Time::getCurrentTime(),{350.5f,22},Time::getCurrentTime(),1,dragged};
}
class WorkflowChecks final : private Timer
{
public:
    WorkflowChecks(MainComponent& v,AudioEngine& e,PluginManager& p,const File& dir,const File& vst,std::function<void(int)> cb)
        :view(v),engine(e),manager(p),output(dir),fixture(vst),complete(std::move(cb))
    {
        output.createDirectory();initialChecks();mixConsoleChecks();trackListChecks();snapChecks();keyCommandChecks();
        engine.clear();track=engine.addAudioTrack("Workflow test");
        for(auto* format:engine.pluginFormats.getFormats())if(format->getName()=="AudioUnit")
        {
            OwnedArray<PluginDescription> found;format->findAllTypesForFile(found,"AudioUnit:Effects/aufx,lpas,appl");
            if(!found.isEmpty())apple=*found[0];
            found.clear();format->findAllTypesForFile(found,"AudioUnit:Synths/aumu,dls ,appl");
            if(!found.isEmpty())instrument=*found[0];
        }
        if(apple.name.isEmpty()){failures+="Apple AULowpass was not found\n";finish();return;}
        manager.insertPlugin(0,apple);deadline=Time::getMillisecondCounterHiRes()+15000;startTimer(50);
    }
private:
    MainComponent& view;AudioEngine& engine;PluginManager& manager;
    File output,fixture;std::function<void(int)> complete;
    String failures,report;PluginDescription apple,vst,instrument;std::shared_ptr<Track> track;std::shared_ptr<PluginSlot> first;
    double deadline=0;int stage=0;
    void expect(bool result,const String& name){if(!result)failures+=name+"\n";else report+="PASS: "+name+"\n";}
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
        view.transportMenuItem(0); // refresh: the punch out closes the take
        expect(!engine.recording.load()&&engine.playing.load(),"punch out ends recording, playback continues");
        expect(track->clips.size()==1&&std::abs(track->clips[0].startSeconds-0.1)<1e-4&&std::abs(track->clips[0].durationSeconds-0.1)<1e-4,"punched take sits between the locators");
        expect(view.editHistory().size()>0&&view.editHistory()[view.editHistory().size()-1]==i18n::tr("Grabar"),"recording is an undo step named Grabar");
        view.runCommand("undo");expect(engine.tracks.size()>0&&engine.tracks.back()->clips.empty(),"undo removes the recorded take");
        view.runCommand("stop");view.runCommand("punchIn");view.runCommand("punchOut");
        engine.clear();engine.prepareForOffline(48000,128);
        fadeChecks();
    }
    void fadeChecks()
    {   // Fundidos (manual pp. 396–418): Audio › Fundidos on the selected event and the event handles.
        auto track=engine.addAudioTrack("Fundidos");AudioClip clip;clip.sourceRate=48000;clip.durationSeconds=4.0;clip.fadeIn=clip.fadeOut=0.0;
        clip.audio=std::make_shared<AudioBuffer<float>>(1,48000*4);clip.audio->clear();track->clips.push_back(clip);
        view.runCommand("trackTop");view.runCommand("eventRight");
        view.audioMenuItem(201);view.audioMenuItem(202);
        expect(std::abs(track->clips[0].fadeIn-0.5)<1e-9&&std::abs(track->clips[0].fadeOut-0.5)<1e-9,"Audio > Fundidos applies the standard fade-in and fade-out");
        view.audioMenuItem(205);expect(track->clips[0].fadeIn==0.0&&track->clips[0].fadeOut==0.0,"Eliminar fundidos removes both fades");
        if(auto* arrangement=find(view,"arrangement"))
        {   // Event at 0 s on the first track: x = 250 + seconds × 75, rows 60 px under a 30 px ruler.
            arrangement->mouseDown(mouse(*arrangement,255,64));arrangement->mouseDrag(mouse(*arrangement,330,64,true));arrangement->mouseUp(mouse(*arrangement,330,64,true));
            expect(std::abs(track->clips[0].fadeIn-1.0)<0.02,"dragging the fade-in handle sets the fade length");
            arrangement->mouseDown(mouse(*arrangement,258,76));arrangement->mouseDrag(mouse(*arrangement,258,26,true));arrangement->mouseUp(mouse(*arrangement,258,26,true));
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
        auto track=engine.addAudioTrack("Rango");AudioClip clip;clip.sourceRate=48000;clip.durationSeconds=4.0;
        clip.audio=std::make_shared<AudioBuffer<float>>(1,48000*4);clip.audio->clear();track->clips.push_back(clip);
        view.runCommand("toolRange");auto* rangeButton=dynamic_cast<TextButton*>(find(view,"tool-range"));
        auto* arrangement=find(view,"arrangement");
        if(arrangement&&rangeButton)
        {
            arrangement->mouseDown(mouse(*arrangement,325,64));arrangement->mouseDrag(mouse(*arrangement,400,64,true));arrangement->mouseUp(mouse(*arrangement,400,64,true));
            capture(view,output.getChildFile("range.png"));
            view.runCommand("deleteTime");
            expect(rangeButton->getToggleState()&&track->clips.size()==2&&std::abs(track->clips[1].startSeconds-1.0)<1e-6&&std::abs(track->clips[1].offsetSeconds-2.0)<1e-6,"range tool: drag a range and Shift+Backspace deletes the time");
            view.runCommand("undo");expect(track->clips.size()==1&&std::abs(track->clips[0].durationSeconds-4.0)<1e-9,"range edit undoes in one step");
            view.runCommand("insertSilence");expect(track->clips.size()==2&&std::abs(track->clips[1].startSeconds-2.0)<1e-6,"Insert Silence moves the rest by the range length");
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
        int undos=0,changes=0;ui::MixConsoleHooks hooks;hooks.beforeEdit=[&undos]{++undos;};hooks.changed=[&changes]{++changes;};hooks.selected=[]{return 0;};
        ui::MixConsole console(engine,manager,hooks);console.setSize(1200,760);
        expect(console.visibleStripCount()==5,"MixConsole shows audio, FX, instrument and group channels plus Stereo Out, not the folder");
        expect(find(console,"mix-send-0-0")!=nullptr&&find(console,"mix-send-0-3")!=nullptr,"MixConsole shows four send slots per channel");
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
            if(auto xml=XmlDocument::parse(fixture))if(auto* item=xml->getChildByName("PLUGIN"))vst.loadFromXml(*item);
            if(vst.name.isEmpty()){report+="SKIP: optional installed VST3 fixture not supplied\n";finish();return;}
            manager.insertPlugin(0,vst);stage=2;deadline=Time::getMillisecondCounterHiRes()+20000;
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

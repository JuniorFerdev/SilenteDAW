#include "WorkflowUiTests.h"
#include "SettingsUiTests.h"
#include "ToolboxUiTests.h"
#include "MarkerUiTests.h"
#include "FolderUiTests.h"
#include "PoolUiTests.h"
#include "AutomationUiTests.h"
#include "VideoUiTests.h"
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
#include "../src/app/MidiEditCommands.h"
#include "../src/ui/tempo/TempoMapPanel.h"
#include "../src/ui/automation/AutomationLane.h"
#include "../src/ui/mixconsole/MixConsole.h"
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
        pianoRollChecks();tempoMapChecks();automationChecks();
        runFolderUiChecks(view,engine,output,[this](bool ok,const String& label){expect(ok,label);});
        runPoolUiChecks(view,engine,output,[this](bool ok,const String& label){expect(ok,label);});
        runAutomationUiChecks(view,engine,[this](bool ok,const String& label){expect(ok,label);});
        runVideoUiChecks(view,engine,output,[this](bool ok,const String& label){expect(ok,label);});
        {   // Abrir proyecto con un .cpr entra en el flujo de importación; otras extensiones no (el selector nativo no es testeable)
            File routed;view.cubaseImportOverride=[&routed](const File& f){routed=f;};
            const auto cprFile=output.getChildFile("open-route.cpr");
            expect(view.openProjectFile(cprFile)&&routed==cprFile,"open: a .cpr goes to the Cubase import flow");
            routed=File();
            expect(!view.openProjectFile(output.getChildFile("open-route.silente"))&&routed==File(),"open: a .silente is not routed to the import");
            view.cubaseImportOverride=nullptr;
        }
        waveAndHeightChecks();
        zoomBarChecks();
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
    void zoomBarChecks()
    {   // Barras de zoom como Cubase: horizontal (abajo a la derecha) y vertical (borde derecho; altura y onda).
        engine.clear();for(const char* name:{"Z1","Z2"}){auto t=engine.addAudioTrack(name);AudioClip c;c.sourceRate=48000;c.durationSeconds=4;c.startSeconds=25;c.audio=std::make_shared<AudioBuffer<float>>(1,48000*4);for(int i=0;i<48000*4;++i)c.audio->setSample(0,i,0.8f*std::sin(i*0.05f));t->clips.push_back(c);}
        auto* a=find(view,"arrangement");auto* h=dynamic_cast<Slider*>(find(view,"h-zoom"));auto* out=dynamic_cast<TextButton*>(find(view,"h-zoom-out"));auto* in=dynamic_cast<TextButton*>(find(view,"h-zoom-in"));auto* wave=dynamic_cast<Slider*>(find(view,"wave-zoom"));
        if(!a||!h||!out||!in||!wave){expect(false,"zoom bars: horizontal slider, buttons and wave slider exist");return;}
        const auto pps=[&]{return (double)(view.projectXForTime(10)-view.projectXForTime(0))/10.0;};
        expect(h->isShowing()&&out->isShowing()&&in->isShowing()&&h->getBottom()>=a->getBottom()&&h->getX()>a->getX()+a->getWidth()/2&&h->getRight()<=view.getWidth(),"zoom bars: horizontal bar sits below the event display at the right");
        const double original=pps();view.runCommand("goStart");h->setValue(75.0,sendNotificationSync);
        const double base=pps();expect(std::abs(base-75.0)<0.5,"zoom bars: slider value 75 gives 75 px/s ("+String(base)+")");
        const auto before=a->createComponentSnapshot(a->getLocalBounds());
        in->onClick();const double up=pps();expect(std::abs(up/base-1.25)<0.01&&std::abs(h->getValue()-up)<0.1,"zoom bars: + zooms in one step (x1.25) and the slider follows");
        const auto after=a->createComponentSnapshot(a->getLocalBounds());int differing=0;for(int x=0;x<before.getWidth();x+=3)for(int y=0;y<before.getHeight();y+=3)if(before.getPixelAt(x,y)!=after.getPixelAt(x,y))++differing;
        expect(differing>200,"zoom bars: the window repaints with the new scale ("+String(differing)+" px changed)");
        out->onClick();out->onClick();expect(std::abs(pps()/up-1/1.25/1.25)<0.01,"zoom bars: - zooms out one step (x0.8) each time");
        const double mid=75.0;h->setValue(mid,sendNotificationSync);
        { // arrastre real del deslizador: del centro hacia la derecha
            const auto y=(float)h->getHeight()*.5f;const float x0=(float)h->valueToProportionOfLength(h->getValue())*h->getWidth()*0+(float)h->getPositionOfValue(h->getValue());
            h->mouseDown(mouse(*h,x0,y));h->mouseDrag(mouse(*h,x0+(float)h->getWidth()*.25f,y,true));h->mouseUp(mouse(*h,x0+(float)h->getWidth()*.25f,y,true));
            expect(pps()>mid*1.2,"zoom bars: dragging the horizontal slider to the right zooms in ("+String(pps())+" px/s)"); }
        h->setValue(1.0,sendNotificationSync);expect(std::abs(pps()-12.0)<0.01,"zoom bars: lower limit is 12 px/s");
        h->setValue(10000.0,sendNotificationSync);expect(std::abs(pps()-480.0)<0.01,"zoom bars: upper limit is 480 px/s");
        for(int i=0;i<5;++i)in->onClick();expect(pps()<=480.0+1e-6,"zoom bars: + stops at the limit");
        for(int i=0;i<40;++i)out->onClick();expect(pps()>=12.0-1e-6,"zoom bars: - stops at the limit");
        { // rueda con ⌘ y pellizco: el tiempo bajo el puntero no se mueve
            h->setValue(75.0,sendNotificationSync);view.runCommand("goStart");
            auto e=mouse(*a,500.0f,100.0f,false,ModifierKeys::commandModifier);const double secAt=(500.0-250.0)/pps();
            MouseWheelDetails w{};w.deltaY=1.0f;a->mouseWheelMove(e,w);
            expect(pps()>75.0&&std::abs((view.projectXForTime(secAt)-500.0f))<1.5f,"zoom bars: Cmd+wheel zooms keeping the time under the pointer");
            const double p1=pps();a->mouseMagnify(mouse(*a,500.0f,100.0f),1.3f);expect(pps()>p1*1.2,"zoom bars: trackpad pinch zooms"); }
        // Vertical: onda de todas las pistas y altura
        wave->setValue(1.0,sendNotificationSync);expect(view.waveZoomValue()==1.0f,"zoom bars: wave zoom starts at 1");
        const auto savedDragEnd=wave->onDragEnd;wave->onDragEnd=nullptr; // sin escribir el layout.xml del usuario
        { const float x=(float)wave->getWidth()*.5f,y0=(float)wave->getPositionOfValue(1.0),y1=y0-(float)wave->getHeight()*.5f;
          wave->mouseDown(mouse(*wave,x,y0));wave->mouseDrag(mouse(*wave,x,y1,true));wave->mouseUp(mouse(*wave,x,y1,true)); }
        wave->onDragEnd=savedDragEnd;
        expect(view.waveZoomValue()>1.5f&&std::abs(wave->getValue()-view.waveZoomValue())<1e-3,"zoom bars: dragging the wave slider enlarges the wave of every track ("+String(view.waveZoomValue())+")");
        wave->setValue(100.0,sendNotificationSync);expect(view.waveZoomValue()==16.0f,"zoom bars: wave zoom upper limit is 16");
        wave->setValue(0.0,sendNotificationSync);expect(view.waveZoomValue()==1.0f,"zoom bars: wave zoom lower limit is 1");
        wave->setValue(1.0,sendNotificationSync);h->setValue(original,sendNotificationSync);view.runCommand("goStart");engine.clear();
    }
    void waveAndHeightChecks()
    {   // Ondas visibles tras guardar y abrir, y altura de pistas como Cubase (arrastrar el borde inferior, ⌘ para todas, menú de zoom).
        engine.clear();view.runCommand("trackZoomRows1");
        auto* arrangement=find(view,"arrangement");auto* slider=dynamic_cast<Slider*>(find(view,"track-height"));
        if(!arrangement||!slider||!find(view,"track-zoom-menu")){expect(false,"track height: arrangement, slider and zoom menu button exist");return;}
        {
            auto wave=engine.addAudioTrack("Onda");AudioClip clip;clip.sourceRate=48000;clip.durationSeconds=4.0;clip.audio=std::make_shared<AudioBuffer<float>>(1,48000*4);
            for(int i=0;i<48000*4;++i)clip.audio->setSample(0,i,0.8f*std::sin(i*0.05f));
            wave->clips.push_back(clip);
            const auto file=output.getChildFile("wave-roundtrip.silente");
            const bool saved=engine.saveProject(file).wasOk();engine.clear();const bool loaded=saved&&engine.loadProject(file).wasOk();
            expect(loaded&&engine.tracks.size()==1&&engine.tracks[0]->clips.size()==1&&engine.tracks[0]->clips[0].audio&&engine.tracks[0]->clips[0].audio->getNumSamples()>0,"waveform: a saved and reopened clip has its audio loaded");
            view.runCommand("goStart");view.runCommand("trackZoomRows1");
            const auto image=arrangement->createComponentSnapshot(arrangement->getLocalBounds());
            const int x0=(int)view.projectXForTime(0.0),top=30+8; // first row under the 30 px ruler; the event starts 8 px below it
            const auto body=image.getPixelAt(x0+3,top+42);int differing=0;
            for(int x=x0+10;x<x0+210;++x)for(int y=top+23;y<top+27;++y)if(image.getPixelAt(x,y)!=body)++differing; // rows 23-26 of the event: below the 15 px title; the old 22 px title with 10 px of wave never drew there
            expect(differing>150,"waveform: an audio event at the default track height draws wave pixels below its title ("+String(differing)+" px)");
            capture(*arrangement,output.getChildFile("waveform-visible.png"));
        }
        engine.clear();for(const char* name:{"A","B","C"})engine.addAudioTrack(name);
        view.runCommand("trackZoomRows1");view.runCommand("goStart");
        const auto bottomOf=[&](int rowTop,int height){return rowTop+height;};
        arrangement->mouseDown(mouse(*arrangement,100.0f,(float)bottomOf(30,60)));arrangement->mouseDrag(mouse(*arrangement,100.0f,(float)bottomOf(30,60)+30,true));arrangement->mouseUp(mouse(*arrangement,100.0f,(float)bottomOf(30,60)+30,true));
        expect(view.trackHeightOf(0)==90&&view.trackHeightOf(1)==60&&view.trackHeightOf(2)==60&&slider->getValue()==60.0,"track height: dragging the lower edge of one track changes only that track");
        expect(MainComponent::settingsFolder().getChildFile("layout.xml").loadFileAsString().contains(engine.tracks[0]->id+":90"),"track height: the per-track height is saved in the layout");
        arrangement->mouseDown(mouse(*arrangement,100.0f,120.0f,false,ModifierKeys::commandModifier));arrangement->mouseDrag(mouse(*arrangement,100.0f,100.0f,true,ModifierKeys::commandModifier));arrangement->mouseUp(mouse(*arrangement,100.0f,100.0f,true,ModifierKeys::commandModifier));
        expect(view.trackHeightOf(0)==70&&view.trackHeightOf(1)==70&&view.trackHeightOf(2)==70&&slider->getValue()==70.0,"track height: Cmd + dragging the lower edge changes every track");
        arrangement->mouseDown(mouse(*arrangement,100.0f,100.0f));arrangement->mouseDrag(mouse(*arrangement,100.0f,900.0f,true));arrangement->mouseUp(mouse(*arrangement,100.0f,900.0f,true));
        expect(view.trackHeightOf(0)==240,"track height: the drag stops at the maximum height");
        arrangement->mouseDown(mouse(*arrangement,100.0f,270.0f));arrangement->mouseDrag(mouse(*arrangement,100.0f,-500.0f,true));arrangement->mouseUp(mouse(*arrangement,100.0f,-500.0f,true));
        expect(view.trackHeightOf(0)==40,"track height: the drag stops at the minimum height");
        for(int rows=1;rows<=4;++rows){view.runCommand("trackZoomRows"+String(rows));expect(view.trackHeightOf(0)==60*rows&&view.trackHeightOf(2)==60*rows&&slider->getValue()==60.0*rows,"track zoom: "+String(rows)+" row(s) sets every track to "+String(60*rows)+" px");}
        view.runCommand("trackZoomMinimum");expect(view.trackHeightOf(1)==40,"track zoom: minimum");
        const int avail=arrangement->getHeight()-30;
        const auto fit=[&](int count){return jlimit(40,240,avail/count);};
        view.runCommand("trackZoomFull");expect(view.trackHeightOf(0)==fit(3)&&view.trackHeightOf(2)==fit(3),"track zoom: full makes every track fit the window");
        view.runCommand("trackZoomFour");expect(view.trackHeightOf(0)==fit(4),"track zoom: 4 tracks");
        view.runCommand("trackZoomEight");expect(view.trackHeightOf(0)==fit(8),"track zoom: 8 tracks");
        view.runCommand("trackZoomRows1");arrangement->mouseDown(mouse(*arrangement,228.0f,30.0f+60.0f+30.0f));arrangement->mouseUp(mouse(*arrangement,228.0f,30.0f+60.0f+30.0f));
        view.runCommand("expandTrack");
        expect(view.trackHeightOf(1)==jlimit(40,240,avail)&&view.trackHeightOf(0)==60,"Edit > Expand selected track: the selected track takes the window height");
        view.runCommand("trackZoomRows1");engine.clear();
    }
    void tempoMapChecks()
    {   // Estudio › Mapa de tempo y de compás: add, edit, refuse, remove; one undo step each.
        engine.clear();engine.tempo=120;view.showTempoMap();
        ui::TempoMapPanel* panel=nullptr;
        for(int n=0;n<Desktop::getInstance().getNumComponents()&&!panel;++n)panel=dynamic_cast<ui::TempoMapPanel*>(find(*Desktop::getInstance().getComponent(n),"tempo-map-panel"));
        expect(panel!=nullptr&&panel->isShowing(),"tempo map window opens");
        if(!panel)return;
        const auto before=view.editHistory().size();
        panel->kind.setSelectedId(1,dontSendNotification);panel->position.setText("3. 1. 1. 0");panel->bpm.setText("90");panel->ramp.setToggleState(false,dontSendNotification);
        if(auto* add=dynamic_cast<TextButton*>(find(*panel,"tempo-map-add")))add->onClick();
        expect(engine.tempoMap.tempos.size()==1&&engine.tempoMap.tempos[0].beat==8.0&&engine.tempoMap.tempos[0].bpm==90.0,"tempo map: Add puts 90 BPM at bar 3");
        expect(view.editHistory().size()==before+1,"tempo map: an edit is one undo step");
        panel->kind.setSelectedId(2,dontSendNotification);panel->position.setText("2");panel->numerator.setText("3");panel->denominator.setSelectedId(4,dontSendNotification);
        expect(panel->addFromFields().isEmpty()&&engine.tempoMap.signatures.size()==1&&engine.tempoMap.signatures[0].bar==1&&engine.tempoMap.signatures[0].numerator==3,"tempo map: 3/4 from bar 2");
        expect(std::abs(engine.tempoView().secondsAt(12.0)-(4.0+4.0*60.0/90.0))<1e-9,"tempo map: time after the jump");
        expect(ui::SnapGrid::formatTime(engine.tempoView().secondsAt(7.0),ui::TimeFormat::barsBeats,engine.tempoView(),48000)=="3. 1. 1. 0","tempo map: Compases+Tiempos follow 3/4");
        panel->selectRow(1);
        expect(panel->rowCount()==2&&panel->kind.getSelectedId()==1&&panel->bpm.getText().getDoubleValue()==90.0,"tempo map: selecting a row fills the fields");
        panel->bpm.setText("100");panel->ramp.setToggleState(true,dontSendNotification);
        expect(panel->updateSelected().isEmpty()&&engine.tempoMap.tempos.size()==1&&engine.tempoMap.tempos[0].bpm==100.0&&engine.tempoMap.tempos[0].ramp,"tempo map: Update makes a 100 BPM ramp");
        panel->position.setText("1. 1. 1. 0");expect(panel->addFromFields().isNotEmpty()&&engine.tempoMap.tempos.size()==1,"tempo map: a point at the project start is refused");
        panel->position.setText("5");panel->bpm.setText("999");expect(panel->addFromFields().isNotEmpty()&&engine.tempoMap.tempos.size()==1,"tempo map: BPM out of range is refused");
        panel->selectRow(0);
        expect(panel->removeSelected().isEmpty()&&engine.tempoMap.signatures.empty()&&engine.tempoMap.tempos.size()==1,"tempo map: Remove deletes the signature");
        view.runCommand("undo");expect(engine.tempoMap.signatures.size()==1&&panel->rowCount()==2,"tempo map: undo restores it (and the window)");
        view.runCommand("redo");expect(engine.tempoMap.signatures.empty(),"tempo map: redo removes it again");
        if(auto* window=panel->findParentComponentOfClass<DocumentWindow>())window->setVisible(false);
        engine.clear();
    }
    // Automation (etapa 6): lane add/move/delete with one undo step per gesture, R/W modes,
    // undo through the project history, and the MixConsole fader following the curve / writing.
    void automationChecks()
    {
        engine.clear();engine.tempo=120;engine.positionSeconds=0;auto track=engine.addAudioTrack("Automation test");
        auto* lane=dynamic_cast<ui::AutomationLane*>(find(view,"automation-lane"));
        expect(lane!=nullptr,"Automation lane is reachable in the lower zone");if(!lane)return;
        const auto savedBefore=lane->beforeEdit,savedChanged=lane->changed;const auto savedTimeline=lane->timeline;const auto savedBounds=lane->getBounds();
        int undoSteps=0,changes=0;lane->beforeEdit=[&]{++undoSteps;};lane->changed=[&]{++changes;};
        lane->timeline=[](double& pps,double& scroll){pps=100.0;scroll=0.0;};lane->header=250;lane->setBounds(0,0,900,208);
        lane->setTrack(0);lane->setTarget({domain::AutomationParameter::pan});
        const auto points=[&]{const ScopedLock lock(engine.modelLock);return engine.tracks[0]->automation.pan.points;};
        // 100 px/s from x 250: beat 1 -> x 300, beat 2 -> x 350, beat 3 -> x 400 (120 BPM); pan +0.5 -> y 56, -0.5 -> y 152 (curve area 8..200).
        lane->mouseDown(mouse(*lane,350.f,56.f));lane->mouseUp(mouse(*lane,350.f,56.f));
        auto p=points();
        expect(p.size()==1&&std::abs(p[0].beat-2.0)<1e-9&&std::abs(p[0].value-0.5)<1e-9&&undoSteps==1&&changes==1,"Automation lane: click adds a pan point (beat 2, +0.5) with one undo step");
        lane->mouseDown(mouse(*lane,350.f,56.f));lane->mouseDrag(mouse(*lane,380.f,100.f,true));lane->mouseDrag(mouse(*lane,400.f,152.f,true));lane->mouseUp(mouse(*lane,400.f,152.f));
        p=points();

        expect(p.size()==1&&std::abs(p[0].beat-3.0)<1e-9&&std::abs(p[0].value+0.5)<1e-9&&undoSteps==2,"Automation lane: dragging a point moves it to beat 3, -0.5 with one undo step");
        lane->mouseDown(mouse(*lane,300.f,104.f));lane->mouseUp(mouse(*lane,300.f,104.f));p=points();
        const bool added=p.size()==2&&std::abs(p[0].beat-1.0)<1e-9&&std::abs(p[0].value)<1e-9;
        lane->mouseDown(mouse(*lane,300.f,104.f,false,ModifierKeys::altModifier));p=points();
        expect(added&&p.size()==1&&std::abs(p[0].beat-3.0)<1e-9&&undoSteps==4,"Automation lane: a second point is inserted in order and Option+click removes it");
        lane->mouseDown(mouse(*lane,400.f,152.f));lane->mouseUp(mouse(*lane,400.f,152.f));const int beforeDelete=undoSteps;
        lane->keyPressed(KeyPress(KeyPress::deleteKey));
        expect(points().empty()&&undoSteps==beforeDelete+1&&lane->selected==-1,"Automation lane: selecting a point (no undo step) and Delete removes it with one undo step");
        if(auto* w=dynamic_cast<TextButton*>(find(*lane,"automation-write"))){w->setToggleState(true,dontSendNotification);if(w->onClick)w->onClick();}
        expect(track->automation.write&&track->automation.read&&undoSteps==beforeDelete+2,"Automation lane: W turns Write on and also Read, with one undo step");
        if(auto* r=dynamic_cast<TextButton*>(find(*lane,"automation-read"))){r->setToggleState(false,dontSendNotification);if(r->onClick)r->onClick();}
        expect(!track->automation.write&&!track->automation.read,"Automation lane: R off turns Write off too");
        lane->beforeEdit=savedBefore;lane->changed=savedChanged;
        // Real undo through the project history: add a point, then Undo removes it and Redo restores it.
        lane->setTarget({domain::AutomationParameter::volume});
        const float yHalf=lane->valueToY(0.5);
        lane->mouseDown(mouse(*lane,400.f,yHalf));lane->mouseUp(mouse(*lane,400.f,yHalf));
        const auto volume=[&]{const ScopedLock lock(engine.modelLock);return engine.tracks[0]->automation.volume.points;};
        const bool drawn=volume().size()==1&&std::abs(volume()[0].beat-3.0)<1e-9&&std::abs(volume()[0].value-0.5)<1e-3;
        view.runCommand("undo");const bool undone=volume().empty();view.runCommand("redo");
        expect(drawn&&undone&&volume().size()==1,"Automation lane: Undo removes the drawn volume point and Redo restores it");
        {const ScopedLock lock(engine.modelLock);track->automation.volume.points={{0.0,0.1},{2.0,1.0},{4.0,1.0},{6.0,0.3},{8.0,0.6}};track->automation.read=true;}
        lane->setTarget({domain::AutomationParameter::volume});lane->repaint();capture(*lane,output.getChildFile("automation-lane.png"));
        lane->timeline=savedTimeline;lane->setBounds(savedBounds);
        // MixConsole: R/W buttons, fader following the curve at the cursor, W writing at the cursor when stopped.
        {const ScopedLock lock(engine.modelLock);track->automation.volume.points={{0.0,0.25},{8.0,1.0}};track->automation.read=false;track->gain=0.8f;}
        engine.positionSeconds=2.0; // beat 4 -> 0.625
        int undos=0,dirty=0;ui::MixConsoleHooks hooks;hooks.beforeEdit=[&undos]{++undos;};hooks.changed=[&dirty]{++dirty;};hooks.selected=[]{return 0;};
        {
            ui::MixConsole console(engine,manager,hooks);console.setSize(1200,760);
            auto* fader=dynamic_cast<Slider*>(find(console,"mix-fader-0"));auto* read=dynamic_cast<TextButton*>(find(console,"mix-read-0"));
            expect(fader&&read&&find(console,"mix-write-0")&&std::abs(fader->getValue()-Decibels::gainToDecibels(0.8))<0.05,"MixConsole strip has R/W buttons; with Read off the fader shows the static gain");
            if(read){read->setToggleState(true,dontSendNotification);if(read->onClick)read->onClick();}
            expect(track->automation.read&&undos==1&&fader&&std::abs(fader->getValue()-Decibels::gainToDecibels(0.625))<0.05,"MixConsole: R turns Read on (one undo step) and the fader follows the curve at the cursor (0.625 at beat 4)");
            capture(console,output.getChildFile("automation-console.png"));
        }
        {
            {const ScopedLock lock(engine.modelLock);track->automation.write=true;track->automation.volume.points.clear();}
            engine.positionSeconds=1.5;engine.playing=false; // beat 3
            ui::MixConsole console(engine,manager,hooks);console.setSize(1200,760);
            if(auto* fader=dynamic_cast<Slider*>(find(console,"mix-fader-0")))fader->setValue(-6.0,sendNotificationSync);
            if(auto* panSlider=dynamic_cast<Slider*>(find(console,"mix-pan-0")))panSlider->setValue(-40.0,sendNotificationSync);
            const auto v=track->automation.volume.points;const auto pn=track->automation.pan.points;
            expect(v.size()==1&&std::abs(v[0].beat-3.0)<1e-9&&std::abs(v[0].value-Decibels::decibelsToGain(-6.0))<1e-4&&pn.size()==1&&std::abs(pn[0].value+0.4)<1e-6,
                   "MixConsole: with W and the transport stopped, the fader and pan write a point at the cursor (beat 3)");
        }
        engine.positionSeconds=0;engine.clear();
    }
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
        midiEditCommandChecks(*roll,notes);
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
    // Subetapa 2: each MIDI edit command is one named history step that Undo and Redo walk through.
    template<typename Notes> void midiEditCommandChecks(ui::PianoRoll& roll,const Notes& notes)
    {
        const auto setNotes=[&]{const ScopedLock lock(engine.modelLock);auto& n=engine.tracks[0]->notes;n.clear();int k=0;for(int p:{60,64,67}){MidiNote m;m.pitch=p;m.startBeats=k+(k==0?0.1:0.0);m.lengthBeats=1;n.push_back(m);++k;}};
        const auto fingerprint=[&]{String s;for(auto& m:notes())s<<m.pitch<<"/"<<m.startBeats<<"/"<<m.lengthBeats<<";";return s;};
        app::MidiEditContext ctx{engine,0,roll.selection,[&](const String& name){roll.editName=name;roll.beforeEdit();},[&]{roll.changed();}};
        domain::QuantizeSettings q;q.division=4;
        struct Case{const char* key;std::function<int()> run;std::function<bool()> verify;};
        const Case cases[]{
            {"Mover notas",[&]{return app::moveSelection(ctx,1.0,2);},[&]{auto n=notes();return n.size()==3&&n[0].pitch==62&&std::abs(n[0].startBeats-1.1)<1e-9&&n[2].pitch==69&&n[2].startBeats==3.0;}},
            {"Cambiar duración de notas",[&]{return app::resizeSelection(ctx,0.5);},[&]{auto n=notes();return n.size()==3&&n[0].lengthBeats==1.5&&n[1].lengthBeats==1.5&&n[2].lengthBeats==1.5;}},
            {"Cuantizar notas",[&]{return app::quantizeSelection(ctx,q);},[&]{auto n=notes();return n.size()==3&&n[0].startBeats==0.0&&n[1].startBeats==1.0;}},
            {"Duplicar notas",[&]{return app::duplicateSelection(ctx);},[&]{auto n=notes();return n.size()==6&&n[3].pitch==60&&std::abs(n[3].startBeats-3.0)<1e-9&&n[3].id!=n[0].id&&roll.selection.size()==3&&roll.selection.contains(n[5].id)&&!roll.selection.contains(n[0].id);}},
            {"Transponer notas",[&]{return app::transposeSelection(ctx,12);},[&]{auto n=notes();return n.size()==3&&n[0].pitch==72&&n[1].pitch==76&&n[2].pitch==79;}},
            {"Borrar notas",[&]{return app::deleteSelection(ctx);},[&]{return notes().empty()&&roll.selection.empty();}}};
        for(const auto& c:cases)
        {
            setNotes();roll.selectAll();const auto before=fingerprint();const int sizeBefore=view.editHistory().size();
            const int affected=c.run();const auto after=fingerprint();const auto history=view.editHistory();
            const bool named=history.size()==jmin(100,sizeBefore+1)&&history[history.size()-1]==i18n::tr(c.key);
            const bool applied=affected>0&&c.verify();
            view.runCommand("undo");const bool undone=fingerprint()==before&&view.editHistory().size()==history.size()-1;
            view.runCommand("redo");const bool redone=fingerprint()==after&&view.editHistory().size()==history.size();
            expect(named&&applied&&undone&&redone,String("MIDI edit command \"")+c.key+"\": result, one named history step, Undo and Redo");
        }
        setNotes();roll.clearSelection();const int h=view.editHistory().size();
        expect(app::moveSelection(ctx,1.0,0)==0&&app::deleteSelection(ctx)==0&&app::transposeSelection(ctx,1)==0&&view.editHistory().size()==h,"MIDI edit commands with an empty selection change nothing and record no history step");
        roll.selectAll();expect(app::transposeSelection(ctx,100)==3&&notes()[2].pitch==127&&notes()[0].pitch==120,"Transpose limits the whole group to pitch 127 keeping intervals");
        view.runCommand("undo");
    }
    // Real history: one group move is undone in one step and redone in one step.
    template<typename Notes> void pianoRollUndoCheck(ui::PianoRoll& roll,const Notes& notes)
    {
        {const ScopedLock lock(engine.modelLock);auto& n=engine.tracks[0]->notes;n.clear();for(int p:{60,64,67}){MidiNote m;m.pitch=p;m.startBeats=1;n.push_back(m);}}
        roll.track=0;roll.clearSelection();roll.selectAll();
        const auto x=[&](double beat){return 52.f+(float)(beat*roll.pixelsPerBeat);};const float y64=24.f+(float)((roll.topPitch-64)*12)+5.f;
        const auto historyBefore=view.editHistory();
        roll.mouseDown(mouse(roll,x(1.5),y64));roll.mouseDrag(mouse(roll,x(3.0),y64,true));roll.mouseDrag(mouse(roll,x(3.5),y64,true));roll.mouseUp(mouse(roll,x(3.5),y64));
        const auto moved=notes();const bool oneStep=view.editHistory().size()==jmin(100,historyBefore.size()+1);
        view.runCommand("undo");const auto undone=notes();
        view.runCommand("redo");const auto redone=notes();
        const auto starts=[](const std::vector<MidiNote>& v,double b){if(v.size()!=3)return false;for(auto& m:v)if(m.startBeats!=b)return false;return true;};
        expect(oneStep&&starts(moved,3.0)&&starts(undone,1.0)&&starts(redone,3.0)&&roll.selection.empty(),"Piano roll: a multi-step group drag is one undo step; Undo restores all three notes and Redo reapplies it");
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
    static std::vector<int> itemIds(ComboBox& box){std::vector<int> ids;for(int i=0;i<box.getNumItems();++i)ids.push_back(box.getItemId(i));return ids;}
    void exportWindowChecks(ui::ExportDialog& dialog)
    {
        auto* type=dynamic_cast<ComboBox*>(find(dialog,"format-type"));auto* rate=dynamic_cast<ComboBox*>(find(dialog,"format-rate"));
        auto* bits=dynamic_cast<ComboBox*>(find(dialog,"format-bits"));auto* bitrate=dynamic_cast<ComboBox*>(find(dialog,"format-bitrate"));
        auto* range=dynamic_cast<ComboBox*>(find(dialog,"export-range"));auto* conflict=dynamic_cast<ComboBox*>(find(dialog,"export-conflict"));
        if(!type||!rate||!bits||!bitrate||!range||!conflict){expect(false,"export window exposes format, range and conflict controls");return;}
        for(int f=0;f<io::fileFormatCount;++f)
        {
            const auto format=(io::FileFormat)f;type->setSelectedId(f+1,sendNotificationSync);
            const auto rates=io::supportedRates(format);
            bool every=itemIds(*rate).size()==rates.size();String why=every?"":"count "+String(itemIds(*rate).size());
            for(int id:itemIds(*rate)){rate->setSelectedId(id,sendNotificationSync);const auto s=dialog.buildJob().settings;every=every&&io::validate(s).wasOk();if(!every&&why.isEmpty())why="validate "+String(id)+" "+io::validate(s).getErrorMessage();
                const bool lossy=format==io::FileFormat::mp3||format==io::FileFormat::m4a;
                const auto offered=itemIds(lossy?*bitrate:*bits);const auto valid=lossy?io::supportedBitRates(format,id):io::supportedBitDepths(format);
                every=every&&offered==valid&&bitrate->isVisible()==lossy&&bits->isVisible()==!lossy;if(!every&&why.isEmpty())why="options "+String(id)+" got "+String(offered.size())+" want "+String(valid.size())+" vis "+String((int)bitrate->isVisible());}
            expect(every,"export window offers only valid rates/depths/bit rates for "+io::displayName(format)+" "+why);
        }
        type->setSelectedId((int)io::FileFormat::mp3+1,sendNotificationSync);
        const auto mp3Rates=itemIds(*rate);expect(!mp3Rates.empty()&&*std::max_element(mp3Rates.begin(),mp3Rates.end())==48000,"MP3 offers no rate above 48 kHz");
        rate->setSelectedId(22050,sendNotificationSync);expect(!itemIds(*bitrate).empty()&&itemIds(*bitrate).back()==160,"MP3 at 22.05 kHz tops out at 160 kbit/s (MPEG-2)");
        rate->setSelectedId(44100,sendNotificationSync);expect(itemIds(*bitrate).size()==14&&itemIds(*bitrate).front()==32&&itemIds(*bitrate).back()==320,"MP3 at 44.1 kHz offers 32 to 320 kbit/s");
        type->setSelectedId((int)io::FileFormat::flac+1,sendNotificationSync);expect(itemIds(*bits)==std::vector<int>({16,24})&&itemIds(*rate).size()==11,"FLAC offers 16/24 bit and 11 rates");
        type->setSelectedId((int)io::FileFormat::wav+1,sendNotificationSync);expect(itemIds(*bits)==std::vector<int>({16,24,32}),"WAV offers 16, 24 and 32-bit float");
        type->setSelectedId((int)io::FileFormat::aiff+1,sendNotificationSync);expect(itemIds(*bits)==std::vector<int>({16,24}),"AIFF offers 16 and 24 bit (no float writer)");
        // Cycle-marker range export through the dialog's own job.
        const auto dir=output.getChildFile("export-window");dir.deleteRecursively();dir.createDirectory();
        AudioEngine local;local.prepareForOffline(48000.0,512);local.masterGain=1.0f;
        juce::AudioBuffer<float> ramp(1,4*48000);for(int i=0;i<ramp.getNumSamples();++i)ramp.setSample(0,i,0.1f*(float)i/48000.0f);
        io::EncodeSettings fixture;fixture.format=io::FileFormat::wav;fixture.sampleRate=48000.0;fixture.bitDepth=32;
        io::EncodedFileWriter writer;const auto source=dir.getChildFile("source.wav");
        const bool wrote=writer.open(source,fixture,1).wasOk()&&writer.write(ramp,0,ramp.getNumSamples())&&writer.finish().wasOk()&&local.importAudio(source).wasOk();
        expect(wrote,"export window fixture audio imports");if(!wrote)return;
        local.tracks[0]->gain=1.0f;local.tracks[0]->pan=-1.0f;for(auto& t:local.tracks)for(auto& c:t->clips){c.fadeIn=0;c.fadeOut=0;}
        auto markers=local.addTrack(TrackKind::marker);
        domain::Marker one;one.description="Verso";one.number=1;one.position=1.0;one.end=2.0;one.cycle=true;
        domain::Marker two;two.description="Coro";two.number=2;two.position=2.5;two.end=3.0;two.cycle=true;
        markers->data.markers={one,two};
        ui::ExportDialog window(local,0);window.setSize(820,520);window.setFolder(dir.getChildFile("out"));
        auto* wrange=dynamic_cast<ComboBox*>(find(window,"export-range"));auto* wconflict=dynamic_cast<ComboBox*>(find(window,"export-conflict"));
        auto* wname=dynamic_cast<TextEditor*>(find(window,"export-name"));auto* each=dynamic_cast<ToggleButton*>(find(window,"export-per-range"));
        if(!wrange||!wconflict||!wname||!each){expect(false,"export window exposes name, range and one-file-per-range controls");return;}
        wname->setText("Mezcla",dontSendNotification);wrange->setSelectedId(3,sendNotificationSync);wconflict->setSelectedId(3,sendNotificationSync);
        expect(each->isVisible(),"one-file-per-range toggle appears with cycle markers");
        auto job=window.buildJob();juce::StringArray written;
        const bool ok=io::exportChannels(local,job,{},written).wasOk()&&written.size()==2;
        expect(ok&&File(written[0]).getFileName()=="Mezcla - Verso.wav"&&File(written[1]).getFileName()=="Mezcla - Coro.wav","cycle markers export one file per range, named after the marker");
        if(ok)
        {
            AudioFormatManager formats;formats.registerBasicFormats();bool right=true;const double starts[]{1.0,2.5},lengths[]{1.0,0.5};
            for(int n=0;n<2;++n)
            {
                std::unique_ptr<AudioFormatReader> reader(formats.createReaderFor(File(written[n])));
                if(!reader){right=false;break;}
                AudioBuffer<float> got(2,(int)reader->lengthInSamples);reader->read(&got,0,got.getNumSamples(),0,true,true);
                right=right&&std::abs(got.getNumSamples()-lengths[n]*48000.0)<=2&&std::abs(got.getSample(0,100)-0.1f*(float)(starts[n]+100.0/48000.0))<2e-3f;
            }
            expect(right,"each cycle-marker file holds exactly its section of the mix");
        }
        each->setToggleState(false,sendNotificationSync);job=window.buildJob();written.clear();
        const bool joined=io::exportChannels(local,job,{},written).wasOk()&&written.size()==1;
        expect(joined&&job.ranges.size()==1&&job.ranges[0].start==1.0&&job.ranges[0].end==3.0,"without one-file-per-range the cycle ranges export as a single file from first start to last end");
        wconflict->setSelectedId(1,sendNotificationSync);expect(window.buildJob().conflict==io::NameConflict::ask,"existing-file conflict defaults to asking");
        dir.deleteRecursively();
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
            exportWindowChecks(exportDialog);
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

#include "PoolUiTests.h"
#include "../src/MainComponent.h"
#include "../src/ui/pool/PoolPanel.h"

namespace silente
{
namespace
{
using namespace juce;
Component* find(Component& root,const String& id)
{
    if(root.getComponentID()==id)return &root;for(auto* child:root.getChildren())if(auto* value=find(*child,id))return value;return nullptr;
}
File writeTone(const File& file,int channels,int samples)
{
    AudioBuffer<float> data(channels,samples);data.clear();
    for(int c=0;c<channels;++c)for(int i=0;i<samples;++i)data.setSample(c,i,0.1f);
    std::unique_ptr<OutputStream> stream(file.createOutputStream());if(!stream)return {};
    WavAudioFormat format;std::unique_ptr<AudioFormatWriter> writer(format.createWriterFor(stream.get(),48000.0,(unsigned)channels,16,{},0));
    if(!writer)return {};stream.release();writer->writeFromAudioSampleBuffer(data,0,samples);return file;
}
}
void runPoolUiChecks(MainComponent& view,AudioEngine& engine,const File& output,const std::function<void(bool,const String&)>& expect)
{
    engine.clear();
    const auto dir=output.getChildFile("pool-ui");dir.createDirectory();
    const auto a=writeTone(dir.getChildFile("a.wav"),1,48000),b=writeTone(dir.getChildFile("b.wav"),2,24000),c=writeTone(dir.getChildFile("c.wav"),1,96000);
    expect(a.existsAsFile()&&b.existsAsFile()&&c.existsAsFile(),"pool: test files written");
    engine.importAudio(a,-1,0.0);engine.importAudio(b,-1,0.0);
    engine.importAudio(a,0,2.0);engine.importAudio(a,0,4.0);   // a.wav: 3 uses (clips at 0, 2 and 4 s), b.wav: 1
    a.deleteFile();
    expect(view.runCommand("openPool"),"pool: the Open Pool command exists");
    ui::PoolPanel* panel=nullptr;
    for(int n=0;n<Desktop::getInstance().getNumComponents()&&!panel;++n)panel=dynamic_cast<ui::PoolPanel*>(find(*Desktop::getInstance().getComponent(n),"pool-panel"));
    expect(panel!=nullptr&&panel->isShowing(),"pool: window opens");if(!panel)return;
    panel->sortByColumn(4,false);
    const auto* first=panel->entry(0);const auto* second=panel->entry(1);
    expect(panel->rowCount()==2&&first&&first->name=="a.wav"&&first->uses==3&&first->state==domain::MediaState::missing,"pool: a.wav 3 uses, missing (sorted by uses, descending)");
    expect(second&&second->name=="b.wav"&&second->uses==1&&second->lengthSamples==24000&&second->seconds==0.5&&second->state==domain::MediaState::present,"pool: b.wav 1 use, 0.5 s, present");
    panel->sortByColumn(4,true);
    expect(panel->entry(0)&&panel->entry(0)->name=="b.wav","pool: sorting ascending by uses puts b.wav first");
    // Double click on a.wav selects its first clip (a project holds a single selected event).
    engine.seek(1.0);
    for(int row=0;row<panel->rowCount();++row)if(panel->entry(row)->name=="a.wav"){panel->selectRow(row);panel->activateRow(row);}
    view.runCommand("selectionStart");
    expect(engine.positionSeconds.load()==0.0,"pool: double click selects the first clip of the medium (cursor moved from 1 s to its start, 0 s)");
    // Locate: relink the missing medium, one undo step.
    expect(dynamic_cast<TextButton*>(find(*panel,"pool-locate"))->isEnabled(),"pool: Locate enabled on a missing medium");
    const auto steps=view.editHistory().size();
    const auto error=panel->relinkSelected(c);
    int onC=0;for(auto& t:engine.tracks)for(auto& clip:t->clips)if(clip.file==c){++onC;if(clip.audio->getNumSamples()!=96000)onC+=100;}
    expect(error.isEmpty()&&onC==3&&view.editHistory().size()==steps+1,"pool: relink points the 3 clips to c.wav in one undo step - "+error);
    const auto* replaced=panel->entry(1);
    expect(panel->rowCount()==2&&replaced&&replaced->name=="c.wav"&&replaced->uses==3&&replaced->state==domain::MediaState::replaced,"pool: the medium is shown as replaced");
    view.runCommand("undo");panel->refresh();
    int onA=0;for(auto& t:engine.tracks)for(auto& clip:t->clips)if(clip.file==a)++onA;
    bool missingAgain=false;for(int row=0;row<panel->rowCount();++row)if(panel->entry(row)->name=="a.wav"&&panel->entry(row)->state==domain::MediaState::missing)missingAgain=true;
    expect(onA==3&&missingAgain,"pool: undo restores the original path (missing again)");
    // Relinking to a file that cannot be read changes nothing and leaves no undo step.
    for(int row=0;row<panel->rowCount();++row)if(panel->entry(row)->name=="a.wav")panel->selectRow(row);
    const auto before=view.editHistory().size();
    const auto refused=panel->relinkSelected(dir.getChildFile("nada.wav"));
    expect(refused.isNotEmpty()&&view.editHistory().size()==before,"pool: an unreadable file is refused without an undo step");
    if(auto* window=panel->getParentComponent();window)window->setVisible(false);
    engine.clear();dir.deleteRecursively();
}
}

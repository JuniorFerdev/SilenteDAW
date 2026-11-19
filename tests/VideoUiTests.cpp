#include "VideoUiTests.h"
#include "VideoFixture.h"
#include "../src/MainComponent.h"
#include "../src/ui/video/VideoViewer.h"
#include <cmath>

namespace silente
{
namespace
{
using namespace juce;
Component* find(Component& root,const String& id)
{
    if(root.getComponentID()==id)return &root;for(auto* child:root.getChildren())if(auto* value=find(*child,id))return value;return nullptr;
}
ui::VideoViewer* findViewer()
{
    for(int n=0;n<Desktop::getInstance().getNumComponents();++n)
        if(auto* viewer=dynamic_cast<ui::VideoViewer*>(find(*Desktop::getInstance().getComponent(n),"video-viewer")))return viewer;
    return nullptr;
}
// Which of red/green/blue/yellow the picture shows below its white top quarter (H.264 shifts
// colours a little, so the dominant channels decide), plus whether the top is white (upright).
String colourName(const Image& image)
{
    if(!image.isValid())return "none";
    const auto c=image.getPixelAt(image.getWidth()/2,image.getHeight()*3/4);const int r=c.getRed(),g=c.getGreen(),b=c.getBlue();
    if(r>150&&g>150&&b<100)return "yellow";
    if(r>150&&g<100&&b<100)return "red";
    if(g>150&&r<100&&b<100)return "green";
    if(b>150&&r<100&&g<100)return "blue";
    return "other "+c.toDisplayString(false);
}
bool whiteTop(const Image& image)
{
    if(!image.isValid())return false;const auto c=image.getPixelAt(image.getWidth()/2,image.getHeight()/10);return c.getRed()>220&&c.getGreen()>220&&c.getBlue()>220;
}
File writeStill(const File& file)
{
    Image image(Image::RGB,64,48,true);Graphics g(image);g.fillAll(Colours::yellow);g.setColour(Colours::white);g.fillRect(0,0,64,12);
    file.deleteFile();if(auto out=file.createOutputStream()){PNGImageFormat().writeImageToStream(image,*out);}return file;
}
}
void runVideoUiChecks(MainComponent& view,AudioEngine& engine,const File& output,const std::function<void(bool,const String&)>& expect)
{
    const auto dir=output.getChildFile("video-ui");dir.createDirectory();
    const auto movie=dir.getChildFile("colores.mov"),still=writeStill(dir.getChildFile("cartel.png"));
    const auto written=testing::writeColourMovie(movie,{Colours::red,Colours::lime,Colours::blue});
    expect(written.wasOk()&&still.existsAsFile(),"video: synthetic H.264 movie (red/green/blue seconds) and PNG written "+written.getErrorMessage());
    if(written.failed())return;
    engine.clear();engine.seek(0);
    view.addTrack(TrackKind::video);
    auto track=engine.tracks.empty()?nullptr:engine.tracks.back();
    expect(track&&track->kind==TrackKind::video&&!track->hasAudioChannel()&&track->name.startsWith(i18n::tr("Video ")),"video: a video track has no audio channel");
    if(!track)return;
    const auto historyBefore=view.editHistory().size();
    const auto imported=view.importVisualMedia({movie.getFullPathName(),still.getFullPathName()});
    const auto& events=track->data.videoEvents;
    expect(imported.wasOk()&&events.size()==2,"video: movie and image become two events on the existing video track "+imported.getErrorMessage());
    if(events.size()!=2)return;
    expect(std::abs(events[0].start)<1e-9&&std::abs(events[0].length-3.0)<0.05&&!events[0].still&&std::abs(events[0].frameRate-10.0)<0.01&&events[0].width==160&&events[0].height==96,
           "video: the movie starts at the cursor with its duration (3 s), 10 fps and 160x96");
    expect(events[1].still&&std::abs(events[1].start-events[0].length)<1e-9&&std::abs(events[1].length-domain::defaultStillLength)<1e-9,"video: the image follows the movie and lasts the default still length");
    expect(engine.tracks.size()==1&&view.editHistory().size()==historyBefore+1,"video: one undoable step, no audio track created");
    if(auto* arrangement=find(view,"arrangement"))
        if(auto out=dir.getChildFile("video-track.png").createOutputStream()){out->setPosition(0);out->truncate();PNGImageFormat().writeImageToStream(arrangement->createComponentSnapshot(arrangement->getLocalBounds()),*out);}
    auto* viewer=findViewer();
    expect(viewer&&viewer->isShowing(),"video: importing opens the Video Player");
    if(!viewer)return;
    struct Probe{double at;const char* colour;};
    for(const auto& probe:{Probe{.5,"red"},Probe{1.5,"green"},Probe{2.95,"blue"},Probe{5.0,"yellow"},Probe{1.05,"green"}})
    {
        engine.seek(probe.at);const bool ready=viewer->waitForFrame(5000);const auto name=colourName(viewer->picture());
        expect(ready&&name==probe.colour&&whiteTop(viewer->picture()),"video: at "+String(probe.at,2)+" s the viewer shows the "+probe.colour+" picture upright (got "+name+")");
    }
    engine.seek(1.5);viewer->waitForFrame(5000);
    viewer->getTopLevelComponent()->setSize(640,400);
    if(auto out=dir.getChildFile("video-viewer.png").createOutputStream()){out->setPosition(0);out->truncate();PNGImageFormat().writeImageToStream(viewer->createComponentSnapshot(viewer->getLocalBounds()),*out);}
    engine.seek(30.0);viewer->waitForFrame(1000);
    expect(!viewer->target()&&!viewer->picture().isValid(),"video: past the last event the viewer is empty");
    // Undo and redo the import.
    view.runCommand("undo");
    auto current=engine.findTrack(track->id);
    expect(current&&current->data.videoEvents.empty(),"video: undo removes the imported events");
    view.runCommand("redo");current=engine.findTrack(track->id);
    expect(current&&current->data.videoEvents.size()==2,"video: redo restores them");
    // Save and reopen: events and picture come back.
    const auto project=dir.getChildFile("video.silente");
    const auto saved=engine.saveProject(project);
    const auto copy=current?current->data.videoEvents:std::vector<domain::VideoEvent>{};
    engine.clear();const auto loaded=engine.loadProject(project);
    auto reopened=engine.findTrack(track->id);
    expect(saved.wasOk()&&loaded.wasOk()&&reopened&&reopened->kind==TrackKind::video&&reopened->data.videoEvents==copy,"video: save and reopen keep the video track and its events");
    view.refreshLanguage();
    engine.seek(1.5);viewer->waitForFrame(5000);
    expect(colourName(viewer->picture())=="green","video: after reopening the viewer shows the frame at the cursor again");
    // A missing file is reported in the viewer instead of a picture.
    if(reopened)reopened->data.videoEvents[0].path=dir.getChildFile("ausente.mov").getFullPathName();
    engine.seek(0.5);viewer->waitForFrame(5000);
    expect(viewer->showsMissingFile()&&!viewer->picture().isValid(),"video: a missing movie shows the missing-file notice");
    expect(view.runCommand("videoPlayer"),"video: F8 «Reproductor de video» is a working command");
    viewer->getTopLevelComponent()->setVisible(false);
    engine.clear();
}
}

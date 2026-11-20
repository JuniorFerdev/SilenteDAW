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
MouseEvent mouseAt(Component& c,float x,float y,Point<float> down={})
{
    if(down.isOrigin())down={x,y};return {Desktop::getInstance().getMainMouseSource(),{x,y},ModifierKeys(ModifierKeys::leftButtonModifier),1.0f,0,0,0,0,&c,&c,Time::getCurrentTime(),down,Time::getCurrentTime(),1,down!=Point<float>{x,y}};
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

    // Stage 2: move, trim and delete events with the mouse, and drop files on the video track.
    engine.clear();engine.tempo=120;engine.seek(0);view.runCommand("toolSelect");
    view.addTrack(TrackKind::video);auto lane=engine.tracks.empty()?nullptr:engine.tracks.back();
    auto* arrangement=find(view,"arrangement");
    expect(lane&&arrangement&&view.importVisualMedia({movie.getFullPathName()}).wasOk()&&lane->data.videoEvents.size()==1,"video: movie imported for editing");
    if(!lane||!arrangement||lane->data.videoEvents.size()!=1)return;
    const auto eventId=lane->data.videoEvents[0].id;
    const auto event=[&]()->const domain::VideoEvent*{for(const auto& v:lane->data.videoEvents)if(v.id==eventId)return &v;return nullptr;};
    const int index=(int)engine.tracks.size()-1;const float y=(float)(view.projectYForTrack(index)+view.trackHeightOf(index)/2);
    const auto x=[&](double seconds){return view.projectXForTime(seconds);};
    const auto drag=[&](float from,float to)
    {
        arrangement->mouseDown(mouseAt(*arrangement,from,y));arrangement->mouseDrag(mouseAt(*arrangement,(from+to)/2,y,{from,y}));
        arrangement->mouseDrag(mouseAt(*arrangement,to,y,{from,y}));arrangement->mouseUp(mouseAt(*arrangement,to,y,{from,y}));
    };
    const auto shows=[&](double at,const char* colour){engine.seek(at);return viewer->waitForFrame(5000)&&colourName(viewer->picture())==colour;};
    expect(shows(1.5,"green"),"video: before moving, 1.5 s shows the green second");
    auto history=view.editHistory().size();
    drag(x(1.5),x(2.5));
    expect(event()&&std::abs(event()->start-1.0)<1e-6&&std::abs(event()->offset)<1e-9&&view.editHistory().size()==history+1&&view.editHistory()[view.editHistory().size()-1]==i18n::tr("Mover evento de video"),
           "video: dragging the event 1 s to the right moves it to 1 s in one named undo step");
    expect(shows(1.5,"red")&&shows(3.5,"blue"),"video: after the move the Video Player shows the frames shifted by 1 s (1.5 s red, 3.5 s blue)");
    view.runCommand("undo");expect(event()&&std::abs(event()->start)<1e-9&&shows(1.5,"green"),"video: undo puts the event back at 0 s and the viewer follows");
    view.runCommand("redo");expect(event()&&std::abs(event()->start-1.0)<1e-6,"video: redo moves it again");
    // Trim the start from 1 s to 2 s: the end stays at 4 s and the source offset becomes 1 s.
    history=view.editHistory().size();drag(x(1.0)+2.0f,x(2.0));
    expect(event()&&std::abs(event()->start-2.0)<1e-6&&std::abs(event()->offset-1.0)<1e-6&&std::abs(event()->length-2.0)<1e-6&&view.editHistory().size()==history+1,"video: dragging the start grip trims the event and moves its source offset");
    expect(shows(2.5,"green")&&(engine.seek(1.5),viewer->waitForFrame(1000))&&!viewer->target(),"video: the trimmed event starts with the green second; before it there is no picture");
    // Trim the end from 4 s to 3.5 s, then try to extend it past the source end (limited to 4 s).
    drag(x(4.0)-2.0f,x(3.5));expect(event()&&std::abs(event()->length-1.5)<1e-6,"video: dragging the end grip shortens the event");
    drag(x(3.5)-2.0f,x(6.0));expect(event()&&std::abs(event()->start+event()->length-4.0)<0.03,"video: the end cannot go past the end of the movie");
    // Supr deletes the selected event; undo brings it back.
    arrangement->mouseDown(mouseAt(*arrangement,x(3.0),y));arrangement->mouseUp(mouseAt(*arrangement,x(3.0),y));
    history=view.editHistory().size();
    expect(view.runCommand("delete")&&lane->data.videoEvents.empty()&&view.editHistory().size()==history+1&&view.editHistory()[view.editHistory().size()-1]==i18n::tr("Eliminar evento de video"),"video: Supr deletes the selected video event in one undo step");
    view.runCommand("undo");expect(event()!=nullptr,"video: undo restores the deleted event");
    // Erase tool deletes on click.
    view.runCommand("toolErase");
    arrangement->mouseDown(mouseAt(*arrangement,x(3.0),y));arrangement->mouseUp(mouseAt(*arrangement,x(3.0),y));
    expect(lane->data.videoEvents.empty(),"video: the Erase tool deletes the clicked video event");
    view.runCommand("undo");view.runCommand("toolSelect");expect(event()!=nullptr,"video: undo after the Erase tool");
    // Drop a movie and an image on the video track: picture events where dropped, no audio track, one step.
    auto* target=dynamic_cast<FileDragAndDropTarget*>(arrangement);
    history=view.editHistory().size();const auto tracksBefore=engine.tracks.size();
    if(target)target->filesDropped({movie.getFullPathName(),still.getFullPathName()},(int)x(6.0),(int)y);
    MessageManager::getInstance()->runDispatchLoopUntil(300); // a background audio extraction would have started here
    const auto& dropped=lane->data.videoEvents;
    expect(target&&dropped.size()==3&&std::abs(dropped[1].start-6.0)<0.02&&!dropped[1].still&&dropped[2].still&&std::abs(dropped[2].start-dropped[1].start-dropped[1].length)<1e-9,
           "video: a movie and an image dropped at 6 s become picture events from the drop point");
    bool audioTrack=false;for(const auto& t:engine.tracks)audioTrack|=t->hasAudioChannel();
    expect(engine.tracks.size()==tracksBefore&&!audioTrack&&view.editHistory().size()==history+1,"video: dropping on the video track creates no audio track and one undo step");
    expect(shows(6.5,"red"),"video: the dropped movie shows its first second at 6.5 s");
    view.runCommand("undo");expect(lane->data.videoEvents.size()==1,"video: one undo removes both dropped events");
    if(auto out=dir.getChildFile("video-edit.png").createOutputStream()){out->setPosition(0);out->truncate();PNGImageFormat().writeImageToStream(arrangement->createComponentSnapshot(arrangement->getLocalBounds()),*out);}
    viewer->getTopLevelComponent()->setVisible(false);
    engine.clear();
}
}

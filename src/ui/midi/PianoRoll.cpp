#include "PianoRoll.h"
#include "../../i18n/Translations.h"
#include "../theme/Drawing.h"
#include "../../app/MidiEditCommands.h"
#include "../../app/Preferences.h"
#include <cmath>

namespace silente::ui
{
using namespace theme;
PianoRoll::PianoRoll(AudioEngine& e):engine(e){setComponentID("piano-roll");setWantsKeyboardFocus(true);}
void PianoRoll::paint(juce::Graphics& g)
{
    g.fillAll(bg);if(track<0||track>=(int)engine.tracks.size()||!engine.tracks[(size_t)track]->isMidi){text(g,i18n::tr("Selecciona una pista de instrumento"),getLocalBounds().withTrimmedBottom(25),17,muted,juce::Justification::centred);text(g,i18n::tr("Añade notas con doble clic · arrastra para mover · borde derecho para duración"),getLocalBounds().removeFromBottom(45),12,muted,juce::Justification::centred);return;}
    for(int y=24,p=topPitch;y<getHeight();y+=row,--p){const bool black=p%12==1||p%12==3||p%12==6||p%12==8||p%12==10;g.setColour(black?bg:panel);g.fillRect(52,y,getWidth()-52,row);g.setColour(black?raised:pianoWhite);g.fillRect(0,y,50,row-1);if(p%12==0)text(g,juce::MidiMessage::getMidiNoteName(p,true,true,3),{5,y,43,row},9,black?ink:bg);g.setColour(line.withAlpha(.3f));g.drawHorizontalLine(y,52,(float)getWidth());}
    const auto timeline=engine.tempoView();
    if(timeline.isConstant())
    for(int b=(int)scrollBeats;b<scrollBeats+(getWidth()-52)/pixelsPerBeat+1;++b){int x=(int)beatX(b);g.setColour(b%4==0?line.brighter(.1f):line.withAlpha(.5f));g.drawVerticalLine(x,24,(float)getHeight());if(b%4==0)text(g,juce::String(b/4+1),{x+4,0,40,24},10,muted);if(pixelsPerBeat>40)for(int sub=1;sub<4;++sub){g.setColour(line.withAlpha(.18f));g.drawVerticalLine((int)(x+sub*pixelsPerBeat/4),24,(float)getHeight());}}
    else
    {   // Time signature changes: the axis stays in beats; bar lines follow the signature map.
        const double last=scrollBeats+(getWidth()-52)/pixelsPerBeat+1;
        for(int b=(int)scrollBeats;b<last;++b){int x=(int)beatX(b);g.setColour(line.withAlpha(.5f));g.drawVerticalLine(x,24,(float)getHeight());if(pixelsPerBeat>40)for(int sub=1;sub<4;++sub){g.setColour(line.withAlpha(.18f));g.drawVerticalLine((int)(x+sub*pixelsPerBeat/4),24,(float)getHeight());}}
        for(int bar=timeline.barAt(scrollBeats).bar,guard=0;guard<4000&&timeline.barStart(bar)<last;++bar,++guard){int x=(int)beatX(timeline.barStart(bar));if(x<52)continue;g.setColour(line.brighter(.1f));g.drawVerticalLine(x,24,(float)getHeight());text(g,juce::String(bar+1),{x+4,0,40,24},10,muted);}
    }
    auto& t=*engine.tracks[(size_t)track];for(int i=0;i<(int)t.notes.size();++i){auto r=noteRect(t.notes[(size_t)i]);const auto c=selection.contains(t.notes[(size_t)i].id)?ink:t.colour.withAlpha(.75f);
        // Muted notes: outline only (Cubase draws them dimmed).
        if(t.notes[(size_t)i].muted){g.setColour(c.withAlpha(.18f));g.fillRoundedRectangle(r.reduced(1),2);g.setColour(c.withAlpha(.7f));g.drawRoundedRectangle(r.reduced(1.5f),2,1.f);continue;}
        g.setColour(c);g.fillRoundedRectangle(r.reduced(1),2);if(r.getWidth()>38)text(g,juce::MidiMessage::getMidiNoteName(t.notes[(size_t)i].pitch,true,true,3),r.toNearestInt().reduced(4,0),9,bg);}
    if(!marquee.isEmpty()){g.setColour(ink.withAlpha(.12f));g.fillRect(marquee);g.setColour(ink.withAlpha(.7f));g.drawRect(marquee,1.f);}
    float x=beatX(timeline.isConstant()?engine.positionSeconds*engine.tempo/60:timeline.beatsAt(engine.positionSeconds.load()));if(x>=52){g.setColour(ink.withAlpha(.6f));g.drawVerticalLine((int)x,24,(float)getHeight());}
}
void PianoRoll::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();gesture=Gesture::none;gestureRecorded=false;
    int hit=-1;juce::String hitId;bool hitMuted=false;
    {
        const juce::ScopedLock lock(engine.modelLock);if(!valid()){selected=-1;return;}
        if(e.x<52){selected=-1;previewPitch=juce::jlimit(0,127,topPitch-(e.y-24)/row);engine.keyboardState.noteOn(1,previewPitch,.75f);return;}
        const auto& notes=engine.tracks[(size_t)track]->notes;
        for(int i=(int)notes.size()-1;i>=0;--i)if(noteRect(notes[(size_t)i]).contains(e.position)){hit=i;break;}
        if(hit>=0){hitId=notes[(size_t)hit].id;hitMuted=notes[(size_t)hit].muted;}
    }
    if(e.mods.isPopupMenu()){if(e.y>=24)openContextTools(e,hit);return;}
    // Alt/Option temporarily turns Select into Draw (manual p. 1254).
    const int active=tool==midiToolSelect&&e.mods.isAltDown()?midiToolDraw:tool;
    // The clicked note, or the whole selection when the clicked note belongs to it.
    const auto targets=[&]{domain::NoteSelection t;if(selection.contains(hitId))t=selection;else t.add(hitId);return t;};
    switch(active)
    {
        case midiToolErase:{if(hit<0)return;domain::NoteSelection t;t.add(hitId);if(app::deleteNotes(context(),t)>0)selected=-1;return;}
        case midiToolSplit:{if(hit>=0)app::splitNotesAt(context(),targets(),quantize(beatAt(e.position.x)));return;}
        case midiToolGlue:{if(hit>=0)app::glueNote(context(),hitId);return;}
        case midiToolMute:{if(hit>=0)app::setNotesMuted(context(),targets(),!hitMuted);return;}
        case midiToolDraw:
            if(hit<0)
            {   // One click draws a note of the quantize length; dragging right lengthens it in multiples.
                if(e.y<24)return;
                const double g=grid(),b=beatAt(e.position.x),start=g>0?std::floor(b/g)*g:b;int index=-1;
                if(app::createNote(context(),topPitch-(int)std::floor((e.position.y-24.f)/(float)row),start,drawLength(),&index)>0){selected=index;gesture=Gesture::draw;dragStart=e.position;}
                return;
            }
            break; // Draw on an existing note edits it like Select.
        default:break;
    }
    const juce::ScopedLock lock(engine.modelLock);if(!valid())return;
    auto& notes=engine.tracks[(size_t)track]->notes;if(hit>=(int)notes.size())return;
    if(hit<0)
    {
        selected=-1;if(e.y<24){if(!e.mods.isShiftDown())selection.clear();repaint();return;}
        // Marquee on empty space; Shift keeps the current selection and adds to it.
        if(!e.mods.isShiftDown())selection.clear();marqueeBase=selection;gesture=Gesture::marquee;dragStart=e.position;marquee={};repaint();return;
    }
    const auto& id=hitId;
    if(e.mods.isShiftDown()){if(!selection.toggle(id)){selected=-1;repaint();return;}}
    else if(!selection.contains(id)){selection.clear();selection.add(id);}
    selected=hit;gestureOriginal=notes;gestureCurrent=notes;gestureIndices=domain::selectedIndices(notes,selection);dragStart=e.position;
    gesture=e.x>noteRect(notes[(size_t)hit]).getRight()-8?Gesture::resize:Gesture::move;
    repaint();
}
void PianoRoll::openContextTools(const juce::MouseEvent& e,int hit)
{
    // Right click on a note outside the selection selects it, so the menu acts on it.
    if(hit>=0){const juce::ScopedLock lock(engine.modelLock);if(valid()&&hit<(int)engine.tracks[(size_t)track]->notes.size()&&!selection.contains(engine.tracks[(size_t)track]->notes[(size_t)hit].id))selectOnly(hit);}
    const bool showOnRightClick=app::showToolboxOnRightClick();
    const auto at=localAreaToGlobal(juce::Rectangle<int>(e.x,e.y,1,1));
    juce::Component::SafePointer<PianoRoll> safe(this);
    if(opensProjectToolbox(showOnRightClick,e.mods))
    {
        juce::CallOutBox::launchAsynchronously(std::make_unique<ProjectToolbox>(midiToolDefinitions(),"midi-toolbox",tool,[safe](int id){if(safe)safe->setTool(id);},showOnRightClick),at,nullptr);
        return;
    }
    juce::PopupMenu menu,tools;
    for(const auto& t:midiToolDefinitions())tools.addItem(100+t.id,t.name,true,t.id==tool);
    menu.addSubMenu(i18n::tr("Herramientas"),tools);menu.addSeparator();
    const bool any=!selection.empty();
    menu.addItem(1,i18n::tr("Borrar"),any);menu.addItem(2,i18n::tr("Duplicar"),any);menu.addItem(3,i18n::tr("Cuantizar"));
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(at),[safe](int result)
    {
        if(!safe||result==0)return;
        if(result>100){safe->setTool(result-100);return;}
        if(result==1)safe->deleteSelected();else if(result==2)safe->duplicateSelected();else if(result==3)safe->quantizeNotes();
    });
}
void PianoRoll::setTool(int id)
{
    bool known=false;for(const auto& t:midiToolDefinitions())known|=t.id==id;
    if(!known)return;
    tool=id;gesture=Gesture::none;marquee={};if(toolChanged)toolChanged(id);repaint();
}
void PianoRoll::mouseDoubleClick(const juce::MouseEvent& e)
{
    if(tool!=midiToolSelect||e.mods.isAltDown()||e.x<52||e.y<24)return;
    {const juce::ScopedLock lock(engine.modelLock);if(!valid())return;}
    gesture=Gesture::none;marquee={};int index=-1;
    if(app::createNote(context(),juce::jlimit(0,127,topPitch-(e.y-24)/row),quantize(beatAt(e.position.x)),1.0,&index)>0)selected=index;
}
void PianoRoll::mouseDrag(const juce::MouseEvent& e)
{
    const juce::ScopedLock lock(engine.modelLock);if(!valid()||gesture==Gesture::none)return;auto& notes=engine.tracks[(size_t)track]->notes;
    if(gesture==Gesture::marquee)
    {
        marquee=juce::Rectangle<float>(dragStart,e.position);
        const double b0=scrollBeats+(marquee.getX()-52)/pixelsPerBeat,b1=scrollBeats+(marquee.getRight()-52)/pixelsPerBeat;
        const int high=topPitch-(int)std::floor((marquee.getY()-24)/row),low=topPitch-(int)std::floor((marquee.getBottom()-24)/row);
        selection=marqueeBase;selection.addAll(domain::notesInRegion(notes,b0,b1,low,high));repaint();return;
    }
    if(gesture==Gesture::draw)
    {   // Same undo step as the click that created the note.
        if(selected<0||selected>=(int)notes.size()||!selection.contains(notes[(size_t)selected].id)){gesture=Gesture::none;return;}
        auto& n=notes[(size_t)selected];const double unit=drawLength();
        const double length=juce::jmax(unit,std::ceil((beatAt(e.position.x)-n.startBeats)/unit-1e-9)*unit);
        if(length!=n.lengthBeats){n.lengthBeats=length;if(changed)changed();repaint();}
        return;
    }
    // The model changed under the gesture (undo, track switch): stop instead of writing stale notes.
    if(notes.size()!=gestureOriginal.size()||selected<0||selected>=(int)notes.size()){gesture=Gesture::none;return;}
    const double delta=(e.x-dragStart.x)/pixelsPerBeat;bool moved;
    if(gesture==Gesture::resize)moved=domain::resizeNotes(gestureOriginal,gestureCurrent,gestureIndices,selected,delta,grid());
    else moved=domain::moveNotes(gestureOriginal,gestureCurrent,gestureIndices,selected,delta,-(int)std::round((e.y-dragStart.y)/(float)row),grid());
    if(!moved)return;
    // One undo step per gesture, taken just before the first real change.
    if(!gestureRecorded){gestureRecorded=true;editName=gesture==Gesture::resize?i18n::tr("Cambiar duración de notas"):i18n::tr("Mover notas");if(beforeEdit)beforeEdit();}
    for(int i:gestureIndices)notes[(size_t)i]=gestureCurrent[(size_t)i];
    if(changed)changed();repaint();
}
void PianoRoll::mouseUp(const juce::MouseEvent&){if(previewPitch>=0){engine.keyboardState.noteOff(1,previewPitch,0);previewPitch=-1;}if(gesture==Gesture::marquee){marquee={};repaint();}gesture=Gesture::none;}
void PianoRoll::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w){if(e.mods.isCommandDown())pixelsPerBeat=juce::jlimit(20.0,240.0,pixelsPerBeat*(w.deltaY>0?1.15:1/1.15));else if(e.mods.isShiftDown()||std::abs(w.deltaX)>.01)scrollBeats=juce::jmax(0.0,scrollBeats-(w.deltaX!=0?w.deltaX:w.deltaY)*8);else topPitch=juce::jlimit(24,127,topPitch+(w.deltaY>0?2:-2));repaint();}
void PianoRoll::selectAll(){const juce::ScopedLock lock(engine.modelLock);if(!valid())return;selection=domain::selectAllNotes(engine.tracks[(size_t)track]->notes);repaint();}
void PianoRoll::clearSelection(){selection.clear();selected=-1;repaint();}
void PianoRoll::selectOnly(int index)
{
    const juce::ScopedLock lock(engine.modelLock);selection.clear();selected=-1;
    if(valid()&&index>=0&&index<(int)engine.tracks[(size_t)track]->notes.size()){selected=index;selection.add(engine.tracks[(size_t)track]->notes[(size_t)index].id);}
    repaint();
}
app::MidiEditContext PianoRoll::context()
{
    return {engine,track,selection,[this](const juce::String& name){editName=name;if(beforeEdit)beforeEdit();},[this]{if(changed)changed();repaint();}};
}
void PianoRoll::deleteSelected(){if(app::deleteSelection(context())>0)selected=-1;}
int PianoRoll::quantizeNotes(){return app::quantizeSelection(context(),quantizeSettings)>0?1:0;}
int PianoRoll::duplicateSelected(){int anchor=-1;const int n=app::duplicateSelection(context(),&anchor);if(n>0)selected=anchor;return n;}
int PianoRoll::transposeSelected(int semitones){return app::transposeSelection(context(),semitones);}
bool PianoRoll::keyPressed(const juce::KeyPress& k)
{
    if(k==juce::KeyPress::deleteKey||k==juce::KeyPress::backspaceKey){deleteSelected();return true;}
    if(const int chosen=midiToolForKey(k,tool)){setTool(chosen);return true;}
    const int code=k.getKeyCode();
    if((code=='D'||code=='d')&&k.getModifiers().isCommandDown()&&!k.getModifiers().isAltDown()&&!k.getModifiers().isShiftDown()){duplicateSelected();return true;}
    if((code=='A'||code=='a')&&k.getModifiers().isCommandDown()&&!k.getModifiers().isAltDown()){if(k.getModifiers().isShiftDown())clearSelection();else selectAll();return true;}
    return false;
}
bool PianoRoll::valid()const{return track>=0&&track<(int)engine.tracks.size()&&engine.tracks[(size_t)track]->isMidi;}
double PianoRoll::grid()const{return snap?.25:0.0;}
double PianoRoll::drawLength()const{return .25;}
double PianoRoll::beatAt(float x)const{return scrollBeats+(x-52)/pixelsPerBeat;}
double PianoRoll::quantize(double b)const{return domain::snapBeat(b,grid());}
float PianoRoll::beatX(double b)const{return (float)(52+(b-scrollBeats)*pixelsPerBeat);}
juce::Rectangle<float> PianoRoll::noteRect(const MidiNote& n)const{return {beatX(n.startBeats),(float)(24+(topPitch-n.pitch)*row),(float)(n.lengthBeats*pixelsPerBeat),(float)row};}
}

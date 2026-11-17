#include "PianoRoll.h"
#include "../../i18n/Translations.h"
#include "../theme/Drawing.h"
#include <cmath>

namespace silente::ui
{
using namespace theme;
PianoRoll::PianoRoll(AudioEngine& e):engine(e){setComponentID("piano-roll");setWantsKeyboardFocus(true);}
void PianoRoll::paint(juce::Graphics& g)
{
    g.fillAll(bg);if(track<0||track>=(int)engine.tracks.size()||!engine.tracks[(size_t)track]->isMidi){text(g,i18n::tr("Selecciona una pista de instrumento"),getLocalBounds().withTrimmedBottom(25),17,muted,juce::Justification::centred);text(g,i18n::tr("Añade notas con doble clic · arrastra para mover · borde derecho para duración"),getLocalBounds().removeFromBottom(45),12,muted,juce::Justification::centred);return;}
    for(int y=24,p=topPitch;y<getHeight();y+=row,--p){const bool black=p%12==1||p%12==3||p%12==6||p%12==8||p%12==10;g.setColour(black?bg:panel);g.fillRect(52,y,getWidth()-52,row);g.setColour(black?raised:pianoWhite);g.fillRect(0,y,50,row-1);if(p%12==0)text(g,juce::MidiMessage::getMidiNoteName(p,true,true,3),{5,y,43,row},9,black?ink:bg);g.setColour(line.withAlpha(.3f));g.drawHorizontalLine(y,52,(float)getWidth());}
    for(int b=(int)scrollBeats;b<scrollBeats+(getWidth()-52)/pixelsPerBeat+1;++b){int x=(int)beatX(b);g.setColour(b%4==0?line.brighter(.1f):line.withAlpha(.5f));g.drawVerticalLine(x,24,(float)getHeight());if(b%4==0)text(g,juce::String(b/4+1),{x+4,0,40,24},10,muted);if(pixelsPerBeat>40)for(int sub=1;sub<4;++sub){g.setColour(line.withAlpha(.18f));g.drawVerticalLine((int)(x+sub*pixelsPerBeat/4),24,(float)getHeight());}}
    auto& t=*engine.tracks[(size_t)track];for(int i=0;i<(int)t.notes.size();++i){auto r=noteRect(t.notes[(size_t)i]);g.setColour(i==selected?ink:t.colour.withAlpha(.75f));g.fillRoundedRectangle(r.reduced(1),2);if(r.getWidth()>38)text(g,juce::MidiMessage::getMidiNoteName(t.notes[(size_t)i].pitch,true,true,3),r.toNearestInt().reduced(4,0),9,bg);}
    float x=beatX(engine.positionSeconds*engine.tempo/60);if(x>=52){g.setColour(ink.withAlpha(.6f));g.drawVerticalLine((int)x,24,(float)getHeight());}
}
void PianoRoll::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();selected=-1;const juce::ScopedLock lock(engine.modelLock);if(!valid())return;
    if(e.x<52){previewPitch=juce::jlimit(0,127,topPitch-(e.y-24)/row);engine.keyboardState.noteOn(1,previewPitch,.75f);return;}
    auto& notes=engine.tracks[(size_t)track]->notes;for(int i=(int)notes.size()-1;i>=0;--i)if(noteRect(notes[(size_t)i]).contains(e.position)){selected=i;original=notes[(size_t)i];dragStart=e.position;resize=e.x>noteRect(original).getRight()-8;if(beforeEdit)beforeEdit();if(e.mods.isPopupMenu())deleteSelected();break;}
    repaint();
}
void PianoRoll::mouseDoubleClick(const juce::MouseEvent& e)
{
    const juce::ScopedLock lock(engine.modelLock);if(!valid()||e.x<52||e.y<24)return;if(beforeEdit)beforeEdit();MidiNote n;n.pitch=juce::jlimit(0,127,topPitch-(e.y-24)/row);n.startBeats=quantize(scrollBeats+(e.x-52)/pixelsPerBeat);n.lengthBeats=1;engine.tracks[(size_t)track]->notes.push_back(n);selected=(int)engine.tracks[(size_t)track]->notes.size()-1;if(changed)changed();repaint();
}
void PianoRoll::mouseDrag(const juce::MouseEvent& e)
{
    const juce::ScopedLock lock(engine.modelLock);if(!valid()||selected<0)return;auto& notes=engine.tracks[(size_t)track]->notes;if(selected>=(int)notes.size())return;auto& n=notes[(size_t)selected];double delta=(e.x-dragStart.x)/pixelsPerBeat;
    if(resize)n.lengthBeats=juce::jmax(.0625,quantize(original.lengthBeats+delta));else{n.startBeats=juce::jmax(0.0,quantize(original.startBeats+delta));n.pitch=juce::jlimit(0,127,original.pitch-(int)std::round((e.y-dragStart.y)/(float)row));}if(changed)changed();repaint();
}
void PianoRoll::mouseUp(const juce::MouseEvent&){if(previewPitch>=0){engine.keyboardState.noteOff(1,previewPitch,0);previewPitch=-1;}}
void PianoRoll::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w){if(e.mods.isCommandDown())pixelsPerBeat=juce::jlimit(20.0,240.0,pixelsPerBeat*(w.deltaY>0?1.15:1/1.15));else if(e.mods.isShiftDown()||std::abs(w.deltaX)>.01)scrollBeats=juce::jmax(0.0,scrollBeats-(w.deltaX!=0?w.deltaX:w.deltaY)*8);else topPitch=juce::jlimit(24,127,topPitch+(w.deltaY>0?2:-2));repaint();}
void PianoRoll::deleteSelected(){const juce::ScopedLock lock(engine.modelLock);if(!valid())return;auto& n=engine.tracks[(size_t)track]->notes;if(selected>=0&&selected<(int)n.size()){if(beforeEdit)beforeEdit();n.erase(n.begin()+selected);selected=-1;if(changed)changed();repaint();}}
int PianoRoll::quantizeNotes()
{
    const juce::ScopedLock lock(engine.modelLock);if(!valid())return 0;auto& n=engine.tracks[(size_t)track]->notes;
    const int index=selected>=0&&selected<(int)n.size()?selected:-1;auto copy=n;
    if(domain::quantizeNotes(copy,index,quantizeSettings)==0)return 0;
    if(beforeEdit)beforeEdit();n=std::move(copy);if(changed)changed();repaint();return 1;
}
bool PianoRoll::keyPressed(const juce::KeyPress& k){if(k==juce::KeyPress::deleteKey||k==juce::KeyPress::backspaceKey){deleteSelected();return true;}return false;}
bool PianoRoll::valid()const{return track>=0&&track<(int)engine.tracks.size()&&engine.tracks[(size_t)track]->isMidi;}
double PianoRoll::quantize(double b)const{return snap?std::round(b*4)/4:b;}
float PianoRoll::beatX(double b)const{return (float)(52+(b-scrollBeats)*pixelsPerBeat);}
juce::Rectangle<float> PianoRoll::noteRect(const MidiNote& n)const{return {beatX(n.startBeats),(float)(24+(topPitch-n.pitch)*row),(float)(n.lengthBeats*pixelsPerBeat),(float)row};}
}

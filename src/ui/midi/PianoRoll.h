#pragma once
#include "../../AudioEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../domain/Quantize.h"
#include "../../domain/NoteEdit.h"
#include "../../app/MidiEditCommands.h"
#include "MidiTools.h"
#include <functional>

namespace silente::ui
{
// Lower-zone key editor for the selected MIDI/instrument track. It edits engine.tracks[track]
// notes under modelLock through the pure rules of domain/NoteEdit and reports through
// beforeEdit (undo snapshot, at most once per gesture) and changed (dirty).
// Selection: click selects one note, Shift+click toggles, dragging on empty space draws a
// marquee (Shift adds), Cmd+A selects all, Cmd+Shift+A deselects. Moving or resizing a selected
// note acts on the whole selection.
// Tools (subetapa 3): the active tool decides what a left click does (Select, Draw, Erase, Split,
// Glue, Mute); the edits go through app/MidiEditCommands. Right click opens the toolbox or, with
// the preference off (or a modifier), the context menu.
class PianoRoll final : public juce::Component
{
public:
    explicit PianoRoll(AudioEngine& e);
    // `selected` is the anchor note (last clicked or created), -1 when none; `selection` holds
    // every selected note by id.
    AudioEngine& engine;int track=0,selected=-1,topPitch=79;double pixelsPerBeat=64,scrollBeats=0;bool snap=true;
    domain::NoteSelection selection;
    std::function<void()> beforeEdit,changed;
    // Name of the undo step about to be recorded; set just before beforeEdit is called.
    juce::String editName;
    void paint(juce::Graphics& g)override;
    void mouseDown(const juce::MouseEvent& e)override;
    void mouseDoubleClick(const juce::MouseEvent& e)override;
    void mouseDrag(const juce::MouseEvent& e)override;
    void mouseUp(const juce::MouseEvent&)override;
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w)override;
    domain::QuantizeSettings quantizeSettings;
    void selectAll();
    void clearSelection();
    // Selects only the note at `index` and makes it the anchor.
    void selectOnly(int index);
    // Deletes every selected note with one undo step.
    void deleteSelected();
    // Quantize command (Q): the selected notes, or every note when none is selected.
    int quantizeNotes();
    // Cmd+D: copies the selected group after its span; returns how many notes were copied.
    int duplicateSelected();
    // Transposes the selection by semitones (group limited to 0..127).
    int transposeSelected(int semitones);
    // Subetapa 4: clipboard (Cmd+C/X/V) and nudge (arrows). Paste puts the earliest copied note on
    // the project cursor; Left/Right nudge by nudgeStep(); Up/Down transpose by a semitone, with
    // Shift by an octave. Every edit is one named undo step through app/MidiEditCommands.
    int copySelected();
    int cutSelected();
    int pasteAtCursor();
    int nudgeSelected(int steps);
    // The drawn grid (1/16) when snap is on; otherwise the quantize grid.
    double nudgeStep()const;
    // Project cursor in beats (follows the tempo map).
    double cursorBeat()const;
    // Notes copied or cut in this editor; shared by every MIDI track shown here.
    app::NoteClipboard clipboard;
    bool keyPressed(const juce::KeyPress& k)override;
    // Active tool (MidiToolId). setTool notifies toolChanged so the toolbar follows keys and the toolbox.
    int tool=midiToolSelect;
    void setTool(int id);
    std::function<void(int)> toolChanged;
private:
    enum class Gesture{none,move,resize,marquee,draw};
    void openContextTools(const juce::MouseEvent& e,int hit);
    // Length of a note drawn with one click (Cubase: the quantize length, 1/16 here).
    double drawLength()const;
    double beatAt(float x)const;
    static constexpr int row=12;
    juce::Point<float> dragStart;int previewPitch=-1;
    Gesture gesture=Gesture::none;bool gestureRecorded=false;
    std::vector<MidiNote> gestureOriginal,gestureCurrent;std::vector<int> gestureIndices;
    domain::NoteSelection marqueeBase;juce::Rectangle<float> marquee;
    app::MidiEditContext context();
    bool valid()const;
    double grid()const;
    double quantize(double b)const;
    float beatX(double b)const;
    juce::Rectangle<float> noteRect(const MidiNote& n)const;
};
}

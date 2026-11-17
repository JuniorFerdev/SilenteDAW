#pragma once
#include "../../AudioEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../domain/Quantize.h"
#include "../../domain/NoteEdit.h"
#include <functional>

namespace silente::ui
{
// Lower-zone key editor for the selected MIDI/instrument track. It edits engine.tracks[track]
// notes under modelLock through the pure rules of domain/NoteEdit and reports through
// beforeEdit (undo snapshot, at most once per gesture) and changed (dirty).
// Selection: click selects one note, Shift+click toggles, dragging on empty space draws a
// marquee (Shift adds), Cmd+A selects all, Cmd+Shift+A deselects. Moving or resizing a selected
// note acts on the whole selection.
class PianoRoll final : public juce::Component
{
public:
    explicit PianoRoll(AudioEngine& e);
    // `selected` is the anchor note (last clicked or created), -1 when none; `selection` holds
    // every selected note by id.
    AudioEngine& engine;int track=0,selected=-1,topPitch=79;double pixelsPerBeat=64,scrollBeats=0;bool snap=true;
    domain::NoteSelection selection;
    std::function<void()> beforeEdit,changed;
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
    bool keyPressed(const juce::KeyPress& k)override;
private:
    enum class Gesture{none,move,resize,marquee};
    static constexpr int row=12;
    juce::Point<float> dragStart;int previewPitch=-1;
    Gesture gesture=Gesture::none;bool gestureRecorded=false;
    std::vector<MidiNote> gestureOriginal,gestureCurrent;std::vector<int> gestureIndices;
    domain::NoteSelection marqueeBase;juce::Rectangle<float> marquee;
    bool valid()const;
    double grid()const;
    double quantize(double b)const;
    float beatX(double b)const;
    juce::Rectangle<float> noteRect(const MidiNote& n)const;
};
}

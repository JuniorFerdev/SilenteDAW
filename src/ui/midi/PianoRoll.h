#pragma once
#include "../../AudioEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../domain/Quantize.h"
#include <functional>

namespace silente::ui
{
// Lower-zone key editor for the selected MIDI/instrument track. It edits engine.tracks[track]
// notes under modelLock and reports through beforeEdit (undo snapshot) and changed (dirty).
class PianoRoll final : public juce::Component
{
public:
    explicit PianoRoll(AudioEngine& e);
    AudioEngine& engine;int track=0,selected=-1,topPitch=79;double pixelsPerBeat=64,scrollBeats=0;bool snap=true;
    std::function<void()> beforeEdit,changed;
    void paint(juce::Graphics& g)override;
    void mouseDown(const juce::MouseEvent& e)override;
    void mouseDoubleClick(const juce::MouseEvent& e)override;
    void mouseDrag(const juce::MouseEvent& e)override;
    void mouseUp(const juce::MouseEvent&)override;
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w)override;
    domain::QuantizeSettings quantizeSettings;
    void deleteSelected();
    // Quantize command (Q): the selected note, or every note when none is selected.
    int quantizeNotes();
    bool keyPressed(const juce::KeyPress& k)override;
private:
    static constexpr int row=12;MidiNote original;juce::Point<float> dragStart;bool resize=false;int previewPitch=-1;
    bool valid()const;
    double quantize(double b)const;
    float beatX(double b)const;
    juce::Rectangle<float> noteRect(const MidiNote& n)const;
};
}

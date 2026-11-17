#pragma once
#include "../../AudioEngine.h"
#include "../../app/AutomationCommands.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace silente::ui
{
// Lower-zone automation lane («Automatización») for the selected track: a header with the
// parameter (volume, pan, sends), the track's R/W buttons and the curve, drawn on the same time
// axis as the project window (x = header + (seconds − scroll) · pixelsPerSecond).
// Editing (one undo step per gesture through beforeEdit; changed marks the project dirty):
//   click on empty space adds a point (and dragging keeps moving it), dragging a point moves it
//   between its neighbours, ⌥click on a point or Delete/Backspace on the selected point removes it.
// Points are placed freely (no grid snap yet). Model edits go through app::AutomationCommands.
class AutomationLane final : public juce::Component, private juce::Timer
{
public:
    explicit AutomationLane(AudioEngine&);
    ~AutomationLane() override;
    AudioEngine& engine;
    int track = 0, selected = -1;
    domain::AutomationTarget target;
    // Width of the header; the curve area starts there, like the event display.
    int header = 250;
    // Time axis of the project window; when empty the lane uses its own defaults.
    std::function<void(double& pixelsPerSecond, double& scrollSeconds)> timeline;
    std::function<void()> beforeEdit, changed;
    static constexpr int top = 8, bottomMargin = 8;

    void setTrack(int index);
    void setTarget(domain::AutomationTarget);
    void refresh(); // parameter list and R/W state from the model

    // Geometry (also used by the tests).
    float beatToX(double beat) const;
    double xToBeat(float x) const;
    float valueToY(double value) const;
    double yToValue(float y) const;
    int pointAt(juce::Point<float>) const;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void deleteSelected();

private:
    juce::ComboBox parameter;
    juce::TextButton read { "R" }, write { "W" };
    bool dragging = false, moved = false, gestureRecorded = false, syncing = false;
    juce::Point<float> dragStart;
    domain::AutomationPoint dragOrigin;
    Track* current() const;
    void view(double& pixelsPerSecond, double& scrollSeconds) const;
    // Display scale 0–1: volume in dB (−∞…+12), pan linear, sends in dB (−60…+6).
    double toDisplay(double value) const;
    double fromDisplay(double proportion) const;
    void timerCallback() override;
};
}

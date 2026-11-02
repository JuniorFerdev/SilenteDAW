#pragma once
#include "../theme/Colours.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace silente::ui
{
// Draggable border between two zones of the project window (Cubase: drag the divider between
// the Inspector, the event display, the right zone and the lower zone to resize them).
// `onDrag` receives the offset in pixels from where the drag started.
class ZoneSplitter final : public juce::Component
{
public:
    explicit ZoneSplitter(bool movesHorizontally) : horizontal(movesHorizontally)
    {
        setMouseCursor(horizontal ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor);
        setRepaintsOnMouseActivity(true);
    }
    std::function<void()> onStart, onEnd;
    std::function<void(int)> onDrag;

    void paint(juce::Graphics& g) override
    {
        if (! isMouseOverOrDragging()) return;
        g.setColour(theme::accent.withAlpha(.7f));
        g.fillRect(horizontal ? getLocalBounds().withSizeKeepingCentre(2, getHeight()) : getLocalBounds().withSizeKeepingCentre(getWidth(), 2));
    }
    void mouseDown(const juce::MouseEvent&) override { if (onStart) onStart(); }
    void mouseDrag(const juce::MouseEvent& e) override { if (onDrag) onDrag(horizontal ? e.getDistanceFromDragStartX() : e.getDistanceFromDragStartY()); }
    void mouseUp(const juce::MouseEvent&) override { if (onEnd) onEnd(); }

private:
    bool horizontal;
};
}

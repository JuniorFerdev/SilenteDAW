#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace silente::ui
{
// Main area opens the editor; arrow/right click opens slot actions.
class PluginSlotButton final : public juce::TextButton
{
public:
    std::function<void()> onOptions;
    void mouseDown(const juce::MouseEvent& event) override
    {
        if(event.mods.isPopupMenu()||event.x>=getWidth()-21){if(onOptions)onOptions();return;}
        juce::TextButton::mouseDown(event);
    }
    void paintButton(juce::Graphics& g,bool over,bool down) override
    {
        juce::TextButton::paintButton(g,over,down);
        g.setColour(findColour(juce::TextButton::textColourOffId).withAlpha(.7f));
        juce::Path arrow;const float x=static_cast<float>(getWidth()-12),y=static_cast<float>(getHeight())*.5f;
        arrow.addTriangle(x-3,y-2,x+3,y-2,x,y+2);g.fillPath(arrow);
    }
};
}

#include "Drawing.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace silente::ui::theme
{
void font(juce::Graphics& g, float size, bool bold) { g.setFont(juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain))); }
void text(juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, float size, juce::Colour c, juce::Justification j)
{ g.setColour(c); font(g, size); g.drawText(t, r, j, true); }
void logo(juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour(accent); g.fillRoundedRectangle(r, 10);
    juce::Path p; p.startNewSubPath(26, 9); p.cubicTo(8, 3, 4, 19, 17, 20); p.cubicTo(31, 21, 27, 37, 10, 30);
    g.setColour(bg); g.strokePath(p, juce::PathStrokeType(4, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::scale(r.getWidth()/38, r.getHeight()/38).translated(r.getX(), r.getY()));
}
}

#include "ToolIcons.h"

namespace silente::ui::theme
{
void setButtonIcon(juce::TextButton& button, ToolIcon icon)
{
    button.getProperties().set("silenteIcon", static_cast<int>(icon));
    button.setWantsKeyboardFocus(false);
    button.repaint();
}
void drawToolIcon(juce::Graphics& g, ToolIcon icon, juce::Rectangle<float> bounds, juce::Colour colour)
{
    juce::Graphics::ScopedSaveState saved(g);
    const float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    g.addTransform(juce::AffineTransform::scale(size / 24.0f).translated(bounds.getCentreX() - size * .5f, bounds.getCentreY() - size * .5f));
    g.setColour(colour);
    juce::Path p;
    const juce::PathStrokeType stroke(1.65f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    switch (icon)
    {
        case ToolIcon::select:
            p.startNewSubPath(5, 2); p.lineTo(5, 19); p.lineTo(9.5f, 15); p.lineTo(13, 22); p.lineTo(16, 20.5f); p.lineTo(12.5f, 14); p.lineTo(19, 14); p.closeSubPath(); g.fillPath(p); break;
        case ToolIcon::scissors:
            g.drawEllipse(2, 3, 6, 6, 1.7f); g.drawEllipse(2, 15, 6, 6, 1.7f);
            p.startNewSubPath(7, 7); p.lineTo(21, 20); p.startNewSubPath(7, 17); p.lineTo(21, 4); g.strokePath(p, stroke); break;
        case ToolIcon::erase:
            p.startNewSubPath(3, 14); p.lineTo(12, 4); p.lineTo(21, 12); p.lineTo(13, 21); p.lineTo(10, 21); p.closeSubPath(); g.strokePath(p, stroke);
            p.clear(); p.startNewSubPath(7, 10); p.lineTo(17, 18); g.strokePath(p, stroke); g.drawLine(10, 22, 22, 22, 1.5f); break;
        case ToolIcon::pencil:
            p.startNewSubPath(4, 20); p.lineTo(6, 13); p.lineTo(17, 2); p.lineTo(22, 7); p.lineTo(11, 18); p.closeSubPath(); g.strokePath(p, stroke);
            g.drawLine(14, 5, 19, 10, 1.5f); g.drawLine(6, 13, 11, 18, 1.5f); g.fillEllipse(3, 19, 3, 3); break;
        case ToolIcon::palette:
            p.startNewSubPath(12, 3); p.cubicTo(1, 3, 0, 18, 9, 21); p.cubicTo(15, 23, 11, 16, 16, 16); p.cubicTo(25, 18, 25, 3, 12, 3); g.strokePath(p, stroke);
            for (auto point : {juce::Point<float>(7, 8), {13, 6}, {18, 9}, {6, 14}}) g.fillEllipse(point.x - 1.5f, point.y - 1.5f, 3, 3); break;
        case ToolIcon::rewind:
            g.fillRect(4.0f, 5.0f, 2.0f, 14.0f); p.addTriangle(19, 4, 8, 12, 19, 20); g.fillPath(p); break;
        case ToolIcon::stop: g.fillRect(5.0f, 5.0f, 14.0f, 14.0f); break;
        case ToolIcon::play: p.addTriangle(6, 3, 21, 12, 6, 21); g.fillPath(p); break;
        case ToolIcon::pause: g.fillRect(5.0f, 4.0f, 5.0f, 16.0f); g.fillRect(14.0f, 4.0f, 5.0f, 16.0f); break;
        case ToolIcon::record: g.fillEllipse(4, 4, 16, 16); break;
        case ToolIcon::cycle:
            p.startNewSubPath(19, 8); p.cubicTo(15, 0, 3, 3, 3, 12); p.startNewSubPath(5, 16); p.cubicTo(9, 24, 21, 21, 21, 12); g.strokePath(p, stroke);
            p.clear(); p.addTriangle(14, 8, 21, 8, 21, 1); p.addTriangle(3, 16, 10, 16, 3, 23); g.fillPath(p); break;
        case ToolIcon::click:
            // Metronome: tapered case with a rounded top, base, swinging pendulum and its weight.
            p.startNewSubPath(6.5f, 20.5f); p.lineTo(10.0f, 4.5f); p.quadraticTo(12.0f, 2.5f, 14.0f, 4.5f); p.lineTo(17.5f, 20.5f); p.closeSubPath(); g.strokePath(p, stroke);
            g.drawLine(4.5f, 21.0f, 19.5f, 21.0f, 2.0f);
            g.drawLine(12.0f, 17.0f, 17.5f, 6.0f, 1.6f);
            g.fillRoundedRectangle(14.2f, 9.2f, 3.2f, 3.2f, 0.8f);
            g.fillEllipse(11.0f, 16.0f, 2.0f, 2.0f); break;
        case ToolIcon::undo: case ToolIcon::redo:
            if (icon == ToolIcon::redo) g.addTransform(juce::AffineTransform(-1, 0, 24, 0, 1, 0));
            p.startNewSubPath(5, 8); p.cubicTo(20, 2, 26, 16, 14, 20); g.strokePath(p, stroke);
            p.clear(); p.addTriangle(3, 10, 11, 11, 5, 3); g.fillPath(p); break;
        case ToolIcon::mixer:
            // Three faders with their caps.
            for (float x : { 6.0f, 12.0f, 18.0f }) g.drawLine(x, 3.0f, x, 21.0f, 1.4f);
            g.fillRoundedRectangle(3.5f, 13.0f, 5.0f, 3.0f, 1.0f); g.fillRoundedRectangle(9.5f, 6.0f, 5.0f, 3.0f, 1.0f); g.fillRoundedRectangle(15.5f, 10.0f, 5.0f, 3.0f, 1.0f); break;
        case ToolIcon::zoneChannel: case ToolIcon::zoneLeft: case ToolIcon::zoneLower: case ToolIcon::zoneRight:
        {
            // Window outline with the zone it toggles filled in.
            g.drawRoundedRectangle(2.5f, 4.5f, 19.0f, 15.0f, 1.5f, 1.5f);
            juce::Rectangle<float> zone;
            if (icon == ToolIcon::zoneChannel) zone = { 3.5f, 5.5f, 3.0f, 13.0f };
            else if (icon == ToolIcon::zoneLeft) zone = { 3.5f, 5.5f, 6.0f, 13.0f };
            else if (icon == ToolIcon::zoneLower) zone = { 3.5f, 13.5f, 17.0f, 5.0f };
            else zone = { 14.5f, 5.5f, 6.0f, 13.0f };
            g.fillRect(zone); break;
        }
        case ToolIcon::filter:
            p.startNewSubPath(3, 4); p.lineTo(21, 4); p.lineTo(14, 12); p.lineTo(14, 20); p.lineTo(10, 18); p.lineTo(10, 12); p.closeSubPath(); g.strokePath(p, stroke); break;
        case ToolIcon::search:
            g.drawEllipse(4, 3, 12, 12, 1.8f); g.drawLine(14.5f, 13.5f, 20.5f, 20.5f, 2.2f); break;
        case ToolIcon::hub: // house outline: the Hub button at the far left of the toolbar
            p.startNewSubPath(3, 11.5f); p.lineTo(12, 3.5f); p.lineTo(21, 11.5f); g.strokePath(p, stroke);
            g.drawRect(6.0f, 11.0f, 12.0f, 9.5f, 1.6f); g.fillRect(10.5f, 15.0f, 3.0f, 5.5f); break;
        case ToolIcon::exportAudio: // tray with an arrow leaving it
            p.startNewSubPath(4, 13); p.lineTo(4, 20); p.lineTo(20, 20); p.lineTo(20, 13); g.strokePath(p, stroke);
            g.drawLine(12, 15, 12, 4, 1.8f); { juce::Path a; a.addTriangle(7.5f, 8.0f, 16.5f, 8.0f, 12.0f, 3.0f); g.fillPath(a); } break;
        case ToolIcon::punchIn: // arrow entering a bracket: recording starts at the left locator
            g.drawLine(15.5f, 5.0f, 15.5f, 19.0f, 1.8f); g.drawLine(15.5f, 5.0f, 20.0f, 5.0f, 1.8f); g.drawLine(15.5f, 19.0f, 20.0f, 19.0f, 1.8f);
            g.drawLine(3.0f, 12.0f, 11.0f, 12.0f, 1.8f); { juce::Path a; a.addTriangle(9.0f, 8.0f, 9.0f, 16.0f, 14.0f, 12.0f); g.fillPath(a); } break;
        case ToolIcon::punchOut: // arrow leaving a bracket: recording stops at the right locator
            g.drawLine(8.5f, 5.0f, 8.5f, 19.0f, 1.8f); g.drawLine(4.0f, 5.0f, 8.5f, 5.0f, 1.8f); g.drawLine(4.0f, 19.0f, 8.5f, 19.0f, 1.8f);
            g.drawLine(11.0f, 12.0f, 17.0f, 12.0f, 1.8f); { juce::Path a; a.addTriangle(16.0f, 8.0f, 16.0f, 16.0f, 21.0f, 12.0f); g.fillPath(a); } break;
        case ToolIcon::layout:
            g.drawRoundedRectangle(2.5f, 4.5f, 19.0f, 15.0f, 1.5f, 1.5f);
            g.drawLine(2.5f, 9.0f, 21.5f, 9.0f, 1.2f); g.drawLine(9.0f, 9.0f, 9.0f, 19.5f, 1.2f); break;
        case ToolIcon::snap:
            p.startNewSubPath(5, 3); p.lineTo(5, 13); p.cubicTo(5, 24, 19, 24, 19, 13); p.lineTo(19, 3); p.lineTo(14, 3); p.lineTo(14, 13); p.cubicTo(14, 17, 10, 17, 10, 13); p.lineTo(10, 3); p.closeSubPath(); g.strokePath(p, stroke); g.drawLine(5, 8, 10, 8, 1.5f); g.drawLine(14, 8, 19, 8, 1.5f); break;
    }
}
}

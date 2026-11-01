#include "SilenteLookAndFeel.h"
#include "ToolIcons.h"

namespace silente::ui::theme
{
SilenteLookAndFeel::SilenteLookAndFeel()
    {
        setColour(juce::TextButton::buttonColourId, raised); setColour(juce::TextButton::buttonOnColourId, activeFill);
        setColour(juce::TextButton::textColourOffId, ink); setColour(juce::TextButton::textColourOnId, activeInk);
        setColour(juce::Slider::thumbColourId, faderCap); setColour(juce::Slider::trackColourId, sliderFill); setColour(juce::Slider::backgroundColourId, line); setColour(juce::Slider::rotarySliderFillColourId, knobRing); setColour(juce::Slider::rotarySliderOutlineColourId, raised);
        setColour(juce::Slider::textBoxTextColourId, ink); setColour(juce::Slider::textBoxBackgroundColourId, bg); setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Label::textColourId, ink); setColour(juce::TextEditor::textColourId, ink); setColour(juce::TextEditor::backgroundColourId, bg); setColour(juce::TextEditor::outlineColourId, line);
        setColour(juce::ComboBox::backgroundColourId, raised); setColour(juce::ComboBox::textColourId, ink); setColour(juce::ComboBox::outlineColourId, line);
        setColour(juce::PopupMenu::backgroundColourId, panel); setColour(juce::PopupMenu::textColourId, ink); setColour(juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha(.35f));
        setColour(juce::AlertWindow::backgroundColourId, panel); setColour(juce::AlertWindow::textColourId, ink); setColour(juce::AlertWindow::outlineColourId, line);
        setColour(juce::ScrollBar::thumbColourId, line.brighter(.15f)); setColour(juce::ListBox::backgroundColourId, bg); setColour(juce::ListBox::textColourId, ink);
    }
juce::Font SilenteLookAndFeel::getTextButtonFont(juce::TextButton&, int h) { return juce::Font(juce::FontOptions(juce::jmin(14.0f, h * .42f))); }
void SilenteLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& c, bool over, bool down)
    {
        auto r = b.getLocalBounds().toFloat().reduced(.5f);
        g.setColour(c.withMultipliedAlpha(b.isEnabled() ? 1.0f : .35f).brighter(down ? .12f : over ? .06f : 0)); g.fillRoundedRectangle(r, 2);
        g.setColour(line); g.drawRoundedRectangle(r, 2, 1);
    }
void SilenteLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool over, bool down)
{
    if (button.getProperties().contains("silenteIcon"))
    {
        auto icon = static_cast<ToolIcon>(static_cast<int>(button.getProperties()["silenteIcon"]));
        if (icon == ToolIcon::play && button.getToggleState()) icon = ToolIcon::pause;
        auto colour = button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);
        drawToolIcon(g, icon, button.getLocalBounds().toFloat().reduced(6), colour.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.35f));
    }
    else if (button.getWidth() < 26)
    {
        // Narrow channel buttons (M, S, e, R…): the letter is drawn whole, centred, without the
        // default side indent that turned it into «…».
        g.setColour(button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId).withMultipliedAlpha(button.isEnabled() ? 1.0f : .5f));
        g.setFont(juce::Font(juce::FontOptions(juce::jmin(12.0f, button.getHeight() * .55f), juce::Font::bold)));
        g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
    }
    else LookAndFeel_V4::drawButtonText(g, button, over, down);
}

int SilenteLookAndFeel::getSliderThumbRadius(juce::Slider& s)
{
    return s.isVertical() ? 8 : 6;
}

void SilenteLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float position, float, float,
                                          juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style == juce::Slider::LinearVertical)
    {
        // Fader: narrow dark groove, rectangular light-grey cap with a centre line.
        const float cx = (float) x + (float) width * 0.5f;
        g.setColour(faderGroove); g.fillRoundedRectangle(cx - 2.5f, (float) y, 5.0f, (float) height, 2.0f);
        g.setColour(line.brighter(.25f)); g.drawRoundedRectangle(cx - 2.5f, (float) y, 5.0f, (float) height, 2.0f, 1.0f);
        const float capW = juce::jmin(26.0f, (float) width - 4.0f), capH = 15.0f;
        juce::Rectangle<float> cap(cx - capW * 0.5f, position - capH * 0.5f, capW, capH);
        juce::ColourGradient grad(faderCap.brighter(.25f), cap.getX(), cap.getY(), faderCap.darker(.35f), cap.getX(), cap.getBottom(), false);
        g.setGradientFill(grad); g.fillRoundedRectangle(cap, 2.0f);
        g.setColour(juce::Colours::black.withAlpha(.65f)); g.drawRoundedRectangle(cap, 2.0f, 1.0f);
        g.setColour(faderGroove); g.fillRect(cap.getX() + 3.0f, cap.getCentreY() - 0.75f, cap.getWidth() - 6.0f, 1.5f);
        juce::ignoreUnused(s);
        return;
    }
    if (style == juce::Slider::LinearHorizontal)
    {
        // Thin line, lighter up to the value, white dot.
        const float cy = (float) y + (float) height * 0.5f;
        g.setColour(raised.brighter(.15f)); g.fillRoundedRectangle((float) x, cy - 1.5f, (float) width, 3.0f, 1.5f);
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0 && s.getSkewFactor() == 1.0;
        const float zero = bipolar ? (float) x + (float) width * (float) ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) : (float) x;
        g.setColour(s.findColour(juce::Slider::trackColourId)); g.fillRoundedRectangle(juce::jmin(zero, position), cy - 1.5f, std::abs(position - zero), 3.0f, 1.5f);
        g.setColour(s.findColour(juce::Slider::thumbColourId).brighter(.3f)); g.fillEllipse(position - 5.5f, cy - 5.5f, 11.0f, 11.0f);
        g.setColour(juce::Colours::black.withAlpha(.6f)); g.drawEllipse(position - 5.5f, cy - 5.5f, 11.0f, 11.0f, 1.0f);
        return;
    }
    LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, position, 0, 0, style, s);
}

void SilenteLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position, float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(3.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + position * (endAngle - startAngle);
    // Value ring (from the centre for bipolar controls such as EQ gain), dark body, white pointer.
    juce::Path ring; ring.addCentredArc(centre.x, centre.y, radius - 1.5f, radius - 1.5f, 0.0f, startAngle, endAngle, true);
    g.setColour(raised.brighter(.1f)); g.strokePath(ring, juce::PathStrokeType(2.5f));
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    juce::Path value; value.addCentredArc(centre.x, centre.y, radius - 1.5f, radius - 1.5f, 0.0f, juce::jmin(from, angle), juce::jmax(from, angle), true);
    g.setColour(s.findColour(juce::Slider::rotarySliderFillColourId)); g.strokePath(value, juce::PathStrokeType(2.5f));
    const float body = radius - 5.0f;
    juce::ColourGradient grad(raised.brighter(.2f), centre.x, centre.y - body, faderGroove, centre.x, centre.y + body, false);
    g.setGradientFill(grad); g.fillEllipse(centre.x - body, centre.y - body, body * 2.0f, body * 2.0f);
    g.setColour(juce::Colours::black.withAlpha(.7f)); g.drawEllipse(centre.x - body, centre.y - body, body * 2.0f, body * 2.0f, 1.0f);
    g.setColour(ink);
    g.drawLine(centre.x + std::sin(angle) * body * 0.25f, centre.y - std::cos(angle) * body * 0.25f, centre.x + std::sin(angle) * body * 0.9f, centre.y - std::cos(angle) * body * 0.9f, 2.0f);
}
}

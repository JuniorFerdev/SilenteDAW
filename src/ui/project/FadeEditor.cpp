#include "FadeEditor.h"
#include "../../audio/FadeCurves.h"
#include "../../i18n/Translations.h"
#include "../theme/Drawing.h"

namespace silente::ui
{
using namespace theme;

FadeEditor::FadeEditor(Settings initial, double eventLength, std::function<void(Settings)> a, std::function<void(Settings)> d)
    : settings(initial), length(juce::jmax(0.01, eventLength)), apply(std::move(a)), makeDefault(std::move(d))
{
    setLookAndFeel(&look);
    const juce::String names[] { i18n::tr("Lineal"), i18n::tr("Igual energía"), i18n::tr("Curva S"), i18n::tr("Rápida"), i18n::tr("Lenta") };
    for (int side = 0; side < 2; ++side)
    {
        for (int n = 0; n < 5; ++n)
        {
            auto& b = shapes[(size_t) side][(size_t) n];
            b.setButtonText(names[n]); b.setClickingTogglesState(false); addAndMakeVisible(b);
            b.setComponentID((side == 0 ? "fade-in-shape-" : "fade-out-shape-") + juce::String(n));
            b.onClick = [this, side, n] { setShape(side == 0, n); };
        }
        auto& s = lengths[(size_t) side];
        s.setSliderStyle(juce::Slider::LinearHorizontal); s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 22);
        s.setRange(0.0, length, 0.001); s.setTextValueSuffix(" s"); s.setComponentID(side == 0 ? "fade-in-length" : "fade-out-length");
        s.onValueChange = [this, side] { setLength(side == 0, lengths[(size_t) side].getValue()); };
        addAndMakeVisible(s);
    }
    defaults.setButtonText(i18n::tr("Por defecto")); defaults.setTooltip(i18n::tr("Guarda el fundido de entrada como fundido estándar"));
    applyButton.setButtonText(i18n::tr("Aplicar"));
    defaults.onClick = [this] { pressDefault(); }; applyButton.onClick = [this] { pressApply(); };
    addAndMakeVisible(defaults); addAndMakeVisible(applyButton);
    setSize(620, 330);
    sync();
}

void FadeEditor::setShape(bool fadeIn, int shape)
{
    (fadeIn ? settings.inShape : settings.outShape) = juce::jlimit(0, (int) audio::FadeShape::count - 1, shape);
    sync();
}

void FadeEditor::setLength(bool fadeIn, double seconds)
{
    // A fade-in and a fade-out together never exceed the event.
    if (fadeIn) settings.fadeIn = juce::jlimit(0.0, length - settings.fadeOut, seconds);
    else settings.fadeOut = juce::jlimit(0.0, length - settings.fadeIn, seconds);
    sync();
}

void FadeEditor::sync()
{
    for (int side = 0; side < 2; ++side)
    {
        const int chosen = side == 0 ? settings.inShape : settings.outShape;
        for (int n = 0; n < 5; ++n) shapes[(size_t) side][(size_t) n].setToggleState(n == chosen, juce::dontSendNotification);
        lengths[(size_t) side].setValue(side == 0 ? settings.fadeIn : settings.fadeOut, juce::dontSendNotification);
    }
    repaint();
}

juce::Rectangle<int> FadeEditor::curveArea(int side) const { return { 16 + side * 304, 44, 284, 150 }; }

void FadeEditor::resized()
{
    for (int side = 0; side < 2; ++side)
    {
        const auto area = curveArea(side);
        auto row = juce::Rectangle<int>(area.getX(), area.getBottom() + 8, area.getWidth(), 26);
        const int w = (row.getWidth() - 8) / 5;
        for (auto& b : shapes[(size_t) side]) { b.setBounds(row.removeFromLeft(w)); row.removeFromLeft(2); }
        lengths[(size_t) side].setBounds(area.getX(), area.getBottom() + 42, area.getWidth(), 24);
    }
    applyButton.setBounds(getWidth() - 116, getHeight() - 40, 100, 28);
    defaults.setBounds(getWidth() - 226, getHeight() - 40, 104, 28);
}

void FadeEditor::paint(juce::Graphics& g)
{
    g.fillAll(panel);
    for (int side = 0; side < 2; ++side)
    {
        const auto area = curveArea(side);
        text(g, side == 0 ? i18n::tr("Fundido de entrada") : i18n::tr("Fundido de salida"), { area.getX(), 12, area.getWidth(), 24 }, 13.0f, ink);
        g.setColour(juce::Colour(0xff0b0c0d)); g.fillRect(area);
        g.setColour(line.brighter(.3f)); g.drawRect(area);
        const int shape = side == 0 ? settings.inShape : settings.outShape;
        juce::Path curve;
        for (int x = 0; x <= area.getWidth(); ++x)
        {
            const double t = (double) x / area.getWidth();
            const float gain = audio::fadeGain(shape, side == 0 ? t : 1.0 - t);
            const float y = (float) area.getBottom() - 6.0f - gain * ((float) area.getHeight() - 12.0f);
            if (x == 0) curve.startNewSubPath((float) area.getX(), y); else curve.lineTo((float) (area.getX() + x), y);
        }
        g.setColour(accent); g.strokePath(curve, juce::PathStrokeType(2.0f));
    }
}

void showFadeEditor(juce::Component& parent, FadeEditor::Settings initial, double eventLength,
                    std::function<void(FadeEditor::Settings)> apply, std::function<void(FadeEditor::Settings)> makeDefault)
{
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(new FadeEditor(initial, eventLength, std::move(apply), std::move(makeDefault)));
    options.dialogTitle = i18n::tr("Fundidos");
    options.componentToCentreAround = &parent;
    options.dialogBackgroundColour = panel;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}
}

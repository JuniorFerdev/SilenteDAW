#include "TapTempoPanel.h"
#include "../../i18n/Translations.h"
#include "../theme/Drawing.h"

namespace silente::ui
{
using namespace theme;

TapTempoPanel::TapTempoPanel()
{
    setLookAndFeel(&look);
    setWantsKeyboardFocus(true);
    for (auto* b : { &half, &twice, &reset, &apply }) { addAndMakeVisible(b); b->setWantsKeyboardFocus(false); }
    reset.setButtonText(i18n::tr("Reiniciar"));
    apply.setButtonText(i18n::tr("Aplicar al proyecto"));
    half.setTooltip(i18n::tr("Mitad del tempo")); twice.setTooltip(i18n::tr("Doble del tempo"));
    half.onClick = [this] { factor *= 0.5; repaint(); };
    twice.onClick = [this] { factor *= 2.0; repaint(); };
    reset.onClick = [this] { taps.reset(); factor = 1.0; repaint(); grabKeyboardFocus(); };
    apply.onClick = [this] { if (taps.hasTempo() && onApply) onApply(std::round(shownBpm() * 10.0) / 10.0); grabKeyboardFocus(); };
    setSize(420, 330);
    startTimerHz(30);
}

void TapTempoPanel::tap()
{
    taps.tap(juce::Time::getMillisecondCounterHiRes() / 1000.0);
    flash = 1.0f;
    repaint();
}

bool TapTempoPanel::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::spaceKey || key.getKeyCode() == juce::KeyPress::returnKey) { tap(); return true; }
    if (key.getKeyCode() == juce::KeyPress::backspaceKey || key.getKeyCode() == juce::KeyPress::deleteKey) { reset.triggerClick(); return true; }
    return false;
}

void TapTempoPanel::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (pad.contains(e.getPosition())) tap();
}

void TapTempoPanel::visibilityChanged() { if (isShowing()) grabKeyboardFocus(); }

void TapTempoPanel::timerCallback()
{
    if (flash > 0.0f) { flash = juce::jmax(0.0f, flash - 0.12f); repaint(pad); }
}

void TapTempoPanel::resized()
{
    auto r = getLocalBounds().reduced(16);
    auto buttons = r.removeFromBottom(32);
    apply.setBounds(buttons.removeFromRight(160));
    buttons.removeFromRight(8);
    reset.setBounds(buttons.removeFromLeft(96)); buttons.removeFromLeft(8);
    half.setBounds(buttons.removeFromLeft(44)); buttons.removeFromLeft(6);
    twice.setBounds(buttons.removeFromLeft(44));
    r.removeFromBottom(12);
    pad = r;
}

void TapTempoPanel::paint(juce::Graphics& g)
{
    g.fillAll(panel);
    const auto area = pad.toFloat();
    g.setColour(raised.interpolatedWith(accent, flash * 0.35f));
    g.fillRoundedRectangle(area, 8.0f);
    g.setColour(hasKeyboardFocus(true) ? accent : line);
    g.drawRoundedRectangle(area.reduced(0.5f), 8.0f, 1.2f);
    const auto bpmText = taps.hasTempo() ? juce::String(shownBpm(), 1) : juce::String::fromUTF8("—");
    text(g, bpmText, pad.withTrimmedTop(26).withHeight(76), 56, ink, juce::Justification::centred);
    text(g, "BPM", pad.withTrimmedTop(104).withHeight(20), 12, muted, juce::Justification::centred);
    juce::String detail;
    if (taps.tapCount() == 0) detail = i18n::tr("Pulsa Espacio o haz clic aquí al ritmo de la música");
    else if (! taps.hasTempo()) detail = i18n::tr("Sigue marcando…");
    else detail = juce::String(taps.tapCount()) + i18n::tr(" toques · variación ") + juce::String(taps.spreadPercent(), 1) + " %";
    text(g, detail, pad.withTrimmedTop(140).withHeight(22), 12, muted, juce::Justification::centred);
    text(g, i18n::tr("Retroceso reinicia · una pausa de más de 2 s empieza de nuevo"), pad.withTrimmedTop(pad.getHeight() - 26), 10, muted, juce::Justification::centred);
}
}

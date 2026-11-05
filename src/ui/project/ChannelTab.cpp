#include "ChannelTab.h"
#include "../../i18n/Translations.h"
#include "../../plugins/PluginSelection.h"
#include "../theme/Drawing.h"

namespace silente::ui
{
using namespace theme;

ChannelTab::ChannelTab(AudioEngine& e, PluginManager& p) : engine(e), plugins(p), inserts(e,p,"channel-inserts")
{
    addAndMakeVisible(sendScroll);
    sendScroll.setComponentID("channel-send-scroll");
    sendScroll.setRangeLimits(0, Track::sendCount);
    sendScroll.setCurrentRange(0, 4);
    sendScroll.setSingleStepSize(1);
    sendScroll.setTitle(i18n::tr("Desplazar envíos (1–16)"));
    sendScroll.addListener(this);
    addAndMakeVisible(inserts);
    for (auto* c : std::initializer_list<juce::Component*> { &mute, &solo, &edit, &arm, &monitor, &fader, &volume }) addAndMakeVisible(c);
    arm.setButtonText(juce::String::fromUTF8("●")); monitor.setButtonText(juce::String::fromUTF8("◉"));
    arm.setClickingTogglesState(true); monitor.setClickingTogglesState(true);
    arm.setColour(juce::TextButton::buttonOnColourId, coral); monitor.setColour(juce::TextButton::buttonOnColourId, meterWarning);
    edit.setTooltip(i18n::tr("Editar configuración de canal"));
    edit.onClick = [this] { if (index >= 0 && openSettings) openSettings(index); };
    arm.onClick = [this] { if (track && track->hasEvents()) { if (beforeEdit) beforeEdit(); track->armed = arm.getToggleState(); if (changed) changed(); } };
    monitor.onClick = [this] { if (track && track->hasEvents()) { if (beforeEdit) beforeEdit(); track->monitor = monitor.getToggleState(); if (changed) changed(); } };
    mute.setClickingTogglesState(true); solo.setClickingTogglesState(true);
    mute.setColour(juce::TextButton::buttonOnColourId, muteColour);
    solo.setColour(juce::TextButton::buttonOnColourId, soloColour);
    mute.setTooltip(i18n::tr("Silenciar")); solo.setTooltip(i18n::tr("Solo"));
    fader.setSliderStyle(juce::Slider::LinearVertical);
    fader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    fader.setRange(-60.0, 6.0, 0.1); fader.setSkewFactorFromMidPoint(-14.0);
    fader.setDoubleClickReturnValue(true, 0.0);
    fader.setColour(juce::Slider::backgroundColourId, bg);
    volume.setJustificationType(juce::Justification::centred);
    volume.setFont(juce::Font(juce::FontOptions(11.0f)));
    volume.setColour(juce::Label::backgroundColourId, bg);
    fader.onDragStart = [this] { if (beforeEdit) beforeEdit(); };
    fader.onValueChange = [this]
    {
        if (syncing || ! track) return;
        const double db = fader.getValue();
        track->gain = db <= -60.0 ? 0.0f : juce::Decibels::decibelsToGain((float) db);
        if (changed) changed();
    };
    mute.onClick = [this] { if (track) { if (beforeEdit) beforeEdit(); track->mute = mute.getToggleState(); if (changed) changed(); } };
    solo.onClick = [this] { if (track) { if (beforeEdit) beforeEdit(); track->solo = solo.getToggleState(); if (changed) changed(); } };

}

void ChannelTab::setTrack(int i)
{
    index = juce::isPositiveAndBelow(i, (int) engine.tracks.size()) ? i : -1;
    track = index >= 0 ? engine.tracks[(size_t) index] : nullptr;
    if (track && !track->hasAudioChannel()) { track = nullptr; index = -1; }
    refresh();
}

void ChannelTab::refresh()
{
    // The track may have been removed or replaced since the last selection.
    if (track && (! juce::isPositiveAndBelow(index, (int) engine.tracks.size()) || engine.tracks[(size_t) index] != track)) { track = nullptr; index = -1; }
    const bool has = track != nullptr;
    for (auto* c : std::initializer_list<juce::Component*> { &mute, &solo, &edit, &fader, &volume, &sendScroll }) c->setVisible(has);
    arm.setVisible(has && track->hasEvents()); monitor.setVisible(has && track->hasEvents());
    inserts.setVisible(has);inserts.setTrack(index);
    if (has)
    {
        syncing = true;
        if (! fader.isMouseButtonDown()) fader.setValue(juce::jmax(-60.0f, juce::Decibels::gainToDecibels(track->gain.load(), -100.0f)), juce::dontSendNotification);
        volume.setText(fader.getValue() <= -60.0 ? juce::String::fromUTF8("−∞") : juce::String(fader.getValue(), 1), juce::dontSendNotification);
        mute.setToggleState(track->mute.load(), juce::dontSendNotification);
        solo.setToggleState(track->solo.load(), juce::dontSendNotification);
        arm.setToggleState(track->armed.load(), juce::dontSendNotification); monitor.setToggleState(track->monitor.load(), juce::dontSendNotification);

        syncing = false;
    }
    // Repaint the column only when what it shows changed; otherwise just the meters (24 Hz).
    juce::String shown = juce::String(index) + (track ? track->name + track->colour.toString() + juce::String(track->lowGainDb.load()) + juce::String(track->midGainDb.load())
                                                         + juce::String(track->highGainDb.load()) + juce::String((int) track->plugins.size()) : juce::String());
    if (track) for (const auto& send : track->sends) shown << send.target << juce::String(send.levelDb.load());
    if (shown != lastShown) { lastShown = shown; repaint(); return; }
    const bool active = track && (track->peakL.load() > 1.0e-6f || track->peakR.load() > 1.0e-6f);
    if (active || metersActive) repaint(meterArea.expanded(2));
    metersActive = active;
}

void ChannelTab::resized()
{
    // Cubase «Canal» column: name, EQ curve, Inserts and Sends sections, then the fader section.
    auto r = getLocalBounds().reduced(5, 4);
    r.removeFromTop(24);
    nameArea = r.removeFromTop(22); r.removeFromTop(4);
    eqArea = r.removeFromTop(38); r.removeFromTop(6);
    insertsHeader = r.removeFromTop(18); r.removeFromTop(2);
    inserts.setBounds(r.removeFromTop(84));
    r.removeFromTop(4);
    sendsHeader = r.removeFromTop(18); r.removeFromTop(2);
    sendsArea = r.removeFromTop(4 * 17); r.removeFromTop(8);
    sendScroll.setBounds(sendsArea.removeFromRight(9));
    sendsArea.removeFromRight(2);
    bottomName = r.removeFromBottom(22); r.removeFromBottom(4);
    auto row = r.removeFromBottom(22);
    arm.setBounds(row.removeFromLeft(row.getWidth() / 2 - 2)); row.removeFromLeft(4); monitor.setBounds(row);
    r.removeFromBottom(4);
    volume.setBounds(r.removeFromBottom(20)); r.removeFromBottom(4);
    row = r.removeFromTop(22);
    const int third = (row.getWidth() - 6) / 3;
    mute.setBounds(row.removeFromLeft(third)); row.removeFromLeft(3); solo.setBounds(row.removeFromLeft(third)); row.removeFromLeft(3); edit.setBounds(row);
    r.removeFromTop(6);
    meterArea = r.removeFromRight(22);
    scaleArea = r.removeFromRight(16);
    fader.setBounds(r);
}

void ChannelTab::paint(juce::Graphics& g)
{
    g.fillAll(panel);
    text(g, i18n::tr("Canal"), { 0, 4, getWidth(), 22 }, 11.0f, ink, juce::Justification::centred);
    g.setColour(line); g.drawHorizontalLine(28, 0.0f, (float) getWidth()); g.drawVerticalLine(getWidth() - 1, 0.0f, (float) getHeight());
    if (! track)
    {
        font(g, 10.0f); g.setColour(muted);
        g.drawFittedText(i18n::tr("Selecciona una pista con canal"), getLocalBounds().withTrimmedTop(40).reduced(8, 0).withHeight(40), juce::Justification::centredTop, 3);
        return;
    }
    g.setColour(track->colour); g.fillRoundedRectangle(nameArea.toFloat(), 3.0f);
    text(g, track->name, nameArea.reduced(6, 0), 11.0f, eventInk);
    // EQ curve: the three-band EQ drawn over a log frequency axis.
    g.setColour(juce::Colour(0xff0b0c0d)); g.fillRect(eqArea);
    {
        juce::Path curve;
        const float lo = track->lowGainDb.load(), mi = track->midGainDb.load(), hi = track->highGainDb.load();
        for (int x = 0; x <= eqArea.getWidth(); ++x)
        {
            const double f = 20.0 * std::pow(1000.0, (double) x / eqArea.getWidth()); // 20 Hz – 20 kHz
            const double wl = 1.0 / (1.0 + std::pow(f / 250.0, 2.0)), wh = 1.0 / (1.0 + std::pow(4000.0 / f, 2.0));
            const double db = lo * wl + hi * wh + mi * juce::jmax(0.0, 1.0 - wl - wh);
            const float y = (float) eqArea.getCentreY() - (float) (db / 24.0) * eqArea.getHeight() * 0.9f;
            if (x == 0) curve.startNewSubPath((float) eqArea.getX(), y); else curve.lineTo((float) (eqArea.getX() + x), y);
        }
        g.setColour(line.brighter(.3f)); g.drawHorizontalLine(eqArea.getCentreY(), (float) eqArea.getX(), (float) eqArea.getRight());
        g.setColour(accent); g.strokePath(curve, juce::PathStrokeType(1.4f));
    }
    for (auto [area, title] : { std::pair<juce::Rectangle<int>, juce::String> { insertsHeader, i18n::tr("Inserciones") }, { sendsHeader, i18n::tr("Envíos") + " " + juce::String(firstSend + 1) + "–" + juce::String(firstSend + 4) } })
    {
        g.setColour(raised); g.fillRoundedRectangle(area.toFloat(), 2.5f);
        juce::Path arrow; arrow.addTriangle((float) area.getX() + 6, (float) area.getY() + 7, (float) area.getX() + 13, (float) area.getY() + 7, (float) area.getX() + 9.5f, (float) area.getY() + 12);
        g.setColour(ink); g.fillPath(arrow); text(g, title, area.withTrimmedLeft(18), 10.0f, ink);
    }
    for (int rowIndex = 0; rowIndex < 4; ++rowIndex)
    {
        const int k = firstSend + rowIndex;
        const auto row = sendsArea.withY(sendsArea.getY() + rowIndex * 17).withHeight(15);
        const auto& send = track->sends[(size_t) k];
        const auto fx = send.target.isEmpty() ? nullptr : engine.findTrack(send.target);
        g.setColour(fx ? juce::Colour(0xffc4622d) : bg.brighter(.06f)); g.fillRoundedRectangle(row.toFloat(), 2.0f);
        text(g, juce::String(k + 1) + (fx ? " " + fx->name + "  " + juce::String(send.levelDb.load(), 1) : juce::String()), row.reduced(5, 0), 9.5f, fx ? ink : muted);
    }
    font(g, 8.0f); g.setColour(muted);
    for (float db : { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
    {
        const float p = (float) fader.valueToProportionOfLength(db);
        const float ty = (float) fader.getY() + 8.0f + (1.0f - p) * ((float) fader.getHeight() - 16.0f);
        g.drawText(juce::String((int) db), juce::Rectangle<float>((float) scaleArea.getX(), ty - 5.0f, (float) scaleArea.getWidth(), 10.0f), juce::Justification::centred);
    }
    for (int c = 0; c < 2; ++c)
    {
        auto bar = meterArea.toFloat().withWidth(9.0f).translated(c * 11.0f, 0.0f);
        g.setColour(juce::Colour(0xff0b0c0d)); g.fillRoundedRectangle(bar, 2.0f);
        const float level = c == 0 ? track->peakL.load() : track->peakR.load();
        const float p = juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(level, -100.0f) + 60.0f) / 66.0f);
        g.setColour(level > 1.0f ? coral : level > 0.5f ? meterWarning : meterColour);
        g.fillRoundedRectangle(bar.withTop(bar.getBottom() - bar.getHeight() * p), 2.0f);
    }
    g.setColour(track->colour); g.fillRoundedRectangle(bottomName.toFloat(), 3.0f);
    text(g, track->name, bottomName.reduced(6, 0), 10.5f, eventInk);
}

void ChannelTab::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (sendsArea.contains(e.getPosition()) && wheel.deltaY != 0.0f)
    {
        sendScroll.setCurrentRangeStart(juce::jlimit(0, Track::sendCount - 4, firstSend + (wheel.deltaY < 0 ? 1 : -1)), juce::sendNotificationSync);
        return;
    }
    juce::Component::mouseWheelMove(e, wheel);
}

void ChannelTab::scrollBarMoved(juce::ScrollBar*, double start)
{
    firstSend = juce::jlimit(0, Track::sendCount - 4, (int) start);
    repaint(sendsArea.getUnion(sendsHeader));
}
}

#include "MixConsole.h"
#include "../../i18n/Translations.h"
#include "../../plugins/PluginSelection.h"
#include "../plugins/PluginSlotButton.h"
#include "../theme/Drawing.h"

namespace silente::ui
{
using namespace theme;

namespace
{
constexpr float minimumDb = -60.0f, maximumDb = 6.0f;
constexpr int insertSlots = 4;
// Meter scale: linear in dB from -60 to +6.
float meterProportion(float db) { return juce::jlimit(0.0f, 1.0f, (db - minimumDb) / (maximumDb - minimumDb)); }
float toDb(float gain) { return juce::Decibels::gainToDecibels(gain, -100.0f); }
juce::String dbText(float db) { return db <= minimumDb ? juce::String::fromUTF8("−∞") : juce::String(db, 1); }

juce::String kindName(const Track& t)
{
    switch (t.kind)
    {
        case TrackKind::instrument: return i18n::tr("INSTRUMENTO");
        case TrackKind::midi: return i18n::tr("MIDI");
        case TrackKind::fx: return i18n::tr("FX");
        case TrackKind::group: return i18n::tr("GRUPO");
        case TrackKind::folder: return i18n::tr("CARPETA");
        case TrackKind::audio: break;
    }
    return i18n::tr("AUDIO");
}

// Peak hold and clip latch for one stereo meter, updated from the UI timer.
struct MeterState
{
    std::array<float, 2> holdDb { -100.0f, -100.0f }, maxDb { -100.0f, -100.0f };
    std::array<double, 2> holdTime {};
    bool clipped = false;
    void update(float left, float right)
    {
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const float levels[] { left, right };
        for (size_t c = 0; c < 2; ++c)
        {
            const float db = toDb(levels[c]);
            if (db >= holdDb[c]) { holdDb[c] = db; holdTime[c] = now; }
            else if (now - holdTime[c] > 1.5) holdDb[c] = juce::jmax(-100.0f, holdDb[c] - 0.8f);
            maxDb[c] = juce::jmax(maxDb[c], db);
            if (levels[c] > 1.0f) clipped = true;
        }
    }
    void reset() { holdDb = maxDb = { -100.0f, -100.0f }; clipped = false; }
};
}

//==============================================================================
class MixConsole::Strip final : public juce::Component
{
public:
    Strip(MixConsole& o, AudioEngine& e, PluginManager& p, MixConsoleHooks& h, int i)
        : owner(o), engine(e), plugins(p), hooks(h), index(i)
    {
        if (index >= 0) track = engine.tracks[(size_t) index];
        const auto id = juce::String(index);
        mute.setComponentID("mix-mute-" + id); fader.setComponentID("mix-fader-" + id); volume.setComponentID("mix-volume-" + id); pan.setComponentID("mix-pan-" + id);
        for (auto* c : std::initializer_list<juce::Component*> { &input, &output, &instrument, &low, &mid, &high, &pan, &mute, &solo, &arm, &monitor, &edit, &fader, &volume })
            addAndMakeVisible(c);
        for (auto& b : inserts) addAndMakeVisible(b);
        for (int k = 0; k < Track::sendCount; ++k)
        {
            auto& target = sendTargets[(size_t) k];
            auto& level = sendLevels[(size_t) k];
            addAndMakeVisible(target); addAndMakeVisible(level);
            target.setComponentID("mix-send-" + juce::String(index) + "-" + juce::String(k));
            target.onClick = [this, k] { sendMenu(k); };
            level.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            level.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            level.setRange(minimumDb, maximumDb, 0.1); level.setSkewFactorFromMidPoint(-12.0);
            level.setDoubleClickReturnValue(true, 0.0);
            level.setColour(juce::Slider::rotarySliderOutlineColourId, line);
            level.onDragStart = [this] { edited(); };
            level.onValueChange = [this, k] { if (! syncing && track) { track->sends[(size_t) k].levelDb = (float) sendLevels[(size_t) k].getValue(); changed(); } };
        }
        for (auto* b : { &mute, &solo, &arm, &monitor }) b->setClickingTogglesState(true);
        mute.setButtonText("M"); solo.setButtonText("S"); arm.setButtonText("R"); edit.setButtonText("e");
        monitor.setButtonText(juce::String::fromUTF8("◉"));
        mute.setColour(juce::TextButton::buttonOnColourId, muteColour);
        solo.setColour(juce::TextButton::buttonOnColourId, soloColour);
        arm.setColour(juce::TextButton::buttonOnColourId, coral.withAlpha(.75f));
        monitor.setColour(juce::TextButton::buttonOnColourId, meterWarning.withAlpha(.6f));
        mute.setTooltip(i18n::tr("Silenciar")); solo.setTooltip(i18n::tr("Solo")); arm.setTooltip(i18n::tr("Activar grabación"));
        monitor.setTooltip(i18n::tr("Monitorizar")); edit.setTooltip(i18n::tr("Editar configuración de canal"));

        fader.setSliderStyle(juce::Slider::LinearVertical);
        fader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        fader.setRange(minimumDb, maximumDb, 0.1);
        fader.setSkewFactorFromMidPoint(-14.0);
        fader.setDoubleClickReturnValue(true, 0.0);
                fader.setColour(juce::Slider::backgroundColourId, bg);
        pan.setSliderStyle(juce::Slider::LinearHorizontal);
        pan.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        pan.setRange(-100, 100, 1); pan.setDoubleClickReturnValue(true, 0.0);
        pan.setColour(juce::Slider::backgroundColourId, bg);
        pan.setTooltip(i18n::tr("Panorama · doble clic: centro"));
        for (auto* k : { &low, &mid, &high })
        {
            k->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            k->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            k->setRange(-18, 18, 0.1); k->setDoubleClickReturnValue(true, 0.0);
            k->setColour(juce::Slider::rotarySliderOutlineColourId, line);
            k->onDragStart = [this] { edited(); };
        }
        low.setTooltip(i18n::tr("Graves")); mid.setTooltip(i18n::tr("Medios")); high.setTooltip(i18n::tr("Agudos"));
        volume.setEditable(false, true, false);
        volume.setJustificationType(juce::Justification::centred);
        volume.setFont(juce::Font(juce::FontOptions(11.0f)));
        volume.setColour(juce::Label::backgroundColourId, bg);
        volume.setTooltip(i18n::tr("Volumen en dB · doble clic para escribir"));

        fader.onDragStart = pan.onDragStart = [this] { edited(); };
        fader.onValueChange = [this]
        {
            if (syncing) return;
            const float db = (float) fader.getValue(), gain = db <= minimumDb ? 0.0f : juce::Decibels::decibelsToGain(db);
            if (track) track->gain = gain; else engine.masterGain = gain;
            changed();
        };
        volume.onTextChange = [this]
        {
            edited();
            const auto text = volume.getText().replace(",", ".").retainCharacters("-0123456789.");
            fader.setValue(text.isEmpty() ? minimumDb : juce::jlimit((double) minimumDb, (double) maximumDb, text.getDoubleValue()), juce::sendNotificationSync);
        };
        pan.onValueChange = [this] { if (! syncing && track) { track->pan = (float) pan.getValue() / 100.0f; changed(); } };
        low.onValueChange = [this] { if (! syncing && track) { track->lowGainDb = (float) low.getValue(); changed(); } };
        mid.onValueChange = [this] { if (! syncing && track) { track->midGainDb = (float) mid.getValue(); changed(); } };
        high.onValueChange = [this] { if (! syncing && track) { track->highGainDb = (float) high.getValue(); changed(); } };
        mute.onClick = [this] { if (track) { edited(); track->mute = mute.getToggleState(); changed(); } };
        solo.onClick = [this] { if (track) { edited(); track->solo = solo.getToggleState(); changed(); } };
        arm.onClick = [this] { if (track) { edited(); track->armed = arm.getToggleState(); changed(); } };
        monitor.onClick = [this] { if (track) { edited(); track->monitor = monitor.getToggleState(); changed(); } };
        edit.onClick = [this] { if (index >= 0 && hooks.openChannelSettings) hooks.openChannelSettings(index); };
        input.onClick = [this] { inputMenu(); };
        output.onClick = [this] { outputMenu(); };
        output.setTooltip(i18n::tr("Salida del canal: Stereo Out o un canal de grupo"));
        instrument.onClick = [this] { plugins.showInstrument(index, &instrument); };
        instrument.onOptions = [this] { plugins.showInstrument(index, &instrument); };
        for (int slot = 0; slot < insertSlots; ++slot)
        {
            inserts[(size_t) slot].onClick = [this, slot] { plugins.showEffectEditor(index, slot, &inserts[(size_t) slot]); };
            inserts[(size_t) slot].onOptions = [this, slot] { plugins.showEffectSlot(index, slot, &inserts[(size_t) slot]); };
        }
        if (! track)
        {
            for (auto* c : std::initializer_list<juce::Component*> { &input, &instrument, &low, &mid, &high, &pan, &mute, &solo, &arm, &monitor, &edit })
                c->setVisible(false);
            for (auto& b : inserts) b.setVisible(false);
            for (auto& b : sendTargets) b.setVisible(false);
            for (auto& l : sendLevels) l.setVisible(false);
            output.setButtonText(i18n::tr("Dispositivo"));
            output.setEnabled(false);
        }
        else
        {
            const bool events = track->hasEvents();
            arm.setVisible(events); monitor.setVisible(events);
            instrument.setVisible(track->isMidi);
            input.setEnabled(track->kind == TrackKind::audio);
        }
        refresh();
    }

    void refresh()
    {
        if (track) meters.update(track->peakL.load(), track->peakR.load());
        else meters.update(engine.masterPeakL.load(), engine.masterPeakR.load());
        syncing = true;
        const float gain = track ? track->gain.load() : engine.masterGain.load();
        if (! fader.isMouseButtonDown()) fader.setValue(juce::jmax(minimumDb, toDb(gain)), juce::dontSendNotification);
        if (! volume.isBeingEdited()) volume.setText(dbText((float) fader.getValue()), juce::dontSendNotification);
        if (track)
        {
            if (! pan.isMouseButtonDown()) pan.setValue(track->pan.load() * 100.0, juce::dontSendNotification);
            if (! low.isMouseButtonDown()) low.setValue(track->lowGainDb.load(), juce::dontSendNotification);
            if (! mid.isMouseButtonDown()) mid.setValue(track->midGainDb.load(), juce::dontSendNotification);
            if (! high.isMouseButtonDown()) high.setValue(track->highGainDb.load(), juce::dontSendNotification);
            mute.setToggleState(track->mute.load(), juce::dontSendNotification);
            solo.setToggleState(track->solo.load(), juce::dontSendNotification);
            arm.setToggleState(track->armed.load(), juce::dontSendNotification);
            monitor.setToggleState(track->monitor.load(), juce::dontSendNotification);
            if (track->kind == TrackKind::audio)
                input.setButtonText(track->inputLeft == track->inputRight ? i18n::tr("Mono ") + juce::String(track->inputLeft + 1)
                                                                          : i18n::tr("Estéreo ") + juce::String(track->inputLeft + 1) + "-" + juce::String(track->inputRight + 1));
            else input.setButtonText(track->isMidi ? i18n::tr("MIDI: todas") : juce::String::fromUTF8("—"));
            input.setEnabled(track->kind == TrackKind::audio && ! engine.recording.load());
            const auto destination = track->output.isEmpty() ? nullptr : engine.findTrack(track->output);
            output.setButtonText(destination ? destination->name : juce::String("Stereo Out"));
            output.setColour(juce::TextButton::textColourOffId, destination ? destination->colour.brighter(.3f) : ink);
            for (int k = 0; k < Track::sendCount; ++k)
            {
                const auto& send = track->sends[(size_t) k];
                const auto fx = send.target.isEmpty() ? nullptr : engine.findTrack(send.target);
                auto& b = sendTargets[(size_t) k];
                b.setButtonText(fx ? (send.preFader.load() ? juce::String("PRE ") : juce::String()) + fx->name : juce::String());
                b.setAlpha(fx && ! send.enabled.load() ? .45f : 1.0f);
                b.setTooltip(fx ? i18n::tr("Envío: clic para destino, pre/post y activar") : i18n::tr("Añadir envío a un canal FX"));
                sendLevels[(size_t) k].setEnabled(fx != nullptr);
                if (! sendLevels[(size_t) k].isMouseButtonDown()) sendLevels[(size_t) k].setValue(send.levelDb.load(), juce::dontSendNotification);
                sendLevels[(size_t) k].setTooltip(fx ? i18n::tr("Nivel de envío: ") + dbText(send.levelDb.load()) + " dB" : juce::String());
            }
            const int instrumentSlot = instrumentSlotIndex(*track);
            instrument.setButtonText(instrumentSlot >= 0 ? track->plugins[(size_t) instrumentSlot]->description.name : i18n::tr("Sintetizador interno"));
            for (int i = 0; i < insertSlots; ++i)
            {
                auto& b = inserts[(size_t) i];
                const int slot = effectSlotIndex(*track, i);
                b.setButtonText(slot >= 0 ? track->plugins[(size_t) slot]->description.name : juce::String());
                b.setAlpha(slot >= 0 && track->plugins[(size_t) slot]->bypassed.load() ? .45f : 1.0f);
                b.setTooltip(slot >= 0 ? i18n::tr("Abrir editor del plugin") : i18n::tr("Añadir efecto de inserción"));
            }
        }
        syncing = false;
        repaint(meterArea.getUnion(peakArea).expanded(2));
    }

    void resetClip() { meters.reset(); repaint(); }

    void resized() override
    {
        auto r = getLocalBounds().reduced(4, 0);
        int y = topName;
        // A folded rack takes no space, so the fader section moves up.
        const auto rack = [&](Rack which, int height) { if (! owner.isExpanded(which)) return juce::Rectangle<int>(); auto area = r.withY(y).withHeight(height); y += height; return area.reduced(0, 4); };
        const auto routingArea = rack(MixConsole::routing, rackHeight(MixConsole::routing));
        const auto insertsArea = rack(MixConsole::inserts, rackHeight(MixConsole::inserts));
        const auto eqArea = rack(MixConsole::equaliser, rackHeight(MixConsole::equaliser));
        const auto sendsArea = rack(MixConsole::sends, rackHeight(MixConsole::sends));
        if (! sendsArea.isEmpty())
        {
            auto a = sendsArea.withTrimmedTop(2);
            for (int k = 0; k < Track::sendCount; ++k)
            {
                auto row = a.removeFromTop(22); a.removeFromTop(3);
                sendLevels[(size_t) k].setBounds(row.removeFromRight(22));
                row.removeFromRight(2);
                sendTargets[(size_t) k].setBounds(row);
            }
        }
        else { for (auto& b : sendTargets) b.setBounds({}); for (auto& l : sendLevels) l.setBounds({}); }
        const auto place = [](juce::Component& c, juce::Rectangle<int> area) { c.setBounds(area); };
        if (! routingArea.isEmpty()) { auto a = routingArea.withTrimmedTop(2); place(input, a.removeFromTop(22)); a.removeFromTop(4); place(output, a.removeFromTop(22)); }
        else { input.setBounds({}); output.setBounds({}); }
        if (! insertsArea.isEmpty())
        {
            auto a = insertsArea.withTrimmedTop(2);
            if (instrument.isVisible()) { place(instrument, a.removeFromTop(22)); a.removeFromTop(3); }
            for (auto& b : inserts) { place(b, a.removeFromTop(22)); a.removeFromTop(3); }
        }
        else { instrument.setBounds({}); for (auto& b : inserts) b.setBounds({}); }
        if (! eqArea.isEmpty())
        {
            auto a = eqArea.withTrimmedTop(2).removeFromTop(36);
            eqLabels = eqArea.withTrimmedTop(38).withHeight(12);
            const int w = a.getWidth() / 3;
            place(low, a.removeFromLeft(w)); place(mid, a.removeFromLeft(w)); place(high, a);
        }
        else { low.setBounds({}); mid.setBounds({}); high.setBounds({}); eqLabels = {}; }
        faderTop = y;
        auto f = r.withTop(y + 8);
        name = f.removeFromBottom(owner.isCompact() ? 34 : 46);
        pan.setBounds(f.removeFromTop(18).reduced(8, 0)); f.removeFromTop(6);
        auto row = f.removeFromTop(22);
        if (owner.isCompact())
        {   // Lower zone: every channel button in one row so the fader keeps its height.
            const int c = (row.getWidth() - 8) / 5;
            for (auto* button : { &mute, &solo, &edit, &arm, &monitor }) { button->setBounds(row.removeFromLeft(c)); row.removeFromLeft(2); }
        }
        else
        {
            const int b = (row.getWidth() - 6) / 3;
            mute.setBounds(row.removeFromLeft(b)); row.removeFromLeft(3); solo.setBounds(row.removeFromLeft(b)); row.removeFromLeft(3); edit.setBounds(row);
            f.removeFromTop(3);
            row = f.removeFromTop(22);
            arm.setBounds(row.removeFromLeft(b)); row.removeFromLeft(3); monitor.setBounds(row.removeFromLeft(b));
        }
        f.removeFromTop(6);
        auto readouts = f.removeFromBottom(22);
        volume.setBounds(readouts.removeFromLeft(readouts.getWidth() / 2).reduced(1, 0));
        peakArea = readouts.reduced(1, 0);
        f.removeFromBottom(4);
        clipArea = f.removeFromTop(8).withTrimmedLeft(f.getWidth() - 26);
        f.removeFromTop(2);
        auto meterColumn = f.removeFromRight(26);
        meterArea = meterColumn;
        f.removeFromRight(14); // scale labels
        scaleArea = juce::Rectangle<int>(f.getRight(), f.getY(), 14, f.getHeight());
        fader.setBounds(f.withTrimmedLeft(6));
    }

    void paint(juce::Graphics& g) override
    {
        const auto colour = track ? track->colour : accent;
        const bool selected = index >= 0 && hooks.selected && hooks.selected() == index;
        g.fillAll(track ? panel : masterPanel);
        // Name bar on top, in the channel colour (Cubase shows the name above the racks too).
        g.setColour(track ? colour : masterPanel.brighter(.25f)); g.fillRect(2, 2, getWidth() - 4, topName - 4);
        text(g, track ? track->name : i18n::tr("Stereo Out"), { 6, 2, getWidth() - 12, topName - 4 }, 10.0f, track ? eventInk : ink);
        // Rack backgrounds.
        int y = topName;
        for (int r = 0; r < rackCount; ++r)
        {
            const int h = rackHeight((Rack) r);
            if (owner.isExpanded((Rack) r)) { g.setColour(bg.withAlpha(.45f)); g.fillRect(2, y + 2, getWidth() - 4, h - 4); y += h; }
        }
        g.setColour(line.withAlpha(.7f)); g.drawHorizontalLine(faderTop + 2, 0.0f, (float) getWidth());
        if (track && owner.isExpanded(MixConsole::equaliser) && ! eqLabels.isEmpty())
        {
            const int w = eqLabels.getWidth() / 3;
            const juce::String bands[] { i18n::tr("GRAV"), i18n::tr("MED"), i18n::tr("AGU") };
            for (int b = 0; b < 3; ++b) text(g, bands[b], eqLabels.withX(eqLabels.getX() + b * w).withWidth(w), 8.0f, muted, juce::Justification::centred);
        }
        if (track)
        {   // Pan: centre mark and L/R ends.
            const auto p = pan.getBounds();
            g.setColour(line); g.drawVerticalLine(p.getCentreX(), (float) p.getY() + 2.0f, (float) p.getBottom() - 2.0f);
            text(g, "L", p.withWidth(10).translated(-1, 0), 8.0f, muted, juce::Justification::centred);
            text(g, "R", p.withTrimmedLeft(p.getWidth() - 10).translated(1, 0), 8.0f, muted, juce::Justification::centred);
        }
        // Fader scale and meter.
        font(g, 8.0f); g.setColour(muted);
        float lastLabel = -100.0f;
        for (float db : { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -36.0f, -48.0f })
        {
            const float p = (float) fader.valueToProportionOfLength(db);
            const auto ty = (float) fader.getY() + 10.0f + (1.0f - p) * ((float) fader.getHeight() - 20.0f);
            if (ty - lastLabel < 11.0f) continue; // short faders: keep the labels readable
            lastLabel = ty;
            g.drawText(db > 0 ? "+" + juce::String((int) db) : juce::String((int) db), juce::Rectangle<float>((float) scaleArea.getX(), ty - 5.0f, 14.0f, 10.0f), juce::Justification::centred);
        }
        for (int c = 0; c < 2; ++c)
        {
            auto bar = meterArea.toFloat().withTrimmedTop(0).withWidth(10.0f).translated(c * 13.0f, 0.0f);
            g.setColour(bg); g.fillRoundedRectangle(bar, 2.0f);
            const float level = track ? (c == 0 ? track->peakL.load() : track->peakR.load()) : (c == 0 ? engine.masterPeakL.load() : engine.masterPeakR.load());
            const float p = meterProportion(toDb(level));
            if (p > 0.0f)
            {
                const auto filled = bar.withTop(bar.getBottom() - bar.getHeight() * p);
                juce::ColourGradient gradient(mint, bar.getX(), bar.getBottom(), coral, bar.getX(), bar.getY(), false);
                gradient.addColour(meterProportion(-6.0f), meterWarning.interpolatedWith(mint, .35f));
                gradient.addColour(meterProportion(0.0f), meterWarning);
                g.setGradientFill(gradient); g.fillRoundedRectangle(filled, 2.0f);
            }
            const float hold = meterProportion(meters.holdDb[(size_t) c]);
            if (hold > 0.0f) { g.setColour(meters.holdDb[(size_t) c] > 0.0f ? coral : ink.withAlpha(.85f)); g.fillRect(bar.getX(), bar.getBottom() - bar.getHeight() * hold - 1.0f, bar.getWidth(), 2.0f); }
            g.setColour(line.withAlpha(.5f)); g.drawHorizontalLine((int) (bar.getBottom() - bar.getHeight() * meterProportion(0.0f)), bar.getX(), bar.getRight());
        }
        g.setColour(meters.clipped ? coral : bg.brighter(.1f)); g.fillRoundedRectangle(clipArea.toFloat(), 2.0f);
        // Peak readout (click resets the clip indicator).
        const float peakDb = juce::jmax(meters.maxDb[0], meters.maxDb[1]);
        g.setColour(bg); g.fillRect(peakArea);
        text(g, peakDb <= -99.0f ? juce::String::fromUTF8("−∞") : juce::String(peakDb, 1), peakArea, 11, peakDb > 0.0f ? coral : ink, juce::Justification::centred);
        // Name block.
        g.setColour(colour.withAlpha(selected ? .55f : .3f)); g.fillRect(name);
        g.setColour(colour); g.fillRect(name.getX(), name.getY(), name.getWidth(), 3);
        text(g, track ? track->name : i18n::tr("Stereo Out"), name.withTrimmedTop(5).withHeight(20).reduced(4, 0), 11.5f, ink, juce::Justification::centred);
        if (name.getHeight() > 40) text(g, track ? juce::String(index + 1).paddedLeft('0', 2) + "  " + kindName(*track) : i18n::tr("SALIDA"), name.withTrimmedTop(25).withHeight(16), 8.5f, muted, juce::Justification::centred);
        g.setColour(selected ? accent : line); g.drawRect(getLocalBounds(), selected ? 2 : 1);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (peakArea.contains(e.getPosition()) || clipArea.expanded(2).contains(e.getPosition())) { resetClip(); return; }
        if (index >= 0 && hooks.select) { hooks.select(index); owner.repaint(); }
    }

    const std::shared_ptr<Track>& getTrack() const { return track; }

private:
    MixConsole& owner;
    AudioEngine& engine;
    PluginManager& plugins;
    MixConsoleHooks& hooks;
    int index;
    std::shared_ptr<Track> track;
    juce::TextButton input, output, mute, solo, arm, monitor, edit;
    ui::PluginSlotButton instrument;
    std::array<ui::PluginSlotButton, insertSlots> inserts;
    std::array<juce::TextButton, Track::sendCount> sendTargets;
    std::array<juce::Slider, Track::sendCount> sendLevels;
    juce::Slider low, mid, high, pan, fader;
    juce::Label volume;
    MeterState meters;
    juce::Rectangle<int> meterArea, scaleArea, peakArea, clipArea, name, eqLabels;
    int faderTop = 0;
    static constexpr int topName = 20;
    bool syncing = false;

    void edited() { if (hooks.beforeEdit) hooks.beforeEdit(); }
    void changed() { if (hooks.changed) hooks.changed(); }

    void outputMenu()
    {
        if (! track) return;
        juce::PopupMenu menu;
        menu.addItem(1, "Stereo Out", true, track->output.isEmpty());
        std::vector<juce::String> ids;
        for (const auto& t : engine.tracks)
            if (t->kind == TrackKind::group && t != track)
            {
                ids.push_back(t->id);
                menu.addItem((int) ids.size() + 1, t->name, engine.canRoute(*track, *t, false), track->output == t->id);
            }
        if (ids.empty()) menu.addItem(-1, i18n::tr("(añade una pista de grupo para enrutar aquí)"), false);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&output), [safe = juce::Component::SafePointer<Strip>(this), ids](int result)
        {
            if (! safe || result <= 0 || ! safe->track) return;
            safe->edited();
            safe->engine.setOutput(*safe->track, result == 1 ? juce::String() : ids[(size_t) result - 2]);
            safe->changed(); safe->refresh();
        });
    }

    void sendMenu(int slot)
    {
        if (! track) return;
        auto& send = track->sends[(size_t) slot];
        juce::PopupMenu menu;
        std::vector<juce::String> ids;
        for (const auto& t : engine.tracks)
            if (t->kind == TrackKind::fx && t != track)
            {
                ids.push_back(t->id);
                menu.addItem(100 + (int) ids.size() - 1, t->name, engine.canRoute(*track, *t, true), send.target == t->id);
            }
        if (ids.empty()) menu.addItem(-1, i18n::tr("(añade una pista de FX para enviar aquí)"), false);
        menu.addSeparator();
        menu.addItem(1, i18n::tr("Pre-fader"), send.target.isNotEmpty(), send.preFader.load());
        menu.addItem(2, i18n::tr("Envío activado"), send.target.isNotEmpty(), send.enabled.load());
        menu.addItem(3, i18n::tr("Quitar envío"), send.target.isNotEmpty());
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&sendTargets[(size_t) slot]), [safe = juce::Component::SafePointer<Strip>(this), ids, slot](int result)
        {
            if (! safe || result <= 0 || ! safe->track) return;
            auto& s = safe->track->sends[(size_t) slot];
            safe->edited();
            if (result == 1) s.preFader = ! s.preFader.load();
            else if (result == 2) s.enabled = ! s.enabled.load();
            else if (result == 3) safe->engine.setSend(*safe->track, slot, {});
            else if (result >= 100) safe->engine.setSend(*safe->track, slot, ids[(size_t) result - 100]);
            safe->changed(); safe->refresh();
        });
    }

    void inputMenu()
    {
        if (! track || track->kind != TrackKind::audio) return;
        const auto names = engine.devices.inputNames();
        juce::PopupMenu menu;
        for (int n = 0; n < DeviceSettings::channelLimit; ++n)
            menu.addItem(n + 1, i18n::tr("Mono ") + juce::String(n + 1) + (n < names.size() ? " · " + names[n] : juce::String()), true, track->inputLeft == n && track->inputRight == n);
        menu.addSeparator();
        for (int n = 0; n < DeviceSettings::channelLimit; n += 2)
            menu.addItem(101 + n, i18n::tr("Estéreo ") + juce::String(n + 1) + "-" + juce::String(n + 2), true, track->inputLeft == n && track->inputRight == n + 1);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&input), [safe = juce::Component::SafePointer<Strip>(this)](int result)
        {
            if (! safe || result == 0 || ! safe->track) return;
            auto& self = *safe;
            const juce::ScopedLock lock(self.engine.modelLock);
            if (self.engine.recording.load()) return;
            self.edited();
            self.track->inputLeft = result >= 101 ? result - 101 : result - 1;
            self.track->inputRight = result >= 101 ? self.track->inputLeft + 1 : self.track->inputLeft;
            self.changed(); self.refresh();
        });
    }
};

//==============================================================================
// Rack titles aligned with the strips, like the channel-rack headers of the console.
class MixConsole::Legend final : public juce::Component
{
public:
    explicit Legend(MixConsole& o) : owner(o) {}
    void paint(juce::Graphics& g) override
    {
        g.fillAll(panel);
        const juce::String names[] { i18n::tr("ENRUTADO"), i18n::tr("INSERCIONES"), i18n::tr("EQ"), i18n::tr("ENVÍOS") };
        int y = 20; // below the strips' name bar
        for (int r = 0; r < rackCount; ++r)
        {
            const int h = rackHeight((Rack) r);
            if (owner.isExpanded((Rack) r)) { text(g, names[r], { 8, y + 6, getWidth() - 12, 14 }, 9.0f, muted); g.setColour(line.withAlpha(.5f)); g.drawHorizontalLine(y + h - 1, 6.0f, getWidth() - 6.0f); y += h; }
        }
        text(g, i18n::tr("FADERS"), { 8, y + 10, getWidth() - 12, 14 }, 9.0f, muted);
        g.setColour(line); g.drawVerticalLine(getWidth() - 1, 0.0f, (float) getHeight());
    }
private:
    MixConsole& owner;
};

//==============================================================================
// Left zone «Visibilidad»: show/hide channels and anchor them to the left or right zone.
class MixConsole::LeftZone final : public juce::Component
{
public:
    explicit LeftZone(MixConsole& o) : owner(o) {}
    static constexpr int rowHeight = 24, top = 54;
    void paint(juce::Graphics& g) override
    {
        g.fillAll(panel);
        text(g, i18n::tr("VISIBILIDAD"), { 12, 8, getWidth() - 24, 18 }, 10.0f, ink);
        text(g, i18n::tr("Canal · zona izquierda / derecha"), { 12, 28, getWidth() - 24, 16 }, 9.0f, muted);
        int y = top;
        for (const auto& t : owner.engine.tracks)
        {
            if (t->kind == TrackKind::folder) continue;
            const bool visible = owner.hidden.count(t->id) == 0;
            g.setColour(raised); g.fillRoundedRectangle(10.0f, y + 5.0f, 14.0f, 14.0f, 3.0f);
            if (visible) { g.setColour(accent); g.fillRoundedRectangle(13.0f, y + 8.0f, 8.0f, 8.0f, 2.0f); }
            g.setColour(t->colour); g.fillRect(30, y + 4, 4, 16);
            text(g, t->name, { 40, y, getWidth() - 100, rowHeight }, 11.0f, visible ? ink : muted);
            pin(g, { getWidth() - 54, y + 4, 20, 16 }, juce::String::fromUTF8("◀"), owner.pinnedLeft.count(t->id) > 0);
            pin(g, { getWidth() - 30, y + 4, 20, 16 }, juce::String::fromUTF8("▶"), owner.pinnedRight.count(t->id) > 0);
            y += rowHeight;
        }
        g.setColour(line); g.drawVerticalLine(getWidth() - 1, 0.0f, (float) getHeight());
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const int row = (e.y - top) / rowHeight;
        if (e.y < top || row < 0) return;
        int n = 0;
        for (const auto& t : owner.engine.tracks)
        {
            if (t->kind == TrackKind::folder) continue;
            if (n++ != row) continue;
            const auto& id = t->id;
            if (e.x >= getWidth() - 56 && e.x < getWidth() - 32) { toggle(owner.pinnedLeft, id); owner.pinnedRight.erase(id); }
            else if (e.x >= getWidth() - 32) { toggle(owner.pinnedRight, id); owner.pinnedLeft.erase(id); }
            else toggle(owner.hidden, id);
            owner.rebuild(); repaint(); return;
        }
    }
private:
    MixConsole& owner;
    static void toggle(std::set<juce::String>& set, const juce::String& id) { if (! set.erase(id)) set.insert(id); }
    static void pin(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& s, bool on)
    {
        g.setColour(on ? accent.withAlpha(.35f) : bg); g.fillRoundedRectangle(r.toFloat(), 3.0f);
        text(g, s, r, 9.0f, on ? ink : muted, juce::Justification::centred);
    }
};

//==============================================================================
int MixConsole::rackHeight(Rack r)
{
    switch (r)
    {
        case routing: return 62;
        case inserts: return 8 + 25 * (insertSlots + 1) + 4;
        case equaliser: return 54;
        case sends: return 8 + 25 * Track::sendCount + 4;
        case rackCount: break;
    }
    return 0;
}

int MixConsole::racksHeight() const
{
    int h = 0;
    for (int r = 0; r < rackCount; ++r) if (isExpanded((Rack) r)) h += rackHeight((Rack) r);
    return h;
}

MixConsole::MixConsole(AudioEngine& e, PluginManager& p, MixConsoleHooks h, bool isCompact) : engine(e), plugins(p), hooks(std::move(h)), compact(isCompact)
{
    if (compact) { showLeftZone = false; expanded.fill(false); toolbar = 32; }
    setLookAndFeel(&look);
    setWantsKeyboardFocus(true);
    leftZone = std::make_unique<LeftZone>(*this);
    legend = std::make_unique<Legend>(*this);
    for (auto* c : std::initializer_list<juce::Component*> { &leftZoneButton, &typesButton, &clipsButton, &search, leftZone.get(), legend.get(), &scroller, &leftArea, &rightArea })
        addAndMakeVisible(c);
    scroller.setViewedComponent(&scrolled, false);
    scroller.setScrollBarsShown(false, true);
    leftZoneButton.setButtonText(i18n::tr("Visibilidad"));
    leftZoneButton.setClickingTogglesState(true);
    leftZoneButton.setToggleState(true, juce::dontSendNotification);
    leftZoneButton.setTooltip(i18n::tr("Mostrar/ocultar la zona izquierda"));
    leftZoneButton.onClick = [this] { showLeftZone = leftZoneButton.getToggleState(); resized(); };
    typesButton.setButtonText(i18n::tr("Tipos de canal"));
    typesButton.setTooltip(i18n::tr("Filtrar tipos de canal"));
    typesButton.onClick = [this]
    {
        juce::PopupMenu menu;
        const std::pair<TrackKind, juce::String> kinds[] { { TrackKind::audio, i18n::tr("Audio") }, { TrackKind::instrument, i18n::tr("Instrumento") }, { TrackKind::midi, i18n::tr("MIDI") }, { TrackKind::fx, i18n::tr("FX") }, { TrackKind::group, i18n::tr("Grupo") } };
        for (const auto& [kind, label] : kinds) menu.addItem((int) kind + 1, label, true, hiddenKinds.count((int) kind) == 0);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&typesButton), [safe = juce::Component::SafePointer<MixConsole>(this)](int result)
        {
            if (! safe || result == 0) return;
            if (! safe->hiddenKinds.erase(result - 1)) safe->hiddenKinds.insert(result - 1);
            safe->rebuild();
        });
    };
    const juce::String rackNames[] { i18n::tr("Enrutado"), i18n::tr("Inserciones"), i18n::tr("EQ"), i18n::tr("Envíos") };
    for (int r = 0; r < rackCount; ++r)
    {
        auto& b = rackButtons[(size_t) r];
        addAndMakeVisible(b);
        b.setButtonText(rackNames[r]);
        b.setClickingTogglesState(true);
        b.setToggleState(true, juce::dontSendNotification);
        b.setTooltip(i18n::tr("Mostrar/plegar la sección de canal"));
        b.onClick = [this, r] { expanded[(size_t) r] = rackButtons[(size_t) r].getToggleState(); resized(); repaint(); };
    }
    if (compact) { leftZoneButton.setVisible(false); legend->setVisible(false); for (int r = 0; r < rackCount; ++r) rackButtons[(size_t) r].setToggleState(false, juce::dontSendNotification); }
    clipsButton.setButtonText(i18n::tr("Restablecer clips"));
    clipsButton.onClick = [this] { resetClips(); };
    search.setTextToShowWhenEmpty(i18n::tr("Buscar canal"), muted);
    search.setComponentID("mix-search");
    search.onTextChange = [this] { rebuild(); };
    search.setColour(juce::TextEditor::backgroundColourId, bg);
    rebuild();
    startTimerHz(30);
}

MixConsole::~MixConsole() { stopTimer(); setLookAndFeel(nullptr); }

bool MixConsole::passesFilter(const Track& t) const
{
    if (t.kind == TrackKind::folder || hidden.count(t.id) || hiddenKinds.count((int) t.kind)) return false;
    const auto query = search.getText().trim();
    return query.isEmpty() || t.name.containsIgnoreCase(query);
}

juce::StringArray MixConsole::currentLayoutKey() const
{
    juce::StringArray key;
    for (size_t i = 0; i < engine.tracks.size(); ++i)
    {
        const auto& t = *engine.tracks[i];
        if (! passesFilter(t)) continue;
        key.add(t.id + (pinnedLeft.count(t.id) ? ":L" : pinnedRight.count(t.id) ? ":R" : ":C") + ":" + juce::String((int) i));
    }
    return key;
}

void MixConsole::rebuild()
{
    strips.clear();
    layoutKey = currentLayoutKey();
    for (size_t i = 0; i < engine.tracks.size(); ++i)
    {
        if (! passesFilter(*engine.tracks[i])) continue;
        strips.push_back(std::make_unique<Strip>(*this, engine, plugins, hooks, (int) i));
    }
    strips.push_back(std::make_unique<Strip>(*this, engine, plugins, hooks, -1)); // Stereo Out
    for (auto& s : strips)
    {
        const auto& t = s->getTrack();
        auto& parent = ! t ? rightArea : pinnedLeft.count(t->id) ? leftArea : pinnedRight.count(t->id) ? rightArea : scrolled;
        parent.addAndMakeVisible(*s);
    }
    if (leftZone) leftZone->repaint();
    resized();
}

void MixConsole::layoutStrips()
{
    const int height = juce::jmax(200, scroller.getHeight() - scroller.getScrollBarThickness());
    int left = 0, centre = 0, right = 0;
    for (auto& s : strips)
    {
        const auto& t = s->getTrack();
        if (! t) continue;
        if (pinnedLeft.count(t->id)) s->setBounds(left++ * stripWidth, 0, stripWidth, height);
        else if (pinnedRight.count(t->id)) s->setBounds(right++ * stripWidth, 0, stripWidth, height);
        else s->setBounds(centre++ * stripWidth, 0, stripWidth, height);
    }
    for (auto& s : strips) if (! s->getTrack()) s->setBounds(right * stripWidth + 6, 0, stripWidth, height);
    scrolled.setSize(juce::jmax(centre * stripWidth, scroller.getWidth()), height);
}

void MixConsole::resetClips() { for (auto& s : strips) s->resetClip(); }

void MixConsole::resized()
{
    auto r = getLocalBounds();
    auto bar = r.removeFromTop(toolbar).reduced(8, compact ? 4 : 7);
    if (! compact) { leftZoneButton.setBounds(bar.removeFromLeft(96)); bar.removeFromLeft(8); }
    search.setBounds(bar.removeFromLeft(170)); bar.removeFromLeft(8);
    typesButton.setBounds(bar.removeFromLeft(118)); bar.removeFromLeft(16);
    for (auto& b : rackButtons) { b.setBounds(bar.removeFromLeft(96)); bar.removeFromLeft(4); }
    clipsButton.setBounds(bar.removeFromRight(140));
    leftZone->setVisible(showLeftZone);
    if (showLeftZone) leftZone->setBounds(r.removeFromLeft(leftZoneWidth));
    legend->setBounds(compact ? juce::Rectangle<int>() : r.removeFromLeft(legendWidth));
    int pinnedL = 0, pinnedR = 1; // the output channel always sits on the right
    for (auto& s : strips) if (const auto& t = s->getTrack()) { pinnedL += pinnedLeft.count(t->id) ? 1 : 0; pinnedR += pinnedRight.count(t->id) ? 1 : 0; }
    leftArea.setBounds(r.removeFromLeft(pinnedL * stripWidth));
    rightArea.setBounds(r.removeFromRight(pinnedR * stripWidth + 6));
    scroller.setBounds(r);
    layoutStrips();
    for (auto& s : strips) s->resized();
}

void MixConsole::paint(juce::Graphics& g)
{
    g.fillAll(bg);
    g.setColour(panel); g.fillRect(0, 0, getWidth(), toolbar);
    g.setColour(line); g.drawHorizontalLine(toolbar - 1, 0.0f, (float) getWidth());
    if (strips.size() <= 1)
        text(g, i18n::tr("Sin canales visibles. Añade pistas o revisa Visibilidad y Tipos de canal."), scroller.getBounds().withHeight(60), 13.0f, muted, juce::Justification::centred);
}

bool MixConsole::keyPressed(const juce::KeyPress& key)
{
    return hooks.key && hooks.key(key);
}

void MixConsole::timerCallback()
{
    if (! isShowing()) return;
    if (currentLayoutKey() != layoutKey) rebuild();
    for (auto& s : strips) s->refresh();
}

//==============================================================================
MixConsoleWindow::MixConsoleWindow(AudioEngine& engine, PluginManager& plugins, MixConsoleHooks hooks)
    : juce::DocumentWindow(i18n::tr("Silente · MixConsole"), panel, juce::DocumentWindow::allButtons)
{
    setUsingNativeTitleBar(true);
    setContentOwned(new MixConsole(engine, plugins, std::move(hooks)), false);
    setResizable(true, true);
    setResizeLimits(640, 520, 4000, 2400);
    centreWithSize(1280, 820);
    setVisible(true);
}
}

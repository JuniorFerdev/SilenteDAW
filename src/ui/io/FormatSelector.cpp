#include "FormatSelector.h"
#include "../../i18n/Translations.h"
#include "../theme/Colours.h"

namespace silente::ui
{
namespace
{
juce::String rateText(double r)
{
    const bool whole = std::abs(r - std::round(r / 1000.0) * 1000.0) < 0.5;
    return (whole ? juce::String(juce::roundToInt(r / 1000.0)) : juce::String(r / 1000.0, 3).trimCharactersAtEnd("0")) + " kHz";
}
}

FormatSelector::FormatSelector()
{
    for (auto* c : std::initializer_list<juce::Component*> { &format, &rate, &bits, &bitrate, &formatLabel, &rateLabel, &bitsLabel, &bitrateLabel }) addAndMakeVisible(c);
    format.setComponentID("format-type"); rate.setComponentID("format-rate"); bits.setComponentID("format-bits"); bitrate.setComponentID("format-bitrate");
    formatLabel.setText(i18n::tr("Formato de archivo"), juce::dontSendNotification);
    rateLabel.setText(i18n::tr("Frecuencia de muestreo"), juce::dontSendNotification);
    bitsLabel.setText(i18n::tr("Profundidad de bits"), juce::dontSendNotification);
    bitrateLabel.setText(i18n::tr("Tasa de bits"), juce::dontSendNotification);
    for (auto* l : { &formatLabel, &rateLabel, &bitsLabel, &bitrateLabel }) l->setColour(juce::Label::textColourId, theme::muted);
    for (int f = 0; f < io::fileFormatCount; ++f) format.addItem(io::displayName((io::FileFormat) f), f + 1);
    format.setSelectedId(1, juce::dontSendNotification);
    update(true);
    format.onChange = [this] { update(true); if (onChange) onChange(); };
    rate.onChange = [this] { update(false); if (onChange) onChange(); };
    bits.onChange = bitrate.onChange = [this] { if (onChange) onChange(); };
}

// Rebuilds the lists from what the chosen format accepts, keeping the previous choice when it is
// still valid (otherwise the nearest sensible default). Only valid options are ever offered.
void FormatSelector::update(bool rebuildRates)
{
    const auto f = (io::FileFormat) (format.getSelectedId() - 1);
    const bool lossy = f == io::FileFormat::mp3 || f == io::FileFormat::m4a;
    if (rebuildRates)
    {
        const int oldRate = rate.getSelectedId(), oldBits = bits.getSelectedId();
        rate.clear(juce::dontSendNotification);
        int rateId = 0;
        for (double r : io::supportedRates(f)) { rate.addItem(rateText(r), juce::roundToInt(r)); if (juce::roundToInt(r) == oldRate) rateId = oldRate; }
        rate.setSelectedId(rateId != 0 ? rateId : 48000, juce::dontSendNotification);
        bits.clear(juce::dontSendNotification);
        const auto depths = io::supportedBitDepths(f);
        for (int d : depths) bits.addItem(d == 32 ? i18n::tr("32 bit (coma flotante)") : juce::String(d) + " bit", d);
        if (! depths.empty()) bits.setSelectedId(std::find(depths.begin(), depths.end(), oldBits) != depths.end() ? oldBits : 24, juce::dontSendNotification);
    }
    const int wanted = bitrate.getSelectedId() > 0 ? bitrate.getSelectedId() : 256;
    bitrate.clear(juce::dontSendNotification);
    int pick = 0;
    for (int b : io::supportedBitRates(f, (double) rate.getSelectedId()))
    {
        bitrate.addItem(juce::String(b) + " kbit/s", b);
        if (pick == 0 || std::abs(b - wanted) < std::abs(pick - wanted)) pick = b;
    }
    if (pick != 0) bitrate.setSelectedId(pick, juce::dontSendNotification);
    bits.setVisible(! lossy); bitsLabel.setVisible(! lossy);
    bitrate.setVisible(lossy); bitrateLabel.setVisible(lossy);
    resized();
}

io::EncodeSettings FormatSelector::settings() const
{
    io::EncodeSettings s;
    s.format = (io::FileFormat) (format.getSelectedId() - 1);
    s.sampleRate = (double) rate.getSelectedId();
    s.bitDepth = bits.getSelectedId();
    s.kilobitsPerSecond = bitrate.getSelectedId();
    return s;
}

void FormatSelector::resized()
{
    auto r = getLocalBounds();
    const auto row = [&r](juce::Label& l, juce::Component& c) { auto line = r.removeFromTop(28); r.removeFromTop(6); l.setBounds(line.removeFromLeft(170)); c.setBounds(line); };
    row(formatLabel, format); row(rateLabel, rate);
    auto line = r.removeFromTop(28);
    if (bits.isVisible()) { bitsLabel.setBounds(line.removeFromLeft(170)); bits.setBounds(line); bitrate.setBounds({}); bitrateLabel.setBounds({}); }
    else { bitrateLabel.setBounds(line.removeFromLeft(170)); bitrate.setBounds(line); bits.setBounds({}); bitsLabel.setBounds({}); }
}
}

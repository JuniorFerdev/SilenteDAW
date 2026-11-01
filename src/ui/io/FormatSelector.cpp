#include "FormatSelector.h"
#include "../../i18n/Translations.h"
#include "../theme/Colours.h"

namespace silente::ui
{
namespace
{
constexpr double rates[] { 44100.0, 48000.0, 88200.0, 96000.0 };
constexpr int bitrates[] { 128, 192, 256, 320 };
}

FormatSelector::FormatSelector()
{
    for (auto* c : std::initializer_list<juce::Component*> { &format, &rate, &bits, &bitrate, &formatLabel, &rateLabel, &bitsLabel, &bitrateLabel }) addAndMakeVisible(c);
    formatLabel.setText(i18n::tr("Formato de archivo"), juce::dontSendNotification);
    rateLabel.setText(i18n::tr("Frecuencia de muestreo"), juce::dontSendNotification);
    bitsLabel.setText(i18n::tr("Profundidad de bits"), juce::dontSendNotification);
    bitrateLabel.setText(i18n::tr("Tasa de bits"), juce::dontSendNotification);
    for (auto* l : { &formatLabel, &rateLabel, &bitsLabel, &bitrateLabel }) l->setColour(juce::Label::textColourId, theme::muted);
    for (int f = 0; f < io::fileFormatCount; ++f) format.addItem(io::displayName((io::FileFormat) f), f + 1);
    for (int i = 0; i < 4; ++i) rate.addItem(juce::String(rates[i] / 1000.0, 1) + " kHz", i + 1);
    bits.addItem("16 bit", 16); bits.addItem("24 bit", 24); bits.addItem(i18n::tr("32 bit (coma flotante)"), 32);
    for (int i = 0; i < 4; ++i) bitrate.addItem(juce::String(bitrates[i]) + " kbit/s", bitrates[i]);
    format.setSelectedId(1, juce::dontSendNotification); rate.setSelectedId(2, juce::dontSendNotification);
    bits.setSelectedId(24, juce::dontSendNotification); bitrate.setSelectedId(320, juce::dontSendNotification);
    for (auto* c : { &format, &rate, &bits, &bitrate }) c->onChange = [this] { update(); if (onChange) onChange(); };
    update();
}

void FormatSelector::update()
{
    const auto f = (io::FileFormat) (format.getSelectedId() - 1);
    const bool lossy = f == io::FileFormat::mp3 || f == io::FileFormat::m4a;
    bits.setVisible(! lossy); bitsLabel.setVisible(! lossy);
    bitrate.setVisible(lossy); bitrateLabel.setVisible(lossy);
    rate.setItemEnabled(3, ! lossy); rate.setItemEnabled(4, ! lossy);
    if (lossy && rate.getSelectedId() > 2) rate.setSelectedId(2, juce::dontSendNotification);
    bits.setItemEnabled(32, f != io::FileFormat::flac);
    if (f == io::FileFormat::flac && bits.getSelectedId() == 32) bits.setSelectedId(24, juce::dontSendNotification);
}

io::EncodeSettings FormatSelector::settings() const
{
    io::EncodeSettings s;
    s.format = (io::FileFormat) (format.getSelectedId() - 1);
    s.sampleRate = rates[juce::jlimit(0, 3, rate.getSelectedId() - 1)];
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

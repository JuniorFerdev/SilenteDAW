#pragma once
#include "../../io/AudioFileFormats.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace silente::ui
{
// File format, sample rate, bit depth and bit rate, shared by «Exportar mezcla de audio» and
// «Conversión de formatos». Only valid combinations are offered (MP3/AAC up to 48 kHz).
class FormatSelector final : public juce::Component
{
public:
    std::function<void()> onChange;
    FormatSelector();
    io::EncodeSettings settings() const;
    void resized() override;

private:
    juce::ComboBox format, rate, bits, bitrate;
    juce::Label formatLabel, rateLabel, bitsLabel, bitrateLabel;
    void update();
};
}

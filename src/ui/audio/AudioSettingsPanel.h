#pragma once
#include "../../AudioEngine.h"
#include "../theme/SilenteLookAndFeel.h"

namespace silente::ui
{
class AudioSettingsPanel final : public juce::Component,private juce::Timer
{
public:
    explicit AudioSettingsPanel(AudioEngine&);
    ~AudioSettingsPanel() override {setLookAndFeel(nullptr);}
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    AudioEngine& engine;
    theme::SilenteLookAndFeel look;
    juce::AudioDeviceSelectorComponent selector;
    juce::Label status,hint;
    juce::TextButton save,rescan;
    void timerCallback() override;
};
}

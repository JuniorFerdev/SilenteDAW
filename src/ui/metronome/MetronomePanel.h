#pragma once
#include "../../AudioEngine.h"
#include "../theme/SilenteLookAndFeel.h"

namespace silente::ui
{
// Metronome setup: sound, subdivision, level, accent, recording-only click and count-in.
// Changes apply immediately and are saved to the user's metronome preferences.
class MetronomePanel final : public juce::Component
{
public:
    explicit MetronomePanel(AudioEngine&);
    ~MetronomePanel() override { setLookAndFeel(nullptr); }
    void resized() override;
    void paint(juce::Graphics&) override;
    static juce::File settingsFile();

private:
    AudioEngine& engine;
    theme::SilenteLookAndFeel look;
    juce::ComboBox sound, subdivision, countIn;
    juce::Slider level;
    juce::ToggleButton accent, onlyRecording, enabled;
    juce::TextButton preview;
    juce::Label soundLabel, subdivisionLabel, levelLabel, countInLabel;
    void store();
};
}

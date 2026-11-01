#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

namespace silente
{
class DeviceSettings final : private juce::ChangeListener
{
public:
    static constexpr int channelLimit=18;
    explicit DeviceSettings(juce::AudioDeviceManager&);
    ~DeviceSettings() override;
    juce::String initialise();
    juce::Result save();
    void rescan();
    juce::String summary() const;
    juce::StringArray inputNames() const;
    juce::var inventory();
private:
    juce::AudioDeviceManager& manager;
    // JUCE 8.0.12: Core Audio combines separate input/output devices, and that combination
    // never calls back while the input has no active channels (silent transport). With no
    // active inputs Silente opens the output device alone.
    void dropUnusedInputDevice();
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    juce::File preferences;
};
}

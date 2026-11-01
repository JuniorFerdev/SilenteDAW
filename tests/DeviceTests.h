#pragma once
#include <juce_core/juce_core.h>
#include <functional>
namespace silente
{
int runDeviceTests(const juce::File&);
// Hardware check: alternates 48/96 kHz on real Core Audio input+output devices.
// Uses the message loop; calls done(exitCode) when finished.
class AudioEngine;
// Hardware check: plays a WAV through the real default device (master muted) and
// reports transport position, track peak and dropped blocks.
void startPlaybackCheck(AudioEngine&, const juce::File& wav, const juce::File& output, std::function<void(int)> done);
// Diagnostic: plays a project on the real device for `seconds`, captures exactly what the engine
// hands to Core Audio (device output is silenced after capture) and writes it with an offline
// render of the same span, so both can be compared sample by sample.
void startCaptureCheck(AudioEngine&, const juce::File& project, const juce::File& outputDir, double seconds, std::function<void(int)> done);
void startRateSwitchCheck(const juce::File& output, const juce::String& input, const juce::String& outputDevice, std::function<void(int)> done);
}

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>

namespace silente::audio
{
enum class ClickSound { classic, woodblock, cowbell, digital, clave };
constexpr int clickSoundCount = 5;

// Metronome click generator. Sounds are synthesised once per sample rate in prepare()
// (message thread); render() only copies those one-shots, so the audio thread never
// allocates. Settings are atomics written by the UI and read by the audio thread.
class Metronome
{
public:
    std::atomic<int> sound { (int) ClickSound::woodblock };
    std::atomic<int> subdivision { 1 };      // clicks per beat: 1 quarter, 2 eighths, 3 triplets, 4 sixteenths
    std::atomic<int> countInBars { 0 };      // bars of click before recording starts
    std::atomic<float> levelDb { -6.0f };
    std::atomic<bool> accent { true }, onlyWhenRecording { false };

    void prepare(double sampleRate);
    // Adds clicks for the transport span [startSeconds, startSeconds + samples / rate) into
    // both channels of `out`, starting at `offset`. A click starts on every grid line.
    void render(juce::AudioBuffer<float>& out, int offset, int samples, double startSeconds, double bpm, int beatsPerBar);
    void reset() noexcept { voice = {}; }
    void requestPreview() noexcept { previewRequested = true; }
    // Plays a pending preview click while the transport is stopped.
    void renderPreview(juce::AudioBuffer<float>& out, int samples);

    juce::Result save(const juce::File&) const;
    void load(const juce::File&);

private:
    enum Variant { accentClick, beatClick, subClick };
    std::array<std::array<std::vector<float>, 3>, clickSoundCount> clicks;
    double rate = 48000.0;
    struct Voice { int variant = -1, position = 0; };
    Voice voice, previewVoice; // transport clicks and the settings preview play independently
    std::atomic<bool> previewRequested { false };
    void play(Voice&, juce::AudioBuffer<float>& out, int offset, int samples) noexcept;
    static std::vector<float> synthesise(ClickSound, int variant, double rate);
};
}

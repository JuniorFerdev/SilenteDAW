#include "i18n/Translations.h"
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "audio/DeviceSettings.h"
#include "audio/Metronome.h"
#include <functional>
#include <memory>
#include <vector>

namespace silente
{
struct AudioClip
{
    juce::String id { juce::Uuid().toString() }, name;
    juce::File file;
    std::shared_ptr<juce::AudioBuffer<float>> audio;
    double sourceRate = 48000.0, startSeconds = 0.0, offsetSeconds = 0.0, durationSeconds = 0.0;
    float gain = 1.0f;
    double fadeIn = 0.005, fadeOut = 0.005;
    juce::Colour colour {}; // transparent: follows the track colour
    // Herramienta enmudecer: the event is silent. Fundido cruzado: during its fade-in the event
    // below keeps sounding (fading out) instead of being covered.
    bool muted = false, crossfade = false;
    int fadeInShape = 0, fadeOutShape = 0; // audio::FadeShape (0 = linear)
};

struct MidiNote
{
    juce::String id { juce::Uuid().toString() };
    int pitch = 60, velocity = 100;
    double startBeats = 0.0, lengthBeats = 1.0;
};

struct PluginSlot
{
    std::unique_ptr<juce::AudioPluginInstance> processor;
    juce::PluginDescription description;
    std::atomic<bool> bypassed { false };
    // The slot survives a missing plugin, so its saved state is not lost.
    juce::MemoryBlock savedState;
    juce::String error;
};

// Tipos de pista del botón «+», como en Cubase. Carpeta, FX y grupo no llevan eventos;
// el ruteo de envíos/salidas hacia FX y grupo aún no existe (siguiente tramo).
enum class TrackKind { audio, instrument, midi, folder, fx, group };
juce::String trackKindId(TrackKind);
TrackKind trackKindFromId(const juce::String&, bool legacyIsMidi);

// Send to an FX channel. `target` is the FX track id (empty = unused). Strings are changed
// on the message thread under modelLock; level and switches are atomics.
struct TrackSend
{
    juce::String target;
    std::atomic<float> levelDb { 0.0f };
    std::atomic<bool> preFader { false }, enabled { true };
};

struct Track
{
    Track();
    ~Track();
    juce::String id { juce::Uuid().toString() }, name;
    juce::Colour colour { 0xff7aa6ff };
    TrackKind kind = TrackKind::audio;
    bool isMidi = false; // instrument or midi kind
    bool hasEvents() const noexcept { return kind==TrackKind::audio||isMidi; }
    int inputLeft=0,inputRight=1; // Physical device channels; equal indices select mono.
    int stripWidth=0; // MixConsole channel width in pixels (0 = default); message thread only
    std::atomic<float> gain { 0.8f }, pan { 0.0f }, peakL { 0.0f }, peakR { 0.0f };
    std::atomic<bool> mute { false }, solo { false }, armed { false }, monitor { false };
    std::atomic<float> lowGainDb { 0.0f }, midGainDb { 0.0f }, highGainDb { 0.0f };
    static constexpr int sendCount = 4;
    juce::String output; // group track id; empty = Stereo Out
    std::array<TrackSend, sendCount> sends;
    bool isBus() const noexcept { return kind == TrackKind::fx || kind == TrackKind::group; }
    std::vector<AudioClip> clips;
    std::vector<MidiNote> notes;
    std::vector<std::shared_ptr<PluginSlot>> plugins;
    struct Runtime;
    std::unique_ptr<Runtime> runtime;
};

// How a recording starts: input is written from `captureFrom` (seconds, < 0 = at once), after
// the metronome count-in if `countIn`.
struct RecordOptions
{
    double captureFrom = -1.0;
    bool countIn = true;
};

class AudioEngine final : public juce::AudioIODeviceCallback, public juce::MidiInputCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;
    juce::AudioDeviceManager deviceManager;
    DeviceSettings devices { deviceManager };
    juce::AudioFormatManager formatManager;
    juce::AudioPluginFormatManager pluginFormats;
    juce::KnownPluginList knownPlugins;
    juce::MidiKeyboardState keyboardState;
    mutable juce::CriticalSection modelLock;
    // Main-thread edits to these containers require modelLock. Callback uses try-lock.
    std::vector<std::shared_ptr<Track>> tracks;
    std::atomic<bool> playing { false }, recording { false }, metronome { false }, looping { false };
    std::atomic<double> positionSeconds { 0.0 }, tempo { 120.0 }, loopStart { 0.0 }, loopEnd { 8.0 };
    std::atomic<float> masterGain { 0.8f }, masterPeakL { 0.0f }, masterPeakR { 0.0f };
    std::atomic<int> audioUnderruns { 0 }, clippedSamples { 0 }, deviceCallbacks { 0 };
    // Audio load (Cubase: «Carga de audio»): time spent rendering a device block / its duration.
    std::atomic<float> dspLoadAverage { 0.0f }, dspLoadPeak { 0.0f };
    audio::Metronome click;
    // Recording count-in still to play, in samples (0 = none). The transport waits meanwhile.
    std::atomic<juce::int64> countInSamples { 0 }, countInLength { 0 };
    // Playback stops by itself at this position (seconds); -1 = no stop point.
    std::atomic<double> stopAt { -1.0 };
    // Recording (manual pp. 347–359): punch points anchored to the locators, the audio record
    // mode (audio::AudioRecordMode) and the format of new recordings (16, 24 or 32-bit float).
    std::atomic<bool> punchIn { false }, punchOut { false };
    std::atomic<int> audioRecordMode { 1 }, recordBitDepth { 24 };
    // Audio thread → UI: input is being written now; the punch-out point has been passed.
    std::atomic<bool> capturing { false }, punchedOut { false };
    // Live recording for the project window (the red event that grows): where the current lap
    // started (seconds, -1 = nothing captured yet), its offset in the take file and the samples
    // captured so far. Written by the audio thread.
    std::atomic<double> recordLapStart { -1.0 };
    std::atomic<juce::int64> recordLapOffset { 0 }, recordCaptured { 0 };
    juce::String projectName { i18n::tr("Sin título") }, lastWarning;
    juce::File projectFile;

    juce::Result initialise();
    void shutdown();
    void play();
    void playRange(double from, double until); // play and stop at `until`
    void pause();
    void stop();
    void seek(double seconds);
    std::shared_ptr<Track> addAudioTrack(const juce::String& name = {});
    std::shared_ptr<Track> addMidiTrack(const juce::String& name = {});
    std::shared_ptr<Track> addTrack(TrackKind, const juce::String& name = {});
    void removeTrack(int index);
    // Routing (message thread). Output to a group track and sends to FX tracks; anything that
    // would form a loop, target the wrong kind or the track itself is refused.
    bool canRoute(const Track& source, const Track& target, bool asSend) const;
    juce::Result setOutput(Track&, const juce::String& groupId);
    juce::Result setSend(Track&, int slot, const juce::String& fxId);
    std::shared_ptr<Track> findTrack(const juce::String& id) const;
    juce::Result importAudio(const juce::File&, int trackIndex = -1, double startSeconds = 0.0);
    juce::Result importMidi(const juce::File&);
    juce::Result saveProject(const juce::File&);
    juce::Result loadProject(const juce::File&);
    // Offline render of the whole project (plugins in non-realtime mode, latency compensated).
    // `sink` receives each block; `progress` (0–1) may return false to cancel.
    juce::Result renderMix(double sampleRate, const std::function<bool(const juce::AudioBuffer<float>&, int start, int count)>& sink,
                           const std::function<bool(double)>& progress = {});
    juce::Result exportMix(const juce::File&, double sampleRate = 48000.0, int bitDepth = 24);
    bool exportMix(const juce::File& file, juce::String& error);
    void createDemo();
    void clear();
    double duration() const;
    juce::Result addPlugin(int trackIndex, std::unique_ptr<juce::AudioPluginInstance>, const juce::PluginDescription&, int replaceSlot = -1);
    void removePlugin(int trackIndex, int slotIndex);
    juce::Result beginRecording(const juce::File& directory, RecordOptions options = {});
    // «Regrabar»: drops the takes of the running recording (their files are deleted).
    void cancelRecording();
    // Waveform of the take being recorded on `track` (message thread): min/max pairs, one per
    // `samplesPerPeak` samples of the take file. False when the track is not recording audio.
    bool recordingOverview(const Track& track, const float*& minMax, int& count, int& samplesPerPeak) const;
    juce::Result endRecording();
    // Where new audio files of the project go (recordings, glued events): «Audio» in its folder.
    juce::File audioFolder() const;
    void setMidiInputEnabled(const juce::String& identifier, bool enabled);
    void panic();
    double currentSampleRate() const noexcept { return sampleRate.load(); }
    int currentBlockSize() const noexcept { return blockSize.load(); }
    int pluginLatencySamples() const;
    juce::String supportedAudioWildcard() const;

    // Deterministic rendering without opening a hardware device (tests/export).
    void prepareForOffline(double rate, int maxBlockSize, bool withInput = false);
    void renderOfflineBlock(juce::AudioBuffer<float>& output, int numSamples, const juce::AudioBuffer<float>& input);
    void renderOfflineBlock(juce::AudioBuffer<float>& output, int numSamples);
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int, int,
                                          const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;

private:
    struct EnginePlayHead;
    struct RecordingSession;
    std::unique_ptr<EnginePlayHead> playHead;
    std::unique_ptr<RecordingSession> recordingSession;
    juce::TimeSliceThread diskThread { juce::String::fromUTF8("Silente · disco de audio") };
    juce::MidiMessageCollector midiCollector;
    juce::MidiBuffer liveMidi;
    juce::AudioBuffer<float> mixBuffer;
    std::atomic<double> sampleRate { 48000.0 };
    std::atomic<int> blockSize { 512 };
    std::atomic<bool> panicRequested { false }, chaseRequested { false };
    bool initialised = false, exporting = false, offlineInput = false;
    std::array<int,DeviceSettings::channelLimit> inputMap;
    void prepare(double rate, int maxBlockSize);
    void prepareTrack(Track&);
    void render(float* const* output, int outputChannels, int samples,
                const float* const* input = nullptr, int inputChannels = 0);
    void renderSegment(int offset, int samples, double start, bool transportPlaying,
                       const float* const* input, int inputChannels);
    std::shared_ptr<Track> makeTrack(const juce::String&, bool midi);
    std::shared_ptr<Track> makeTrack(const juce::String&, TrackKind);
    juce::Result readAudio(const juce::File&, AudioClip&);
    juce::var projectToJson(const juce::File& destination);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
}

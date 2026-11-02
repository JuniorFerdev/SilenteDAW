#include "i18n/Translations.h"
#include "AudioEngine.h"
#include "audio/BandlimitedInterpolator.h"
#include "audio/RecordingModes.h"
#include <array>
#include <cmath>
#include <limits>
#include <algorithm>

namespace silente
{
namespace
{
constexpr int maxChannels = 16;
constexpr double twoPi = juce::MathConstants<double>::twoPi;
struct ToneSound final : juce::SynthesiserSound
{
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};
struct ToneVoice final : juce::SynthesiserVoice
{
    double phase = 0.0, delta = 0.0;
    float level = 0.0f;
    juce::ADSR envelope;
    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<ToneSound*>(s) != nullptr; }
    void startNote(int note, float velocity, juce::SynthesiserSound*, int pitchWheel) override
    {
        phase = 0.0;
        level = velocity * 0.18f;
        delta = twoPi * juce::MidiMessage::getMidiNoteInHertz(note) / getSampleRate();
        envelope.setSampleRate(getSampleRate());
        envelope.setParameters({ 0.008f, 0.18f, 0.64f, 0.18f });
        envelope.noteOn();
        juce::ignoreUnused(pitchWheel);
    }
    void stopNote(float, bool allowTail) override
    {
        if (allowTail) envelope.noteOff(); else { envelope.reset(); clearCurrentNote(); }
    }
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}
    void renderNextBlock(juce::AudioBuffer<float>& output, int start, int count) override
    {
        for (int i = 0; i < count; ++i)
        {
            const auto value = level * envelope.getNextSample() * static_cast<float>(std::sin(phase) + 0.15 * std::sin(phase * 2.0));
            phase += delta;
            if (phase >= twoPi) phase -= twoPi;
            for (int ch = 0; ch < juce::jmin(2, output.getNumChannels()); ++ch) output.addSample(ch, start + i, value);
            if (!envelope.isActive()) { clearCurrentNote(); break; }
        }
    }
};
float finiteGain(float v, float fallback = 0.0f) { return std::isfinite(v) ? v : fallback; }
double number(const juce::var& object, const char* key, double fallback)
{
    const auto v = object.getProperty(key, fallback);
    const double d = static_cast<double>(v);
    return std::isfinite(d) ? d : fallback;
}
juce::var object() { return juce::var(new juce::DynamicObject()); }
void set(juce::var& v, const char* key, juce::var value) { v.getDynamicObject()->setProperty(key, std::move(value)); }
}

struct Track::Runtime
{
    juce::Synthesiser synth;
    juce::AudioBuffer<float> buffer;
    juce::AudioBuffer<float> delay;
    int delayPosition = 0;
    juce::MidiBuffer midi;
    bool resetNotes = true, chase = false;
    float lowState[2] {}, highState[2] {};
    // Routing state, audio thread only: input bus of FX/group channels, resolved destinations
    // (cached indices re-validated by id every block) and render rank.
    juce::AudioBuffer<float> bus;
    int outputIndex = -1, rank = 0;
    std::array<int, Track::sendCount> sendIndex { -1, -1, -1, -1 };
    bool destinationSoloed = false;
    // Per block: index of the clip heard at each sample (later clips cover earlier ones).
    std::vector<int> owner;
    Runtime()
    {
        for (int i = 0; i < 16; ++i) synth.addVoice(new ToneVoice());
        synth.addSound(new ToneSound());
        midi.ensureSize(65536);
    }
};
Track::Track() : runtime(std::make_unique<Runtime>()) {}
Track::~Track() = default;

struct AudioEngine::EnginePlayHead final : juce::AudioPlayHead
{
    AudioEngine& engine;
    double blockPosition = 0.0;
    explicit EnginePlayHead(AudioEngine& e) : engine(e) {}
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        const double bpm = juce::jlimit(20.0, 400.0, engine.tempo.load());
        info.setTimeInSeconds(blockPosition);
        info.setTimeInSamples(static_cast<juce::int64>(blockPosition * engine.sampleRate.load()));
        info.setBpm(bpm);
        info.setTimeSignature(TimeSignature { 4, 4 });
        info.setPpqPosition(blockPosition * bpm / 60.0);
        info.setPpqPositionOfLastBarStart(std::floor(blockPosition * bpm / 240.0) * 4.0);
        info.setIsPlaying(engine.playing.load());
        info.setIsRecording(engine.recording.load());
        info.setIsLooping(engine.looping.load());
        info.setLoopPoints(LoopPoints { engine.loopStart.load() * bpm / 60.0, engine.loopEnd.load() * bpm / 60.0 });
        return info;
    }
};
struct AudioEngine::RecordingSession
{
    struct Entry
    {
        std::shared_ptr<Track> track;
        juce::File file;
        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
        juce::MidiBuffer midi;
    };
    // One lap per pass through the cycle (one in a linear recording): where it sits in the
    // project and which part of the recorded file it is.
    struct Lap { double start = 0.0; juce::int64 fileOffset = 0, length = 0; };
    std::vector<Entry> entries;
    std::array<Lap, 256> laps {};
    int lapCount = 0;
    double start = 0.0, rate = 48000.0, bpm = 120.0, captureFrom = -1.0, lastEnd = -1.0;
    juce::int64 samples = 0, captured = 0;
    bool failed = false;
};

AudioEngine::AudioEngine() : playHead(std::make_unique<EnginePlayHead>(*this))
{
    for(int i=0;i<DeviceSettings::channelLimit;++i)inputMap[(size_t)i]=i;
    formatManager.registerBasicFormats();
    juce::addDefaultFormatsToManager(pluginFormats);
    liveMidi.ensureSize(65536);
    prepare(48000.0, 512);
}
AudioEngine::~AudioEngine() { shutdown(); }
juce::Result AudioEngine::initialise()
{
    if (initialised) return juce::Result::ok();
    auto error = devices.initialise();
    if (error.isNotEmpty()) return juce::Result::fail(error);
    midiCollector.reset(sampleRate.load());
    deviceManager.addMidiInputDeviceCallback({}, this);
    for (const auto& midi : juce::MidiInput::getAvailableDevices()) deviceManager.setMidiInputDeviceEnabled(midi.identifier, true);
    diskThread.startThread();
    deviceManager.addAudioCallback(this);
    initialised = true;
    return juce::Result::ok();
}
void AudioEngine::shutdown()
{
    if (recording.load()) endRecording();
    if (initialised)
    {
        deviceManager.removeAudioCallback(this);
        deviceManager.removeMidiInputDeviceCallback({}, this);
        deviceManager.closeAudioDevice();
        initialised = false;
    }
    diskThread.stopThread(3000);
    const juce::ScopedLock lock(modelLock);
    for (const auto& track : tracks)
        for (const auto& slot : track->plugins)
            if (slot->processor) { slot->processor->setPlayHead(nullptr); slot->processor->releaseResources(); }
}
void AudioEngine::play() { const juce::ScopedLock lock(modelLock); chaseRequested = true; playing = true; }
void AudioEngine::playRange(double from, double until) { seek(from); stopAt = until; play(); }
void AudioEngine::pause()
{
    { const juce::ScopedLock lock(modelLock); playing = false; countInSamples = 0; stopAt = -1.0; panic(); }
    if (recording.load()) endRecording();
}
void AudioEngine::stop() { pause(); const juce::ScopedLock lock(modelLock); positionSeconds = 0.0; }
void AudioEngine::seek(double seconds)
{
    const juce::ScopedLock lock(modelLock);
    if (recording.load()) return;
    positionSeconds = std::isfinite(seconds) ? juce::jmax(0.0, seconds) : 0.0;
    panicRequested = true;
    chaseRequested = true;
}
void AudioEngine::panic() { panicRequested = true; keyboardState.allNotesOff(0); }
void AudioEngine::clear()
{
    stop();
    const juce::ScopedLock lock(modelLock);
    tracks.clear();
    projectName = i18n::tr("Sin título");
    projectFile = juce::File();
    lastWarning.clear();
    tempo = 120.0;
    masterGain = 0.8f;
    looping = false;
    punchIn = false; punchOut = false; audioRecordMode = 1; recordBitDepth = 24;
    metronome = false;
    loopStart = 0.0;
    loopEnd = 8.0;
}
juce::String trackKindId(TrackKind kind)
{
    switch(kind){case TrackKind::instrument:return "instrument";case TrackKind::midi:return "midi";case TrackKind::folder:return "folder";case TrackKind::fx:return "fx";case TrackKind::group:return "group";default:return "audio";}
}
TrackKind trackKindFromId(const juce::String& id, bool legacyIsMidi)
{
    for(auto kind:{TrackKind::audio,TrackKind::instrument,TrackKind::midi,TrackKind::folder,TrackKind::fx,TrackKind::group})if(id==trackKindId(kind))return kind;
    return legacyIsMidi?TrackKind::instrument:TrackKind::audio;
}
std::shared_ptr<Track> AudioEngine::makeTrack(const juce::String& name, bool midi)
{
    return makeTrack(name, midi ? TrackKind::instrument : TrackKind::audio);
}
std::shared_ptr<Track> AudioEngine::makeTrack(const juce::String& name, TrackKind kind)
{
    auto track = std::make_shared<Track>();
    track->kind = kind;
    track->isMidi = kind == TrackKind::instrument || kind == TrackKind::midi;
    juce::String base;
    switch(kind){case TrackKind::instrument:base=i18n::tr("Instrumento ");break;case TrackKind::midi:base=i18n::tr("MIDI ");break;case TrackKind::folder:base=i18n::tr("Carpeta ");break;case TrackKind::fx:base=i18n::tr("FX ");break;case TrackKind::group:base=i18n::tr("Grupo ");break;default:base=i18n::tr("Audio ");}
    track->name = name.isNotEmpty() ? name : base + juce::String(tracks.size() + 1);
    static constexpr juce::uint32 colours[] { 0xffa994ff, 0xff65d6c5, 0xffffba72, 0xff6eabef, 0xffe391bf, 0xffbdd27a };
    track->colour = kind == TrackKind::folder ? juce::Colour(0xff8a93a3) : juce::Colour(colours[tracks.size() % std::size(colours)]);
    prepareTrack(*track);
    return track;
}
std::shared_ptr<Track> AudioEngine::addAudioTrack(const juce::String& name)
{
    return addTrack(TrackKind::audio, name);
}
std::shared_ptr<Track> AudioEngine::addMidiTrack(const juce::String& name)
{
    return addTrack(TrackKind::instrument, name);
}
std::shared_ptr<Track> AudioEngine::addTrack(TrackKind kind, const juce::String& name)
{
    const juce::ScopedLock lock(modelLock);
    auto t = makeTrack(name, kind); if (t->isMidi) t->monitor = true; tracks.push_back(t); return t;
}
std::shared_ptr<Track> AudioEngine::findTrack(const juce::String& id) const
{
    for (const auto& t : tracks) if (t->id == id) return t;
    return {};
}
bool AudioEngine::canRoute(const Track& source, const Track& target, bool asSend) const
{
    if (&source == &target || source.kind == TrackKind::folder) return false;
    if (target.kind != (asSend ? TrackKind::fx : TrackKind::group)) return false;
    // Refuse loops: nothing reachable from the target may lead back to the source.
    std::vector<const Track*> pending { &target }, seen;
    while (! pending.empty())
    {
        const auto* t = pending.back(); pending.pop_back();
        if (t == &source) return false;
        if (std::find(seen.begin(), seen.end(), t) != seen.end()) continue;
        seen.push_back(t);
        if (auto next = findTrack(t->output)) pending.push_back(next.get());
        for (const auto& send : t->sends) if (auto next = findTrack(send.target)) pending.push_back(next.get());
    }
    return true;
}
juce::Result AudioEngine::setOutput(Track& track, const juce::String& groupId)
{
    const juce::ScopedLock lock(modelLock);
    if (groupId.isNotEmpty())
    {
        auto target = findTrack(groupId);
        if (! target || ! canRoute(track, *target, false)) return juce::Result::fail(i18n::tr("Esa salida crearía un bucle o no es un canal de grupo."));
    }
    track.output = groupId;
    return juce::Result::ok();
}
juce::Result AudioEngine::setSend(Track& track, int slot, const juce::String& fxId)
{
    if (! juce::isPositiveAndBelow(slot, Track::sendCount)) return juce::Result::fail("send slot");
    const juce::ScopedLock lock(modelLock);
    if (fxId.isNotEmpty())
    {
        auto target = findTrack(fxId);
        if (! target || ! canRoute(track, *target, true)) return juce::Result::fail(i18n::tr("Ese envío crearía un bucle o no es un canal de efectos."));
    }
    track.sends[static_cast<size_t>(slot)].target = fxId;
    track.sends[static_cast<size_t>(slot)].enabled = true;
    return juce::Result::ok();
}
void AudioEngine::removeTrack(int index)
{
    if (recording.load()) return;
    const juce::ScopedLock lock(modelLock);
    if (juce::isPositiveAndBelow(index, static_cast<int>(tracks.size()))) tracks.erase(tracks.begin() + index);
}
void AudioEngine::prepare(double rate, int maxBlockSize)
{
    const juce::ScopedLock lock(modelLock);
    sampleRate = juce::jlimit(8000.0, 384000.0, rate);
    blockSize = juce::jlimit(16, 32768, maxBlockSize);
    mixBuffer.setSize(2, blockSize.load());
    click.prepare(sampleRate.load());
    midiCollector.reset(sampleRate.load());
    for (auto& track : tracks) prepareTrack(*track);
}
void AudioEngine::prepareTrack(Track& track)
{
    auto& rt = *track.runtime;
    rt.buffer.setSize(maxChannels, blockSize.load());
    rt.delay.setSize(2, static_cast<int>(std::ceil(sampleRate.load() * 2.0)) + 1);
    rt.bus.setSize(2, blockSize.load());
    rt.owner.assign(static_cast<size_t>(blockSize.load()), -1);
    rt.bus.clear();
    rt.delay.clear();
    rt.delayPosition = 0;
    rt.synth.setCurrentPlaybackSampleRate(sampleRate.load());
    rt.synth.allNotesOff(0, false);
    rt.resetNotes = true;
    rt.lowState[0] = rt.lowState[1] = rt.highState[0] = rt.highState[1] = 0.0f;
    for (auto& slot : track.plugins)
        if (slot->processor)
        {
            slot->processor->releaseResources();
            slot->processor->setRateAndBufferSizeDetails(sampleRate.load(), blockSize.load());
            slot->processor->setPlayHead(playHead.get());
            slot->processor->prepareToPlay(sampleRate.load(), blockSize.load());
        }
}
void AudioEngine::prepareForOffline(double rate, int maxBlockSize, bool withInput) { offlineInput = withInput; prepare(rate, maxBlockSize); }
void AudioEngine::renderOfflineBlock(juce::AudioBuffer<float>& output, int samples, const juce::AudioBuffer<float>& input)
{
    const float* inputs[DeviceSettings::channelLimit] {};
    for (int ch = 0; ch < juce::jmin(input.getNumChannels(), (int) DeviceSettings::channelLimit); ++ch) inputs[ch] = input.getReadPointer(ch);
    samples = juce::jmin(samples, output.getNumSamples(), input.getNumSamples());
    render(output.getArrayOfWritePointers(), output.getNumChannels(), samples, inputs, DeviceSettings::channelLimit);
}
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    inputMap.fill(-1);const auto active=device->getActiveInputChannels();int callbackIndex=0;
    for(int physical=0;physical<=active.getHighestBit();++physical)if(active[physical]){if(physical<DeviceSettings::channelLimit)inputMap[(size_t)physical]=callbackIndex;++callbackIndex;}
    prepare(device->getCurrentSampleRate(),device->getCurrentBufferSizeSamples());
}
void AudioEngine::audioDeviceStopped() { panicRequested = true; }
void AudioEngine::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message) { midiCollector.addMessageToQueue(message); }
void AudioEngine::setMidiInputEnabled(const juce::String& id, bool enabled) { deviceManager.setMidiInputDeviceEnabled(id, enabled); }
void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputs, int inChannels, float* const* outputs,
                                                  int outChannels, int samples, const juce::AudioIODeviceCallbackContext&)
{
    ++deviceCallbacks;
    const float* physicalInputs[DeviceSettings::channelLimit] {};
    for(int physical=0;physical<DeviceSettings::channelLimit;++physical){const int index=inputMap[(size_t)physical];if(index>=0&&index<inChannels&&inputs)physicalInputs[physical]=inputs[index];}
    render(outputs,outChannels,samples,physicalInputs,DeviceSettings::channelLimit);
}
void AudioEngine::renderOfflineBlock(juce::AudioBuffer<float>& output, int samples)
{
    jassert(samples <= output.getNumSamples());
    render(output.getArrayOfWritePointers(), output.getNumChannels(), juce::jmin(samples, output.getNumSamples()));
}

void AudioEngine::render(float* const* output, int outputChannels, int samples,
                         const float* const* input, int inputChannels)
{
    juce::ScopedNoDenormals noDenormals;
    for (int ch = 0; ch < outputChannels; ++ch) if (output[ch]) juce::FloatVectorOperations::clear(output[ch], samples);
    const juce::ScopedTryLock lock(modelLock);
    if (!lock.isLocked()) { ++audioUnderruns; return; }
    if (samples <= 0 || samples > blockSize.load()) { ++audioUnderruns; return; }
    mixBuffer.clear();
    liveMidi.clear();
    if (!exporting)
    {
        midiCollector.removeNextBlockOfMessages(liveMidi, samples);
        keyboardState.processNextMidiBuffer(liveMidi, 0, samples, true);
    }
    if (panicRequested.exchange(false))
    {
        for (auto& track : tracks)
        {
            track->runtime->synth.allNotesOff(0, false);
            track->runtime->resetNotes = true;
            track->runtime->delay.clear();
            track->runtime->delayPosition = 0;
            for (auto& slot : track->plugins) if (slot->processor) slot->processor->reset();
        }
    }
    if (chaseRequested.exchange(false)) for (auto& track : tracks) track->runtime->chase = true;
    bool running = playing.load();
    double cursor = positionSeconds.load();
    const double rate = sampleRate.load();
    const bool useLoop = running && looping.load() && loopEnd.load() > loopStart.load() + 1.0 / rate;
    int processed = 0;
    if (running && countInSamples.load() > 0)
    {
        // Count-in: only the click sounds; the transport and the recorders wait.
        const auto left = countInSamples.load();
        processed = static_cast<int>(juce::jmin<juce::int64>(samples, left));
        const double elapsed = static_cast<double>(countInLength.load() - left) / rate;
        click.render(mixBuffer, 0, processed, elapsed, juce::jlimit(20.0, 400.0, tempo.load()), 4);
        countInSamples = left - processed;
    }
    const int countedIn = processed;
    while (processed < samples)
    {
        int count = samples - processed;
        if (useLoop)
        {
            const double end = loopEnd.load(), start = juce::jmax(0.0, loopStart.load());
            if (cursor >= end - 0.5 / rate)
            {
                cursor = start;
                for (auto& track : tracks)
                {
                    track->runtime->synth.allNotesOff(0, false);
                    track->runtime->resetNotes = true;
                    track->runtime->chase = true;
                }
            }
            count = juce::jmin(count, juce::jmax(1, static_cast<int>(std::ceil((end - cursor) * rate - 1.0e-7))));
        }
        // «Reproducir selección» and friends: stop exactly at the requested position.
        const double stop = stopAt.load();
        const bool stopping = running && stop >= 0.0 && cursor + count / rate >= stop - 0.5 / rate;
        if (stopping) count = juce::jlimit(1, count, static_cast<int>(std::ceil((stop - cursor) * rate - 1.0e-7)));
        renderSegment(processed, count, cursor, running, input, inputChannels);
        if (running) cursor += count / rate;
        processed += count;
        if (stopping) { positionSeconds = stop; cursor = stop; running = false; playing = false; stopAt = -1.0; panicRequested = true; }
    }
    if (running) positionSeconds = cursor;
    if (! exporting) click.renderPreview(mixBuffer, samples);
    if (recording.load() && recordingSession) recordingSession->samples += samples - countedIn;
    float peak[2] {};
    const float gain = juce::jlimit(0.0f, 4.0f, finiteGain(masterGain.load()));
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto* src = mixBuffer.getReadPointer(ch);
        for (int i = 0; i < samples; ++i)
        {
            const float value = finiteGain(src[i] * gain);
            peak[ch] = juce::jmax(peak[ch], std::abs(value));
            if (std::abs(value) > 1.0f) ++clippedSamples;
            if (ch < outputChannels && output[ch]) output[ch][i] = juce::jlimit(-1.0f, 1.0f, value);
        }
    }
    if (outputChannels == 1 && output[0])
        for (int i = 0; i < samples; ++i)
            output[0][i] = juce::jlimit(-1.0f, 1.0f, finiteGain((mixBuffer.getSample(0, i) + mixBuffer.getSample(1, i)) * gain * 0.5f));
    masterPeakL = juce::jmax(peak[0], masterPeakL.load() * 0.88f);
    masterPeakR = juce::jmax(peak[1], masterPeakR.load() * 0.88f);
}

void AudioEngine::renderSegment(int offset, int samples, double start, bool transportPlaying,
                                const float* const* input, int inputChannels)
{
    const double rate = sampleRate.load(), bpm = juce::jlimit(20.0, 400.0, tempo.load());
    const double end = start + samples / rate;
    const bool hasSolo = std::any_of(tracks.begin(), tracks.end(), [](const auto& t) { return t->solo.load(); });
    int maximumLatency = 0;
    for (const auto& track : tracks)
    {
        int latency = 0;
        for (const auto& slot : track->plugins)
            if (slot->processor && !slot->bypassed.load()) latency += juce::jmax(0, slot->processor->getLatencySamples());
        maximumLatency = juce::jmax(maximumLatency, latency);
    }
    playHead->blockPosition = start;
    // Routing: resolve each channel's output (group) and sends (FX), then give every bus a rank
    // above all channels feeding it. Rendering rank by rank guarantees sources fill a bus
    // before the bus itself is processed. No allocation: state lives in each track's runtime.
    const int count = static_cast<int>(tracks.size());
    const auto resolve = [&](int cached, const juce::String& id, TrackKind kind, int self)
    {
        if (id.isEmpty()) return -1;
        int found = -1;
        if (juce::isPositiveAndBelow(cached, count) && tracks[static_cast<size_t>(cached)]->id == id) found = cached;
        else for (int n = 0; n < count && found < 0; ++n) if (tracks[static_cast<size_t>(n)]->id == id) found = n;
        return found >= 0 && found != self && tracks[static_cast<size_t>(found)]->kind == kind ? found : -1;
    };
    for (int n = 0; n < count; ++n)
    {
        auto& t = *tracks[static_cast<size_t>(n)];
        auto& rt = *t.runtime;
        const bool folder = t.kind == TrackKind::folder;
        rt.outputIndex = folder ? -1 : resolve(rt.outputIndex, t.output, TrackKind::group, n);
        for (int k = 0; k < Track::sendCount; ++k)
            rt.sendIndex[static_cast<size_t>(k)] = folder || ! t.sends[static_cast<size_t>(k)].enabled.load() ? -1
                : resolve(rt.sendIndex[static_cast<size_t>(k)], t.sends[static_cast<size_t>(k)].target, TrackKind::fx, n);
        rt.rank = 0;
        if (t.isBus()) rt.bus.clear(0, samples);
    }
    int maxRank = 0;
    for (int pass = 0; pass < count; ++pass)
    {
        bool raised = false;
        for (const auto& t : tracks)
        {
            auto& rt = *t->runtime;
            const auto raise = [&](int destination)
            {
                if (destination < 0) return;
                auto& d = *tracks[static_cast<size_t>(destination)]->runtime;
                if (d.rank <= rt.rank) { d.rank = rt.rank + 1; maxRank = juce::jmax(maxRank, d.rank); raised = true; }
            };
            raise(rt.outputIndex);
            for (int destination : rt.sendIndex) raise(destination);
        }
        if (! raised) break;
    }
    for (const auto& t : tracks)
    {
        // A source stays audible when a channel it feeds is soloed (upstream solo).
        auto& rt = *t->runtime;
        rt.destinationSoloed = rt.outputIndex >= 0 && tracks[static_cast<size_t>(rt.outputIndex)]->solo.load();
        for (int destination : rt.sendIndex) if (destination >= 0 && tracks[static_cast<size_t>(destination)]->solo.load()) rt.destinationSoloed = true;
    }
    const auto processTrack = [&](const std::shared_ptr<Track>& track)
    {
        auto& rt = *track->runtime;
        rt.buffer.clear();
        rt.midi.clear();
        if (track->kind == TrackKind::folder) { track->peakL = track->peakR = 0.0f; return; }
        if (track->isBus())
            for (int ch = 0; ch < 2; ++ch) rt.buffer.addFrom(ch, 0, rt.bus, ch, 0, samples);
        if (rt.resetNotes)
        {
            for (int ch = 1; ch <= 16; ++ch) { rt.midi.addEvent(juce::MidiMessage::allNotesOff(ch), 0); rt.midi.addEvent(juce::MidiMessage::allSoundOff(ch), 0); }
            rt.resetNotes = false;
        }
        if (track->isMidi && (track->monitor.load() || track->armed.load())) rt.midi.addEvents(liveMidi, offset, samples, -offset);
        if (transportPlaying)
        {
            // Cubase plays only the topmost event where audio events overlap (the last one in the
            // list): mark which clip owns every sample of this block first.
            const bool ownership = static_cast<int>(rt.owner.size()) >= samples;
            if (ownership) std::fill(rt.owner.begin(), rt.owner.begin() + samples, -1);
            const auto clipRange = [&](const AudioClip& clip, int& first, int& last)
            {
                if (!clip.audio || clip.audio->getNumSamples() < 1 || clip.sourceRate <= 0.0 || clip.durationSeconds <= 0.0) return false;
                if (clip.startSeconds >= end || clip.startSeconds + clip.durationSeconds <= start) return false;
                first = juce::jlimit(0, samples, static_cast<int>(std::ceil((clip.startSeconds - start) * rate - 1.0e-8)));
                last = juce::jlimit(0, samples, static_cast<int>(std::ceil((clip.startSeconds + clip.durationSeconds - start) * rate - 1.0e-8)));
                return first < last;
            };
            if (ownership)
                for (int k = 0; k < static_cast<int>(track->clips.size()); ++k)
                {
                    const auto& clip = track->clips[(size_t) k];
                    int first = 0, last = 0;
                    if (clip.muted || ! clipRange(clip, first, last)) continue;
                    // A crossfading event does not cover the one below during its fade-in.
                    if (clip.crossfade) first = juce::jlimit(first, last, static_cast<int>(std::ceil((clip.startSeconds + clip.fadeIn - start) * rate - 1.0e-8)));
                    std::fill(rt.owner.begin() + first, rt.owner.begin() + last, k);
                }
            for (int k = 0; k < static_cast<int>(track->clips.size()); ++k)
            {
                const auto& clip = track->clips[(size_t) k];
                int first = 0, last = 0;
                if (clip.muted || !clipRange(clip, first, last)) continue;
                // Same rate: direct (bit-exact on the sample grid). Different rate: band-limited sinc.
                const bool resample = std::abs(clip.sourceRate - rate) > 1.0e-6;
                const double sincScale = audio::BandlimitedInterpolator::scaleFor(clip.sourceRate, rate);
                const auto& sinc = audio::BandlimitedInterpolator::instance();
                for (int i = first; i < last; ++i)
                {
                    const double local = start + i / rate - clip.startSeconds;
                    if (ownership && rt.owner[(size_t) i] != k && ! (clip.crossfade && local < clip.fadeIn)) continue;
                    const double source = (local + clip.offsetSeconds) * clip.sourceRate;
                    if (source < 0.0 || source >= clip.audio->getNumSamples()) continue;
                    const int index = static_cast<int>(source), next = juce::jmin(index + 1, clip.audio->getNumSamples() - 1);
                    const float fraction = static_cast<float>(source - index);
                    float fade = clip.gain;
                    if (clip.fadeIn > 0.0) fade *= static_cast<float>(juce::jlimit(0.0, 1.0, local / clip.fadeIn));
                    if (clip.fadeOut > 0.0) fade *= static_cast<float>(juce::jlimit(0.0, 1.0, (clip.durationSeconds - local) / clip.fadeOut));
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        const auto* data = clip.audio->getReadPointer(juce::jmin(ch, clip.audio->getNumChannels() - 1));
                        const float value = resample ? sinc.read(data, clip.audio->getNumSamples(), source, sincScale)
                                                     : data[index] + fraction * (data[next] - data[index]);
                        rt.buffer.addSample(ch, i, value * fade);
                    }
                }
            }
            for (const auto& note : track->notes)
            {
                const double on = note.startBeats * 60.0 / bpm, off = (note.startBeats + note.lengthBeats) * 60.0 / bpm;
                const auto add = [&](double time, bool isOn)
                {
                    if (time >= start - 1.0e-10 && time < end - 1.0e-10)
                    {
                        const int index = juce::jlimit(0, samples - 1, static_cast<int>(std::llround((time - start) * rate)));
                        rt.midi.addEvent(isOn ? juce::MidiMessage::noteOn(1, juce::jlimit(0, 127, note.pitch), static_cast<juce::uint8>(juce::jlimit(1, 127, note.velocity)))
                                             : juce::MidiMessage::noteOff(1, juce::jlimit(0, 127, note.pitch)), index);
                    }
                };
                if (rt.chase && on < start && off > start) rt.midi.addEvent(juce::MidiMessage::noteOn(1, juce::jlimit(0, 127, note.pitch), static_cast<juce::uint8>(juce::jlimit(1, 127, note.velocity))), 0);
                add(on, true); add(off, false);
            }
        }
        rt.chase = false;
        if (track->kind == TrackKind::audio && track->monitor.load() && input && inputChannels > 0)
            for (int ch = 0; ch < 2; ++ch)
            {const int source=ch==0?track->inputLeft:track->inputRight;if(source>=0&&source<inputChannels&&input[source])rt.buffer.addFrom(ch,0,input[source]+offset,samples);}
        bool hasInstrument = false;
        for (auto& slot : track->plugins)
            if (slot->processor && !slot->bypassed.load() && slot->description.isInstrument) hasInstrument = true;
        if (track->isMidi && !hasInstrument) rt.synth.renderNextBlock(rt.buffer, rt.midi, 0, samples);
        for (auto& slot : track->plugins)
        {
            if (!slot->processor || slot->bypassed.load()) continue;
            auto& processor = *slot->processor;
            const int channels = juce::jlimit(2, maxChannels, juce::jmax(processor.getTotalNumInputChannels(), processor.getTotalNumOutputChannels()));
            juce::AudioBuffer<float> pluginBuffer(rt.buffer.getArrayOfWritePointers(), channels, 0, samples);
            processor.processBlock(pluginBuffer, rt.midi);
            if (processor.getTotalNumOutputChannels() == 1) rt.buffer.copyFrom(1, 0, rt.buffer, 0, 0, samples);
        }
        // Channels fed by a soloed track stay open (solo-implied mute does not apply to buses).
        const bool audible = !track->mute.load() && (!hasSolo || track->solo.load() || track->isBus() || rt.destinationSoloed);
        const float gain = audible ? juce::jlimit(0.0f, 4.0f, finiteGain(track->gain.load())) : 0.0f;
        const float pan = juce::jlimit(-1.0f, 1.0f, finiteGain(track->pan.load()));
        // Equal-power pan, -3 dB at centre. Stereo channels are balanced, not summed.
        const float gains[] { gain * std::cos((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f),
                              gain * std::sin((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) };
        const float low = juce::Decibels::decibelsToGain(juce::jlimit(-24.0f, 24.0f, finiteGain(track->lowGainDb.load())));
        const float mid = juce::Decibels::decibelsToGain(juce::jlimit(-24.0f, 24.0f, finiteGain(track->midGainDb.load())));
        const float high = juce::Decibels::decibelsToGain(juce::jlimit(-24.0f, 24.0f, finiteGain(track->highGainDb.load())));
        const float lowCoeff = static_cast<float>(1.0 - std::exp(-twoPi * 250.0 / rate));
        const float highCoeff = static_cast<float>(1.0 - std::exp(-twoPi * 4000.0 / rate));
        int trackLatency = 0;
        for (const auto& slot : track->plugins)
            if (slot->processor && !slot->bypassed.load()) trackLatency += juce::jmax(0, slot->processor->getLatencySamples());
        const int compensation = juce::jlimit(0, rt.delay.getNumSamples() - 1, maximumLatency - trackLatency);
        float peaks[2] {};
        // Post-fader output goes to the group bus or the Stereo Out mix. Sends tap the signal
        // before the fader (pre: after inserts and EQ, follows mute) or after fader and pan.
        float sendGain[Track::sendCount] {};
        bool sendPre[Track::sendCount] {};
        for (int k = 0; k < Track::sendCount; ++k)
        {
            const auto& send = track->sends[static_cast<size_t>(k)];
            sendPre[k] = send.preFader.load();
            const float level = juce::jlimit(-60.0f, 6.0f, finiteGain(send.levelDb.load()));
            sendGain[k] = rt.sendIndex[static_cast<size_t>(k)] < 0 || level <= -60.0f ? 0.0f : juce::Decibels::decibelsToGain(level) * (sendPre[k] && ! audible ? 0.0f : 1.0f);
        }
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto* src = rt.buffer.getReadPointer(ch);
            auto* dst = rt.outputIndex >= 0 ? tracks[static_cast<size_t>(rt.outputIndex)]->runtime->bus.getWritePointer(ch)
                                            : mixBuffer.getWritePointer(ch, offset);
            float* sendDst[Track::sendCount] {};
            for (int k = 0; k < Track::sendCount; ++k)
                if (sendGain[k] > 0.0f) sendDst[k] = tracks[static_cast<size_t>(rt.sendIndex[static_cast<size_t>(k)])]->runtime->bus.getWritePointer(ch);
            for (int i = 0; i < samples; ++i)
            {
                const float value = finiteGain(src[i]);
                rt.lowState[ch] += lowCoeff * (value - rt.lowState[ch]);
                rt.highState[ch] += highCoeff * (value - rt.highState[ch]);
                const float eq = rt.lowState[ch] * low + (rt.highState[ch] - rt.lowState[ch]) * mid + (value - rt.highState[ch]) * high;
                const float result = finiteGain(eq * gains[ch]);
                const int writeIndex = (rt.delayPosition + i) % rt.delay.getNumSamples();
                const int readIndex = (writeIndex + rt.delay.getNumSamples() - compensation) % rt.delay.getNumSamples();
                rt.delay.setSample(ch, writeIndex, finiteGain(eq));
                const float preFader = rt.delay.getSample(ch, readIndex);
                const float delayed = preFader * gains[ch];
                dst[i] += delayed;
                for (int k = 0; k < Track::sendCount; ++k)
                    if (sendDst[k] != nullptr) sendDst[k][i] += (sendPre[k] ? preFader : delayed) * sendGain[k];
                peaks[ch] = juce::jmax(peaks[ch], std::abs(delayed));
            }
        }
        track->peakL = juce::jmax(peaks[0], track->peakL.load() * 0.88f);
        track->peakR = juce::jmax(peaks[1], track->peakR.load() * 0.88f);
        rt.delayPosition = (rt.delayPosition + samples) % rt.delay.getNumSamples();
    };
    for (int rank = 0; rank <= maxRank; ++rank)
        for (const auto& track : tracks)
            if (track->runtime->rank == rank) processTrack(track);
    if (recording.load() && recordingSession && transportPlaying)
    {
        // Input is written only inside the punch window: punch in/out at the locators, and
        // from `captureFrom` when the recording started with a pre-roll.
        auto& session = *recordingSession;
        double from = session.captureFrom, to = std::numeric_limits<double>::max();
        if (punchIn.load()) from = juce::jmax(from, loopStart.load());
        if (punchOut.load()) to = loopEnd.load();
        const int a = juce::jlimit(0, samples, static_cast<int>(std::ceil((from - start) * rate - 1.0e-7)));
        const int b = juce::jlimit(0, samples, static_cast<int>(std::ceil((juce::jmin(to, end + 1.0) - start) * rate - 1.0e-7)));
        const int written = b - a;
        capturing = written > 0;
        if (written > 0)
        {
            const double at = start + a / rate;
            if (session.lapCount == 0 || std::abs(at - session.lastEnd) > 0.5 / rate)
            {
                if (session.lapCount < static_cast<int>(session.laps.size())) session.laps[(size_t) session.lapCount++] = { at, session.captured, 0 };
            }
            session.laps[(size_t) (session.lapCount - 1)].length += written;
            session.captured += written;
            session.lastEnd = at + written / rate;
            const auto projectSample = static_cast<juce::int64>(std::llround(start * rate));
            for (auto& entry : session.entries)
            {
                if (entry.writer)
                {
                    const float* channels[2] {};
                    juce::AudioBuffer<float>& scratch = entry.track->runtime->buffer;
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        const int source = ch == 0 ? entry.track->inputLeft : entry.track->inputRight;
                        if (input && source >= 0 && source < inputChannels && input[source]) channels[ch] = input[source] + offset + a;
                        else { scratch.clear(ch, 0, written); channels[ch] = scratch.getReadPointer(ch); }
                    }
                    if (!entry.writer->write(channels, written)) session.failed = true;
                }
                else if (projectSample < std::numeric_limits<int>::max() - samples)
                    entry.midi.addEvents(liveMidi, offset + a, written, static_cast<int>(projectSample) - offset);
            }
        }
        else if (session.captured > 0 && ! looping.load() && end >= to) punchedOut = true;
    }
    if (metronome.load() && transportPlaying && (! click.onlyWhenRecording.load() || recording.load()))
        click.render(mixBuffer, offset, samples, start, bpm, 4);
}

juce::String AudioEngine::supportedAudioWildcard() const { return formatManager.getWildcardForAllFormats(); }
juce::Result AudioEngine::readAudio(const juce::File& file, AudioClip& clip)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (!reader) return juce::Result::fail(i18n::tr("No se puede leer el audio: ") + file.getFileName());
    if (reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0 || reader->numChannels < 1)
        return juce::Result::fail(i18n::tr("El archivo de audio está vacío o no es válido."));
    if (reader->lengthInSamples > 100000000)
        return juce::Result::fail(i18n::tr("Este archivo supera el límite de 100 millones de muestras por clip. Divide el archivo antes de importarlo."));
    auto data = std::make_shared<juce::AudioBuffer<float>>(juce::jmin(2, static_cast<int>(reader->numChannels)), static_cast<int>(reader->lengthInSamples));
    if (!reader->read(data.get(), 0, data->getNumSamples(), 0, true, true)) return juce::Result::fail(i18n::tr("No se pudo decodificar ") + file.getFileName());
    clip.name = file.getFileNameWithoutExtension();
    clip.file = file;
    clip.audio = std::move(data);
    clip.sourceRate = reader->sampleRate;
    clip.durationSeconds = reader->lengthInSamples / reader->sampleRate;
    return juce::Result::ok();
}
juce::Result AudioEngine::importAudio(const juce::File& file, int trackIndex, double start)
{
    AudioClip clip;
    auto result = readAudio(file, clip);
    if (result.failed()) return result;
    clip.startSeconds = juce::jmax(0.0, start);
    const juce::ScopedLock lock(modelLock);
    if (!juce::isPositiveAndBelow(trackIndex, static_cast<int>(tracks.size())) || tracks[static_cast<size_t>(trackIndex)]->kind != TrackKind::audio)
    {
        auto track = makeTrack(clip.name, false);
        tracks.push_back(track);
        trackIndex = static_cast<int>(tracks.size()) - 1;
    }
    tracks[static_cast<size_t>(trackIndex)]->clips.push_back(std::move(clip));
    return juce::Result::ok();
}
juce::Result AudioEngine::importMidi(const juce::File& file)
{
    auto stream = file.createInputStream();
    if (!stream) return juce::Result::fail(i18n::tr("No se puede abrir el archivo MIDI."));
    juce::MidiFile midi;
    if (!midi.readFrom(*stream)) return juce::Result::fail(i18n::tr("Archivo MIDI no válido."));
    double firstBpm = 120.0, firstTempoTime = std::numeric_limits<double>::max();
    for (int t = 0; t < midi.getNumTracks(); ++t)
        for (int e = 0; e < midi.getTrack(t)->getNumEvents(); ++e)
        {
            const auto& message = midi.getTrack(t)->getEventPointer(e)->message;
            if (message.isTempoMetaEvent() && message.getTimeStamp() < firstTempoTime)
            { firstBpm = 60.0 / message.getTempoSecondsPerQuarterNote(); firstTempoTime = message.getTimeStamp(); }
        }
    midi.convertTimestampTicksToSeconds();
    const juce::ScopedLock lock(modelLock);
    if (tracks.empty()) tempo = juce::jlimit(20.0, 400.0, firstBpm);
    const double scale = tempo.load() / 60.0;
    int imported = 0;
    for (int t = 0; t < midi.getNumTracks(); ++t)
    {
        juce::MidiMessageSequence sequence(*midi.getTrack(t));
        sequence.updateMatchedPairs();
        auto track = makeTrack(file.getFileNameWithoutExtension() + juce::String::fromUTF8(" · ") + juce::String(t + 1), true);
        for (int e = 0; e < sequence.getNumEvents(); ++e)
        {
            const auto* event = sequence.getEventPointer(e);
            if (event->message.isTrackNameEvent()) track->name = event->message.getTextFromTextMetaEvent();
            if (event->message.isNoteOn() && event->noteOffObject)
            {
                MidiNote note;
                note.pitch = event->message.getNoteNumber(); note.velocity = event->message.getVelocity();
                note.startBeats = juce::jmax(0.0, event->message.getTimeStamp() * scale);
                note.lengthBeats = juce::jmax(0.001, (event->noteOffObject->message.getTimeStamp() - event->message.getTimeStamp()) * scale);
                track->notes.push_back(note);
            }
        }
        if (!track->notes.empty()) { tracks.push_back(track); ++imported; }
    }
    if (imported == 0) return juce::Result::fail(i18n::tr("El archivo MIDI no contiene notas con inicio y final."));
    lastWarning = i18n::tr("MIDI importado como notas; los cambios de tempo se convierten a tiempos absolutos. CC, SysEx y cambios de programa no se conservan.");
    return juce::Result::ok();
}
double AudioEngine::duration() const
{
    const juce::ScopedLock lock(modelLock);
    double result = 0.0;
    for (const auto& track : tracks)
    {
        for (const auto& clip : track->clips) result = juce::jmax(result, clip.startSeconds + clip.durationSeconds);
        for (const auto& note : track->notes) result = juce::jmax(result, (note.startBeats + note.lengthBeats) * 60.0 / juce::jlimit(20.0, 400.0, tempo.load()));
    }
    return result;
}
juce::Result AudioEngine::addPlugin(int index, std::unique_ptr<juce::AudioPluginInstance> processor, const juce::PluginDescription& description, int replaceSlot)
{
    if (!processor) return juce::Result::fail(i18n::tr("El plugin no se ha creado."));
    const juce::ScopedLock lock(modelLock);
    if (!juce::isPositiveAndBelow(index, static_cast<int>(tracks.size()))) return juce::Result::fail(i18n::tr("La pista ya no existe."));
    processor->enableAllBuses();
    auto layout = processor->getBusesLayout();
    if (!layout.outputBuses.isEmpty()) layout.outputBuses.set(0, juce::AudioChannelSet::stereo());
    if (!layout.inputBuses.isEmpty()) layout.inputBuses.set(0, juce::AudioChannelSet::stereo());
    for (int bus = 1; bus < layout.inputBuses.size(); ++bus) layout.inputBuses.set(bus, juce::AudioChannelSet::disabled());
    for (int bus = 1; bus < layout.outputBuses.size(); ++bus) layout.outputBuses.set(bus, juce::AudioChannelSet::disabled());
    if (processor->checkBusesLayoutSupported(layout)) processor->setBusesLayout(layout);
    if (processor->getTotalNumInputChannels() > maxChannels || processor->getTotalNumOutputChannels() > maxChannels)
        return juce::Result::fail(i18n::tr("El plugin necesita más de 16 canales; este anfitrión admite una salida estéreo principal."));
    processor->setPlayHead(playHead.get());
    processor->setRateAndBufferSizeDetails(sampleRate.load(), blockSize.load());
    processor->prepareToPlay(sampleRate.load(), blockSize.load());
    auto& slots=tracks[static_cast<size_t>(index)]->plugins;
    if(replaceSlot>=static_cast<int>(slots.size()))return juce::Result::fail(i18n::tr("La ranura ya no existe."));
    int chainLatency = processor->getLatencySamples();
    for (int n=0;n<static_cast<int>(slots.size());++n)
        if(n!=replaceSlot&&slots[static_cast<size_t>(n)]->processor&&!slots[static_cast<size_t>(n)]->bypassed.load())
            chainLatency+=slots[static_cast<size_t>(n)]->processor->getLatencySamples();
    if (chainLatency > static_cast<int>(sampleRate.load() * 2.0))
        return juce::Result::fail(i18n::tr("La cadena de plugins supera los dos segundos de compensación de latencia disponibles."));
    auto slot = std::make_shared<PluginSlot>();
    slot->processor = std::move(processor); slot->description = description;
    if(replaceSlot>=0)
    {
        if(slots[static_cast<size_t>(replaceSlot)]->processor)slots[static_cast<size_t>(replaceSlot)]->processor->setPlayHead(nullptr);
        slots[static_cast<size_t>(replaceSlot)]=slot;
    }
    else if(description.isInstrument)slots.insert(slots.begin(),slot);
    else slots.push_back(slot);
    return juce::Result::ok();
}
void AudioEngine::removePlugin(int trackIndex, int slotIndex)
{
    const juce::ScopedLock lock(modelLock);
    if (!juce::isPositiveAndBelow(trackIndex, static_cast<int>(tracks.size()))) return;
    auto& slots = tracks[static_cast<size_t>(trackIndex)]->plugins;
    if (juce::isPositiveAndBelow(slotIndex, static_cast<int>(slots.size())))
    {
        if (slots[static_cast<size_t>(slotIndex)]->processor) slots[static_cast<size_t>(slotIndex)]->processor->setPlayHead(nullptr);
        slots.erase(slots.begin() + slotIndex);
    }
}
int AudioEngine::pluginLatencySamples() const
{
    const juce::ScopedLock lock(modelLock);
    int maxLatency = 0;
    for (const auto& track : tracks)
    {
        int latency = 0;
        for (const auto& slot : track->plugins) if (slot->processor && !slot->bypassed.load()) latency += slot->processor->getLatencySamples();
        maxLatency = juce::jmax(maxLatency, latency);
    }
    return maxLatency;
}

juce::var AudioEngine::projectToJson(const juce::File& destination)
{
    auto root = object();
    set(root, "format", "Silente"); set(root, "version", 1); set(root, "name", projectName);
    set(root, "tempo", tempo.load()); set(root, "masterGain", masterGain.load());
    set(root, "punchIn", punchIn.load()); set(root, "punchOut", punchOut.load());
    set(root, "audioRecordMode", audioRecordMode.load()); set(root, "recordBitDepth", recordBitDepth.load());
    set(root, "looping", looping.load()); set(root, "loopStart", loopStart.load()); set(root, "loopEnd", loopEnd.load());
    set(root, "positionSeconds", positionSeconds.load());
    const auto media = destination.getParentDirectory().getChildFile(destination.getFileNameWithoutExtension() + " Media");
    juce::Array<juce::var> trackArray;
    for (const auto& track : tracks)
    {
        auto t = object();
        set(t, "id", track->id); set(t, "name", track->name); set(t, "colour", track->colour.toString()); set(t, "stripWidth", track->stripWidth);
        set(t, "kind", trackKindId(track->kind)); set(t, "isMidi", track->isMidi); set(t, "gain", track->gain.load()); set(t, "pan", track->pan.load());
        set(t, "mute", track->mute.load()); set(t, "solo", track->solo.load());
        set(t,"inputLeft",track->inputLeft);set(t,"inputRight",track->inputRight);
        set(t, "output", track->output);
        {
            juce::Array<juce::var> sends;
            for (const auto& send : track->sends)
            {
                auto sv = object();
                set(sv, "target", send.target); set(sv, "levelDb", send.levelDb.load());
                set(sv, "preFader", send.preFader.load()); set(sv, "enabled", send.enabled.load());
                sends.add(sv);
            }
            set(t, "sends", sends);
        }
        set(t, "armed", track->armed.load()); set(t, "monitor", track->monitor.load());
        set(t, "lowGainDb", track->lowGainDb.load()); set(t, "midGainDb", track->midGainDb.load()); set(t, "highGainDb", track->highGainDb.load());
        juce::Array<juce::var> clips, notes, plugins;
        for (const auto& clip : track->clips)
        {
            auto c = object();
            juce::File audioFile = clip.file;
            if (clip.audio)
            {
                if (media.createDirectory().failed()) { lastWarning = i18n::tr("No se pudo crear la carpeta de audio del proyecto."); return {}; }
                const auto extension = audioFile.existsAsFile() ? audioFile.getFileExtension() : juce::String(".wav");
                auto copy = media.getChildFile(juce::File::createLegalFileName(clip.id) + extension);
                if (audioFile.existsAsFile())
                {
                    if (audioFile != copy && !audioFile.copyFileTo(copy)) { lastWarning = i18n::tr("No se pudo copiar ") + audioFile.getFileName(); return {}; }
                }
                else
                {
                    std::unique_ptr<juce::OutputStream> stream(copy.createOutputStream());
                    if (!stream) { lastWarning = i18n::tr("No se pudo guardar el audio de ") + clip.name; return {}; }
                    stream->setPosition(0);
                    if (auto* fs = dynamic_cast<juce::FileOutputStream*>(stream.get())) fs->truncate();
                    juce::WavAudioFormat wav;
                    auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(clip.sourceRate).withNumChannels(clip.audio->getNumChannels()).withBitsPerSample(24));
                    if (!writer || !writer->writeFromAudioSampleBuffer(*clip.audio, 0, clip.audio->getNumSamples()))
                    { lastWarning = i18n::tr("No se pudo guardar el audio de ") + clip.name; return {}; }
                }
                audioFile = copy;
            }
            set(c, "id", clip.id); set(c, "name", clip.name);
            set(c, "file", audioFile == juce::File() ? juce::String() : audioFile.getRelativePathFrom(destination.getParentDirectory()));
            set(c, "startSeconds", clip.startSeconds); set(c, "offsetSeconds", clip.offsetSeconds); set(c, "durationSeconds", clip.durationSeconds);
            set(c, "sourceRate", clip.sourceRate); set(c, "gain", clip.gain); set(c, "fadeIn", clip.fadeIn); set(c, "fadeOut", clip.fadeOut); if (! clip.colour.isTransparent()) set(c, "colour", clip.colour.toString());
            if (clip.muted) set(c, "muted", true); if (clip.crossfade) set(c, "crossfade", true);
            clips.add(c);
        }
        for (const auto& note : track->notes)
        {
            auto n = object(); set(n, "id", note.id); set(n, "pitch", note.pitch); set(n, "velocity", note.velocity);
            set(n, "startBeats", note.startBeats); set(n, "lengthBeats", note.lengthBeats); notes.add(n);
        }
        for (const auto& slot : track->plugins)
        {
            auto p = object();
            if (auto xml = slot->description.createXml()) set(p, "description", xml->toString());
            if (slot->processor) slot->processor->getStateInformation(slot->savedState);
            set(p, "state", slot->savedState.toBase64Encoding()); set(p, "bypassed", slot->bypassed.load()); plugins.add(p);
        }
        set(t, "clips", clips); set(t, "notes", notes); set(t, "plugins", plugins); trackArray.add(t);
    }
    set(root, "tracks", trackArray);
    return root;
}
juce::Result AudioEngine::saveProject(const juce::File& file)
{
    if (recording.load()) return juce::Result::fail(i18n::tr("Detén la grabación antes de guardar."));
    const juce::ScopedLock lock(modelLock);
    lastWarning.clear();
    auto json = projectToJson(file);
    if (json.isVoid()) return juce::Result::fail(lastWarning);
    juce::TemporaryFile temporary(file);
    if (!temporary.getFile().replaceWithText(juce::JSON::toString(json, false))) return juce::Result::fail(i18n::tr("No se pudo escribir el proyecto."));
    if (!temporary.overwriteTargetFileWithTemporary()) return juce::Result::fail(i18n::tr("No se pudo reemplazar el proyecto. El original está conservado."));
    projectFile = file;
    return juce::Result::ok();
}
juce::Result AudioEngine::loadProject(const juce::File& file)
{
    if (recording.load()) return juce::Result::fail(i18n::tr("Detén la grabación antes de abrir otro proyecto."));
    if (!file.existsAsFile()) return juce::Result::fail(i18n::tr("El proyecto no existe."));
    juce::var json;
    auto parse = juce::JSON::parse(file.loadFileAsString(), json);
    if (parse.failed() || json["format"].toString() != "Silente" || static_cast<int>(json["version"]) != 1 || !json["tracks"].isArray())
        return juce::Result::fail(i18n::tr("El archivo no es un proyecto Silente v1 válido."));
    std::vector<std::shared_ptr<Track>> loaded;
    juce::StringArray warnings;
    for (const auto& t : *json["tracks"].getArray())
    {
        auto track = std::make_shared<Track>();
        track->id = t.getProperty("id", juce::Uuid().toString()).toString(); track->name = t.getProperty("name", i18n::tr("Pista")).toString();
        track->colour = juce::Colour::fromString(t.getProperty("colour", "ffa994ff").toString());
        track->stripWidth = juce::jlimit(0, 240, static_cast<int>(t.getProperty("stripWidth", 0)));
        track->kind = trackKindFromId(t.getProperty("kind", {}).toString(), static_cast<bool>(t["isMidi"]));
        track->isMidi = track->kind == TrackKind::instrument || track->kind == TrackKind::midi;
        track->output = t.getProperty("output", {}).toString();
        if (auto* sends = t["sends"].getArray())
            for (int k = 0; k < juce::jmin(Track::sendCount, sends->size()); ++k)
            {
                const auto& sv = sends->getReference(k);
                auto& send = track->sends[static_cast<size_t>(k)];
                send.target = sv.getProperty("target", {}).toString();
                send.levelDb = static_cast<float>(juce::jlimit(-60.0, 6.0, static_cast<double>(sv.getProperty("levelDb", 0.0))));
                send.preFader = static_cast<bool>(sv.getProperty("preFader", false));
                send.enabled = static_cast<bool>(sv.getProperty("enabled", true));
            }
        track->inputLeft=juce::jlimit(0,DeviceSettings::channelLimit-1,(int)t.getProperty("inputLeft",0));track->inputRight=juce::jlimit(0,DeviceSettings::channelLimit-1,(int)t.getProperty("inputRight",1));
        track->gain = static_cast<float>(juce::jlimit(0.0, 4.0, number(t, "gain", 0.8))); track->pan = static_cast<float>(juce::jlimit(-1.0, 1.0, number(t, "pan", 0.0)));
        track->mute = static_cast<bool>(t["mute"]); track->solo = static_cast<bool>(t["solo"]);
        track->armed = static_cast<bool>(t["armed"]); track->monitor = static_cast<bool>(t["monitor"]);
        track->lowGainDb = static_cast<float>(number(t, "lowGainDb", 0.0)); track->midGainDb = static_cast<float>(number(t, "midGainDb", 0.0)); track->highGainDb = static_cast<float>(number(t, "highGainDb", 0.0));
        if (auto* clips = t["clips"].getArray()) for (const auto& c : *clips)
        {
            AudioClip clip;
            const auto path = c["file"].toString();
            auto audioFile = juce::File::isAbsolutePath(path) ? juce::File(path) : file.getParentDirectory().getChildFile(path);
            if (path.isNotEmpty())
            {
                const auto result = readAudio(audioFile, clip);
                if (result.failed()) { clip.file = audioFile; warnings.add(result.getErrorMessage()); }
            }
            clip.id = c.getProperty("id", juce::Uuid().toString()).toString(); clip.name = c.getProperty("name", i18n::tr("Audio")).toString();
            clip.sourceRate = number(c, "sourceRate", clip.sourceRate);
            clip.startSeconds = juce::jmax(0.0, number(c, "startSeconds", 0.0)); clip.offsetSeconds = juce::jmax(0.0, number(c, "offsetSeconds", 0.0));
            clip.durationSeconds = juce::jmax(0.0, number(c, "durationSeconds", clip.durationSeconds)); clip.gain = static_cast<float>(number(c, "gain", 1.0));
            clip.fadeIn = juce::jmax(0.0, number(c, "fadeIn", 0.005)); clip.fadeOut = juce::jmax(0.0, number(c, "fadeOut", 0.005));
            clip.muted = static_cast<bool>(c.getProperty("muted", false)); clip.crossfade = static_cast<bool>(c.getProperty("crossfade", false));
            if (c.hasProperty("colour")) clip.colour = juce::Colour::fromString(c["colour"].toString());
            track->clips.push_back(std::move(clip));
        }
        if (auto* notes = t["notes"].getArray()) for (const auto& n : *notes)
        {
            MidiNote note; note.id = n.getProperty("id", juce::Uuid().toString()).toString();
            note.pitch = juce::jlimit(0, 127, static_cast<int>(n["pitch"])); note.velocity = juce::jlimit(1, 127, static_cast<int>(n["velocity"]));
            note.startBeats = juce::jmax(0.0, number(n, "startBeats", 0.0)); note.lengthBeats = juce::jmax(0.001, number(n, "lengthBeats", 1.0)); track->notes.push_back(note);
        }
        if (auto* plugins = t["plugins"].getArray()) for (const auto& p : *plugins)
        {
            auto slot = std::make_shared<PluginSlot>();
            if (auto xml = juce::parseXML(p["description"].toString())) slot->description.loadFromXml(*xml);
            slot->savedState.fromBase64Encoding(p["state"].toString()); slot->bypassed = static_cast<bool>(p["bypassed"]);
            slot->processor = pluginFormats.createPluginInstance(slot->description, sampleRate.load(), blockSize.load(), slot->error);
            if (slot->processor)
            {
                if (slot->processor->getTotalNumInputChannels() > maxChannels || slot->processor->getTotalNumOutputChannels() > maxChannels)
                { slot->error = i18n::tr("Más de 16 canales"); slot->processor.reset(); }
                else if (slot->savedState.getSize() > 0) slot->processor->setStateInformation(slot->savedState.getData(), static_cast<int>(slot->savedState.getSize()));
            }
            if (!slot->processor) warnings.add(i18n::tr("Plugin ausente: ") + slot->description.name + juce::String::fromUTF8(" · ") + slot->error);
            track->plugins.push_back(slot);
        }
        loaded.push_back(track);
    }
    stop();
    const juce::ScopedLock lock(modelLock);
    tracks = std::move(loaded);
    punchIn = static_cast<bool>(json["punchIn"]); punchOut = static_cast<bool>(json["punchOut"]);
    audioRecordMode = juce::jlimit(0, 2, static_cast<int>(number(json, "audioRecordMode", 1.0)));
    { const int bits = static_cast<int>(number(json, "recordBitDepth", 24.0)); recordBitDepth = bits == 16 || bits == 32 ? bits : 24; }
    tempo = juce::jlimit(20.0, 400.0, number(json, "tempo", 120.0)); masterGain = static_cast<float>(juce::jlimit(0.0, 4.0, number(json, "masterGain", 0.8)));
    looping = static_cast<bool>(json["looping"]); loopStart = juce::jmax(0.0, number(json, "loopStart", 0.0)); loopEnd = juce::jmax(loopStart.load() + 0.001, number(json, "loopEnd", 8.0));
    positionSeconds = juce::jmax(0.0, number(json, "positionSeconds", 0.0));
    projectName = json.getProperty("name", file.getFileNameWithoutExtension()).toString(); projectFile = file;
    lastWarning = warnings.joinIntoString("\n");
    for (auto& track : tracks) prepareTrack(*track);
    return juce::Result::ok();
}

juce::Result AudioEngine::renderMix(double rate, const std::function<bool(const juce::AudioBuffer<float>&, int, int)>& sink,
                                   const std::function<bool(double)>& progress)
{
    if (recording.load()) return juce::Result::fail(i18n::tr("Detén la grabación antes de exportar."));
    const double length = duration();
    if (length <= 0.0) return juce::Result::fail(i18n::tr("El proyecto no contiene audio ni notas para exportar."));
    if (!std::isfinite(rate) || rate < 8000.0 || rate > 192000.0) return juce::Result::fail(i18n::tr("Frecuencia de exportación no válida."));
    if (length > 14400.0) return juce::Result::fail(i18n::tr("La exportación admite hasta cuatro horas por proyecto."));
    const bool wasPlaying = playing.exchange(false), wasLooping = looping.exchange(false), wasMetronome = metronome.exchange(false);
    const double oldPosition = positionSeconds.load(), oldRate = sampleRate.load();
    const int oldBlock = blockSize.load();
    if (initialised) deviceManager.removeAudioCallback(this);
    bool success = true, cancelled = false;
    {
        const juce::ScopedLock lock(modelLock);
        prepare(rate, 1024);
        exporting = true;
        for (auto& track : tracks) for (auto& slot : track->plugins) if (slot->processor) slot->processor->setNonRealtime(true);
        positionSeconds = 0.0; panicRequested = true; chaseRequested = true; playing = true;
        juce::AudioBuffer<float> block(2, 1024);
        const int latency = pluginLatencySamples();
        const juce::int64 total = static_cast<juce::int64>(std::ceil((length + 2.0) * rate)) + latency;
        for (juce::int64 position = 0; position < total && success; position += 1024)
        {
            const int count = static_cast<int>(juce::jmin<juce::int64>(1024, total - position));
            renderOfflineBlock(block, count);
            const int skip = static_cast<int>(juce::jlimit<juce::int64>(0, count, latency - position));
            if (skip < count) success = sink(block, skip, count - skip);
            if (progress && (position & 0xffff) == 0 && ! progress(static_cast<double>(position) / static_cast<double>(total))) { success = false; cancelled = true; }
        }
        playing = false;
        exporting = false;
        for (auto& track : tracks) for (auto& slot : track->plugins) if (slot->processor) slot->processor->setNonRealtime(false);
        prepare(oldRate, oldBlock);
        positionSeconds = oldPosition; looping = wasLooping; metronome = wasMetronome;
        panicRequested = true; chaseRequested = true;
    }
    if (initialised) deviceManager.addAudioCallback(this);
    playing = wasPlaying;
    if (cancelled) return juce::Result::fail(i18n::tr("Exportación cancelada."));
    if (!success) return juce::Result::fail(i18n::tr("La escritura del archivo falló. Comprueba el espacio libre."));
    return juce::Result::ok();
}
juce::Result AudioEngine::exportMix(const juce::File& file, double rate, int bitDepth)
{
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32) return juce::Result::fail(i18n::tr("La profundidad WAV debe ser 16, 24 o 32 bits."));
    {
        const juce::ScopedLock lock(modelLock);
        for (const auto& track : tracks) for (const auto& clip : track->clips)
            if (clip.file == file) return juce::Result::fail(i18n::tr("Elige otro nombre: el destino es un archivo de audio utilizado por este proyecto."));
    }
    juce::TemporaryFile temporary(file);
    std::unique_ptr<juce::OutputStream> stream(temporary.getFile().createOutputStream());
    if (!stream) return juce::Result::fail(i18n::tr("No se puede escribir en la carpeta de destino."));
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(2).withBitsPerSample(bitDepth));
    if (!writer) return juce::Result::fail(i18n::tr("No se pudo crear el archivo WAV."));
    auto result = renderMix(rate, [&writer](const juce::AudioBuffer<float>& block, int start, int count) { return writer->writeFromAudioSampleBuffer(block, start, count); });
    writer.reset();
    if (result.failed()) return result;
    if (!temporary.overwriteTargetFileWithTemporary()) return juce::Result::fail(i18n::tr("No se pudo finalizar el archivo WAV."));
    return juce::Result::ok();
}
bool AudioEngine::exportMix(const juce::File& file, juce::String& error)
{
    auto result = exportMix(file); error = result.getErrorMessage(); return result.wasOk();
}
juce::Result AudioEngine::beginRecording(const juce::File& directory, RecordOptions options)
{
    if (recording.load()) return juce::Result::fail(i18n::tr("Ya hay una grabación en curso."));
    if (directory.createDirectory().failed()) return juce::Result::fail(i18n::tr("No se pudo crear la carpeta de grabación."));
    bool needsAudio = false, anyArmed = false;
    {
        const juce::ScopedLock lock(modelLock);
        for (const auto& track : tracks) if (track->armed.load() && track->hasEvents()) { anyArmed = true; if (!track->isMidi) needsAudio = true; }
    }
    if (!anyArmed) return juce::Result::fail(i18n::tr("Arma al menos una pista con el botón R para grabar."));
    if (needsAudio && ! offlineInput)
    {
        if (!initialised) return juce::Result::fail(i18n::tr("No hay dispositivo de audio activo para grabar."));
        if (!deviceManager.getCurrentAudioDevice() || deviceManager.getCurrentAudioDevice()->getActiveInputChannels().countNumberOfSetBits() == 0)
            return juce::Result::fail(i18n::tr("Activa una entrada de micrófono o interfaz en Ajustes de audio antes de grabar."));
    }
    if(needsAudio && ! offlineInput)
    {
        const auto active=deviceManager.getCurrentAudioDevice()->getActiveInputChannels();
        for(const auto& track:tracks)if(track->armed&&track->kind==TrackKind::audio)
            if(!active[track->inputLeft]||!active[track->inputRight])return juce::Result::fail(i18n::tr("Activa las entradas asignadas a la pista en Audio: ")+track->name);
    }
    const juce::ScopedLock lock(modelLock);
    auto session = std::make_unique<RecordingSession>();
    session->start = positionSeconds.load(); session->rate = sampleRate.load(); session->bpm = tempo.load();
    session->captureFrom = options.captureFrom;
    const int bits = recordBitDepth.load() == 16 ? 16 : recordBitDepth.load() == 32 ? 32 : 24;
    diskThread.startThread();
    for (const auto& track : tracks)
    {
        if (!track->armed.load() || !track->hasEvents()) continue;
        RecordingSession::Entry entry;
        entry.track = track;
        if (!track->isMidi)
        {
            entry.file = directory.getNonexistentChildFile(juce::File::createLegalFileName(track->name) + " " + juce::Time::getCurrentTime().formatted("%Y-%m-%d %H-%M-%S"), ".wav");
            std::unique_ptr<juce::OutputStream> stream(entry.file.createOutputStream());
            juce::WavAudioFormat wav;
            auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(session->rate).withNumChannels(track->inputLeft==track->inputRight?1:2).withBitsPerSample(bits)
                                                   .withSampleFormat(bits == 32 ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint : juce::AudioFormatWriterOptions::SampleFormat::integral));
            if (!writer) return juce::Result::fail(i18n::tr("No se pudo crear ") + entry.file.getFileName());
            entry.writer = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(writer.release(), diskThread, 262144);
        }
        else entry.midi.ensureSize(1024 * 1024);
        session->entries.push_back(std::move(entry));
    }
    recordingSession = std::move(session);
    const auto countIn = options.countIn ? static_cast<juce::int64>(std::llround(click.countInBars.load() * 4 * 60.0 / juce::jlimit(20.0, 400.0, tempo.load()) * sampleRate.load())) : 0;
    countInLength = countIn; countInSamples = countIn;
    capturing = false; punchedOut = false;
    recording = true; chaseRequested = true; playing = true;
    return juce::Result::ok();
}
juce::File AudioEngine::audioFolder() const
{
    return projectFile != juce::File() ? projectFile.getParentDirectory().getChildFile("Audio")
                                       : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Silente/Audio");
}
void AudioEngine::cancelRecording()
{
    std::unique_ptr<RecordingSession> session;
    {
        const juce::ScopedLock lock(modelLock);
        recording = false; capturing = false;
        session = std::move(recordingSession);
    }
    if (session) for (auto& entry : session->entries) if (entry.writer) { entry.writer.reset(); entry.file.deleteFile(); }
}
juce::Result AudioEngine::endRecording()
{
    std::unique_ptr<RecordingSession> session;
    {
        const juce::ScopedLock lock(modelLock);
        recording = false; capturing = false;
        if (!recordingSession) return juce::Result::ok();
        session = std::move(recordingSession);
    }
    juce::StringArray errors;
    for (auto& entry : session->entries)
    {
        if (entry.writer)
        {
            entry.writer.reset();
            if (session->captured <= 0) { entry.file.deleteFile(); continue; }
            AudioClip recorded;
            auto result = readAudio(entry.file, recorded);
            if (result.failed()) { errors.add(result.getErrorMessage()); continue; }
            // One event per cycle lap, all reading the same file (Cubase: «tomas»).
            std::vector<AudioClip> takes;
            for (int n = 0; n < session->lapCount; ++n)
            {
                const auto& lap = session->laps[(size_t) n];
                if (lap.length <= 0) continue;
                AudioClip take = recorded;
                take.id = juce::Uuid().toString();
                take.startSeconds = lap.start;
                take.offsetSeconds = static_cast<double>(lap.fileOffset) / session->rate;
                take.durationSeconds = static_cast<double>(lap.length) / session->rate;
                if (session->lapCount > 1) take.name = recorded.name + i18n::tr(" · toma ") + juce::String(n + 1);
                takes.push_back(std::move(take));
            }
            const juce::ScopedLock lock(modelLock);
            audio::placeTakes(entry.track->clips, std::move(takes), static_cast<audio::AudioRecordMode>(juce::jlimit(0, 2, audioRecordMode.load())));
        }
        else
        {
            std::array<double, 128> starts;
            std::array<int, 128> velocities {};
            starts.fill(-1.0);
            std::vector<MidiNote> notes;
            const auto finish = [&](int pitch, double seconds)
            {
                if (starts[static_cast<size_t>(pitch)] < 0.0) return;
                MidiNote note; note.pitch = pitch; note.velocity = velocities[static_cast<size_t>(pitch)];
                note.startBeats = starts[static_cast<size_t>(pitch)] * session->bpm / 60.0;
                note.lengthBeats = juce::jmax(0.001, (seconds - starts[static_cast<size_t>(pitch)]) * session->bpm / 60.0);
                notes.push_back(note); starts[static_cast<size_t>(pitch)] = -1.0;
            };
            for (const auto metadata : entry.midi)
            {
                const auto message = metadata.getMessage();
                const double time = metadata.samplePosition / session->rate;
                if (message.isNoteOn())
                {
                    const int pitch = message.getNoteNumber(); finish(pitch, time);
                    starts[static_cast<size_t>(pitch)] = time; velocities[static_cast<size_t>(pitch)] = message.getVelocity();
                }
                else if (message.isNoteOff()) finish(message.getNoteNumber(), time);
                else if (message.isAllNotesOff() || message.isAllSoundOff()) for (int pitch = 0; pitch < 128; ++pitch) finish(pitch, time);
            }
            for (int pitch = 0; pitch < 128; ++pitch) finish(pitch, juce::jmax(0.0, session->lastEnd));
            const juce::ScopedLock lock(modelLock);
            entry.track->notes.insert(entry.track->notes.end(), notes.begin(), notes.end());
        }
    }
    if (session->failed) errors.add(i18n::tr("El disco no siguió el ritmo de grabación; faltan bloques en la toma."));
    return errors.isEmpty() ? juce::Result::ok() : juce::Result::fail(errors.joinIntoString("\n"));
}
void AudioEngine::createDemo()
{
    clear();
    const juce::ScopedLock lock(modelLock);
    projectName = i18n::tr("Silente · First Light"); tempo = 110.0;
    const double beat = 60.0 / tempo.load(), length = beat * 16.0, rate = 48000.0;
    auto drums = makeTrack(i18n::tr("01 · Pulso"), false);
    AudioClip rhythm; rhythm.name = i18n::tr("Pulso original · 4 compases"); rhythm.sourceRate = rate; rhythm.durationSeconds = length;
    rhythm.audio = std::make_shared<juce::AudioBuffer<float>>(2, static_cast<int>(std::ceil(length * rate))); rhythm.audio->clear();
    juce::Random random(49127);
    for (int i = 0; i < rhythm.audio->getNumSamples(); ++i)
    {
        const double time = i / rate, inBeat = std::fmod(time, beat), half = std::fmod(time, beat * 0.5);
        const int beatNumber = static_cast<int>(time / beat);
        const double kick = 0.55 * std::sin(twoPi * (48.0 * inBeat + 1.8 * (1.0 - std::exp(-inBeat * 38.0)))) * std::exp(-inBeat * 13.0);
        const double noise = random.nextFloat() * 2.0 - 1.0;
        const double snare = (beatNumber % 2 == 1 ? 0.22 * noise * std::exp(-inBeat * 24.0) : 0.0);
        const double hat = 0.09 * noise * std::exp(-half * 95.0);
        rhythm.audio->setSample(0, i, static_cast<float>(kick + snare + hat));
        rhythm.audio->setSample(1, i, static_cast<float>(kick + snare + hat * 0.8));
    }
    drums->clips.push_back(rhythm); drums->gain = 0.75f; tracks.push_back(drums);
    auto bass = makeTrack(i18n::tr("02 · Bajo cálido"), true); bass->gain = 0.68f;
    const int roots[] { 45, 41, 48, 43 };
    for (int bar = 0; bar < 4; ++bar) for (int b = 0; b < 4; ++b)
    { MidiNote n; n.pitch = roots[bar]; n.velocity = b == 0 ? 112 : 85; n.startBeats = bar * 4.0 + b; n.lengthBeats = 0.8; bass->notes.push_back(n); }
    tracks.push_back(bass);
    auto keys = makeTrack(i18n::tr("03 · Cristal"), true); keys->gain = 0.55f; keys->pan = 0.12f;
    const int chords[4][3] { {57,60,64}, {53,57,60}, {60,64,67}, {55,59,62} };
    for (int bar = 0; bar < 4; ++bar) for (int b = 0; b < 8; ++b)
    { MidiNote n; n.pitch = chords[bar][b % 3] + 12; n.velocity = 70 + (b % 3) * 8; n.startBeats = bar * 4.0 + b * 0.5; n.lengthBeats = 0.38; keys->notes.push_back(n); }
    tracks.push_back(keys);
    auto pad = makeTrack(i18n::tr("04 · Horizonte"), true); pad->gain = 0.34f; pad->pan = -0.16f;
    for (int bar = 0; bar < 4; ++bar) for (int voice = 0; voice < 3; ++voice)
    { MidiNote n; n.pitch = chords[bar][voice]; n.velocity = 62; n.startBeats = bar * 4.0; n.lengthBeats = 3.8; pad->notes.push_back(n); }
    tracks.push_back(pad);
    loopStart = 0.0; loopEnd = length; looping = true;
}
}

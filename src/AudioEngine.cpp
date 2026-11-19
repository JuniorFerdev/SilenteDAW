#include "i18n/Translations.h"
#include "AudioEngine.h"
#include <map>
#include "plugins/PluginSelection.h"
#include "persistence/TrackDataJson.h"
#include "persistence/TempoMapJson.h"
#include "persistence/AutomationJson.h"
#include "audio/BandlimitedInterpolator.h"
#include "audio/RecordingModes.h"
#include "audio/FadeCurves.h"
#include "audio/AutomationRamp.h"
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
    // Folder (cached index re-validated by id every block) and the mute/solo it passes down.
    int parentIndex = -1;
    bool inheritedMute = false, inheritedSolo = false;
    std::array<int, Track::sendCount> sendIndex;
    bool destinationSoloed = false;
    // Per block: index of the clip heard at each sample (later clips cover earlier ones).
    std::vector<int> owner;
    // Automation: fader, pan and send values applied at the end of the last segment (NaN until
    // the first segment after prepareTrack). See audio::automationRamp.
    float lastGain = std::numeric_limits<float>::quiet_NaN(), lastPan = std::numeric_limits<float>::quiet_NaN();
    std::array<float, Track::sendCount> lastSendDb;
    Runtime()
    {
        sendIndex.fill(-1);
        lastSendDb.fill(std::numeric_limits<float>::quiet_NaN());
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
    // Musical position of the block being processed. Written by the audio thread in renderSegment
    // (under modelLock) before the plugins run; getPosition() only copies these values.
    double blockPosition = 0.0, bpm = 120.0, ppq = 0.0, barStartPpq = 0.0, loopStartPpq = 0.0, loopEndPpq = 0.0;
    int numerator = 4, denominator = 4;
    explicit EnginePlayHead(AudioEngine& e) : engine(e) {}
    void update(double position, const domain::TempoView& view) noexcept
    {
        blockPosition = position;
        ppq = view.beatsAt(position);
        bpm = view.bpmAt(ppq);
        loopStartPpq = view.beatsAt(engine.loopStart.load()); loopEndPpq = view.beatsAt(engine.loopEnd.load());
        if (view.isConstant()) { barStartPpq = std::floor(position * view.bpm / 240.0) * 4.0; numerator = denominator = 4; return; }
        const auto bar = view.barAt(ppq);
        barStartPpq = ppq - bar.beatInBar; numerator = bar.signature.numerator; denominator = bar.signature.denominator;
    }
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setTimeInSeconds(blockPosition);
        info.setTimeInSamples(static_cast<juce::int64>(blockPosition * engine.sampleRate.load()));
        info.setBpm(bpm);
        info.setTimeSignature(TimeSignature { numerator, denominator });
        info.setPpqPosition(ppq);
        info.setPpqPositionOfLastBarStart(barStartPpq);
        info.setIsPlaying(engine.playing.load());
        info.setIsRecording(engine.recording.load());
        info.setIsLooping(engine.looping.load());
        info.setLoopPoints(LoopPoints { loopStartPpq, loopEndPpq });
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
        // Live waveform: preallocated min/max pairs filled by the audio thread; `count` is
        // published after the pair is written, so the message thread reads only finished pairs.
        struct Overview { std::vector<float> minMax; std::atomic<int> count { 0 }; float lo = 0.0f, hi = 0.0f; int filled = 0; };
        std::unique_ptr<Overview> overview;
    };
    int samplesPerPeak = 480;
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
    activeMarkerTrack.clear();
    projectName = i18n::tr("Sin título");
    projectFile = juce::File();
    lastWarning.clear();
    tempo = 120.0;
    tempoMap = {};
    masterGain = 0.8f;
    looping = false;
    punchIn = false; punchOut = false; audioRecordMode = 1; recordBitDepth = 24;
    metronome = false;
    loopStart = 0.0;
    loopEnd = 8.0;
}
juce::String trackKindId(TrackKind kind)
{
    switch(kind){case TrackKind::instrument:return "instrument";case TrackKind::midi:return "midi";case TrackKind::folder:return "folder";case TrackKind::fx:return "fx";case TrackKind::group:return "group";case TrackKind::ruler:return "ruler";case TrackKind::marker:return "marker";case TrackKind::video:return "video";default:return "audio";}
}
TrackKind trackKindFromId(const juce::String& id, bool legacyIsMidi)
{
    for(auto kind:{TrackKind::audio,TrackKind::instrument,TrackKind::midi,TrackKind::folder,TrackKind::fx,TrackKind::group,TrackKind::ruler,TrackKind::marker,TrackKind::video})if(id==trackKindId(kind))return kind;
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
    switch(kind){case TrackKind::instrument:base=i18n::tr("Instrumento ");break;case TrackKind::midi:base=i18n::tr("MIDI ");break;case TrackKind::folder:base=i18n::tr("Carpeta ");break;case TrackKind::fx:base=i18n::tr("FX ");break;case TrackKind::group:base=i18n::tr("Grupo ");break;case TrackKind::ruler:base=i18n::tr("Regla ");break;case TrackKind::marker:base=i18n::tr("Marcador ");break;case TrackKind::video:base=i18n::tr("Video ");break;default:base=i18n::tr("Audio ");}
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
    auto t = makeTrack(name, kind); if (t->isMidi) t->monitor = true; tracks.push_back(t);if(kind==TrackKind::marker)activeMarkerTrack=t->id;return t;
}
std::shared_ptr<Track> AudioEngine::findTrack(const juce::String& id) const
{
    for (const auto& t : tracks) if (t->id == id) return t;
    return {};
}
bool AudioEngine::canRoute(const Track& source, const Track& target, bool asSend) const
{
    if (&source == &target || !source.hasAudioChannel()) return false;
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
    if (juce::isPositiveAndBelow(index, static_cast<int>(tracks.size()))) {
        const auto removed = tracks[(size_t) index];
        for (auto& track : tracks) if (track->data.parentFolder == removed->id) track->data.parentFolder = removed->data.parentFolder;
        tracks.erase(tracks.begin() + index);if(!findTrack(activeMarkerTrack)){activeMarkerTrack.clear();for(const auto& track:tracks)if(track->kind==TrackKind::marker){activeMarkerTrack=track->id;break;}}}
}
domain::TrackOrder trackOrderOf(const std::vector<std::shared_ptr<Track>>& tracks)
{
    domain::TrackOrder order;
    order.reserve(tracks.size());
    for (const auto& t : tracks) order.push_back({ t->id, t->data.parentFolder, t->kind == TrackKind::folder, t->data.folderCollapsed });
    return order;
}

domain::TrackOrder AudioEngine::trackOrder() const
{
    return trackOrderOf(tracks);
}

juce::Result AudioEngine::applyTrackOrder(const domain::TrackOrder& requested)
{
    if (recording.load()) return juce::Result::fail(i18n::tr("Detén la grabación antes de reordenar pistas."));
    const juce::ScopedLock lock(modelLock);
    if (requested.size() != tracks.size()) return juce::Result::fail(i18n::tr("El orden de pistas no es válido."));
    std::vector<std::shared_ptr<Track>> next;
    next.reserve(tracks.size());
    for (const auto& node : requested)
    {
        auto it = std::find_if(tracks.begin(), tracks.end(), [&](const auto& t) { return t->id == node.id; });
        if (it == tracks.end() || std::find(next.begin(), next.end(), *it) != next.end()) return juce::Result::fail(i18n::tr("El orden de pistas no es válido."));
        next.push_back(*it);
    }
    // Folder kinds come from the tracks, not from the request.
    auto order = requested;
    for (size_t k = 0; k < order.size(); ++k) order[k].folder = next[k]->kind == TrackKind::folder;
    order = domain::normalised(std::move(order));
    std::vector<std::shared_ptr<Track>> sorted;
    sorted.reserve(next.size());
    for (const auto& node : order)
    {
        auto t = *std::find_if(next.begin(), next.end(), [&](const auto& candidate) { return candidate->id == node.id; });
        t->data.parentFolder = node.parent; t->data.folderCollapsed = node.collapsed;
        sorted.push_back(std::move(t));
    }
    tracks = std::move(sorted); // the old vector is freed here, under the lock but on the message thread
    return juce::Result::ok();
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
    rt.lastGain = rt.lastPan = std::numeric_limits<float>::quiet_NaN();
    rt.lastSendDb.fill(std::numeric_limits<float>::quiet_NaN());
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
    const auto started=juce::Time::getHighResolutionTicks();
    render(outputs,outChannels,samples,physicalInputs,DeviceSettings::channelLimit);
    const double seconds=juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks()-started);
    const float load=(float)(seconds*sampleRate.load()/juce::jmax(1,samples));
    dspLoadAverage=dspLoadAverage.load()*0.98f+load*0.02f;dspLoadPeak=juce::jmax(load,dspLoadPeak.load()*0.995f);
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
        const auto timeline = tempoView();
        const double beat = timeline.beatsAt(cursor);
        const auto signature = timeline.isConstant() ? domain::SignatureEvent {} : timeline.barAt(beat).signature;
        click.render(mixBuffer, 0, processed, elapsed, timeline.isConstant() ? timeline.bpm : timeline.bpmAt(beat), juce::jmax(1, juce::roundToInt(signature.barBeats())));
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
    const double rate = sampleRate.load();
    const auto timeline = tempoView(); // under modelLock (render holds the try-lock)
    const double end = start + samples / rate;
    const bool hasSolo = std::any_of(tracks.begin(), tracks.end(), [](const auto& t) { return (t->hasAudioChannel()||t->kind==TrackKind::folder)&&t->solo.load(); });
    int maximumLatency = 0;
    for (const auto& track : tracks)
    {
        int latency = 0;
        for (const auto& slot : track->plugins)
            if (slot->processor && !slot->bypassed.load()) latency += juce::jmax(0, slot->processor->getLatencySamples());
        maximumLatency = juce::jmax(maximumLatency, latency);
    }
    playHead->update(start, timeline);
    // Automation is evaluated at the segment edges (beats follow the tempo map).
    const double automationFrom = timeline.beatsAt(start), automationTo = transportPlaying ? timeline.beatsAt(end) : automationFrom;
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
        const bool folder = !t.hasAudioChannel();
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
    // Folders: mute and solo of a folder apply to everything inside it (Cubase folder tracks).
    // Pre-order list: an enclosing folder comes first, so its inherited state is already known.
    for (int n = 0; n < count; ++n)
    {
        auto& t = *tracks[static_cast<size_t>(n)];
        auto& rt = *t.runtime;
        rt.parentIndex = resolve(rt.parentIndex, t.data.parentFolder, TrackKind::folder, n);
        rt.inheritedMute = rt.inheritedSolo = false;
        if (rt.parentIndex >= 0 && rt.parentIndex < n)
        {
            const auto& folder = *tracks[static_cast<size_t>(rt.parentIndex)];
            rt.inheritedMute = folder.mute.load() || folder.runtime->inheritedMute;
            rt.inheritedSolo = folder.solo.load() || folder.runtime->inheritedSolo;
        }
    }
    const auto soloed = [&](int index) { const auto& d = *tracks[static_cast<size_t>(index)]; return d.solo.load() || d.runtime->inheritedSolo; };
    for (const auto& t : tracks)
    {
        // A source stays audible when a channel it feeds is soloed (upstream solo).
        auto& rt = *t->runtime;
        rt.destinationSoloed = rt.outputIndex >= 0 && soloed(rt.outputIndex);
        for (int destination : rt.sendIndex) if (destination >= 0 && soloed(destination)) rt.destinationSoloed = true;
    }
    // Pro Tools: while recording, an armed track is heard through its input only inside the punch window.
    // Before and after the punch it plays its own material alone; the take never records monitored sound.
    const bool recordingBlock = recording.load() && recordingSession && transportPlaying;
    double monitorFrom = 0.0, monitorTo = 0.0;
    if (recordingBlock)
    {
        monitorFrom = recordingSession->captureFrom;
        monitorTo = std::numeric_limits<double>::max();
        if (punchIn.load()) monitorFrom = juce::jmax(monitorFrom, loopStart.load());
        if (punchOut.load()) monitorTo = loopEnd.load();
    }
    const auto inputMonitored = [&](const std::shared_ptr<Track>& track)
    {
        if (! recordingBlock || ! track->armed.load() || end <= monitorFrom || start >= monitorTo) return false;
        for (const auto& entry : recordingSession->entries)
            if (entry.track == track) return true;
        return false;
    };
    const auto processTrack = [&](const std::shared_ptr<Track>& track)
    {
        auto& rt = *track->runtime;
        rt.buffer.clear();
        rt.midi.clear();
        if (!track->hasAudioChannel()) { track->peakL = track->peakR = 0.0f; return; }
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
                    if (clip.fadeIn > 0.0) fade *= audio::fadeGain(clip.fadeInShape, local / clip.fadeIn);
                    if (clip.fadeOut > 0.0) fade *= audio::fadeGain(clip.fadeOutShape, (clip.durationSeconds - local) / clip.fadeOut);
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
                const double on = timeline.secondsAt(note.startBeats), off = timeline.secondsAt(note.startBeats + note.lengthBeats);
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
        if (track->kind == TrackKind::audio && (track->monitor.load() || inputMonitored(track)) && input && inputChannels > 0)
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
        const bool audible = !track->mute.load() && !rt.inheritedMute
                             && (!hasSolo || track->solo.load() || rt.inheritedSolo || track->isBus() || rt.destinationSoloed);
        // Fader and pan: the static value, or the curve in Read mode, ramped across the segment.
        const auto& automation = track->automation;
        const bool suspendRead = automationSuspendRead.load();
        using domain::AutomationParameter;
        const auto volumeRamp = audio::automationRamp(rt.lastGain, automation.volume, (! suspendRead && automation.reads({ AutomationParameter::volume })),
                                                      automationFrom, automationTo, juce::jlimit(0.0f, 4.0f, finiteGain(track->gain.load())));
        const auto panRamp = audio::automationRamp(rt.lastPan, automation.pan, (! suspendRead && automation.reads({ AutomationParameter::pan })),
                                                   automationFrom, automationTo, juce::jlimit(-1.0f, 1.0f, finiteGain(track->pan.load())));
        // Equal-power pan, -3 dB at centre. Stereo channels are balanced, not summed.
        const auto channelGains = [](float g, float p, float* out)
        {
            out[0] = g * std::cos((p + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
            out[1] = g * std::sin((p + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
        };
        float gains[2] {}, gainStep[2] {}, gainsFrom[2] {};
        channelGains(audible ? volumeRamp.to : 0.0f, panRamp.to, gains);
        const bool faderRamp = ! volumeRamp.constant() || ! panRamp.constant();
        if (faderRamp)
        {
            channelGains(audible ? volumeRamp.from : 0.0f, panRamp.from, gainsFrom);
            for (int ch = 0; ch < 2; ++ch) gainStep[ch] = (gains[ch] - gainsFrom[ch]) / static_cast<float>(samples);
        }
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
        float sendGain[Track::sendCount] {}, sendFrom[Track::sendCount] {}, sendStep[Track::sendCount] {};
        bool sendPre[Track::sendCount] {}, sendRamp[Track::sendCount] {};
        int activeSends[Track::sendCount] {}, activeSendCount = 0;
        for (int k = 0; k < Track::sendCount; ++k)
        {
            const auto& send = track->sends[static_cast<size_t>(k)];
            sendPre[k] = send.preFader.load();
            const float level = juce::jlimit(-60.0f, 6.0f, finiteGain(send.levelDb.load()));
            const auto ramp = audio::automationRamp(rt.lastSendDb[static_cast<size_t>(k)], automation.sends[static_cast<size_t>(k)],
                                                    (! suspendRead && automation.reads({ AutomationParameter::send, k })), automationFrom, automationTo, level);
            const auto toGain = [&](float db) { return rt.sendIndex[static_cast<size_t>(k)] < 0 || db <= -60.0f ? 0.0f : juce::Decibels::decibelsToGain(db) * (sendPre[k] && ! audible ? 0.0f : 1.0f); };
            sendGain[k] = toGain(ramp.to);
            sendFrom[k] = ramp.constant() ? sendGain[k] : toGain(ramp.from);
            sendRamp[k] = sendFrom[k] != sendGain[k];
            if (sendRamp[k]) sendStep[k] = (sendGain[k] - sendFrom[k]) / static_cast<float>(samples);
            if (sendGain[k] > 0.0f || sendFrom[k] > 0.0f) activeSends[activeSendCount++] = k;
        }
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto* src = rt.buffer.getReadPointer(ch);
            auto* dst = rt.outputIndex >= 0 ? tracks[static_cast<size_t>(rt.outputIndex)]->runtime->bus.getWritePointer(ch)
                                            : mixBuffer.getWritePointer(ch, offset);
            float* sendDst[Track::sendCount] {};
            for (int n = 0; n < activeSendCount; ++n)
            {
                const int k = activeSends[n];
                sendDst[k] = tracks[static_cast<size_t>(rt.sendIndex[static_cast<size_t>(k)])]->runtime->bus.getWritePointer(ch);
            }
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
                const float delayed = preFader * (faderRamp ? gainsFrom[ch] + gainStep[ch] * static_cast<float>(i) : gains[ch]);
                dst[i] += delayed;
                for (int n = 0; n < activeSendCount; ++n)
                {
                    const int k = activeSends[n];
                    sendDst[k][i] += (sendPre[k] ? preFader : delayed) * (sendRamp[k] ? sendFrom[k] + sendStep[k] * static_cast<float>(i) : sendGain[k]);
                }
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
            recordLapStart = session.laps[(size_t) (session.lapCount - 1)].start;
            recordLapOffset = session.laps[(size_t) (session.lapCount - 1)].fileOffset;
            recordCaptured = session.captured;
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
                    if (auto* view = entry.overview.get())
                        for (int i = 0; i < written; ++i)
                        {
                            const float l = channels[0][i], r = channels[1][i];
                            view->lo = juce::jmin(view->lo, l, r); view->hi = juce::jmax(view->hi, l, r);
                            if (++view->filled < session.samplesPerPeak) continue;
                            const int n = view->count.load(std::memory_order_relaxed);
                            if (2 * n + 1 < static_cast<int>(view->minMax.size()))
                            {
                                view->minMax[(size_t) (2 * n)] = view->lo; view->minMax[(size_t) (2 * n + 1)] = view->hi;
                                view->count.store(n + 1, std::memory_order_release);
                            }
                            view->lo = view->hi = 0.0f; view->filled = 0;
                        }
                }
                else if (projectSample < std::numeric_limits<int>::max() - samples)
                    entry.midi.addEvents(liveMidi, offset + a, written, static_cast<int>(projectSample) - offset);
            }
        }
        else if (session.captured > 0 && ! looping.load() && end >= to) punchedOut = true;
    }
    if (metronome.load() && transportPlaying && (! click.onlyWhenRecording.load() || recording.load()))
        click.render(mixBuffer, offset, samples, start, timeline);
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
std::vector<domain::MediaClipRef> AudioEngine::mediaClips() const
{
    std::vector<domain::MediaClipRef> refs;
    const juce::ScopedLock lock(modelLock);
    for (const auto& track : tracks)
        for (const auto& clip : track->clips)
        {
            domain::MediaClipRef ref;
            ref.trackId = track->id; ref.clipId = clip.id; ref.path = clip.file.getFullPathName();
            ref.name = clip.file != juce::File() ? clip.file.getFileName() : clip.name;
            if (clip.audio) { ref.lengthSamples = clip.audio->getNumSamples(); ref.channels = clip.audio->getNumChannels(); }
            ref.sampleRate = clip.sourceRate;
            refs.push_back(std::move(ref));
        }
    return refs;
}
juce::Result AudioEngine::relinkMedia(const juce::String& fromPath, const juce::File& newFile, int* changed)
{
    if (changed) *changed = 0;
    if (fromPath.isEmpty()) return juce::Result::fail(i18n::tr("Este medio no tiene ruta de archivo."));
    AudioClip loaded;
    if (auto result = readAudio(newFile, loaded); result.failed()) return result;
    const juce::ScopedLock lock(modelLock);
    int count = 0;
    for (auto& track : tracks)
        for (auto& clip : track->clips)
            if (clip.file.getFullPathName() == fromPath)
            { clip.file = newFile; clip.audio = loaded.audio; clip.sourceRate = loaded.sourceRate; ++count; }
    if (changed) *changed = count;
    return count > 0 ? juce::Result::ok() : juce::Result::fail(i18n::tr("Ningún clip usa este medio."));
}
juce::Result AudioEngine::importMidi(const juce::File& file)
{
    auto stream = file.createInputStream();
    if (!stream) return juce::Result::fail(i18n::tr("No se puede abrir el archivo MIDI."));
    juce::MidiFile midi;
    if (!midi.readFrom(*stream)) return juce::Result::fail(i18n::tr("Archivo MIDI no válido."));
    // Tempo and time signature meta events of every track, in ticks.
    const int ppq = midi.getTimeFormat();
    std::vector<std::pair<double, double>> tempoTicks;
    std::vector<std::pair<double, std::pair<int, int>>> signatureTicks;
    for (int t = 0; t < midi.getNumTracks(); ++t)
        for (int e = 0; e < midi.getTrack(t)->getNumEvents(); ++e)
        {
            const auto& message = midi.getTrack(t)->getEventPointer(e)->message;
            if (message.isTempoMetaEvent() && message.getTempoSecondsPerQuarterNote() > 0.0)
                tempoTicks.push_back({ message.getTimeStamp(), 60.0 / message.getTempoSecondsPerQuarterNote() });
            if (message.isTimeSignatureMetaEvent())
            { int numerator = 4, denominator = 4; message.getTimeSignatureInfo(numerator, denominator); signatureTicks.push_back({ message.getTimeStamp(), { numerator, denominator } }); }
        }
    std::stable_sort(tempoTicks.begin(), tempoTicks.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
    std::stable_sort(signatureTicks.begin(), signatureTicks.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
    // An empty project adopts the file's tempo map (notes stay in musical time, ticks / PPQ).
    // Otherwise the notes keep their absolute times, placed with the project's map.
    juce::MidiFile timed(midi);
    timed.convertTimestampTicksToSeconds();
    const juce::ScopedLock lock(modelLock);
    const bool adoptMap = tracks.empty() && ppq > 0;
    if (tracks.empty())
    {
        // SMF: 120 BPM until the first tempo event.
        double base = 120.0;
        domain::TempoMap map;
        for (const auto& [tick, bpm] : tempoTicks)
            if (tick <= 0.0 || ppq <= 0) base = bpm;
            else map.tempos.push_back({ tick / ppq, bpm, false });
        if (ppq <= 0 && ! tempoTicks.empty()) { base = tempoTicks.front().second; map.tempos.clear(); }
        if (ppq > 0)
            for (const auto& [tick, signature] : signatureTicks)
            {
                if (! domain::validDenominator(signature.second)) continue;
                // A change belongs to the bar that starts at (or right after) its position.
                const domain::TempoView view { base, &map };
                const auto at = view.barAt(tick / ppq);
                const int bar = at.bar + (at.beatInBar > 1.0e-6 ? 1 : 0);
                const auto current = view.signatureAtBar(bar);
                if (current.numerator == signature.first && current.denominator == signature.second) continue; // already in force
                map.signatures.push_back({ bar, signature.first, signature.second });
                map.normalise();
            }
        map.normalise();
        tempo = juce::jlimit(domain::minimumBpm, domain::maximumBpm, base);
        tempoMap = std::move(map);
    }
    const auto timeline = tempoView();
    int imported = 0;
    for (int t = 0; t < midi.getNumTracks(); ++t)
    {
        juce::MidiMessageSequence sequence(adoptMap ? *midi.getTrack(t) : *timed.getTrack(t));
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
                const double on = event->message.getTimeStamp(), off = event->noteOffObject->message.getTimeStamp();
                note.startBeats = juce::jmax(0.0, adoptMap ? on / ppq : timeline.beatsAt(on));
                note.lengthBeats = juce::jmax(0.001, adoptMap ? (off - on) / ppq : timeline.beatsBetween(on, off));
                track->notes.push_back(note);
            }
        }
        if (!track->notes.empty()) { tracks.push_back(track); ++imported; }
    }
    if (imported == 0) return juce::Result::fail(i18n::tr("El archivo MIDI no contiene notas con inicio y final."));
    lastWarning = adoptMap ? i18n::tr("MIDI importado como notas con su mapa de tempo y de compás. CC, SysEx y cambios de programa no se conservan.")
                           : i18n::tr("MIDI importado como notas; los cambios de tempo se convierten a tiempos absolutos. CC, SysEx y cambios de programa no se conservan.");
    return juce::Result::ok();
}
juce::Result AudioEngine::exportMidi(const juce::File& file, int trackIndex)
{
    juce::MidiFile midi;
    midi.setTicksPerQuarterNote(midiExportPpq);
    int written = 0;
    {
        const juce::ScopedLock lock(modelLock);
        // Conductor track: the tempo map (SMF has only tempo steps, so a ramp is written as one
        // step per sixteenth whose tempo keeps the exact elapsed time of that sixteenth) and the
        // time signature changes at their bar starts.
        const auto timeline = tempoView();
        std::map<juce::int64, double> steps { { 0, timeline.bpm } };
        double fromBeat = 0.0;
        for (const auto& e : tempoMap.tempos)
        {
            if (e.ramp)
            {
                const int count = juce::jmax(1, static_cast<int>(std::ceil((e.beat - fromBeat) * 4.0 - 1.0e-9)));
                const double step = (e.beat - fromBeat) / count;
                for (int i = 0; i < count; ++i)
                {
                    const double b0 = fromBeat + i * step, b1 = b0 + step;
                    steps[std::llround(b0 * midiExportPpq)] = 60.0 * step / (timeline.secondsAt(b1) - timeline.secondsAt(b0));
                }
            }
            steps[std::llround(e.beat * midiExportPpq)] = e.bpm;
            fromBeat = e.beat;
        }
        juce::MidiMessageSequence conductor;
        for (const auto& [tick, bpm] : steps)
            conductor.addEvent(juce::MidiMessage::tempoMetaEvent(juce::roundToInt(60000000.0 / juce::jlimit(domain::minimumBpm, domain::maximumBpm, bpm))), static_cast<double>(tick));
        if (tempoMap.signatures.empty() || tempoMap.signatures.front().bar > 0) conductor.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4, 4), 0.0);
        for (const auto& signature : tempoMap.signatures)
            conductor.addEvent(juce::MidiMessage::timeSignatureMetaEvent(signature.numerator, signature.denominator), std::round(timeline.barStart(signature.bar) * midiExportPpq));
        conductor.sort();
        conductor.addEvent(juce::MidiMessage::endOfTrack(), conductor.getEndTime());
        midi.addTrack(conductor);
        for (int t = 0; t < static_cast<int>(tracks.size()); ++t)
        {
            if (trackIndex >= 0 && t != trackIndex) continue;
            const auto& track = *tracks[static_cast<size_t>(t)];
            if (!track.isMidi || track.notes.empty()) continue;
            juce::MidiMessageSequence sequence;
            sequence.addEvent(juce::MidiMessage::textMetaEvent(3, track.name), 0.0);
            double last = 0.0;
            for (const auto& note : track.notes)
            {
                const double on = std::round(juce::jmax(0.0, note.startBeats) * midiExportPpq);
                const double off = on + juce::jmax(1.0, std::round(note.lengthBeats * midiExportPpq));
                const auto pitch = juce::jlimit(0, 127, note.pitch);
                sequence.addEvent(juce::MidiMessage::noteOn(1, pitch, static_cast<juce::uint8>(juce::jlimit(1, 127, note.velocity))), on);
                sequence.addEvent(juce::MidiMessage::noteOff(1, pitch, 0.0f), off);
                last = juce::jmax(last, off);
            }
            sequence.sort();
            sequence.addEvent(juce::MidiMessage::endOfTrack(), last);
            midi.addTrack(sequence);
            ++written;
        }
    }
    if (written == 0) return juce::Result::fail(i18n::tr("No hay notas MIDI para exportar."));
    juce::TemporaryFile temporary(file);
    {
        std::unique_ptr<juce::OutputStream> stream(temporary.getFile().createOutputStream());
        if (!stream) return juce::Result::fail(i18n::tr("No se puede escribir en la carpeta de destino."));
        if (!midi.writeTo(*stream, 1)) return juce::Result::fail(i18n::tr("La escritura del archivo falló. Comprueba el espacio libre."));
    }
    if (!temporary.overwriteTargetFileWithTemporary()) return juce::Result::fail(i18n::tr("No se pudo finalizar el archivo MIDI."));
    return juce::Result::ok();
}
double AudioEngine::duration() const
{
    const juce::ScopedLock lock(modelLock);
    double result = 0.0;
    for (const auto& track : tracks)
    {
        for (const auto& clip : track->clips) result = juce::jmax(result, clip.startSeconds + clip.durationSeconds);
        for (const auto& note : track->notes) result = juce::jmax(result, tempoView().secondsAt(note.startBeats + note.lengthBeats));
        for(const auto& marker:track->data.markers)result=juce::jmax(result,marker.cycle?marker.end:marker.position);
        for(const auto& event:track->data.videoEvents)result=juce::jmax(result,event.start+event.length);
    }
    return result;
}
juce::Result AudioEngine::addPlugin(int index, std::unique_ptr<juce::AudioPluginInstance> processor, const juce::PluginDescription& description, int replaceSlot, int insertPosition)
{
    if (!processor) return juce::Result::fail(i18n::tr("El plugin no se ha creado."));
    const juce::ScopedLock lock(modelLock);
    if (!juce::isPositiveAndBelow(index, static_cast<int>(tracks.size()))) return juce::Result::fail(i18n::tr("La pista ya no existe."));
    if (!tracks[(size_t)index]->hasAudioChannel()) return juce::Result::fail(i18n::tr("Esta pista no admite inserciones de audio."));
    auto& track=*tracks[(size_t)index];auto& existing=track.plugins;
    if(replaceSlot>=(int)existing.size())return juce::Result::fail(i18n::tr("La ranura ya no existe."));
    if(!description.isInstrument)
    {
        if(replaceSlot>=0)insertPosition=existing[(size_t)replaceSlot]->insertPosition;
        else if(insertPosition<0){insertPosition=0;while(effectSlotIndex(track,insertPosition)>=0)++insertPosition;}
        if(insertPosition<0||insertPosition>=Track::insertCount)return juce::Result::fail(i18n::tr("La pista admite diez efectos de inserción."));
        const int occupied=effectSlotIndex(track,insertPosition);
        if(occupied>=0&&occupied!=replaceSlot)return juce::Result::fail(i18n::tr("La ranura de inserción ya está ocupada."));
    }
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
    slot->processor = std::move(processor); slot->description = description;slot->insertPosition=description.isInstrument?-1:insertPosition;
    if(replaceSlot>=0)
    {
        if(slots[static_cast<size_t>(replaceSlot)]->processor)slots[static_cast<size_t>(replaceSlot)]->processor->setPlayHead(nullptr);
        slots[static_cast<size_t>(replaceSlot)]=slot;
    }
    else if(description.isInstrument)slots.insert(slots.begin(),slot);
    else slots.push_back(slot);
    std::stable_sort(slots.begin(),slots.end(),[](const auto& a,const auto& b){return (a->description.isInstrument?-1:a->insertPosition)<(b->description.isInstrument?-1:b->insertPosition);});
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
    set(root, "tempo", tempo.load()); set(root, "masterGain", masterGain.load());set(root,"activeMarkerTrack",activeMarkerTrack);
    set(root, "punchIn", punchIn.load()); set(root, "punchOut", punchOut.load());
    set(root, "audioRecordMode", audioRecordMode.load()); set(root, "recordBitDepth", recordBitDepth.load());
    set(root, "looping", looping.load()); set(root, "loopStart", loopStart.load()); set(root, "loopEnd", loopEnd.load());
    set(root, "positionSeconds", positionSeconds.load());
    if (! tempoMap.empty()) set(root, "tempoMap", persistence::writeTempoMap(tempoMap));
    const auto media = destination.getParentDirectory().getChildFile(destination.getFileNameWithoutExtension() + " Media");
    juce::Array<juce::var> trackArray;
    for (const auto& track : tracks)
    {
        auto t = object();
        set(t, "id", track->id); set(t, "name", track->name); set(t, "colour", track->colour.toString()); set(t, "stripWidth", track->stripWidth);
        set(t, "kind", trackKindId(track->kind)); set(t, "isMidi", track->isMidi); set(t, "gain", track->gain.load()); set(t, "pan", track->pan.load());
        set(t,"trackData",persistence::writeTrackData(track->data,destination.getParentDirectory()));
        if (! track->automation.empty()) set(t, "automation", persistence::writeAutomation(track->automation));
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
            if (clip.fadeInShape != 0) set(c, "fadeInShape", clip.fadeInShape); if (clip.fadeOutShape != 0) set(c, "fadeOutShape", clip.fadeOutShape);
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
            if(!slot->description.isInstrument)set(p,"insertPosition",slot->insertPosition);
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
        track->data=persistence::readTrackData(t.getProperty("trackData",{}),file.getParentDirectory());
        track->automation = persistence::readAutomation(t.getProperty("automation", {}));
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
            clip.fadeInShape = juce::jlimit(0, (int) audio::FadeShape::count - 1, static_cast<int>(c.getProperty("fadeInShape", 0)));
            clip.fadeOutShape = juce::jlimit(0, (int) audio::FadeShape::count - 1, static_cast<int>(c.getProperty("fadeOutShape", 0)));
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
            if(!slot->description.isInstrument)
            {
                slot->insertPosition=(int)p.getProperty("insertPosition",-1);
                if(slot->insertPosition<0||effectSlotIndex(*track,slot->insertPosition)>=0){slot->insertPosition=0;while(effectSlotIndex(*track,slot->insertPosition)>=0)++slot->insertPosition;}
            }
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
        std::stable_sort(track->plugins.begin(),track->plugins.end(),[](const auto& a,const auto& b){return (a->description.isInstrument?-1:a->insertPosition)<(b->description.isInstrument?-1:b->insertPosition);});
        loaded.push_back(track);
    }
    {   // Folder hierarchy repaired before it is used (unknown or cyclic parents go to top level).
        const auto order = domain::normalised(trackOrderOf(loaded));
        std::vector<std::shared_ptr<Track>> sorted;
        for (const auto& node : order)
            for (auto& track : loaded)
                if (track && track->id == node.id) { track->data.parentFolder = node.parent; track->data.folderCollapsed = node.collapsed; sorted.push_back(std::move(track)); break; }
        for (auto& track : loaded) if (track) { track->data.parentFolder.clear(); sorted.push_back(std::move(track)); } // duplicated ids
        loaded = std::move(sorted);
    }
    stop();
    const juce::ScopedLock lock(modelLock);
    tracks = std::move(loaded);
    activeMarkerTrack=json.getProperty("activeMarkerTrack",{}).toString();
    if(auto active=findTrack(activeMarkerTrack);!active||active->kind!=TrackKind::marker)
    {activeMarkerTrack.clear();for(const auto& track:tracks)if(track->kind==TrackKind::marker){activeMarkerTrack=track->id;break;}}
    punchIn = static_cast<bool>(json["punchIn"]); punchOut = static_cast<bool>(json["punchOut"]);
    audioRecordMode = juce::jlimit(0, 2, static_cast<int>(number(json, "audioRecordMode", 1.0)));
    { const int bits = static_cast<int>(number(json, "recordBitDepth", 24.0)); recordBitDepth = bits == 16 || bits == 32 ? bits : 24; }
    tempo = juce::jlimit(20.0, 400.0, number(json, "tempo", 120.0)); masterGain = static_cast<float>(juce::jlimit(0.0, 4.0, number(json, "masterGain", 0.8)));
    looping = static_cast<bool>(json["looping"]); loopStart = juce::jmax(0.0, number(json, "loopStart", 0.0)); loopEnd = juce::jmax(loopStart.load() + 0.001, number(json, "loopEnd", 8.0));
    positionSeconds = juce::jmax(0.0, number(json, "positionSeconds", 0.0));
    tempoMap = persistence::readTempoMap(json["tempoMap"]);
    projectName = json.getProperty("name", file.getFileNameWithoutExtension()).toString(); projectFile = file;
    lastWarning = warnings.joinIntoString("\n");
    for (auto& track : tracks) prepareTrack(*track);
    return juce::Result::ok();
}

juce::Result AudioEngine::renderMix(double rate, const std::function<bool(const juce::AudioBuffer<float>&, int, int)>& sink,
                                   const std::function<bool(double)>& progress, double rangeStart, double rangeEnd)
{
    if (recording.load()) return juce::Result::fail(i18n::tr("Detén la grabación antes de exportar."));
    const bool ranged = rangeEnd > rangeStart;
    if (ranged && rangeStart < 0.0) return juce::Result::fail(i18n::tr("El rango de localizadores no es válido."));
    const double length = ranged ? rangeEnd - rangeStart : duration();
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
        positionSeconds = ranged ? rangeStart : 0.0; panicRequested = true; chaseRequested = true; playing = true;
        juce::AudioBuffer<float> block(2, 1024);
        const int latency = pluginLatencySamples();
        const juce::int64 total = static_cast<juce::int64>(std::ceil((ranged ? length : length + 2.0) * rate)) + latency;
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
juce::Result AudioEngine::exportMix(const juce::File& file, double rate, int bitDepth, double rangeStart, double rangeEnd)
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
    auto result = renderMix(rate, [&writer](const juce::AudioBuffer<float>& block, int start, int count) { return writer->writeFromAudioSampleBuffer(block, start, count); }, {}, rangeStart, rangeEnd);
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
    session->samplesPerPeak = juce::jmax(16, (int) std::lround(session->rate / 100.0));
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
            entry.overview = std::make_unique<RecordingSession::Entry::Overview>();
            entry.overview->minMax.resize(2 * 2 * 3600 * 100); // two hours at one pair per 10 ms
        }
        else entry.midi.ensureSize(1024 * 1024);
        session->entries.push_back(std::move(entry));
    }
    recordingSession = std::move(session);
    // Count-in at the tempo and signature where the recording starts (whole quarter beats per bar).
    const auto timeline = tempoView();
    const double startBeat = timeline.beatsAt(positionSeconds.load());
    const int countInBeats = timeline.isConstant() ? 4 : juce::jmax(1, juce::roundToInt(timeline.barAt(startBeat).signature.barBeats()));
    const double countInBpm = timeline.isConstant() ? timeline.bpm : timeline.bpmAt(startBeat);
    const auto countIn = options.countIn ? static_cast<juce::int64>(std::llround(click.countInBars.load() * countInBeats * 60.0 / countInBpm * sampleRate.load())) : 0;
    countInLength = countIn; countInSamples = countIn;
    capturing = false; punchedOut = false; recordLapStart = -1.0; recordLapOffset = 0; recordCaptured = 0;
    recording = true; chaseRequested = true; playing = true;
    return juce::Result::ok();
}
juce::File AudioEngine::audioFolder() const
{
    return projectFile != juce::File() ? projectFile.getParentDirectory().getChildFile("Audio")
                                       : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Silente/Audio");
}
bool AudioEngine::recordingOverview(const Track& track, const float*& minMax, int& count, int& samplesPerPeak) const
{
    if (! recordingSession) return false;
    for (const auto& entry : recordingSession->entries)
        if (entry.track.get() == &track && entry.overview)
        {
            minMax = entry.overview->minMax.data();
            count = entry.overview->count.load(std::memory_order_acquire);
            samplesPerPeak = recordingSession->samplesPerPeak;
            return true;
        }
    return false;
}
void AudioEngine::cancelRecording()
{
    std::unique_ptr<RecordingSession> session;
    {
        const juce::ScopedLock lock(modelLock);
        recording = false; capturing = false; recordLapStart = -1.0;
        session = std::move(recordingSession);
    }
    if (session) for (auto& entry : session->entries) if (entry.writer) { entry.writer.reset(); entry.file.deleteFile(); }
}
juce::Result AudioEngine::endRecording()
{
    std::unique_ptr<RecordingSession> session;
    {
        const juce::ScopedLock lock(modelLock);
        recording = false; capturing = false; recordLapStart = -1.0;
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
                const domain::TempoView timeline { session->bpm, &tempoMap };
                note.startBeats = timeline.beatsAt(starts[static_cast<size_t>(pitch)]);
                note.lengthBeats = juce::jmax(0.001, timeline.beatsBetween(starts[static_cast<size_t>(pitch)], seconds));
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

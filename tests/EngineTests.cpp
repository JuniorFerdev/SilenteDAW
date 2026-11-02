#include "EngineTests.h"
#include "../src/audio/TapTempo.h"
#include "../src/audio/RecordingModes.h"
#include "../src/audio/EventEdit.h"
#include "../src/audio/FadeCurves.h"
#include "../src/audio/RangeEdit.h"
#include "../src/AudioEngine.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace silente
{
namespace
{
constexpr double rate = 48000.0;
constexpr int block = 128;

void require(bool condition, const juce::String& message)
{
    if (! condition) throw std::runtime_error(message.toStdString());
}

void requireOk(const juce::Result& result)
{
    require(result.wasOk(), result.getErrorMessage());
}

void near(double actual, double expected, double tolerance, const juce::String& label)
{
    require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance,
            label + ": actual=" + juce::String(actual, 8) + ", expected="
            + juce::String(expected, 8) + ", tolerance=" + juce::String(tolerance, 8));
}

juce::File writeWav(const juce::File& dir, const juce::String& name, double sampleRate,
                    const juce::AudioBuffer<float>& data)
{
    auto file = dir.getChildFile(name);
    auto stream = file.createOutputStream();
    require(stream != nullptr && stream->openedOk(), "Cannot create fixture " + file.getFullPathName());
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(), sampleRate,
        static_cast<unsigned int>(data.getNumChannels()), 24, {}, 0));
    require(writer != nullptr, "Cannot create WAV writer");
    stream.release();
    require(writer->writeFromAudioSampleBuffer(data, 0, data.getNumSamples()), "Cannot write fixture samples");
    return file;
}

juce::AudioBuffer<float> render(AudioEngine& engine, int sampleCount, int blockSize = block)
{
    juce::AudioBuffer<float> result(2, sampleCount), temp(2, blockSize);
    result.clear();
    for (int done = 0; done < sampleCount; done += blockSize)
    {
        const int n = std::min(blockSize, sampleCount - done);
        temp.clear();
        engine.renderOfflineBlock(temp, n);
        for (int c = 0; c < 2; ++c) result.copyFrom(c, done, temp, c, 0, n);
    }
    return result;
}

double peak(const juce::AudioBuffer<float>& data, int channel = -1)
{
    double value = 0;
    for (int c = 0; c < data.getNumChannels(); ++c)
        if (channel < 0 || channel == c)
            for (int i = 0; i < data.getNumSamples(); ++i)
            {
                const auto sample = data.getSample(c, i);
                require(std::isfinite(sample), "Non-finite audio sample");
                value = std::max(value, static_cast<double>(std::abs(sample)));
            }
    return value;
}

void setup(AudioEngine& engine)
{
    engine.prepareForOffline(rate, block);
    engine.masterGain = 1.0f;
    engine.metronome = false;
}

std::shared_ptr<Track> importFixture(AudioEngine& engine, const juce::File& file, double start = 0.0)
{
    requireOk(engine.importAudio(file, -1, start));
    require(! engine.tracks.empty(), "Import produced no track");
    auto track = engine.tracks.back();
    track->gain = 1.0f;
    for (auto& clip : track->clips) { clip.fadeIn = 0; clip.fadeOut = 0; }
    return track;
}

// An intentionally simple host test double, not an external VST3/AU compatibility test.
// A real delayed processor verifies the engine's compensation rather than mirroring it.
class DelayTestPlugin final : public juce::AudioPluginInstance
{
public:
    explicit DelayTestPlugin(int delaySamples)
        : AudioPluginInstance(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                               .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          delay(delaySamples)
    { setLatencySamples(delay); }
    const juce::String getName() const override { return "Silente test delay"; }
    void fillInPluginDescription(juce::PluginDescription& d) const override
    {
        d.name = getName(); d.pluginFormatName = "Silente Test"; d.manufacturerName = "Silente tests";
        d.numInputChannels = 2; d.numOutputChannels = 2; d.isInstrument = false;
    }
    bool isBusesLayoutSupported(const BusesLayout& l) const override
    { return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void prepareToPlay(double, int) override { ring.setSize(2, delay); reset(); setLatencySamples(delay); }
    void releaseResources() override {}
    void reset() override { ring.clear(); cursor = 0; }
    using juce::AudioPluginInstance::processBlock;
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) override
    {
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                const auto input = audio.getSample(ch, i);
                audio.setSample(ch, i, ring.getSample(ch, cursor));
                ring.setSample(ch, cursor, input);
            }
            cursor = (cursor + 1) % delay;
        }
    }
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& state) override { state.reset(); }
    void setStateInformation(const void*, int) override {}
private:
    int delay, cursor = 0;
    juce::AudioBuffer<float> ring;
};

void addTestDelay(AudioEngine& engine, int trackIndex, int delaySamples)
{
    auto plugin = std::make_unique<DelayTestPlugin>(delaySamples);
    const auto description = plugin->getPluginDescription();
    requireOk(engine.addPlugin(trackIndex, std::move(plugin), description));
}

struct Suite
{
    juce::Array<juce::var> results;
    juce::String text;
    int failures = 0;
    void run(const juce::String& name, const std::function<void()>& test)
    {
        auto* item = new juce::DynamicObject();
        item->setProperty("name", name);
        const double started = juce::Time::getMillisecondCounterHiRes();
        juce::String error;
        try { test(); }
        catch (const std::exception& e) { error = e.what(); }
        catch (...) { error = "Unknown exception"; }
        const bool passed = error.isEmpty();
        if (! passed) ++failures;
        item->setProperty("passed", passed);
        item->setProperty("error", error);
        item->setProperty("milliseconds", juce::Time::getMillisecondCounterHiRes() - started);
        results.add(juce::var(item));
        const auto line = juce::String(passed ? "PASS " : "FAIL ") + name
            + (error.isEmpty() ? juce::String() : ": " + error);
        text += line + "\n";
        std::cout << line << std::endl;
    }
};
}

int runEngineTests(const juce::File& outputDir)
{
    const auto dir = outputDir.getNonexistentChildFile("engine-fixtures", {}, false);
    if (auto result = dir.createDirectory(); result.failed())
    {
        std::cerr << result.getErrorMessage() << std::endl;
        return 1;
    }
    Suite suite;
    juce::File mono, stereo, tone441, impulses;
    suite.run("create owned synthetic WAV fixtures", [&]
    {
        juce::AudioBuffer<float> data(1, 48000);
        for (int i = 0; i < data.getNumSamples(); ++i) data.setSample(0, i, 0.25f);
        mono = writeWav(dir, "mono.wav", rate, data);
        juce::AudioBuffer<float> two(2, 48000);
        for (int i = 0; i < two.getNumSamples(); ++i)
        { two.setSample(0, i, 0.125f); two.setSample(1, i, -0.25f); }
        stereo = writeWav(dir, "stereo.wav", rate, two);
        juce::AudioBuffer<float> tone(1, 44100);
        for (int i = 0; i < tone.getNumSamples(); ++i)
            tone.setSample(0, i, 0.2f * std::sin(static_cast<float>(juce::MathConstants<double>::twoPi * 440.0 * i / 44100.0)));
        tone441 = writeWav(dir, "tone-44100.wav", 44100, tone);
        data.clear();
        data.setSample(0, 10, 0.25f);
        data.setSample(0, 250, 0.5f);
        impulses = writeWav(dir, "impulses.wav", rate, data);
    });

    suite.run("mono import and hard-left gain", [&]
    {
        AudioEngine engine; setup(engine);
        auto track = importFixture(engine, mono);
        require(track->clips.size() == 1, "Expected one imported clip");
        near(track->clips[0].durationSeconds, 1.0, 1.0 / rate, "Clip duration");
        track->pan = -1.0f; track->gain = 0.5f;
        engine.play(); const auto audio = render(engine, 4096);
        near(audio.getSample(0, 2048), 0.125, 0.0002, "Left gain");
        near(peak(audio, 1), 0, 0.00001, "Right isolation");
    });

    suite.run("mono hard-right pan", [&]
    {
        AudioEngine engine; setup(engine);
        auto track = importFixture(engine, mono); track->pan = 1.0f;
        engine.play(); const auto audio = render(engine, 4096);
        near(peak(audio, 0), 0, 0.00001, "Left isolation");
        near(audio.getSample(1, 2048), 0.25, 0.0002, "Right pan");
    });

    suite.run("stereo channel separation and polarity", [&]
    {
        AudioEngine engine; setup(engine);
        auto track = importFixture(engine, stereo); track->pan = 0.0f;
        engine.play(); const auto audio = render(engine, 4096);
        require(audio.getSample(0, 2048) > 0.06f, "Missing positive left channel");
        require(audio.getSample(1, 2048) < -0.12f, "Missing negative right channel");
        near(audio.getSample(1, 2048) / audio.getSample(0, 2048), -2.0, 0.002, "Stereo amplitude ratio");
    });

    suite.run("group output, sends pre/post, solo through buses and loop refusal", [&]
    {
        AudioEngine engine; setup(engine);
        auto a = importFixture(engine, mono); a->pan = -1.0f;                     // 0.25 on the left
        auto b = importFixture(engine, mono); b->pan = -1.0f;
        auto group = engine.addTrack(TrackKind::group, "Bus"); group->pan = -1.0f; group->gain = 0.5f;
        requireOk(engine.setOutput(*a, group->id)); requireOk(engine.setOutput(*b, group->id));
        engine.play(); auto audio = render(engine, 2048);
        near(audio.getSample(0, 1024), 0.25, 0.0002, "Group sums its sources and applies its fader");
        near(audio.getSample(1, 1024), 0.0, 0.0002, "Group pan");
        b->solo = true; audio = render(engine, 2048);
        near(audio.getSample(0, 1024), 0.125, 0.0002, "Soloed source still reaches its group");
        b->solo = false; group->solo = true; audio = render(engine, 2048);
        near(audio.getSample(0, 1024), 0.25, 0.0002, "Soloed group keeps its sources playing");
        group->solo = false;

        auto fx = engine.addTrack(TrackKind::fx, "Reverb"); fx->pan = -1.0f; fx->gain = 1.0f;
        requireOk(engine.setOutput(*b, {})); b->mute = true;                    // b out of the way
        a->gain = 0.5f; requireOk(engine.setSend(*a, 0, fx->id));
        audio = render(engine, 2048);                                           // post-fader: 0.125 via group*0.5 + 0.125 via FX
        near(audio.getSample(0, 1024), 0.0625 + 0.125, 0.0002, "Post-fader send follows the fader");
        a->sends[0].preFader = true; a->gain = 0.0f; audio = render(engine, 2048);
        near(audio.getSample(0, 1024), 0.25, 0.0002, "Pre-fader send ignores the fader");
        a->mute = true; audio = render(engine, 2048);
        near(peak(audio), 0.0, 1.0e-6, "Pre-fader send follows mute");
        a->mute = false; a->sends[0].levelDb = -6.0f; audio = render(engine, 2048);
        near(audio.getSample(0, 1024), 0.25 * juce::Decibels::decibelsToGain(-6.0f), 0.0002, "Send level");
        a->sends[0].enabled = false; audio = render(engine, 2048);
        near(peak(audio), 0.0, 1.0e-6, "Disabled send");
        a->sends[0].enabled = true;

        auto second = engine.addTrack(TrackKind::group, "Bus 2");
        requireOk(engine.setOutput(*group, second->id));
        require(engine.setOutput(*second, group->id).failed(), "Group loop accepted");
        require(engine.setSend(*fx, 0, fx->id).failed(), "FX send to itself accepted");
        require(engine.setOutput(*a, fx->id).failed(), "Output to an FX channel accepted");

        const auto file = dir.getChildFile("routing.silente"); requireOk(engine.saveProject(file));
        AudioEngine restored; setup(restored); requireOk(restored.loadProject(file));
        require(restored.tracks[0]->output == group->id && restored.tracks[0]->sends[0].target == fx->id, "Routing not restored");
        require(restored.tracks[0]->sends[0].preFader.load() && std::abs(restored.tracks[0]->sends[0].levelDb.load() + 6.0f) < 1.0e-4f, "Send settings not restored");
    });

    suite.run("mute, solo and sum of independent tracks", [&]
    {
        AudioEngine engine; setup(engine);
        auto a = importFixture(engine, mono); a->pan = -1.0f;
        auto b = importFixture(engine, mono); b->pan = -1.0f; b->gain = 0.5f;
        engine.play(); auto audio = render(engine, 2048);
        near(audio.getSample(0, 1024), 0.375, 0.0002, "Track sum");
        a->mute = true; audio = render(engine, 2048);
        near(audio.getSample(0, 1024), 0.125, 0.0002, "Muted track excluded");
        a->mute = false; a->solo = true; audio = render(engine, 2048);
        near(audio.getSample(0, 1024), 0.25, 0.0002, "Solo excludes other track");
        a->solo = false; a->mute = true; b->mute = true;
        near(peak(render(engine, 2048)), 0, 0.00001, "All muted silence");
    });

    suite.run("44.1 kHz to 48 kHz preserves duration and pitch", [&]
    {
        AudioEngine engine; setup(engine);
        auto track = importFixture(engine, tone441); track->pan = -1.0f;
        near(track->clips[0].durationSeconds, 1.0, 1.0 / 44100.0, "Source duration");
        engine.play(); const auto audio = render(engine, 48000);
        int crossings = 0;
        for (int i = 1; i < 48000; ++i)
            if (audio.getSample(0, i - 1) <= 0 && audio.getSample(0, i) > 0) ++crossings;
        near(crossings, 440, 1, "Positive zero crossings");
        require(peak(audio) > 0.19, "Resampled signal unexpectedly attenuated");
        near(peak(render(engine, 2048)), 0, 0.00001, "Audio silent after source end");
    });

    suite.run("clip start and source offset across buffer boundaries", [&]
    {
        AudioEngine engine; setup(engine);
        auto track = importFixture(engine, impulses, 117.0 / rate); track->pan = -1.0f;
        auto& clip = track->clips[0]; clip.offsetSeconds = 10.0 / rate; clip.durationSeconds = 300.0 / rate;
        engine.play(); const auto audio = render(engine, 1024);
        near(audio.getSample(0, 117), 0.25, 0.0002, "First shifted impulse");
        near(audio.getSample(0, 357), 0.5, 0.0002, "Second shifted impulse");
        int active = 0;
        for (int i = 0; i < 1024; ++i) if (std::abs(audio.getSample(0, i)) > 0.0001) ++active;
        require(active == 2, "Expected exactly two nonzero samples, got " + juce::String(active));
    });

    suite.run("seek, pause and stop do not advance stopped transport", [&]
    {
        AudioEngine engine; setup(engine);
        auto track = importFixture(engine, mono); track->pan = -1.0f;
        engine.seek(0.25); engine.play(); render(engine, 480);
        near(engine.positionSeconds.load(), 0.26, 1.0 / rate, "Position after playback");
        engine.pause(); const auto paused = engine.positionSeconds.load();
        near(peak(render(engine, 1024)), 0, 0.00001, "Paused audio");
        near(engine.positionSeconds.load(), paused, 1.0 / rate, "Paused position");
        engine.seek(0.5); near(engine.positionSeconds.load(), 0.5, 1.0 / rate, "Seek position");
        engine.stop(); require(! engine.playing.load(), "Stop left transport playing");
        const auto stopped = engine.positionSeconds.load(); render(engine, 1024);
        near(engine.positionSeconds.load(), stopped, 1.0 / rate, "Stopped position");
    });

    suite.run("loop boundary keeps constant audio continuous", [&]
    {
        AudioEngine engine; setup(engine);
        auto track = importFixture(engine, mono); track->pan = -1.0f;
        engine.loopStart = 0.0; engine.loopEnd = 257.0 / rate; engine.looping = true;
        engine.play(); const auto audio = render(engine, 4096);
        for (int i = 0; i < audio.getNumSamples(); ++i)
            near(audio.getSample(0, i), 0.25, 0.0002, "Loop sample " + juce::String(i));
        near(engine.positionSeconds.load(), (4096 % 257) / rate, 2.0 / rate, "Wrapped position");
    });

    suite.run("MIDI synthesizer produces audio and stops without stuck notes", [&]
    {
        AudioEngine engine; setup(engine); auto track = engine.addMidiTrack("Test synth");
        track->gain = 1.0f;
        MidiNote note; note.pitch = 69; note.velocity = 100; note.startBeats = 0; note.lengthBeats = 4;
        track->notes.push_back(note);
        engine.play(); require(peak(render(engine, 8192)) > 0.001, "MIDI synth rendered silence");
        engine.stop(); render(engine, 48000);
        near(peak(render(engine, 4096)), 0, 0.00001, "Stuck note after stop");
    });

    suite.run("MIDI note within a block ends without hanging", [&]
    {
        AudioEngine engine; setup(engine); auto track = engine.addMidiTrack("Short note");
        MidiNote note; note.pitch = 72; note.startBeats = 0.003; note.lengthBeats = 0.03;
        track->notes.push_back(note); engine.play();
        require(peak(render(engine, 8192)) > 0.001, "Short MIDI note not scheduled");
        render(engine, 48000);
        near(peak(render(engine, 4096)), 0, 0.00001, "Short note failed to release");
    });

    // Sample indices where a click starts: signal after at least 20 ms of silence.
    const auto onsets = [](const juce::AudioBuffer<float>& audio)
    {
        std::vector<std::pair<int, float>> found; int quiet = 1 << 30;
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const float v = std::abs(audio.getSample(0, i));
            if (v > 0.003f) { if (quiet > (int) (rate * 0.02)) found.push_back({ i, 0.0f }); quiet = 0; if (! found.empty()) found.back().second = std::max(found.back().second, v); }
            else ++quiet;
        }
        return found;
    };

    suite.run("metronome clicks on every beat with an accented downbeat", [&]
    {
        AudioEngine engine; setup(engine); engine.tempo = 120.0; engine.metronome = true;
        engine.click.sound = (int) audio::ClickSound::classic; engine.click.levelDb = 0.0f; engine.click.subdivision = 1; engine.click.accent = true;
        engine.play(); const auto out = render(engine, (int) (rate * 2.0));
        const auto found = onsets(out);
        require(found.size() == 4, "Expected 4 clicks in 2 s at 120 BPM, got " + juce::String((int) found.size()));
        for (size_t k = 0; k < found.size(); ++k) near(found[k].first, k * rate * 0.5, 2.0, "Click " + juce::String((int) k) + " position");
        require(found[0].second > found[1].second * 1.3f, "Downbeat is not accented");
    });

    suite.run("metronome eighth-note subdivision and five sounds", [&]
    {
        for (int sound = 0; sound < audio::clickSoundCount; ++sound)
        {
            AudioEngine engine; setup(engine); engine.tempo = 120.0; engine.metronome = true;
            engine.click.sound = sound; engine.click.levelDb = 0.0f; engine.click.subdivision = 2;
            engine.play(); const auto found = onsets(render(engine, (int) (rate * 2.0)));
            if (found.size() != 8) for (auto& f : found) std::cout << "  sound " << sound << " onset " << f.first << " peak " << f.second << "\n";
            require(found.size() == 8, "Sound " + juce::String(sound) + ": expected 8 eighth clicks, got " + juce::String((int) found.size()));
            require(found[1].second < found[2].second, "Sound " + juce::String(sound) + ": subdivision not softer than the beat");
            require(found[0].second <= 1.0f, "Sound " + juce::String(sound) + " clips");
        }
    });

    suite.run("recording count-in holds the transport for one bar", [&]
    {
        AudioEngine engine; setup(engine); engine.tempo = 120.0; engine.metronome = false;
        engine.click.sound = (int) audio::ClickSound::classic; engine.click.countInBars = 1;
        engine.addMidiTrack("Count-in")->armed = true;
        requireOk(engine.beginRecording(dir.getChildFile("count-in")));
        const auto first = render(engine, (int) rate);
        near(engine.positionSeconds.load(), 0.0, 1.0e-9, "Transport moved during count-in");
        require(onsets(first).size() == 2, "Count-in clicks missing");
        render(engine, (int) (rate * 1.5));
        near(engine.positionSeconds.load(), 0.5, 2.0 / rate, "Transport did not start after the count-in");
        requireOk(engine.endRecording());
    });

    suite.run("metronome preview sounds while stopped", [&]
    {
        AudioEngine engine; setup(engine); engine.click.levelDb = 0.0f; engine.click.requestPreview();
        require(peak(render(engine, (int) (rate * 0.25))) > 0.1, "Preview click silent");
        near(peak(render(engine, (int) rate)), 0.0, 1.0e-6, "Preview click repeats");
    });

    suite.run("tap tempo median, missed beats, sloppy taps and pauses", [&]
    {
        audio::TapTempo taps; double t = 10.0;
        for (int i = 0; i < 8; ++i) { taps.tap(t); t += 0.5; }
        near(taps.bpm(), 120.0, 1.0e-6, "Steady taps");
        taps.tap(t + 0.12); t += 0.62;                 // one late tap
        for (int i = 0; i < 3; ++i) { t += 0.5; taps.tap(t); }
        near(taps.bpm(), 120.0, 0.5, "One sloppy tap moved the tempo");
        t += 1.0; taps.tap(t);                         // missed a beat
        near(taps.bpm(), 120.0, 0.5, "Missed beat counted as a slow interval");
        t += 3.0; taps.tap(t);                         // pause: new chain at 150 BPM
        for (int i = 0; i < 6; ++i) { t += 0.4; taps.tap(t); }
        near(taps.bpm(), 150.0, 1.0e-6, "Pause did not restart the measurement");
        taps.reset(); require(! taps.hasTempo() && taps.tapCount() == 0, "Reset");
    });

    suite.run("48 kHz clip on a 96 kHz device keeps treble and rejects images", [&]
    {
        constexpr double source = 48000.0, device = 96000.0, twoPi = juce::MathConstants<double>::twoPi;
        juce::AudioBuffer<float> tones(1, (int) source * 2);
        for (int i = 0; i < tones.getNumSamples(); ++i)
            tones.setSample(0, i, (float) (0.4 * std::sin(twoPi * 1000.0 * i / source) + 0.4 * std::sin(twoPi * 18000.0 * i / source)));
        const auto file = writeWav(dir, "tones-48k.wav", source, tones);
        AudioEngine engine; engine.prepareForOffline(device, block); engine.masterGain = 1.0f; engine.metronome = false;
        auto track = importFixture(engine, file); track->pan = -1.0f;
        engine.play(); render(engine, (int) device / 2);
        const auto out = render(engine, (int) device);
        const auto level = [&](double hz)
        {
            // Goertzel magnitude of a full-scale-normalised sinusoid over a Blackman-Harris window.
            double re = 0, im = 0, w = 0; const int n = out.getNumSamples();
            for (int i = 0; i < n; ++i)
            {
                const double u = (double) i / (n - 1), win = 0.35875 - 0.48829 * std::cos(twoPi * u) + 0.14128 * std::cos(2 * twoPi * u) - 0.01168 * std::cos(3 * twoPi * u);
                re += win * out.getSample(0, i) * std::cos(twoPi * hz * i / device); im += win * out.getSample(0, i) * std::sin(twoPi * hz * i / device); w += win;
            }
            return 2.0 * std::sqrt(re * re + im * im) / w;
        };
        const auto db = [](double x) { return juce::Decibels::gainToDecibels(x, -200.0); };
        const double low = level(1000), high = level(18000), image18 = level(30000), image1 = level(47000);
        std::cout << "  1 kHz " << db(low / 0.4) << " dB, 18 kHz " << db(high / 0.4) << " dB, images " << db(image18 / 0.4) << " / " << db(image1 / 0.4) << " dB\n";
        near(db(low / 0.4), 0.0, 0.05, "1 kHz level");
        near(db(high / 0.4), 0.0, 0.1, "18 kHz treble lost in resampling");
        require(db(image18 / 0.4) < -80.0 && db(image1 / 0.4) < -80.0, "Resampling images above -80 dB");
    });

    suite.run("empty start and six track kinds from the add button", [&]
    {
        AudioEngine engine; setup(engine); engine.clear();
        require(engine.tracks.empty(), "New session is not empty");
        const TrackKind kinds[] { TrackKind::audio, TrackKind::instrument, TrackKind::midi, TrackKind::folder, TrackKind::fx, TrackKind::group };
        for (auto kind : kinds) engine.addTrack(kind);
        require(engine.tracks.size() == 6, "Add button kinds not created");
        for (size_t i = 0; i < 6; ++i) require(engine.tracks[i]->kind == kinds[i], "Wrong kind " + juce::String((int)i));
        require(engine.tracks[1]->isMidi && engine.tracks[2]->isMidi && !engine.tracks[4]->isMidi, "MIDI flag mismatch");
        require(!engine.tracks[3]->hasEvents() && !engine.tracks[4]->hasEvents() && !engine.tracks[5]->hasEvents(), "Bus/folder accepts events");
        // Audio dropped on a folder goes to a new audio track, never into the folder.
        requireOk(engine.importAudio(mono, 3, 0.0)); require(engine.tracks.size() == 7 && engine.tracks.back()->kind == TrackKind::audio && engine.tracks[3]->clips.empty(), "Folder received audio");
        engine.tracks[3]->armed = true; engine.tracks[4]->armed = true;
        engine.play(); render(engine, 4096);
        near(engine.tracks[3]->peakL.load(), 0, 0.0, "Folder carries signal");
        const auto file = dir.getChildFile("kinds.silente"); requireOk(engine.saveProject(file));
        AudioEngine restored; setup(restored); requireOk(restored.loadProject(file));
        require(restored.tracks.size() == 7, "Kinds roundtrip changed track count");
        for (size_t i = 0; i < 6; ++i) require(restored.tracks[i]->kind == kinds[i], "Kind not restored " + juce::String((int)i));
    });

    suite.run("play selection stops exactly at the end of the range", [&]
    {
        AudioEngine engine; setup(engine); importFixture(engine, mono);
        engine.playRange(0.25, 0.5);
        render(engine, (int) (rate * 0.4));
        require(! engine.playing.load(), "Playback did not stop at the range end");
        near(engine.positionSeconds.load(), 0.5, 1.0e-9, "Stop position");
        require(engine.stopAt.load() < 0.0, "Stop point not cleared");
    });

    // Recording (manual pp. 347–359). The input is a ramp of the render clock: input = seconds / 10.
    double inputClock = 0.0;
    const auto recordInput = [&](AudioEngine& engine, double seconds)
    {
        juce::AudioBuffer<float> out(2, block), in(2, block);
        for (int done = 0, total = (int) std::llround(seconds * rate); done < total; done += block)
        {
            const int n = std::min(block, total - done);
            for (int i = 0; i < n; ++i) for (int c = 0; c < 2; ++c) in.setSample(c, i, (float) ((inputClock + i / rate) / 10.0));
            out.clear(); engine.renderOfflineBlock(out, n, in);
            inputClock += n / rate;
        }
    };
    const auto recorder = [&](AudioEngine& engine)
    {
        engine.prepareForOffline(rate, block, true); engine.masterGain = 1.0f; engine.metronome = false; inputClock = 0.0;
        auto track = engine.addAudioTrack("Toma"); track->inputLeft = 0; track->inputRight = 1; track->armed = true;
        return track;
    };
    const auto constantClip = [](double start, double length, float value)
    {
        AudioClip clip; clip.sourceRate = rate; clip.startSeconds = start; clip.durationSeconds = length; clip.fadeIn = clip.fadeOut = 0.0;
        clip.audio = std::make_shared<juce::AudioBuffer<float>>(2, (int) std::llround(length * rate));
        for (int c = 0; c < 2; ++c) clip.audio->getWritePointer(c)[0] = 0.0f, juce::FloatVectorOperations::fill(clip.audio->getWritePointer(c), value, clip.audio->getNumSamples());
        return clip;
    };
    const auto outputAt = [&](AudioEngine& engine, double seconds)
    {
        engine.looping = false; engine.seek(seconds); engine.play();
        const auto out = render(engine, block); engine.stop(); return out.getSample(0, 0);
    };

    suite.run("cut range trims, splits and removes overlapped events", [&]
    {
        std::vector<AudioClip> clips { constantClip(0.0, 3.0, 1.0f), constantClip(1.2, 0.5, 1.0f), constantClip(1.8, 1.0, 1.0f) };
        audio::cutRange(clips, 1.0, 2.0);
        require(clips.size() == 3, "Expected left part, right part and trimmed third clip, got " + juce::String((int) clips.size()));
        near(clips[0].startSeconds, 0.0, 1e-12, "Left part start"); near(clips[0].durationSeconds, 1.0, 1e-12, "Left part length");
        near(clips[1].startSeconds, 2.0, 1e-12, "Right part start"); near(clips[1].offsetSeconds, 2.0, 1e-12, "Right part offset");
        near(clips[1].durationSeconds, 1.0, 1e-12, "Right part length"); require(clips[0].id != clips[1].id, "Split parts share an id");
        near(clips[2].startSeconds, 2.0, 1e-12, "Third clip start"); near(clips[2].offsetSeconds, 0.2, 1e-12, "Third clip offset"); near(clips[2].durationSeconds, 0.8, 1e-12, "Third clip length");
        std::vector<AudioClip> track { constantClip(0.0, 3.0, 1.0f) }, takes { constantClip(1.0, 1.0, 1.0f), constantClip(1.0, 1.0, 1.0f), constantClip(1.0, 0.5, 1.0f) };
        auto keep = track; audio::placeTakes(keep, takes, audio::AudioRecordMode::keepHistory);
        require(keep.size() == 4, "Keep history must keep the old event and every take");
        auto cycle = track; audio::placeTakes(cycle, takes, audio::AudioRecordMode::cycleHistoryReplace);
        require(cycle.size() == 5, "Cycle history + replace: two old parts and three takes");
        auto replace = track; audio::placeTakes(replace, takes, audio::AudioRecordMode::replace);
        require(replace.size() == 4, "Replace: two old parts, the rest of take 2 and take 3, got " + juce::String((int) replace.size()));
        near(replace[2].startSeconds, 1.5, 1e-12, "Replace keeps take 2 only after take 3"); near(replace[3].durationSeconds, 0.5, 1e-12, "Last take");
    });

    suite.run("overlapping audio events: only the top event is heard", [&]
    {
        AudioEngine engine; setup(engine); auto track = engine.addAudioTrack("Capas");
        track->clips.push_back(constantClip(0.0, 2.0, 0.2f)); track->clips.push_back(constantClip(0.5, 0.5, 0.6f));
        const float under = outputAt(engine, 0.25f), top = outputAt(engine, 0.75f), after = outputAt(engine, 1.5f);
        near(top / under, 3.0, 1e-5, "Top event not alone (summed with the one below?)");
        near(after / under, 1.0, 1e-5, "Lower event not heard again after the top one");
    });

    suite.run("glue rejoins split pieces and consolidates other events", [&]
    {
        auto ramp = std::make_shared<juce::AudioBuffer<float>>(1, (int) (rate * 2));
        for (int i = 0; i < ramp->getNumSamples(); ++i) ramp->setSample(0, i, (float) i / (float) ramp->getNumSamples());
        AudioClip whole; whole.audio = ramp; whole.sourceRate = rate; whole.durationSeconds = 2.0;
        AudioClip left = whole, right = whole; left.durationSeconds = 1.0; right.startSeconds = 1.0; right.offsetSeconds = 1.0; right.durationSeconds = 1.0;
        std::vector<AudioClip> pieces { right, left };
        requireOk(audio::glueEvents(pieces, 1));
        require(pieces.size() == 1 && pieces[0].audio == ramp, "Split pieces not rejoined into one event of the same file");
        near(pieces[0].durationSeconds, 2.0, 1e-12, "Rejoined length"); near(pieces[0].offsetSeconds, 0.0, 1e-12, "Rejoined offset");
        std::vector<AudioClip> other { constantClip(0.0, 1.0, 0.2f), constantClip(0.5, 1.0, 0.6f) };
        requireOk(audio::glueEvents(other, 0));
        require(other.size() == 1 && other[0].file == juce::File(), "Consolidated event expected");
        near(other[0].durationSeconds, 1.5, 1e-9, "Consolidated length");
        const auto at = [&](double t) { return other[0].audio->getSample(0, (int) std::llround(t * rate)); };
        near(at(0.25), 0.2, 1e-6, "Lower event before the overlap"); near(at(0.75), 0.6, 1e-6, "Upper event wins the overlap"); near(at(1.25), 0.6, 1e-6, "Upper event after");
        requireOk(audio::writeEventFile(other[0], dir.getChildFile("glued")));
        std::unique_ptr<juce::AudioFormatReader> reader(AudioEngine().formatManager.createReaderFor(other[0].file));
        require(reader && reader->lengthInSamples == other[0].audio->getNumSamples() && reader->usesFloatingPointData, "Glued event file not written as float WAV");
    });

    suite.run("crossfade overlaps touching events and both sound across it", [&]
    {
        AudioEngine engine; setup(engine); auto track = engine.addAudioTrack("Fundido");
        auto steady = constantClip(0.0, 2.0, 0.5f);
        AudioClip a = steady, b = steady; a.durationSeconds = 1.0; b.startSeconds = 1.0; b.offsetSeconds = 1.0; b.durationSeconds = 1.0;
        track->clips = { a, b };
        requireOk(audio::crossfadeEvents(track->clips, 0, 0.2));
        const auto& fa = track->clips[0]; const auto& fb = track->clips[1];
        near(fa.durationSeconds, 1.1, 1e-9, "Earlier event extended"); near(fb.startSeconds, 0.9, 1e-9, "Later event starts earlier");
        near(fb.offsetSeconds, 0.9, 1e-9, "Later event offset follows"); require(fb.crossfade && std::abs(fb.fadeIn - 0.2) < 1e-9 && std::abs(fa.fadeOut - 0.2) < 1e-9, "Fades not set to the overlap");
        near(outputAt(engine, 1.0) / outputAt(engine, 0.5), 1.0, 0.01, "Level dips or jumps in the middle of the crossfade");
        near(outputAt(engine, 0.95) / outputAt(engine, 0.5), 1.0, 0.01, "Level not constant through the crossfade");
        std::vector<AudioClip> apart { constantClip(0.0, 1.0, 0.5f), constantClip(1.5, 1.0, 0.5f) };
        require(audio::crossfadeEvents(apart, 0).failed(), "Events with a gap must be refused");
    });

    suite.run("muted event is silent and uncovers the event below", [&]
    {
        AudioEngine engine; setup(engine); auto track = engine.addAudioTrack("Mute");
        track->clips.push_back(constantClip(0.0, 2.0, 0.2f)); track->clips.push_back(constantClip(0.5, 0.5, 0.6f));
        const float under = outputAt(engine, 0.25f);
        track->clips[1].muted = true;
        near(outputAt(engine, 0.75f) / under, 1.0, 1e-5, "Muted top event still heard");
        track->clips[0].muted = true;
        near(outputAt(engine, 0.25f), 0.0, 1e-9, "Muted event sounds");
    });

    suite.run("fade shapes, event volume and fade settings survive save/load", [&]
    {
        AudioEngine engine; setup(engine); auto track = engine.addAudioTrack("Fundidos");
        track->clips.push_back(constantClip(0.0, 3.0, 0.5f));
        auto& clip = track->clips[0];
        const float full = outputAt(engine, 1.5);
        clip.fadeIn = 1.0; clip.fadeInShape = (int) audio::FadeShape::equalPower;
        near(outputAt(engine, 0.5) / full, std::sin(0.25 * juce::MathConstants<double>::pi), 1e-3, "Equal-power fade-in at its middle");
        clip.fadeInShape = (int) audio::FadeShape::slowStart;
        near(outputAt(engine, 0.5) / full, 0.25, 1e-3, "Slow-start fade-in at its middle");
        clip.fadeOut = 1.0; clip.fadeOutShape = (int) audio::FadeShape::sCurve;
        near(outputAt(engine, 2.5) / full, 0.5, 1e-3, "S-curve fade-out at its middle");
        clip.gain = juce::Decibels::decibelsToGain(-6.0f);
        near(outputAt(engine, 1.5) / full, juce::Decibels::decibelsToGain(-6.0f), 1e-4, "Event volume");
        AudioEngine source; setup(source); auto saved = importFixture(source, mono);
        auto& e = saved->clips[0]; e.fadeIn = 0.2; e.fadeInShape = 1; e.fadeOut = 0.3; e.fadeOutShape = 2; e.muted = true; e.crossfade = true;
        const auto file = dir.getChildFile("fades.silente"); requireOk(source.saveProject(file));
        AudioEngine restored; setup(restored); requireOk(restored.loadProject(file));
        const auto& r = restored.tracks[0]->clips[0];
        require(r.fadeInShape == 1 && r.fadeOutShape == 2 && r.muted && r.crossfade && std::abs(r.fadeIn - 0.2) < 1e-9 && std::abs(r.fadeOut - 0.3) < 1e-9, "Fade settings not restored");
    });

    suite.run("range editing: delete, delete time, insert silence, truncate, duplicate, fades", [&]
    {
        const auto base = [&] { return std::vector<AudioClip> { constantClip(0.0, 4.0, 0.5f) }; };
        const audio::TimeRange r { 1.0, 2.0 };
        std::vector<MidiNote> none;
        auto a = base(); audio::deleteRange(a, none, r, 120.0);
        require(a.size() == 2, "Delete: two parts expected");
        near(a[0].durationSeconds, 1.0, 1e-9, "Delete: left part"); near(a[1].startSeconds, 2.0, 1e-9, "Delete: right part start"); near(a[1].offsetSeconds, 2.0, 1e-9, "Delete: right part offset");
        auto b = base(); audio::deleteTime(b, none, r, 120.0);
        require(b.size() == 2, "Delete time: two parts"); near(b[1].startSeconds, 1.0, 1e-9, "Delete time closes the gap"); near(b[1].offsetSeconds, 2.0, 1e-9, "Delete time keeps the audio");
        auto c = base(); audio::insertSilence(c, none, r, 120.0);
        require(c.size() == 2, "Insert silence: split at the range start"); near(c[1].startSeconds, 2.0, 1e-9, "Insert silence moves the rest right"); near(c[1].offsetSeconds, 1.0, 1e-9, "Insert silence keeps the audio");
        auto d = base(); d.push_back(constantClip(1.25, 0.5, 0.5f)); d.push_back(constantClip(5.0, 1.0, 0.5f)); audio::truncate(d, { 1.0, 3.0 });
        near(d[0].startSeconds, 1.0, 1e-9, "Truncate start"); near(d[0].durationSeconds, 2.0, 1e-9, "Truncate length"); near(d[0].offsetSeconds, 1.0, 1e-9, "Truncate offset");
        near(d[1].startSeconds, 1.25, 1e-9, "Event inside the range untouched"); near(d[2].startSeconds, 5.0, 1e-9, "Event outside the range untouched");
        auto e = base(); audio::duplicateRange(e, none, r, 120.0);
        require(e.size() == 2, "Duplicate adds the range content on top"); near(e[1].startSeconds, 2.0, 1e-9, "Duplicate goes after the range"); near(e[1].offsetSeconds, 1.0, 1e-9, "Duplicate copies the range audio");
        auto f = base(); audio::adjustFadesToRange(f, { 0.0, 1.0 }); near(f[0].fadeIn, 1.0, 1e-9, "Range over the start sets the fade-in");
        audio::adjustFadesToRange(f, { 3.0, 4.0 }); near(f[0].fadeOut, 1.0, 1e-9, "Range over the end sets the fade-out");
        auto g = base(); audio::adjustFadesToRange(g, { 1.0, 3.0 }); near(g[0].fadeIn, 1.0, 1e-9, "Inner range: fade-in to its start"); near(g[0].fadeOut, 1.0, 1e-9, "Inner range: fade-out from its end");
        // MIDI at 120 BPM (2 beats per second): notes at beats 1, 3 and 5.
        const auto notes = [] { std::vector<MidiNote> n(3); n[0].startBeats = 1; n[1].startBeats = 3; n[2].startBeats = 5; for (auto& x : n) x.lengthBeats = 1; return n; };
        std::vector<AudioClip> empty; auto m = notes(); audio::deleteTime(empty, m, r, 120.0);
        require(m.size() == 2 && std::abs(m[0].startBeats - 1) < 1e-9 && std::abs(m[1].startBeats - 3) < 1e-9, "Delete time on MIDI");
        {   // Move = copy the content, delete the range, paste at the new place (on top).
            auto mv = base(); std::vector<MidiNote> mn = notes();
            const auto content = audio::copyRange(mv, mn, r, 120.0);
            require(content.clips.size() == 1 && std::abs(content.clips[0].startSeconds) < 1e-9 && std::abs(content.clips[0].offsetSeconds - 1.0) < 1e-9 && content.notes.size() == 1, "Range content relative to its start");
            audio::deleteRange(mv, mn, r, 120.0); audio::pasteContent(mv, mn, content, 3.0, 120.0);
            require(mv.size() == 3 && std::abs(mv[2].startSeconds - 3.0) < 1e-9 && std::abs(mv[2].offsetSeconds - 1.0) < 1e-9, "Moved audio lands at 3 s with the range's audio");
            require(mn.size() == 3 && std::abs(mn[2].startBeats - 7.0) < 1e-9, "Moved note keeps its place in the range (1.5 s → 3.5 s = beat 7)");
        }
        auto s2 = notes(); audio::insertSilence(empty, s2, r, 120.0);
        require(std::abs(s2[0].startBeats - 1) < 1e-9 && std::abs(s2[1].startBeats - 5) < 1e-9 && std::abs(s2[2].startBeats - 7) < 1e-9, "Insert silence on MIDI");
    });

    suite.run("punch in/out records only between the locators", [&]
    {
        AudioEngine engine; auto track = recorder(engine);
        engine.loopStart = 1.0; engine.loopEnd = 2.0; engine.punchIn = true; engine.punchOut = true; engine.seek(0.5);
        requireOk(engine.beginRecording(dir.getChildFile("punch"), { -1.0, false }));
        recordInput(engine, 2.0);
        require(engine.punchedOut.load(), "Punch out not reported");
        require(engine.playing.load(), "Playback must continue after punch out");
        requireOk(engine.endRecording());
        require(track->clips.size() == 1, "Expected one recorded event, got " + juce::String((int) track->clips.size()));
        const auto& clip = track->clips[0];
        near(clip.startSeconds, 1.0, 1.0 / rate, "Event starts at punch in");
        near(clip.durationSeconds, 1.0, 1.0 / rate, "Event ends at punch out");
        near(clip.audio->getSample(0, 0), 0.05, 1e-5, "First recorded sample is the input at punch in");
        near(clip.audio->getSample(1, clip.audio->getNumSamples() - 1), 0.15, 1e-4, "Last recorded sample is the input before punch out");
    });

    suite.run("cycle recording makes one take per lap and the last lap is heard", [&]
    {
        AudioEngine engine; auto track = recorder(engine);
        track->clips.push_back(constantClip(0.0, 3.0, 0.9f));
        engine.loopStart = 1.0; engine.loopEnd = 2.0; engine.looping = true; engine.audioRecordMode = 1; engine.seek(1.0);
        requireOk(engine.beginRecording(dir.getChildFile("cycle"), { -1.0, false }));
        recordInput(engine, 2.5);
        requireOk(engine.endRecording());
        require(track->clips.size() == 5, "Expected 2 old parts + 3 takes, got " + juce::String((int) track->clips.size()));
        for (int n = 0; n < 3; ++n)
        {
            const auto& take = track->clips[(size_t) (2 + n)];
            near(take.startSeconds, 1.0, 1.0 / rate, "Take " + juce::String(n + 1) + " start");
            near(take.offsetSeconds, (double) n, 1.0 / rate, "Take " + juce::String(n + 1) + " file offset");
            require(take.audio == track->clips[2].audio, "Takes must share the recorded file");
        }
        near(track->clips[4].durationSeconds, 0.5, 1.0 / rate, "Last lap length");
        track->armed = false;
        // At 1.25 s the third lap (input 2.25 s) is on top; at 1.75 s only the second lap (1.75 s) covers it.
        near(outputAt(engine, 1.25) / outputAt(engine, 1.75), 0.225 / 0.175, 1e-3, "Wrong take heard");
        near(outputAt(engine, 2.5) / outputAt(engine, 0.5), 1.0, 1e-6, "Old event not kept outside the cycle");
    });

    suite.run("recording in 32-bit float and MIDI at project positions", [&]
    {
        AudioEngine engine; auto track = recorder(engine); engine.recordBitDepth = 32; engine.tempo = 120.0;
        auto keys = engine.addMidiTrack("Teclas"); keys->armed = true; keys->notes.clear();
        engine.seek(2.0);
        requireOk(engine.beginRecording(dir.getChildFile("float"), { -1.0, false }));
        recordInput(engine, 0.5); engine.keyboardState.noteOn(1, 60, 0.8f);
        recordInput(engine, 0.5); engine.keyboardState.noteOff(1, 60, 0.0f);
        recordInput(engine, 0.25);
        requireOk(engine.endRecording());
        require(track->clips.size() == 1, "Audio take missing");
        std::unique_ptr<juce::AudioFormatReader> reader(engine.formatManager.createReaderFor(track->clips[0].file));
        require(reader != nullptr && reader->bitsPerSample == 32 && reader->usesFloatingPointData, "Take is not 32-bit float");
        require(keys->notes.size() == 1, "Expected one recorded note, got " + juce::String((int) keys->notes.size()));
        near(keys->notes[0].startBeats, 5.0, 2.0 * block / rate * 2.0, "Note start (2.5 s at 120 BPM)");
        near(keys->notes[0].lengthBeats, 1.0, 2.0 * block / rate * 2.0, "Note length");
    });

    suite.run("event colour overrides the track colour and survives save/load", [&]
    {
        AudioEngine source; setup(source);
        auto a = importFixture(source, mono); a->clips[0].colour = juce::Colour(0xff123456);
        auto b = importFixture(source, mono);
        const auto file = dir.getChildFile("colours.silente"); requireOk(source.saveProject(file));
        AudioEngine restored; setup(restored); requireOk(restored.loadProject(file));
        require(restored.tracks[0]->clips[0].colour == juce::Colour(0xff123456), "Event colour lost");
        require(restored.tracks[1]->clips[0].colour.isTransparent(), "Event without own colour must follow the track");
        juce::ignoreUnused(b);
    });

    suite.run("audio and MIDI project roundtrip", [&]
    {
        AudioEngine source; setup(source);
        auto audio = importFixture(source, mono, 0.125); audio->name = juce::String::fromUTF8("Voz ñ"); audio->gain = 0.625f; audio->pan = -1.0f;
        audio->clips[0].offsetSeconds = 0.125; audio->clips[0].durationSeconds = 0.5;
        auto midi = source.addMidiTrack("Piano");
        MidiNote note; note.pitch = 65; note.velocity = 87; note.startBeats = 2.5; note.lengthBeats = 0.75;
        midi->notes.push_back(note); source.tempo = 93.0; source.projectName = juce::String::fromUTF8("Prueba ñ");
        const auto file = dir.getChildFile("roundtrip.silente"); requireOk(source.saveProject(file));
        AudioEngine restored; setup(restored); requireOk(restored.loadProject(file));
        require(restored.tracks.size() == 2, "Roundtrip changed track count");
        require(restored.tracks[0]->name == juce::String::fromUTF8("Voz ñ"), "Unicode name not restored");
        near(restored.tempo.load(), 93, 0.00001, "Tempo");
        near(restored.tracks[0]->gain.load(), 0.625, 0.00001, "Track gain");
        const auto& clip = restored.tracks[0]->clips.at(0);
        near(clip.startSeconds, 0.125, 0.00001, "Clip start");
        near(clip.offsetSeconds, 0.125, 0.00001, "Clip offset");
        near(clip.durationSeconds, 0.5, 0.00001, "Clip length");
        require(clip.audio != nullptr, "Audio was not reloaded");
        const auto& savedNote = restored.tracks[1]->notes.at(0);
        require(savedNote.pitch == 65 && savedNote.velocity == 87, "Note pitch/velocity changed");
        near(savedNote.startBeats, 2.5, 0.00001, "Note start");
        near(savedNote.lengthBeats, 0.75, 0.00001, "Note length");
        restored.seek(0.25); restored.play();
        require(peak(render(restored, 2048)) > 0.01, "Reloaded audio is silent");
    });

    suite.run("malformed project preserves the current session", [&]
    {
        AudioEngine engine; setup(engine); importFixture(engine, mono);
        const auto id = engine.tracks[0]->id;
        const auto file = dir.getChildFile("broken.silente");
        require(file.replaceWithText("{ invalid json"), "Cannot create malformed fixture");
        require(engine.loadProject(file).failed(), "Malformed file accepted");
        require(engine.tracks.size() == 1 && engine.tracks[0]->id == id, "Failed load destroyed current project");
    });

    suite.run("export writes a valid stereo WAV with expected signal", [&]
    {
        AudioEngine engine; setup(engine);
        auto track = importFixture(engine, mono); track->pan = -1.0f; track->gain = 0.5f;
        const auto file = dir.getChildFile("export.wav"); requireOk(engine.exportMix(file, rate, 24));
        std::unique_ptr<juce::AudioFormatReader> reader(engine.formatManager.createReaderFor(file));
        require(reader != nullptr, "Export is not a readable audio file");
        require(reader->numChannels == 2, "Export is not stereo");
        near(reader->sampleRate, rate, 0.001, "Export sample rate");
        require(reader->lengthInSamples >= 48000, "Export truncated the one-second clip");
        juce::AudioBuffer<float> data(2, 4096);
        require(reader->read(&data, 0, 4096, 2048, true, true), "Cannot decode export");
        near(data.getSample(0, 1000), 0.125, 0.0003, "Export gain");
        near(peak(data, 1), 0, 0.00001, "Export pan");
    });

    suite.run("plugin latency compensation aligns dry and delayed impulses", [&]
    {
        AudioEngine engine; setup(engine);
        auto dry = importFixture(engine, impulses); dry->pan = -1.0f;
        auto wet = importFixture(engine, impulses); wet->pan = -1.0f;
        addTestDelay(engine, 1, 128);
        near(engine.pluginLatencySamples(), 128, 0, "Reported plugin latency");
        engine.play(); const auto audio = render(engine, 1024);
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const double expected = i == 138 ? 0.5 : i == 378 ? 1.0 : 0.0;
            near(audio.getSample(0, i), expected, 0.0003, "Compensated impulse sample " + juce::String(i));
        }
    });

    suite.run("export removes plugin compensation preroll", [&]
    {
        AudioEngine engine; setup(engine);
        auto dry = importFixture(engine, impulses); dry->pan = -1.0f;
        auto wet = importFixture(engine, impulses); wet->pan = -1.0f;
        addTestDelay(engine, 1, 128);
        const auto file = dir.getChildFile("export-compensated.wav"); requireOk(engine.exportMix(file, rate, 24));
        std::unique_ptr<juce::AudioFormatReader> reader(engine.formatManager.createReaderFor(file));
        require(reader != nullptr, "Compensated WAV unreadable");
        juce::AudioBuffer<float> audio(2, 1024);
        require(reader->read(&audio, 0, 1024, 0, true, true), "Cannot decode compensated export");
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const double expected = i == 10 ? 0.5 : i == 250 ? 1.0 : 0.0;
            near(audio.getSample(0, i), expected, 0.0003, "Export impulse sample " + juce::String(i));
        }
    });

    auto* report = new juce::DynamicObject();
    report->setProperty("suite", "Silente offline engine tests");
    report->setProperty("timestamp", juce::Time::getCurrentTime().toISO8601(true));
    report->setProperty("hardwareOpened", false);
    report->setProperty("sampleRate", rate);
    report->setProperty("blockSize", block);
    report->setProperty("fixtures", dir.getFullPathName());
    report->setProperty("total", suite.results.size());
    report->setProperty("failed", suite.failures);
    report->setProperty("passed", suite.results.size() - suite.failures);
    report->setProperty("results", juce::var(suite.results));
    suite.text += "\n" + juce::String(suite.results.size() - suite.failures) + "/"
        + juce::String(suite.results.size()) + " passed. Fixtures: " + dir.getFullPathName() + "\n";
    const bool jsonWritten = outputDir.getChildFile("engine-tests.json").replaceWithText(juce::JSON::toString(juce::var(report), true));
    const bool textWritten = outputDir.getChildFile("engine-tests.txt").replaceWithText(suite.text);
    if (! jsonWritten || ! textWritten)
    {
        std::cerr << "Failed to write test report" << std::endl;
        ++suite.failures;
    }
    std::cout << suite.text.substring(suite.text.lastIndexOf("\n\n") + 2) << std::endl;
    return suite.failures;
}
}

#include "EngineTests.h"
#include "../src/audio/TapTempo.h"
#include "../src/audio/RecordingModes.h"
#include "../src/audio/EventEdit.h"
#include "../src/audio/FadeCurves.h"
#include "../src/audio/RangeEdit.h"
#include "../src/AudioEngine.h"
#include "../src/domain/Quantize.h"
#include "../src/domain/NoteEdit.h"
#include "../src/domain/TempoMap.h"
#include "../src/domain/Automation.h"
#include "../src/persistence/TrackDataJson.h"
#include "../src/plugins/PluginSelection.h"
#include "../src/app/TrackCreation.h"
#include "../src/app/TrackOrderCommands.h"
#include "../src/domain/TrackTree.h"
#include "../src/domain/MediaPool.h"
#include "../src/io/CprImport.h"
#include "../src/ui/project/TimelineTracks.h"
#include <limits>
#include <cstring>
#include <map>
#include <string>
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
    explicit DelayTestPlugin(int delaySamples,float multiply=1.0f,float add=0.0f)
        : AudioPluginInstance(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                               .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          delay(delaySamples),factor(multiply),addition(add)
    { setLatencySamples(delay); }
    const juce::String getName() const override { return "Silente test delay"; }
    void fillInPluginDescription(juce::PluginDescription& d) const override
    {
        d.name = getName(); d.pluginFormatName = "Silente Test"; d.manufacturerName = "Silente tests";
        d.numInputChannels = 2; d.numOutputChannels = 2; d.isInstrument = false;
    }
    bool isBusesLayoutSupported(const BusesLayout& l) const override
    { return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void prepareToPlay(double, int) override { ring.setSize(2, juce::jmax(1,delay)); reset(); setLatencySamples(delay); }
    void releaseResources() override {}
    void reset() override { ring.clear(); cursor = 0; }
    using juce::AudioPluginInstance::processBlock;
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) override
    {
        if(delay==0){for(int ch=0;ch<audio.getNumChannels();++ch)for(int i=0;i<audio.getNumSamples();++i)audio.setSample(ch,i,audio.getSample(ch,i)*factor+addition);return;}
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
    int delay, cursor = 0;float factor=1.0f,addition=0.0f;
    juce::AudioBuffer<float> ring;
};

void addTestDelay(AudioEngine& engine, int trackIndex, int delaySamples)
{
    auto plugin = std::make_unique<DelayTestPlugin>(delaySamples);
    const auto description = plugin->getPluginDescription();
    requireOk(engine.addPlugin(trackIndex, std::move(plugin), description));
}

// Minimal Cubase .cpr writer for the importer tests: RIF2 (8-byte words) or RIFF (4-byte words),
// one ROOT/ARCH section, objects with the class-marker grammar of docs/CPR_INVESTIGACION.md.
struct CprBuilder
{
    int W;
    std::vector<juce::uint8> b;
    std::map<std::string, size_t> markers;
    std::vector<size_t> open;
    explicit CprBuilder(bool wide) : W(wide ? 8 : 4) {}
    static void put(std::vector<juce::uint8>& out, juce::uint64 v, int n) { for (int i = n - 1; i >= 0; --i) out.push_back((juce::uint8) (v >> (8 * i))); }
    void be(juce::uint64 v, int n) { put(b, v, n); }
    void u16(int v) { be((juce::uint64) v, 2); }
    void u32(juce::uint64 v) { be(v, 4); }
    void i64(juce::int64 v) { be((juce::uint64) v, 8); }
    void f64(double x) { juce::uint64 v; std::memcpy(&v, &x, 8); be(v, 8); }
    void f32(float x) { juce::uint32 v; std::memcpy(&v, &x, 4); be(v, 4); }
    void str(const char* s) { const auto n = std::strlen(s); u32(n + 1); b.insert(b.end(), s, s + n); b.push_back(0); }
    void word(juce::uint64 v) { be(v, W); }
    juce::uint64 begin(const char* cls, int version) // returns the object id (offset of its size field)
    {
        const juce::uint64 all = W == 8 ? ~juce::uint64 {} : 0xFFFFFFFFull, ref = W == 8 ? (1ull << 63) : (1ull << 31);
        if (auto it = markers.find(cls); it != markers.end()) word(ref | it->second);
        else { markers[cls] = b.size(); word(all); str(cls); u16(version); }
        const auto id = b.size(); word(0); open.push_back(id); return id;
    }
    void end()
    {
        const auto at = open.back(); open.pop_back();
        const auto size = (juce::uint64) (b.size() - at - (size_t) W);
        for (int i = 0; i < W; ++i) b[at + (size_t) i] = (juce::uint8) (size >> (8 * (W - 1 - i)));
    }
    void attribute(const char* key, juce::int64 value) { str(key); u16(1); i64(value); }
    juce::uint64 fnPath(const char* file, const char* folder)
    {
        const auto id = begin("FNPath", 0); str(file); b.insert(b.end(), { 'W', 'A', 'V', 'E' }); str("wav"); str("wav"); str("Archivo Wave");
        u32(0); u16(0); str(folder); end();
        return id;
    }
    juce::uint64 clip(const char* name, const char* file, const char* folder, juce::uint64 frames, int bits, int channels, float hz)
    {
        const auto id = begin("PAudioClip", 4); str(name);
        const auto path = fnPath(file, folder);
        begin("AudioCluster", 0); begin("AudioFile", 1);
        be(frames, 8); u16(bits); u16(bits / 8 * channels); u16(channels); f32(hz);
        word(path); i64(0); // reference to the FNPath (its id)
        end(); end(); end();
        return id;
    }
    void event(double startTicks, double length, double offset) { begin("MAudioEvent", 6); u16(0); f64(startTicks); f64(length); f64(offset); }
    juce::MemoryBlock file() const
    {
        std::vector<juce::uint8> out;
        const char* section = "Arrangement1";
        out.insert(out.end(), { 'R', 'I', 'F', (juce::uint8) (W == 8 ? '2' : 'F') }); put(out, 0, W); out.insert(out.end(), { 'N', 'U', 'N', 'D' });
        out.insert(out.end(), { 'R', 'O', 'O', 'T' }); put(out, 4 + std::strlen(section), W); put(out, std::strlen(section), 4);
        out.insert(out.end(), section, section + std::strlen(section));
        out.insert(out.end(), { 'A', 'R', 'C', 'H' }); put(out, b.size(), W); out.insert(out.end(), b.begin(), b.end());
        std::vector<juce::uint8> size; put(size, out.size() - (W == 8 ? 16 : 12), W);
        std::copy(size.begin(), size.end(), out.begin() + 4);
        return juce::MemoryBlock(out.data(), out.size());
    }
};

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

    suite.run("marker/ruler persistence, active track, stable identities and legacy defaults", [&]
    {
        AudioEngine engine;setup(engine);auto marker=engine.addTrack(TrackKind::marker,"Secciones");auto ruler=engine.addTrack(TrackKind::ruler,"Tiempo");
        auto intro=domain::makeMarker(marker->data,1.25);intro.description="INTRO";marker->data.markers.push_back(intro);
        auto verse=domain::makeMarker(marker->data,8);verse.description="ESTROFA";marker->data.markers.push_back(verse);
        auto chorus=domain::makeMarker(marker->data,12,true,20);chorus.description="CORO";chorus.colour=juce::Colour(0xfff0bd48);marker->data.markers.push_back(chorus);
        require(intro.number==1&&verse.number==2&&chorus.number==1,"Point and cycle numbering must be independent");
        ruler->data.rulerFormat=2;ruler->data.rulerOffset=3600;
        const auto file=dir.getChildFile("marker-ruler.silente");requireOk(engine.saveProject(file));
        AudioEngine loaded;setup(loaded);requireOk(loaded.loadProject(file));
        auto copy=loaded.findTrack(marker->id);require(copy&&copy->kind==TrackKind::marker&&!copy->hasAudioChannel(),"Marker changed track kind or gained an audio channel");
        require(copy->data.markers==marker->data.markers,"Marker attributes did not round-trip");
        require(loaded.activeMarkerTrack==marker->id,"Active marker track changed");
        auto copiedRuler=loaded.findTrack(ruler->id);require(copiedRuler&&copiedRuler->kind==TrackKind::ruler&&copiedRuler->data.rulerFormat==2,"Independent ruler format changed");
        near(copiedRuler->data.rulerOffset,3600,0,"Ruler offset");near(loaded.duration(),20,0,"Last cycle endpoint contributes to timeline duration");
        require(persistence::readTrackData({}).markers.empty(),"Legacy project should start with no markers");
        loaded.removeTrack(0);require(loaded.activeMarkerTrack.isEmpty(),"Removing the active marker track left a stale target");
    });
    suite.run("marker data rejects invalid bounds and repairs duplicate identities and numbers", [&]
    {
        domain::TrackData data;auto point=domain::makeMarker(data,1);data.markers.push_back(point);data.markers.push_back(point);
        auto invalid=domain::makeMarker(data,2,true,1);data.markers.push_back(invalid);
        require(!domain::validMarker(invalid),"Backwards cycles must be rejected");invalid.position=std::numeric_limits<double>::infinity();require(!domain::validMarker(invalid),"Infinite marker position accepted");
        const auto repaired=persistence::readTrackData(persistence::writeTrackData(data));
        require(repaired.markers.size()==2,"Invalid cycle wasn't removed");
        require(repaired.markers[0].id!=repaired.markers[1].id&&repaired.markers[0].number!=repaired.markers[1].number,"Duplicate IDs/numbers weren't repaired");
        require(!ui::parseTimelinePosition("3. 5. 1. 0",0,120,rate),"Invalid musical beat accepted");
        for(int format=0;format<5;++format)
        {
            const auto text=ui::timelinePosition(12.5,format,120,rate);auto parsed=ui::parseTimelinePosition(text,format,120,rate);
            require(parsed.has_value(),"Cannot parse displayed marker position: "+text);near(*parsed,12.5,1.0/30,"Timeline time-format round trip");
        }
    });
    suite.run("marker/ruler tracks cannot route audio or silence the mix by solo", [&]
    {
        AudioEngine engine;setup(engine);auto audio=importFixture(engine,mono);audio->gain=1;audio->pan=-1;
        engine.play();render(engine,128);engine.seek(0);const auto baseline=render(engine,128);engine.pause();engine.seek(0);
        auto marker=engine.addTrack(TrackKind::marker);auto ruler=engine.addTrack(TrackKind::ruler);auto group=engine.addTrack(TrackKind::group);group->gain=1;
        marker->solo=true;ruler->solo=true;marker->armed=true;ruler->monitor=true;
        require(!engine.canRoute(*marker,*group,false)&&!engine.canRoute(*ruler,*group,false),"Non-audio timeline track can route into a group");
        engine.play();const auto result=render(engine,128);for(int k=0;k<128;++k)for(int c=0;c<2;++c)near(result.getSample(c,k),baseline.getSample(c,k),0,"Markers changed an audio sample");
        near(marker->peakL.load()+ruler->peakL.load(),0,0,"Non-audio tracks have no meter signal");
    });

    suite.run("ten numbered inserts process in rack order, bypass independently and survive sparse reload", [&]
    {
        AudioEngine engine;setup(engine);auto track=importFixture(engine,mono);track->pan=-1;
        const int order[]{9,0,8,1,7,2,6,3,5,4};
        for(int position:order)
        {
            auto plugin=std::make_unique<DelayTestPlugin>(0,.5f,(position+1)*.001f);auto description=plugin->getPluginDescription();description.name="Effect "+juce::String(position+1);
            requireOk(engine.addPlugin(0,std::move(plugin),description,-1,position));
        }
        require(track->plugins.size()==10,"Ten processors were not retained");
        for(int n=0;n<10;++n)require(effectSlotIndex(*track,n)==n&&track->plugins[(size_t)n]->insertPosition==n,"Processing order differs from numbered rack order");
        const auto expected=[](int count){double value=.25;for(int n=0;n<count;++n)value=value*.5+(n+1)*.001;return value;};
        engine.play();const auto full=render(engine,256);near(full.getSample(0,200),expected(10),.000001,"All ten effects process");
        track->plugins[9]->bypassed=true;engine.seek(0);const auto bypassed=render(engine,256);near(bypassed.getSample(0,200),expected(9),.000001,"Bypass slot ten");
        auto extra=std::make_unique<DelayTestPlugin>(0);const auto description=extra->getPluginDescription();
        require(engine.addPlugin(0,std::move(extra),description).failed()&&track->plugins.size()==10,"An eleventh effect displaced an existing slot");
        const auto last=track->plugins[9];engine.removePlugin(0,4);require(effectSlotIndex(*track,4)<0&&track->plugins[(size_t)effectSlotIndex(*track,9)]==last,"Removing slot five renumbered slot ten");
        const auto file=dir.getChildFile("ten-inserts.silente");requireOk(engine.saveProject(file));
        AudioEngine loaded;setup(loaded);requireOk(loaded.loadProject(file));const auto copy=loaded.tracks.front();
        require(copy->plugins.size()==9&&effectSlotIndex(*copy,4)<0&&effectSlotIndex(*copy,9)>=0,"Sparse slot positions did not round-trip");
        require(copy->plugins[(size_t)effectSlotIndex(*copy,9)]->bypassed.load(),"Tenth-slot bypass did not round-trip");
        auto legacy=juce::JSON::parse(file);auto* root=legacy.getDynamicObject();auto tracks=root->getProperty("tracks");for(auto& item:*tracks.getArray())for(auto& plugin:*item.getDynamicObject()->getProperty("plugins").getArray())plugin.getDynamicObject()->removeProperty("insertPosition");
        const auto legacyFile=dir.getChildFile("ten-inserts-legacy.silente");legacyFile.replaceWithText(juce::JSON::toString(legacy));requireOk(loaded.loadProject(legacyFile));
        require(effectSlotIndex(*loaded.tracks.front(),8)>=0,"Legacy dense insert order was lost");
        auto midi=engine.addMidiTrack();auto instrument=std::make_unique<DelayTestPlugin>(0);auto instrumentDescription=instrument->getPluginDescription();instrumentDescription.isInstrument=true;
        requireOk(engine.addPlugin(1,std::move(instrument),instrumentDescription));
        for(int n=0;n<10;++n){auto effect=std::make_unique<DelayTestPlugin>(0);requireOk(engine.addPlugin(1,std::move(effect),description,-1,n));}
        require(midi->plugins.size()==11&&instrumentSlotIndex(*midi)==0&&effectSlotIndex(*midi,9)==10,"Instrument consumed one of the ten effect slots");
    });

    suite.run("track creation batches validate atomically, name tracks and apply recording inputs", [&]
    {
        AudioEngine engine;setup(engine);app::TrackCreation request;request.name="Voz";request.count=3;request.inputLeft=request.inputRight=2;
        std::vector<std::shared_ptr<Track>> created;requireOk(app::createTracks(engine,request,created));
        require(created.size()==3&&created[0]->name=="Voz 01"&&created[2]->name=="Voz 03"&&created[2]->inputLeft==2&&created[2]->inputRight==2,"Batch names/input configuration changed");
        const auto size=engine.tracks.size();request.count=101;require(app::createTracks(engine,request,created).failed()&&engine.tracks.size()==size,"Invalid count created part of a batch");
        request.count=0;require(app::createTracks(engine,request,created).failed(),"Zero-track request accepted");request.count=2;request.output="deleted-group";
        require(app::createTracks(engine,request,created).failed()&&engine.tracks.size()==size,"Missing routing target created a partial batch");request.output.clear();
        request.kind=TrackKind::marker;request.count=11;require(app::createTracks(engine,request,created).failed()&&engine.tracks.size()==size,"Marker limit created a partial batch");
        engine.clear();request.kind=TrackKind::folder;request.count=100;request.name.clear();requireOk(app::createTracks(engine,request,created));require(created.size()==100&&engine.tracks.size()==100&&created[0]->name.isNotEmpty(),"Count 100 or default names failed");
        engine.clear();engine.prepareForOffline(rate,block,true);request.kind=TrackKind::audio;request.count=2;request.name="Take";
        requireOk(app::createTracks(engine,request,created));for(auto& track:created)track->armed=true;
        requireOk(engine.beginRecording(dir.getChildFile("batch-input-recording"),{-1.0,false}));
        juce::AudioBuffer<float> input(3,block),out(2,block);input.clear();juce::FloatVectorOperations::fill(input.getWritePointer(2),.25f,block);
        for(int n=0;n<20;++n){out.clear();engine.renderOfflineBlock(out,block,input);}engine.endRecording();engine.pause();
        for(const auto& track:created)
        {
            require(track->clips.size()==1&&track->clips.front().audio&&track->clips.front().audio->getNumChannels()==1,"Mono batch produced a stereo take");
            near(track->clips.front().audio->getSample(0,128),.25,.00001,"Recording uses the selected input for each created track");
        }
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

    suite.run("sixteen independent sends, legacy projects and high-slot persistence", [&]
    {
        AudioEngine engine; setup(engine);
        auto source = importFixture(engine, mono); source->pan = -1.0f; source->gain = 0.5f;
        auto fx = engine.addTrack(TrackKind::fx, "Sixteen sends"); fx->pan = -1.0f; fx->gain = 1.0f;
        engine.play();
        near(render(engine, 2048).getSample(0, 1024), 0.125, 0.0002, "Unused high slots must not route to track zero");
        double expected = 0.125;
        for (int k = 0; k < 16; ++k)
        {
            requireOk(engine.setSend(*source, k, fx->id));
            auto& send = source->sends[(size_t) k];
            send.levelDb = -24.0f - (float) k;
            send.preFader = k % 2 == 1;
            expected += (send.preFader.load() ? 0.25 : 0.125) * juce::Decibels::decibelsToGain(send.levelDb.load());
            near(render(engine, 2048).getSample(0, 1024), expected, 0.0003, "Each slot adds its own pre/post signal " + juce::String(k + 1));
        }
        require(engine.setSend(*source, 16, fx->id).failed() && engine.setSend(*source, -1, fx->id).failed(), "Invalid send slot accepted");
        require(engine.setSend(*fx, 15, fx->id).failed(), "High-slot feedback loop accepted");
        source->sends[15].enabled = false;
        const auto full = dir.getChildFile("sixteen-sends.silente"); requireOk(engine.saveProject(full));
        AudioEngine restored; setup(restored); requireOk(restored.loadProject(full));
        const auto& last = restored.tracks[0]->sends[15];
        require(last.target == fx->id && last.preFader.load() && ! last.enabled.load(), "Slot 16 flags lost on load");
        near(last.levelDb.load(), -39.0, 0.0001, "Slot 16 level lost on load");
        restored.play();
        near(render(restored, 2048).getSample(0, 1024), render(engine, 2048).getSample(0, 1024), 0.0001, "Sixteen sends round trip changes the audio");

        auto json = juce::JSON::parse(full);
        auto* tracks = json["tracks"].getArray(); require(tracks != nullptr, "Missing tracks in fixture");
        for (auto& t : *tracks) if (auto* sends = t["sends"].getArray()) sends->removeRange(4, sends->size() - 4);
        const auto legacy = dir.getChildFile("four-sends-legacy.silente");
        require(legacy.replaceWithText(juce::JSON::toString(json)), "Cannot write legacy fixture");
        AudioEngine old; setup(old); requireOk(old.loadProject(legacy));
        for (int k = 4; k < 16; ++k) require(old.tracks[0]->sends[(size_t) k].target.isEmpty(), "Legacy file creates a new send");
        double legacyExpected = 0.125;
        for (int k = 0; k < 4; ++k) legacyExpected += (k % 2 ? 0.25 : 0.125) * juce::Decibels::decibelsToGain(-24.0f - (float) k);
        old.play(); near(render(old, 2048).getSample(0, 1024), legacyExpected, 0.0003, "Legacy four-slot project changes the audio");
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

    suite.run("track tree: move up/down, nest, out of folder, drop and cycles refused", [&]
    {
        using namespace domain;
        const auto ids = [](const TrackOrder& o) { juce::StringArray a; for (auto& n : o) a.add(n.id + (n.parent.isEmpty() ? "" : "<" + n.parent)); return a.joinIntoString(" "); };
        TrackOrder flat { { "a", {}, false, false }, { "b", {}, false, false }, { "c", {}, false, false } };
        require(isValid(flat) && normalised(flat) == flat, "Flat list must be valid and unchanged");
        require(ids(*moveUp(flat, 1)) == "b a c" && ids(*moveDown(flat, 1)) == "a c b", "Flat move up/down = swap with neighbour");
        require(! moveUp(flat, 0) && ! moveDown(flat, 2) && ! moveOutOfFolder(flat, 1), "Edges must be refused");
        TrackOrder tree { { "F", {}, true, false }, { "x", "F", false, false }, { "G", "F", true, false }, { "y", "G", false, false }, { "z", {}, false, false } };
        require(isValid(tree) && depthOf(tree, 3) == 2 && subtreeEnd(tree, 0) == 4 && subtreeEnd(tree, 2) == 4, "Depth and subtree");
        require(ids(*moveIntoFolder(tree, 4, "G")) == "F x<F G<F y<G z<G", "Into nested folder as last child");
        require(ids(*moveOutOfFolder(tree, 3)) == "F x<F G<F y<F z", "Out of folder goes after it, one level up");
        require(ids(*moveOutOfFolder(tree, 1)) == "F G<F y<G x z", "Out of top folder lands after the whole folder");
        require(ids(*moveDown(tree, 1)) == "F G<F y<G x<F z", "Move down swaps with the next block (folder with content)");
        require(ids(*moveDown(tree, 0)) == "z F x<F G<F y<G", "A folder moves with its content");
        require(! moveIntoFolder(tree, 0, "G") && ! moveInto(tree, 0, "F", 0) && ! moveIntoFolder(tree, 1, "z"), "Cycles and non-folder parents must be refused");
        require(! dropOn(tree, 0, 3, DropZone::before) && ! dropOn(tree, 2, 3, DropZone::into), "Dropping a folder inside itself must be refused");
        require(ids(*dropOn(tree, 4, 1, DropZone::before)) == "F z<F x<F G<F y<G", "Drop before a row joins its folder");
        require(ids(*dropOn(tree, 4, 0, DropZone::after)) == "F z<F x<F G<F y<G", "Drop after an open folder = first child");
        require(ids(*dropOn(tree, 1, 4, DropZone::after)) == "F G<F y<G z x", "Drop after a top-level row");
        require(ids(*dropOn(tree, 4, -1, DropZone::before)) == "F x<F G<F y<G z", "Drop at the end keeps a top-level track last");
        require(ids(withoutTrack(tree, 2)) == "F x<F y<F z", "Removing a folder hands its content up");
        auto collapsed = tree; collapsed[0].collapsed = true;
        require(hiddenByCollapse(collapsed, 3) && ! hiddenByCollapse(collapsed, 0) && ! hiddenByCollapse(collapsed, 4), "Collapsed folder hides its whole content");
        const auto mask = hiddenByCollapse(collapsed); const auto level = depths(collapsed);
        for (int k = 0; k < (int) collapsed.size(); ++k) require(mask[(size_t) k] == hiddenByCollapse(collapsed, k) && level[(size_t) k] == depthOf(collapsed, k), "One-pass helpers must match");
        TrackOrder broken { { "y", "G", false, false }, { "c1", "c2", true, false }, { "c2", "c1", true, false }, { "G", {}, true, false }, { "n", "y", false, true }, { "m", "?", false, false } };
        const auto fixed = normalised(broken);
        require(isValid(fixed) && fixed.size() == broken.size(), "Repair must give a valid list with every track");
        require(ids(fixed) == "c1 c2<c1 G y<G n m", "Repair result: " + ids(fixed));
        require(! fixed[(size_t) indexOf(fixed, "n")].collapsed, "Only folders can be collapsed");
    });

    suite.run("folders: mute/solo pass down, reordering keeps group/send/automation and persistence", [&]
    {
        AudioEngine engine; setup(engine);
        auto a = importFixture(engine, mono); a->pan = -1.0f;                       // 0.25 left
        auto b = importFixture(engine, mono); b->pan = -1.0f; b->gain = 0.5f;       // 0.125 left
        auto folder = engine.addTrack(TrackKind::folder, "Carpeta");
        auto inner = engine.addTrack(TrackKind::folder, "Interior");
        using app::TrackMove;
        requireOk(app::moveTrack(engine, { TrackMove::intoFolder, a->id, folder->id }));
        requireOk(app::moveTrack(engine, { TrackMove::intoFolder, inner->id, folder->id }));
        requireOk(app::moveTrack(engine, { TrackMove::intoFolder, b->id, inner->id }));
        require(engine.tracks[0] == folder && engine.tracks[1] == a && engine.tracks[2] == inner && engine.tracks[3] == b, "Folder content follows its folder");
        engine.play();
        near(render(engine, 2048).getSample(0, 1024), 0.375, 0.0002, "Folders do not change the sum");
        inner->mute = true; near(render(engine, 2048).getSample(0, 1024), 0.25, 0.0002, "Folder mute silences its content");
        inner->mute = false; folder->mute = true; near(peak(render(engine, 2048)), 0.0, 1.0e-6, "Mute passes through nested folders");
        folder->mute = false; inner->solo = true; near(render(engine, 2048).getSample(0, 1024), 0.125, 0.0002, "Folder solo solos its content only");
        inner->solo = false; folder->solo = true; near(render(engine, 2048).getSample(0, 1024), 0.375, 0.0002, "Top folder solo covers nested content");
        folder->solo = false;

        // Routing and automation refer to ids: reordering must not change what is heard.
        auto group = engine.addTrack(TrackKind::group, "Bus"); group->pan = -1.0f; group->gain = 0.5f;
        auto fx = engine.addTrack(TrackKind::fx, "FX"); fx->pan = -1.0f; fx->gain = 1.0f;
        requireOk(engine.setOutput(*a, group->id)); requireOk(engine.setSend(*b, 0, fx->id));
        a->automation.volume.points = { { 0.0, 1.0 }, { 64.0, 1.0 } }; a->automation.read = true;
        const auto before = render(engine, 4096);
        requireOk(app::moveTrack(engine, { TrackMove::drop, fx->id, folder->id, domain::DropZone::before }));
        requireOk(app::moveTrack(engine, { TrackMove::drop, group->id, a->id, domain::DropZone::before }));
        requireOk(app::moveTrack(engine, { TrackMove::outOfFolder, b->id }));
        require(engine.tracks[0] == fx && engine.tracks[2] == group && group->data.parentFolder == folder->id, "Drop order: " + juce::String(engine.tracks[2]->name));
        require(a->output == group->id && b->sends[0].target == fx->id && ! a->automation.empty(), "References must survive reordering");
        const auto after = render(engine, 4096);
        near(after.getSample(0, 3000), before.getSample(0, 3000), 0.0002, "Reordered routing must sound the same");
        require(engine.applyTrackOrder({ { a->id } }).failed(), "A partial order must be refused");
        auto order = engine.trackOrder(); std::swap(order[0], order[1]); order[1].id = order[0].id;
        require(engine.applyTrackOrder(order).failed(), "A duplicated id must be refused");

        // Persistence: hierarchy and folding survive; plain and old projects read flat.
        requireOk(app::setFolderCollapsed(engine, inner->id, true) ? juce::Result::ok() : juce::Result::fail("fold"));
        const auto file = dir.getChildFile("folders.silente"); requireOk(engine.saveProject(file));
        AudioEngine restored; setup(restored); requireOk(restored.loadProject(file));
        require(restored.trackOrder() == engine.trackOrder(), "Folder hierarchy changed after save/load");
        auto json = juce::JSON::parse(file);
        for (auto& t : *json["tracks"].getArray()) if (auto* d = t["trackData"].getDynamicObject()) { d->removeProperty("parentFolder"); d->removeProperty("folderCollapsed"); }
        const auto old = dir.getChildFile("folders-old.silente"); require(old.replaceWithText(juce::JSON::toString(json)), "Cannot write old project");
        requireOk(restored.loadProject(old));
        for (size_t k = 0; k < restored.tracks.size(); ++k)
            require(restored.tracks[k]->id == engine.tracks[k]->id && restored.tracks[k]->data.parentFolder.isEmpty(), "Old project must keep its flat order");
        json = juce::JSON::parse(file);
        json["tracks"][0]["trackData"].getDynamicObject()->setProperty("parentFolder", inner->id); // fx listed before its folder
        json["tracks"][1]["trackData"].getDynamicObject()->setProperty("parentFolder", inner->id); // folder -> inner -> folder: cycle
        const auto cyclic = dir.getChildFile("folders-cycle.silente"); require(cyclic.replaceWithText(juce::JSON::toString(json)), "Cannot write cyclic project");
        requireOk(restored.loadProject(cyclic));
        require(domain::isValid(restored.trackOrder()) && restored.tracks.size() == engine.tracks.size(), "A cyclic project must load repaired");
        AudioEngine plain; setup(plain); plain.addAudioTrack("Plain");
        const auto plainFile = dir.getChildFile("folders-plain.silente"); requireOk(plain.saveProject(plainFile));
        require(! plainFile.loadFileAsString().contains("parentFolder") && ! plainFile.loadFileAsString().contains("folderCollapsed"), "A project without folders must not write folder fields");

        // Removing a folder keeps its content one level up.
        engine.removeTrack(domain::indexOf(engine.trackOrder(), folder->id));
        require(domain::isValid(engine.trackOrder()) && engine.findTrack(a->id) && engine.findTrack(b->id) && a->data.parentFolder.isEmpty()
                && b->data.parentFolder.isEmpty() && inner->data.parentFolder.isEmpty(), "Content of a removed folder must stay, one level up");
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

    suite.run("punch over an armed track: existing material stays audible; input is monitored only inside the punch", [&]
    {
        // Pro Tools: the armed track keeps playing its material while recording. Its input is heard only
        // between punch in and punch out; before and after, the track plays its own material alone.
        AudioEngine engine; auto track = recorder(engine);
        track->clips.push_back(constantClip(0.0, 4.0, 0.4f));
        engine.loopStart = 1.0; engine.loopEnd = 2.0; engine.punchIn = true; engine.punchOut = true; engine.seek(0.5);
        requireOk(engine.beginRecording(dir.getChildFile("punch-monitor"), { -1.0, false }));
        juce::AudioBuffer<float> out(2, block), in(2, block);
        double clock = 0.0, base = -1.0; // base: the material alone (track gain and pan law included)
        for (int done = 0, total = (int) std::llround(2.0 * rate); done < total; done += block)
        {
            const int n = std::min(block, total - done);
            const double at = 0.5 + clock; // project time at the start of this block
            for (int i = 0; i < n; ++i) for (int c = 0; c < 2; ++c) in.setSample(c, i, (float) ((clock + i / rate) / 10.0));
            out.clear(); engine.renderOfflineBlock(out, n, in);
            double sum = 0.0; for (int i = 0; i < n; ++i) sum += out.getSample(0, i);
            const double mean = sum / n, inputMean = (clock + (n - 1) / (2.0 * rate)) / 10.0;
            if (at > 0.6 && at < 0.9) { if (base < 0.0) base = mean; require(base > 0.2 && std::abs(mean - base) < 1e-4, "Track material must stay audible before punch in, got " + juce::String(mean)); }
            if (at > 1.1 && at < 1.9) require(std::abs(mean - (base + base / 0.4 * inputMean)) < 0.003, "Input must be monitored over the material inside the punch, got " + juce::String(mean));
            if (at > 2.1 && at < 2.4) require(std::abs(mean - base) < 1e-4, "Only the material must play after punch out, got " + juce::String(mean));
            clock += n / rate;
        }
        requireOk(engine.endRecording());
        // Default mode (cycle history + replace): the old event is cut inside the punch and the take takes its place.
        require(track->clips.size() == 3, "Expected left remnant, take and right remnant, got " + juce::String((int) track->clips.size()));
        const auto& take = track->clips.back();
        near(take.startSeconds, 1.0, 1.0 / rate, "Take starts at punch in");
        near(take.durationSeconds, 1.0, 1.0 / rate, "Take ends at punch out");
        near(take.audio->getSample(0, 0), 0.05, 1e-4, "Take must hold only the input, not the monitored material");
    });

    suite.run("live recording view: lap start, captured length and waveform peaks", [&]
    {
        AudioEngine engine; auto track = recorder(engine);
        engine.loopStart = 1.0; engine.loopEnd = 3.0; engine.punchIn = true; engine.seek(0.5);
        requireOk(engine.beginRecording(dir.getChildFile("live"), { -1.0, false }));
        recordInput(engine, 0.25);
        near(engine.recordLapStart.load(), -1.0, 1e-12, "Nothing shown before the punch-in");
        recordInput(engine, 1.25); // project 0.5 → 2.0 s: one second captured from 1.0 s
        near(engine.recordLapStart.load(), 1.0, 1.0 / rate, "Red event starts at the punch-in");
        near((double) engine.recordCaptured.load() / rate, 1.0, 2.0 * block / rate, "Red event length follows the recording");
        const float* peaks = nullptr; int count = 0, perPeak = 0;
        require(engine.recordingOverview(*track, peaks, count, perPeak) && peaks != nullptr, "No live waveform");
        require(perPeak == 480 && std::abs(count - 100) <= 1, "Expected 100 peaks of 10 ms, got " + juce::String(count));
        // The input is a ramp of the render clock (seconds / 10): the last peak is near 1.5 s → 0.15.
        near(peaks[2 * (count - 1) + 1], 0.15, 0.002, "Last peak does not follow the input");
        requireOk(engine.endRecording());
        near(engine.recordLapStart.load(), -1.0, 1e-12, "Live view not cleared after recording");
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

    suite.run("MIDI export roundtrip keeps notes, positions, lengths and velocities", [&]
    {
        AudioEngine source; setup(source); source.tempo = 93.0;
        auto a = source.addMidiTrack(juce::String::fromUTF8("Piano ñ")); auto b = source.addMidiTrack("Bajo");
        const double starts[] { 0.0, 0.25, 1.5, 2.3333333, 7.0 }, lengths[] { 1.0, 0.125, 0.75, 0.5, 2.0 };
        const int pitches[] { 60, 64, 67, 0, 127 }, vels[] { 100, 1, 87, 127, 64 };
        for (int i = 0; i < 5; ++i) { MidiNote n; n.pitch = pitches[i]; n.velocity = vels[i]; n.startBeats = starts[i]; n.lengthBeats = lengths[i]; a->notes.push_back(n); }
        MidiNote m; m.pitch = 36; m.velocity = 90; m.startBeats = 4.0; m.lengthBeats = 1.5; b->notes.push_back(m);
        const auto file = dir.getChildFile("export.mid"); requireOk(source.exportMidi(file));
        AudioEngine back; setup(back); requireOk(back.importMidi(file));
        near(back.tempo.load(), 93.0, 0.01, "MIDI tempo");
        require(back.tracks.size() == 2, "MIDI track count");
        require(back.tracks[0]->name == juce::String::fromUTF8("Piano ñ") && back.tracks[1]->name == "Bajo", "MIDI track names");
        auto notes = back.tracks[0]->notes; std::sort(notes.begin(), notes.end(), [](const MidiNote& x, const MidiNote& y) { return x.startBeats < y.startBeats; });
        require(notes.size() == 5, "MIDI note count");
        for (int i = 0; i < 5; ++i)
        {
            const auto what = "MIDI note " + juce::String(i);
            near(notes[(size_t) i].startBeats, std::round(starts[i] * 960.0) / 960.0, 1e-6, what + " start");
            near(notes[(size_t) i].lengthBeats, std::round(lengths[i] * 960.0) / 960.0, 1e-6, what + " length");
            require(notes[(size_t) i].pitch == pitches[i] && notes[(size_t) i].velocity == vels[i], what + " pitch/velocity");
        }
        require(back.tracks[1]->notes.size() == 1 && back.tracks[1]->notes[0].pitch == 36 && back.tracks[1]->notes[0].velocity == 90, "Second track");
        near(back.tracks[1]->notes[0].startBeats, 4.0, 1e-6, "Second track start");
        const auto one = dir.getChildFile("export-one.mid"); requireOk(source.exportMidi(one, 1));
        AudioEngine single; setup(single); requireOk(single.importMidi(one));
        require(single.tracks.size() == 1 && single.tracks[0]->name == "Bajo", "Single-track MIDI export");
        AudioEngine empty; setup(empty);
        require(empty.exportMidi(dir.getChildFile("empty.mid")).failed(), "Empty project must not write MIDI");
    });

    suite.run("export between locators has the exact length and matches the full mix", [&]
    {
        juce::AudioBuffer<float> sweep(1, 96000);
        for (int i = 0; i < sweep.getNumSamples(); ++i) sweep.setSample(0, i, 0.5f * std::sin(0.0003f * (float) i * (float) i * 0.02f + 0.01f * (float) i));
        const auto source = writeWav(dir, "sweep.wav", rate, sweep);
        AudioEngine engine; setup(engine); importFixture(engine, source);
        const auto full = dir.getChildFile("range-full.wav"), part = dir.getChildFile("range-part.wav");
        requireOk(engine.exportMix(full, rate, 32));
        requireOk(engine.exportMix(part, rate, 32, 0.5, 1.25));
        std::unique_ptr<juce::AudioFormatReader> rf(engine.formatManager.createReaderFor(full)), rp(engine.formatManager.createReaderFor(part));
        require(rf != nullptr && rp != nullptr, "Range export not readable");
        require(rp->lengthInSamples == 36000, "Range length " + juce::String(rp->lengthInSamples) + " != 36000");
        juce::AudioBuffer<float> a(2, 36000), b(2, 36000);
        require(rf->read(&a, 0, 36000, 24000, true, true) && rp->read(&b, 0, 36000, 0, true, true), "Cannot decode range exports");
        float worst = 0.0f, level = 0.0f;
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 36000; ++i) { worst = juce::jmax(worst, std::abs(a.getSample(c, i) - b.getSample(c, i))); level = juce::jmax(level, std::abs(b.getSample(c, i))); }
        require(level > 0.1f, "Range export is silent");
        near(worst, 0.0, 1e-6, "Range samples differ from the full mix");
        require(engine.exportMix(dir.getChildFile("bad.wav"), rate, 32, -1.0, 1.0).failed(), "Negative range accepted");
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

    suite.run("MIDI quantize: exact grid positions, strength, triplets, length and selection", [&]
    {
        using namespace domain;
        near(gridBeats(4, false), 1.0, 1e-12, "1/4 grid");near(gridBeats(8, false), 0.5, 1e-12, "1/8 grid");
        near(gridBeats(16, false), 0.25, 1e-12, "1/16 grid");near(gridBeats(32, false), 0.125, 1e-12, "1/32 grid");near(gridBeats(8, true), 1.0 / 3.0, 1e-12, "1/8 triplet grid");
        near(quantizeBeat(0.37, 1.0, 100), 0.0, 1e-12, "0.37 at 1/4 lands on 0");
        near(quantizeBeat(0.37, 0.25, 100), 0.25, 1e-12, "0.37 at 1/16 lands on 0.25");
        near(quantizeBeat(0.37, 0.25, 50), 0.31, 1e-12, "strength 50% is the midpoint");
        near(quantizeBeat(0.37, 0.25, 0), 0.37, 1e-12, "strength 0% leaves the note");
        near(quantizeBeat(0.70, 1.0 / 3.0, 100), 2.0 / 3.0, 1e-12, "0.70 at 1/8 triplet lands on 2/3");
        std::vector<MidiNote> notes(3);notes[0].startBeats = 0.37;notes[1].startBeats = 1.2;notes[1].lengthBeats = 0.9;notes[2].startBeats = 2.1;
        QuantizeSettings q;q.division = 16;
        require(quantizeNotes(notes, 1, q) == 1, "only the indexed note changes");
        near(notes[0].startBeats, 0.37, 0, "unselected note untouched");near(notes[1].startBeats, 1.25, 1e-12, "selected note quantized");near(notes[1].lengthBeats, 0.9, 0, "length untouched by default");
        q.quantizeLength = true;require(quantizeNotes(notes, -1, q) == 3, "index -1 quantizes all notes");
        near(notes[0].startBeats, 0.25, 1e-12, "all: first note");near(notes[2].startBeats, 2.0, 1e-12, "all: third note");near(notes[1].lengthBeats, 1.0, 1e-12, "length 0.9 snaps to 1.0");
        MidiNote tiny;tiny.lengthBeats = 0.05;quantizeNote(tiny, q);near(tiny.lengthBeats, 0.25, 1e-12, "quantized length is never below one grid step");
    });

    suite.run("MIDI note selection: region, move/resize/erase groups and quantize selection", [&]
    {
        using namespace domain;
        std::vector<MidiNote> notes(4);
        notes[0].pitch = 60; notes[0].startBeats = 0.0; notes[0].lengthBeats = 1.0;
        notes[1].pitch = 64; notes[1].startBeats = 1.0; notes[1].lengthBeats = 0.5;
        notes[2].pitch = 67; notes[2].startBeats = 2.0; notes[2].lengthBeats = 1.0;
        notes[3].pitch = 72; notes[3].startBeats = 4.0; notes[3].lengthBeats = 1.0;
        auto region = notesInRegion(notes, 0.5, 2.1, 62, 70);
        require(region.size() == 2 && region.contains(notes[1].id) && region.contains(notes[2].id), "region selects notes intersecting beats 0.5-2.1 and pitches 62-70");
        require(notesInRegion(notes, 2.1, 0.5, 70, 62).size() == 2, "region accepts reversed corners");
        require(notesInRegion(notes, 1.0, 1.0, 0, 127).size() == 0 && notesInRegion(notes, 0.99, 1.01, 0, 127).size() == 2, "region edges: touching end excluded, overlap included");
        NoteSelection sel; sel.add(notes[0].id);
        require(sel.toggle(notes[2].id) && ! sel.toggle(notes[0].id) && sel.size() == 1, "toggle adds and removes");
        sel.add(notes[0].id); sel.add(notes[1].id);
        const auto idx = selectedIndices(notes, sel);
        require(idx.size() == 3 && idx[0] == 0 && idx[2] == 2, "selected indices follow model order");
        auto target = notes;
        require(moveNotes(notes, target, idx, 1, 1.1, 2, 0.25), "move group changes notes");
        near(target[1].startBeats, 2.0, 0, "anchor lands exactly on grid");near(target[0].startBeats, 1.0, 1e-12, "group keeps interval (first)");near(target[2].startBeats, 3.0, 1e-12, "group keeps interval (third)");
        require(target[0].pitch == 62 && target[1].pitch == 66 && target[2].pitch == 69 && target[3].pitch == 72 && target[3].startBeats == 4.0, "pitches move together; unselected note untouched");
        target = notes; moveNotes(notes, target, idx, 1, -5.0, 100, 0.25);
        near(target[0].startBeats, 0.0, 0, "group clamped at beat 0");near(target[2].startBeats, 2.0, 1e-12, "clamp keeps intervals");
        require(target[2].pitch == 127 && target[0].pitch == 120, "group clamped at pitch 127 keeping intervals");
        require(! moveNotes(notes, target, idx, 1, 0.0, 0, 0.25) || target[1].startBeats == 1.0, "zero move restores originals");
        target = notes; require(! moveNotes(notes, target, idx, 1, 0.05, 0, 0.25), "sub-grid move with snap does not change anything");
        target = notes; resizeNotes(notes, target, idx, 0, 0.4, 0.25);
        near(target[0].lengthBeats, 1.5, 0, "anchor length snapped to 1.5");near(target[1].lengthBeats, 1.0, 1e-12, "same length change for the group");near(target[2].lengthBeats, 1.5, 1e-12, "third note +0.5");
        require(target[0].startBeats == 0.0 && target[1].pitch == 64, "resize keeps start and pitch");
        target = notes; resizeNotes(notes, target, idx, 0, -3.0, 0.25);
        near(target[0].lengthBeats, 0.0625, 0, "anchor never below minimum");near(target[1].lengthBeats, 0.0625, 0, "group notes never below minimum");
        std::vector<MidiNote> single(1); single[0].startBeats = 2.0; auto one = single;
        moveNotes(single, one, {0}, 0, 1.13, 2, 0.25);
        require(one[0].startBeats == 3.25 && one[0].pitch == 62, "single note move equals previous rule max(0, snap(start+delta))");
        one = single; resizeNotes(single, one, {0}, 0, 0.5, 0.25);
        require(one[0].lengthBeats == 1.5, "single note resize equals previous rule max(1/16, snap(len+delta))");
        std::vector<MidiNote> q = notes; q[0].startBeats = 0.3; q[3].startBeats = 4.3;
        QuantizeSettings qs; qs.division = 4;
        require(quantizeSelectedNotes(q, {0, 1}, qs) == 1 && q[0].startBeats == 0.0 && q[3].startBeats == 4.3, "quantize acts only on the selection");
        require(quantizeSelectedNotes(q, {}, qs) == 1 && q[3].startBeats == 4.0, "empty selection quantizes all");
        auto erased = notes; require(eraseNotes(erased, sel) == 3 && erased.size() == 1 && erased[0].id == notes[3].id, "erase removes exactly the selection");
        require(selectAllNotes(notes).size() == 4, "select all");
    });

    suite.run("tempo map: jumps, ramps, signatures and the single-tempo identity", [&]
    {
        using namespace domain;
        // No events: bit-identical to the historical single-tempo formulas.
        const TempoView flat { 97.3, nullptr };
        TempoMap emptyMap; const TempoView flatMap { 97.3, &emptyMap };
        for (double b : { 0.0, 0.013, 1.0 / 3.0, 7.25, 1234.5678 })
        {
            require(flat.secondsAt(b) == b * 60.0 / 97.3 && flatMap.secondsAt(b) == b * 60.0 / 97.3, "flat secondsAt identity");
            require(flat.beatsAt(b) == b * 97.3 / 60.0 && flatMap.beatsAt(b) == b * 97.3 / 60.0, "flat beatsAt identity");
            require(flat.beatsBetween(0.5, b) == (b - 0.5) * 97.3 / 60.0, "flat beatsBetween identity");
        }
        require(flat.isConstant() && flatMap.isConstant(), "empty map is constant");
        require(flat.beatsFromBarsBeats(3, 2, 3, 60) == (3.0 - 1) * 4 + (2.0 - 1) + (3.0 - 1) / 4 + 60.0 / 480, "flat bars.beats identity");
        // 120 -> 90 BPM jump at beat 8.
        TempoMap jump; jump.tempos.push_back({ 8.0, 90.0, false });
        const TempoView j { 120.0, &jump };
        near(j.secondsAt(8.0), 4.0, 1e-12, "jump: beat 8"); near(j.secondsAt(12.0), 4.0 + 4.0 * 60.0 / 90.0, 1e-12, "jump: beat 12");
        near(j.beatsAt(6.0), 11.0, 1e-12, "jump: 6 s"); near(j.beatsAt(3.0), 6.0, 1e-12, "jump: 3 s");
        near(j.bpmAt(7.999), 120.0, 0, "jump: before"); near(j.bpmAt(8.0), 90.0, 0, "jump: at point");
        near(j.beatsBetween(3.0, 6.0), 5.0, 1e-12, "jump: beats between");
        // Linear ramp (in beats) 120 -> 60 over beats 0–4: t(b) = 60/k · ln(v(b)/120), k = -15.
        TempoMap ramp; ramp.tempos.push_back({ 4.0, 60.0, true });
        const TempoView r { 120.0, &ramp };
        near(r.secondsAt(4.0), 4.0 * std::log(2.0), 1e-12, "ramp: end"); near(r.secondsAt(2.0), 4.0 * std::log(4.0 / 3.0), 1e-12, "ramp: middle");
        near(r.bpmAt(2.0), 90.0, 1e-12, "ramp: tempo in the middle"); near(r.secondsAt(6.0), 4.0 * std::log(2.0) + 2.0, 1e-12, "ramp: after");
        near(r.beatsAt(4.0 * std::log(4.0 / 3.0)), 2.0, 1e-12, "ramp: inverse");
        // Accelerando and a nearly flat ramp keep the inverse exact.
        TempoMap mixed; mixed.tempos = { { 4.0, 60.0, true }, { 8.0, 150.0, false }, { 16.0, 150.000001, true }, { 20.0, 200.0, true } };
        const TempoView m { 120.0, &mixed };
        for (double b = 0.0; b < 30.0; b += 0.37) near(m.beatsAt(m.secondsAt(b)), b, 1e-9, "round trip at beat " + juce::String(b));
        for (double b = 0.0; b < 29.0; b += 0.5) require(m.secondsAt(b + 0.5) > m.secondsAt(b), "time is monotonic");
        // 3/4 for bars 1–2, 4/4 from bar 3, 6/8 from bar 5 (1-based).
        TempoMap sig; sig.signatures = { { 4, 6, 8 }, { 0, 3, 4 }, { 2, 4, 4 } }; sig.normalise();
        require(sig.signatures.front().bar == 0 && sig.signatures.back().bar == 4, "signatures sorted");
        const TempoView s { 120.0, &sig };
        require(! s.isConstant() && s.constantTempo(), "signature map is not plain 4/4");
        near(s.barStart(1), 3.0, 0, "bar 2 start"); near(s.barStart(2), 6.0, 0, "bar 3 start"); near(s.barStart(4), 14.0, 0, "bar 5 start"); near(s.barStart(5), 17.0, 0, "bar 6 start");
        const auto p = s.barAt(7.0); require(p.bar == 2 && p.beatInBar == 1.0 && p.signature.numerator == 4, "beat 7 is bar 3, beat 2");
        const auto q = s.barAt(5.5); require(q.bar == 1 && q.beatInBar == 2.5 && q.signature.numerator == 3, "beat 5.5 is in bar 2 (3/4)");
        const auto e = s.barAt(15.5); require(e.bar == 4 && e.beatInBar == 1.5 && e.signature.denominator == 8, "beat 15.5 is in bar 5 (6/8)");
        near(s.beatsFromBarsBeats(5, 4, 1, 0), 15.5, 0, "5.4.1.0 in 6/8 counts eighths"); near(s.beatsFromBarsBeats(3, 1, 1, 0), 6.0, 0, "3.1.1.0");
        // normalise: invalid and duplicate entries.
        TempoMap dirty; dirty.tempos = { { 0.0, 90.0 }, { 4.0, 1000.0 }, { 2.0, 100.0 }, { 4.0, 80.0 } }; dirty.signatures = { { 1, 5, 3 }, { -1, 3, 4 }, { 1, 7, 8 } };
        dirty.normalise();
        require(dirty.tempos.size() == 2 && dirty.tempos[0].beat == 2.0 && dirty.tempos[1].bpm == 80.0, "tempo normalise");
        require(dirty.signatures.size() == 1 && dirty.signatures[0].numerator == 7, "signature normalise");
    });

    suite.run("tempo map in the engine: notes, metronome, save/load and MIDI file round trip", [&]
    {
        // A note at beat 12 after a 120 -> 90 jump at beat 8 starts at 4 + 4 · 60/90 s.
        {
            AudioEngine engine; setup(engine); engine.tempo = 120.0;
            engine.tempoMap.tempos.push_back({ 8.0, 90.0, false });
            auto midi = engine.addMidiTrack("Map");
            MidiNote note; note.pitch = 69; note.velocity = 100; note.startBeats = 12.0; note.lengthBeats = 1.0; midi->notes.push_back(note);
            near(engine.duration(), 4.0 + 5.0 * 60.0 / 90.0, 1e-9, "Duration follows the map");
            engine.play(); const auto out = render(engine, (int) (rate * 7.0));
            int first = -1;
            for (int i = 0; i < out.getNumSamples() && first < 0; ++i) if (std::abs(out.getSample(0, i)) > 1.0e-5f) first = i;
            near(first, (4.0 + 4.0 * 60.0 / 90.0) * rate, 2.0, "Note onset with the tempo jump");
        }
        // Metronome: beats 0, 1 at 120 BPM, beats 2, 3, 4 at 60 BPM; 3/4 bars so beat 3 is a downbeat.
        {
            AudioEngine engine; setup(engine); engine.tempo = 120.0; engine.metronome = true;
            engine.click.sound = (int) audio::ClickSound::classic; engine.click.levelDb = 0.0f; engine.click.subdivision = 1; engine.click.accent = true;
            engine.tempoMap.tempos.push_back({ 2.0, 60.0, false }); engine.tempoMap.signatures.push_back({ 0, 3, 4 });
            engine.play(); const auto found = onsets(render(engine, (int) (rate * 3.9)));
            require(found.size() == 5, "Expected 5 clicks with the map, got " + juce::String((int) found.size()));
            const double expected[] { 0.0, 0.5, 1.0, 2.0, 3.0 };
            for (size_t k = 0; k < found.size(); ++k) near(found[k].first, expected[k] * rate, 2.0, "Mapped click " + juce::String((int) k));
            require(found[3].second > found[2].second * 1.3f && found[0].second > found[1].second * 1.3f, "3/4 downbeats are not accented");
        }
        // Project file: the map survives save/load; a project without "tempoMap" reads an empty map.
        {
            AudioEngine source; setup(source); source.tempo = 120.0; source.addMidiTrack("Map");
            source.tempoMap.tempos = { { 8.0, 90.0, false }, { 16.0, 140.5, true } };
            source.tempoMap.signatures = { { 0, 3, 4 }, { 2, 7, 8 } };
            const auto file = dir.getChildFile("tempo-map.silente"); requireOk(source.saveProject(file));
            AudioEngine restored; setup(restored); requireOk(restored.loadProject(file));
            require(restored.tempoMap == source.tempoMap, "Tempo map changed after save/load");
            auto json = juce::JSON::parse(file);
            json.getDynamicObject()->removeProperty("tempoMap");
            const auto old = dir.getChildFile("tempo-map-old.silente"); require(old.replaceWithText(juce::JSON::toString(json)), "Cannot write old project");
            requireOk(restored.loadProject(old));
            require(restored.tempoMap.empty() && restored.tempoView().isConstant(), "Old project must read without tempo events");
            AudioEngine plain; setup(plain); plain.addMidiTrack("Plain");
            const auto plainFile = dir.getChildFile("plain.silente"); requireOk(plain.saveProject(plainFile));
            require(! plainFile.loadFileAsString().contains("tempoMap"), "A project without events must not write a tempo map");
        }
        // Standard MIDI File: jumps and signatures come back; a ramp keeps the elapsed time.
        {
            AudioEngine source; setup(source); source.tempo = 120.0;
            source.tempoMap.tempos = { { 8.0, 90.0, false }, { 12.0, 150.0, true } };
            source.tempoMap.signatures = { { 1, 3, 4 }, { 3, 6, 8 } };
            auto a = source.addMidiTrack("Notas");
            for (double start : { 0.0, 7.5, 8.0, 10.25, 13.0, 15.5 }) { MidiNote n; n.pitch = 60; n.startBeats = start; n.lengthBeats = 0.5; a->notes.push_back(n); }
            const auto file = dir.getChildFile("tempo-map.mid"); requireOk(source.exportMidi(file));
            AudioEngine back; setup(back); requireOk(back.importMidi(file));
            near(back.tempo.load(), 120.0, 1e-9, "Base tempo");
            require(back.tempoMap.signatures == source.tempoMap.signatures, "Signatures after MIDI round trip");
            const auto s = source.tempoView(), b = back.tempoView();
            near(b.bpmAt(12.0), 150.0, 1e-3, "Tempo after the ramp"); require(b.bpmAt(8.0) > 90.0 && b.bpmAt(8.0) < 93.75, "First ramp step holds the mean tempo of its sixteenth"); near(b.bpmAt(7.9), 120.0, 1e-9, "Before the jump");
            for (double beat = 0.0; beat <= 16.0; beat += 0.25) near(b.secondsAt(beat), s.secondsAt(beat), 2e-5, "Time of beat " + juce::String(beat));
            auto notes = back.tracks.at(0)->notes; std::sort(notes.begin(), notes.end(), [](const MidiNote& x, const MidiNote& y) { return x.startBeats < y.startBeats; });
            require(notes.size() == a->notes.size(), "Note count after MIDI round trip");
            for (size_t k = 0; k < notes.size(); ++k) near(notes[k].startBeats, a->notes[k].startBeats, 0, "Note " + juce::String((int) k) + " beat");
            // A project that already has tracks keeps its map and places the notes by time.
            AudioEngine busy; setup(busy); busy.tempo = 60.0; busy.addMidiTrack("Existente"); requireOk(busy.importMidi(file));
            require(busy.tempoMap.empty() && busy.tempo.load() == 60.0, "Import into a busy project changed its tempo");
            auto placed = busy.tracks.at(1)->notes; std::sort(placed.begin(), placed.end(), [](const MidiNote& x, const MidiNote& y) { return x.startBeats < y.startBeats; });
            for (size_t k = 0; k < placed.size(); ++k) near(placed[k].startBeats, s.secondsAt(a->notes[k].startBeats), 2e-5, "Busy note " + juce::String((int) k) + " at its time");
        }
    });

    suite.run("automation curve: evaluation, insert, move and remove with exact values", [&]
    {
        using namespace domain;
        AutomationCurve c;
        near(evaluate(c, 3.0, 0.25), 0.25, 0, "Empty curve returns the fallback");
        require(insertPoint(c, AutomationParameter::volume, { 4.0, 1.0 }) == 0, "First point index");
        require(insertPoint(c, AutomationParameter::volume, { 0.0, 0.0 }) == 0, "Earlier point goes first");
        require(insertPoint(c, AutomationParameter::volume, { 2.0, 0.75 }) == 1, "Middle point index");
        near(evaluate(c, -1.0, 9.0), 0.0, 0, "Before the first point holds its value");
        near(evaluate(c, 1.0, 9.0), 0.375, 0, "Linear between 0 and 2");
        near(evaluate(c, 3.0, 9.0), 0.875, 0, "Linear between 2 and 4");
        near(evaluate(c, 2.0, 9.0), 0.75, 0, "On a point");
        near(evaluate(c, 100.0, 9.0), 1.0, 0, "After the last point holds its value");
        require(insertPoint(c, AutomationParameter::volume, { 2.0, 0.5 }) == 1 && c.points.size() == 3, "Same beat replaces the value");
        near(evaluate(c, 1.0, 9.0), 0.25, 0, "Replaced value");
        require(insertPoint(c, AutomationParameter::volume, { 6.0, 9.0 }) == 3, "Append");
        near(c.points[3].value, 4.0, 0, "Volume clamps to +12 dB (gain 4)");
        require(movePoint(c, AutomationParameter::volume, 1, 5.0, 0.6), "Move");
        near(c.points[1].beat, 4.0, 0, "A point cannot pass its right neighbour");
        require(movePoint(c, AutomationParameter::volume, 1, -3.0, 0.6), "Move left");
        near(c.points[1].beat, 0.0, 0, "Nor its left neighbour");
        require(movePoint(c, AutomationParameter::volume, 1, 1.0, 0.5) && c.points[1] == AutomationPoint { 1.0, 0.5 }, "Free move");
        require(! movePoint(c, AutomationParameter::volume, 7, 1.0, 0.5) && ! removePoint(c, -1), "Invalid indices are refused");
        require(removePoint(c, 3) && c.points.size() == 3, "Remove");
        AutomationCurve step; step.points = { { 0.0, 0.0 }, { 4.0, 0.0 }, { 4.0, 1.0 } };
        near(evaluate(step, 3.999, 9.0), 0.0, 0, "Before a vertical step"); near(evaluate(step, 4.0, 9.0), 1.0, 0, "At a step the later point applies");
        AutomationCurve pan; insertPoint(pan, AutomationParameter::pan, { 0.0, -5.0 }); near(pan.points[0].value, -1.0, 0, "Pan clamps to -1");
        AutomationCurve send; insertPoint(send, AutomationParameter::send, { 0.0, 20.0 }); near(send.points[0].value, 6.0, 0, "Send clamps to +6 dB");
        AutomationCurve messy; messy.points = { { 3.0, 0.5 }, { std::numeric_limits<double>::quiet_NaN(), 1.0 }, { -2.0, 7.0 } }; normalise(messy, AutomationParameter::volume);
        require(messy.points == std::vector<AutomationPoint> { { 0.0, 4.0 }, { 3.0, 0.5 } }, "normalise sorts, clamps and drops non-finite points");
        TrackAutomation t; require(t.empty() && ! t.reads({}), "Default automation is empty");
        t.sends[5] = c; require(! t.empty() && t.curve({ AutomationParameter::send, 5 }) == c && ! t.reads({ AutomationParameter::send, 5 }), "Points without Read do not read");
        t.read = true; require(t.reads({ AutomationParameter::send, 5 }) && ! t.reads({ AutomationParameter::volume }), "Read follows only curves with points");
    });

    suite.run("automation in the engine: volume ramp, pan, send, smoothing, identity and save/load", [&]
    {
        using namespace domain;
        const auto dcClip = [](float value, double seconds)
        {
            AudioClip clip; clip.name = "DC"; clip.sourceRate = rate; clip.durationSeconds = seconds; clip.fadeIn = clip.fadeOut = 0.0;
            clip.audio = std::make_shared<juce::AudioBuffer<float>>(2, (int) std::llround(seconds * rate));
            for (int c = 0; c < 2; ++c) juce::FloatVectorOperations::fill(clip.audio->getWritePointer(c), value, clip.audio->getNumSamples());
            return clip;
        };
        const float centre = std::cos(juce::MathConstants<float>::pi * 0.25f);
        // Volume 0 -> 1 over beats 0-8 (4 s at 120 BPM): the gain at sample n is n / 192000.
        {
            AudioEngine engine; setup(engine); engine.tempo = 120.0;
            auto t = engine.addAudioTrack("Auto"); t->clips.push_back(dcClip(0.5f, 6.0)); t->gain = 0.8f;
            t->automation.volume.points = { { 0.0, 0.0 }, { 8.0, 1.0 } }; t->automation.read = true;
            engine.play(); const auto out = render(engine, (int) (rate * 5.0));
            for (int n : { 0, 1, 77, 1000, 48000, 96123, 150001, 191999, 192000, 230000 })
            {
                const double expected = 0.5 * centre * juce::jmin(1.0, n / 192000.0);
                near(out.getSample(0, n), expected, 2.0e-6, "Volume ramp L at sample " + juce::String(n));
                near(out.getSample(1, n), expected, 2.0e-6, "Volume ramp R at sample " + juce::String(n));
            }
            // Beats follow the tempo: at 60 BPM the same curve is half way at 4 s.
            AudioEngine slow; setup(slow); slow.tempo = 60.0;
            auto s = slow.addAudioTrack("Lenta"); s->clips.push_back(dcClip(0.5f, 6.0)); s->automation = t->automation;
            slow.play(); const auto o = render(slow, (int) (rate * 4.5));
            near(o.getSample(0, (int) (4.0 * rate)), 0.5 * centre * 0.5, 2.0e-6, "Curve in beats at 60 BPM");
            // Read off: the static fader applies.
            AudioEngine off; setup(off); off.tempo = 120.0;
            auto f = off.addAudioTrack("Off"); f->clips.push_back(dcClip(0.5f, 6.0)); f->gain = 0.8f; f->automation = t->automation; f->automation.read = false;
            off.play(); const auto r = render(off, (int) (rate * 1.0));
            near(r.getSample(0, 30000), 0.5 * 0.8 * centre, 1.0e-6, "Read off uses the fader");
        }
        // Pan -1 -> +1 over beats 0-8; exact at block edges (128 samples).
        {
            AudioEngine engine; setup(engine); engine.tempo = 120.0;
            auto t = engine.addAudioTrack("Pan"); t->clips.push_back(dcClip(0.5f, 6.0)); t->gain = 1.0f;
            t->automation.pan.points = { { 0.0, -1.0 }, { 8.0, 1.0 } }; t->automation.read = true;
            engine.play(); const auto out = render(engine, (int) (rate * 4.5));
            near(out.getSample(0, 0), 0.5, 1.0e-6, "Pan hard left: L"); near(out.getSample(1, 0), 0.0, 1.0e-6, "Pan hard left: R");
            near(out.getSample(0, 96000), 0.5 * centre, 2.0e-6, "Pan centre at 2 s: L"); near(out.getSample(1, 96000), 0.5 * centre, 2.0e-6, "Pan centre at 2 s: R");
            near(out.getSample(0, 200000), 0.0, 1.0e-6, "Pan hard right: L"); near(out.getSample(1, 200000), 0.5, 1.0e-6, "Pan hard right: R");
        }
        // Send -60 -> 0 dB (pre-fader) over beats 0-8 to an FX channel; the source fader is down.
        {
            AudioEngine engine; setup(engine); engine.tempo = 120.0;
            auto t = engine.addAudioTrack("Fuente"); t->clips.push_back(dcClip(0.5f, 6.0)); t->gain = 0.0f;
            auto fx = engine.addTrack(TrackKind::fx, "FX"); fx->gain = 1.0f;
            requireOk(engine.setSend(*t, 0, fx->id)); t->sends[0].preFader = true; t->sends[0].levelDb = -60.0f;
            t->automation.sends[0].points = { { 0.0, -60.0 }, { 8.0, 0.0 } }; t->automation.read = true;
            engine.play(); const auto out = render(engine, (int) (rate * 4.5));
            near(out.getSample(0, 96000), 0.5 * std::pow(10.0, -30.0 / 20.0) * centre, 1.0e-6, "Send at -30 dB at 2 s");
            near(out.getSample(0, 200000), 0.5 * centre, 2.0e-6, "Send at 0 dB after the curve");
            near(out.getSample(0, 0), 0.0, 1.0e-9, "Send at -60 dB is off");
        }
        // A vertical step 0 -> 1 at beat 4 is smoothed over one block: no sample jumps.
        {
            AudioEngine engine; setup(engine); engine.tempo = 120.0;
            auto t = engine.addAudioTrack("Salto"); t->clips.push_back(dcClip(0.5f, 6.0));
            t->automation.volume.points = { { 0.0, 0.0 }, { 4.0, 0.0 }, { 4.0, 1.0 } }; t->automation.read = true;
            engine.play(); const auto out = render(engine, (int) (rate * 3.0));
            double jump = 0.0;
            for (int i = 1; i < out.getNumSamples(); ++i) jump = std::max(jump, (double) std::abs(out.getSample(0, i) - out.getSample(0, i - 1)));
            require(jump < 0.5 * centre / 100.0 && std::abs(out.getSample(0, 100000) - 0.5f * centre) < 1.0e-6f,
                    "Step not smoothed: largest sample jump " + juce::String(jump, 6));
        }
        // Identity: Read off with curves, or a flat curve at the fader value, renders bit-identical to no automation.
        {
            const auto build = [&](AudioEngine& engine, int variant)
            {
                setup(engine); engine.tempo = 97.0;
                auto a = engine.addAudioTrack("A"); a->clips.push_back(dcClip(0.3f, 2.0)); a->gain = 0.8f; a->pan = 0.2f;
                auto fx = engine.addTrack(TrackKind::fx, "FX"); fx->gain = 0.6f;
                requireOk(engine.setSend(*a, 0, fx->id)); a->sends[0].levelDb = -6.0f;
                if (variant == 1) { a->automation.volume.points = { { 0.0, 0.1 }, { 4.0, 2.0 } }; a->automation.sends[0].points = { { 1.0, -20.0 } }; }
                if (variant == 2) { a->automation.read = true; a->automation.volume.points = { { 0.0, 0.8 } }; a->automation.pan.points = { { 3.0, 0.2 } }; a->automation.sends[0].points = { { 0.0, -6.0 } }; }
                engine.play(); return render(engine, (int) (rate * 2.5));
            };
            AudioEngine e0, e1, e2; const auto plain = build(e0, 0), readOff = build(e1, 1), flat = build(e2, 2);
            bool same1 = true, same2 = true;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < plain.getNumSamples(); ++i)
                {
                    same1 = same1 && plain.getSample(c, i) == readOff.getSample(c, i);
                    same2 = same2 && plain.getSample(c, i) == flat.getSample(c, i);
                }
            require(same1 && same2 && peak(plain) > 0.05, "Renders without active automation must be bit-identical");
        }
        // Project file: automation survives save/load; older projects and empty automation write nothing.
        {
            AudioEngine source; setup(source);
            auto a = source.addAudioTrack("A"); auto fx = source.addTrack(TrackKind::fx, "FX"); requireOk(source.setSend(*a, 3, fx->id));
            a->automation.read = true; a->automation.write = true;
            a->automation.volume.points = { { 0.0, 0.25 }, { 2.5, 1.5 }, { 2.5, 0.5 } };
            a->automation.pan.points = { { 1.0, -0.75 } };
            a->automation.sends[3].points = { { 0.5, -12.5 }, { 9.0, 3.0 } };
            const auto file = dir.getChildFile("automation.silente"); requireOk(source.saveProject(file));
            AudioEngine restored; setup(restored); requireOk(restored.loadProject(file));
            require(restored.tracks.size() == 2 && restored.tracks[0]->automation == a->automation && restored.tracks[1]->automation.empty(), "Automation changed after save/load");
            auto json = juce::JSON::parse(file);
            for (auto& t : *json["tracks"].getArray()) t.getDynamicObject()->removeProperty("automation");
            const auto old = dir.getChildFile("automation-old.silente"); require(old.replaceWithText(juce::JSON::toString(json)), "Cannot write old project");
            requireOk(restored.loadProject(old));
            require(restored.tracks[0]->automation.empty(), "Old project must read without automation");
            AudioEngine plain; setup(plain); plain.addAudioTrack("Plain");
            const auto plainFile = dir.getChildFile("automation-plain.silente"); requireOk(plain.saveProject(plainFile));
            require(! plainFile.loadFileAsString().contains("automation"), "A project without automation must not write it");
        }
    });

    suite.run("media pool: grouping, usage counts, present/missing/replaced, sorting and relink", [&]
    {
        using namespace domain;
        // Pure model: three references to a.wav, one to b.wav (missing), one in-memory clip.
        const auto ref = [](const char* path, const char* name, long long length, int channels, double hz)
        { MediaClipRef r; r.path = path; r.name = name; r.lengthSamples = length; r.channels = channels; r.sampleRate = hz; return r; };
        std::vector<MediaClipRef> refs { ref("/m/a.wav", "a.wav", 48000, 1, 48000.0), ref("/m/b.wav", "b.wav", 22050, 2, 44100.0),
                                         ref("/m/a.wav", "a.wav", 48000, 1, 48000.0), ref("", "memoria", 100, 1, 48000.0), ref("/m/a.wav", "a.wav", 48000, 1, 48000.0) };
        const auto exists = [](const juce::String& path) { return path == "/m/a.wav"; };
        auto pool = buildPool(refs, exists);
        require(pool.size() == 3, "Pool must group by path (and keep the unnamed clip apart)");
        require(pool[0].path == "/m/a.wav" && pool[0].uses == 3 && pool[0].lengthSamples == 48000 && pool[0].seconds == 1.0 && pool[0].channels == 1 && pool[0].state == MediaState::present, "Medium a.wav: 3 uses, 48000 samples, 1.000 s, present");
        require(pool[1].path == "/m/b.wav" && pool[1].uses == 1 && pool[1].state == MediaState::missing && pool[1].channels == 2, "Medium b.wav: 1 use, missing");
        require(pool[1].seconds == 0.5 && pool[2].uses == 1 && pool[2].state == MediaState::present, "b.wav lasts 0.5 s; the in-memory medium counts as present");
        sortPool(pool, MediaColumn::uses, false);
        require(pool[0].path == "/m/a.wav" && pool[1].path == "/m/b.wav" && pool[2].path.isEmpty(), "Sort by uses, descending, is stable");
        sortPool(pool, MediaColumn::duration, true);
        require(pool[0].lengthSamples == 100 && pool[1].lengthSamples == 22050 && pool[2].lengthSamples == 48000, "Sort by duration, ascending");
        sortPool(pool, MediaColumn::state, true);
        require(pool.back().state == MediaState::missing, "Sort by state puts missing last");
        require(reassignPath(refs, "/m/b.wav", "/m/c.wav") == 1 && refs[1].path == "/m/c.wav" && reassignPath(refs, "/m/zzz.wav", "/m/q.wav") == 0, "Reassign changes exactly the clips of that medium");
        const auto replaced = buildPool(refs, [](const juce::String&) { return true; }, { "/m/c.wav" });
        require(replaced[1].state == MediaState::replaced && replaced[0].state == MediaState::present, "A reassigned path that exists is replaced");
        // Engine: two clips on mono.wav, one on stereo.wav; copy mono.wav, delete the copy and relink.
        AudioEngine engine; setup(engine);
        auto t1 = importFixture(engine, mono); requireOk(engine.importAudio(mono, 0, 3.0));
        importFixture(engine, stereo);
        const auto live = buildPool(engine.mediaClips(), [](const juce::String& p) { return juce::File(p).existsAsFile(); });
        require(live.size() == 2 && live[0].uses == 2 && live[0].lengthSamples == 48000 && live[0].channels == 1 && live[0].state == MediaState::present, "Engine pool: mono.wav used twice");
        require(live[1].uses == 1 && live[1].channels == 2 && live[1].lengthSamples == 48000 && live[1].name == "stereo.wav", "Engine pool: stereo.wav used once, 2 channels");
        const auto gone = dir.getChildFile("gone.wav"); mono.copyFileTo(gone);
        auto t2 = engine.addAudioTrack("Copia"); requireOk(engine.importAudio(gone, 2, 0.0)); requireOk(engine.importAudio(gone, 2, 5.0));
        gone.deleteFile();
        auto withGone = buildPool(engine.mediaClips(), [](const juce::String& p) { return juce::File(p).existsAsFile(); });
        int missingCount = 0; for (const auto& e : withGone) if (e.state == MediaState::missing) { ++missingCount; require(e.uses == 2 && e.path == gone.getFullPathName(), "Missing medium has its 2 uses"); }
        require(missingCount == 1, "Exactly one missing medium");
        int changed = -1;
        require(engine.relinkMedia(gone.getFullPathName(), stereo, &changed).wasOk() && changed == 2, "Relink replaces both clips");
        require(t2->clips[0].file == stereo && t2->clips[1].file == stereo && t2->clips[0].audio->getNumChannels() == 2 && t2->clips[1].startSeconds == 5.0, "Relinked clips keep position and use the new audio");
        require(engine.relinkMedia(gone.getFullPathName(), stereo, &changed).failed() && changed == 0, "Relinking a path nobody uses fails");
        require(engine.relinkMedia(stereo.getFullPathName(), dir.getChildFile("nada.wav"), &changed).failed() && t2->clips[0].file == stereo, "Relinking to an unreadable file changes nothing");
        const auto after = buildPool(engine.mediaClips(), [](const juce::String& p) { return juce::File(p).existsAsFile(); });
        require(after.size() == 2 && after[1].uses == 3, "After relinking, stereo.wav has 3 uses");
    });
    suite.run("Cubase .cpr import: tempo, signature, tracks, verified clips and event positions", [&]
    {
        using namespace io::cpr;
        // Fixture audio: «toma.wav» 48000 frames, 2 channels, 24 bit, 48 kHz; «mal.wav» exists but the .cpr says 999 frames.
        juce::AudioBuffer<float> two(2, 48000);
        for (int i = 0; i < two.getNumSamples(); ++i) { two.setSample(0, i, 0.125f); two.setSample(1, i, -0.25f); }
        const auto cprFolder = dir.getChildFile("cubase"); cprFolder.getChildFile("Audio").createDirectory();
        writeWav(cprFolder.getChildFile("Audio"), "toma.wav", rate, two);
        writeWav(cprFolder.getChildFile("Audio"), "mal.wav", rate, two);
        CprBuilder w(true);
        w.begin("MTempoTrackEvent", 3); w.u32(1); w.f32(60.0f / 90.0f); w.u32(0); w.end();                 // 90 BPM stored as f32
        w.begin("MSignatureTrackEvent", 3); w.u32(1); w.u32(0); w.u16(3); w.u16(4);
        w.begin("FAttributes", 0); w.u32(2); w.attribute("Numerator", 3); w.attribute("Denominator", 4); w.end(); w.end();
        w.begin("MAudioTrackEvent", 1); w.begin("MListNode", 0); w.str("Bajo");
        w.event(960.0, 24000.0, 4800.0); const auto toma = w.clip("toma", "toma.wav", "/sin/carpeta/", 48000, 24, 2, 48000.0f); w.end();
        w.event(1920.0, 12000.0, 0.0); w.word(toma); w.end();               // reference to the clip already written
        w.event(3840.0, 47000.0, 2000.0); w.word(toma); w.end();            // ends 1000 samples past the file: skipped
        w.end(); w.end();
        w.begin("MAudioTrackEvent", 1); w.begin("MListNode", 0); w.str("Voz");
        w.event(0.0, 100.0, 0.0); w.clip("falta", "falta.wav", "/sin/carpeta/", 48000, 24, 2, 48000.0f); w.end();
        w.event(480.0, 100.0, 0.0); w.clip("mal (2)", "mal.wav", "/sin/carpeta/", 999, 24, 2, 48000.0f); w.end();
        w.end(); w.end();
        w.begin("MFadeIn", 0); w.end();
        const auto cpr = cprFolder.getChildFile("sintetico.cpr");
        require(cpr.replaceWithData(w.file().getData(), w.file().getSize()), "Cannot write the synthetic .cpr");
        ProjectInfo info;
        requireOk(readProject(cpr, info));
        require(info.wide && info.tempoPoints == 1 && info.numerator == 3 && info.denominator == 4, "Tempo points and 3/4 signature");
        near(info.bpm(), 90.0, 0.0, "f32 seconds per quarter rounds to 90.000 BPM");
        require(info.tracks.size() == 2 && info.tracks[0].name == "Bajo" && info.tracks[1].name == "Voz", "Two audio tracks with names");
        require(info.clips.size() == 3 && info.clips[0].name == "toma" && info.clips[0].fileName == "toma.wav" && info.clips[0].frames == 48000
                && info.clips[0].bits == 24 && info.clips[0].channels == 2 && info.clips[0].sampleRate == 48000.0 && info.clips[0].folder == "/sin/carpeta/", "Clip toma: file and format");
        require(info.events.size() == 5, "Five audio events, got " + juce::String((int) info.events.size()));
        require(info.events[0].track == 0 && info.events[0].clip == 0 && info.events[0].startTicks == 960.0 && info.events[0].lengthSamples == 24000.0 && info.events[0].offsetSamples == 4800.0, "Event 1 fields");
        require(info.events[1].clip == 0 && info.events[2].clip == 0 && info.events[3].track == 1 && info.events[3].clip == 1 && info.events[4].clip == 2, "Events resolve inline clips and references");
        require(info.warnings.joinIntoString("|").contains(i18n::tr("Fundidos de entrada no importados: ") + "1"), "The fade-in object is reported as omitted");
        const auto resolution = resolveClips(info, cpr);
        require(resolution[0].status == ClipStatus::verified && resolution[0].file == cprFolder.getChildFile("Audio/toma.wav") && ! resolution[0].nameDiffers, "toma.wav verified in Audio/");
        require(resolution[1].status == ClipStatus::missing, "falta.wav is missing");
        require(resolution[2].status == ClipStatus::mismatch && resolution[2].reason.contains("999") && resolution[2].nameDiffers, "mal.wav: frames differ and the name differs");
        AudioEngine engine; setup(engine);
        PreparedImport prepared;
        requireOk(prepare(engine, cpr, prepared));
        require(prepared.audio[0] != nullptr && prepared.audio[1] == nullptr && prepared.audio[2] == nullptr, "Only the verified clip is decoded");
        require(prepared.eventSkip[0].isEmpty() && prepared.eventSkip[1].isEmpty() && prepared.eventSkip[2].isNotEmpty() && prepared.eventSkip[3].isNotEmpty() && prepared.eventSkip[4].isNotEmpty(), "Out-of-file and unverified events are skipped");
        const auto summary = apply(engine, prepared);
        require(summary.tracks == 2 && summary.clips == 3 && summary.clipsVerified == 1 && summary.clipsImported == 1 && summary.events == 5 && summary.eventsImported == 2, "Summary counts");
        require(engine.tracks.size() == 2 && engine.tracks[0]->name == "Bajo" && engine.tracks[1]->name == "Voz" && engine.tracks[0]->clips.size() == 2 && engine.tracks[1]->clips.empty(), "Engine tracks and clips");
        near(engine.tempo.load(), 90.0, 0.0, "Project tempo");
        require(engine.tempoMap.tempos.empty() && engine.tempoMap.signatures.size() == 1 && engine.tempoMap.signatures[0] == domain::SignatureEvent { 0, 3, 4 }, "3/4 from bar 1");
        const auto& first = engine.tracks[0]->clips[0];
        near(first.startSeconds, 2.0 * 60.0 / 90.0, 1e-12, "960 ticks = 2 quarters at 90 BPM");
        near(first.offsetSeconds, 0.1, 1e-12, "Offset 4800 samples");
        near(first.durationSeconds, 0.5, 1e-12, "Length 24000 samples");
        require(first.name == "toma" && first.audio && first.audio->getNumSamples() == 48000 && first.sourceRate == rate, "Clip audio and name");
        near(engine.tracks[0]->clips[1].startSeconds, 4.0 * 60.0 / 90.0, 1e-12, "1920 ticks = 4 quarters");
        require(engine.tracks[0]->clips[1].id != first.id && engine.tracks[0]->clips[1].audio == first.audio, "Shared audio, distinct clip ids");
        require(summary.report.contains("falta") && summary.report.contains("999") && summary.report.contains("mal (2) -> mal.wav"), "Report lists unverified and renamed clips");
        require(isInsideCubaseFolder(cprFolder.getChildFile("x.silente"), cpr) && isInsideCubaseFolder(cprFolder.getChildFile("Audio/x.silente"), cpr)
                && ! isInsideCubaseFolder(dir.getChildFile("x.silente"), cpr), "Never write next to the .cpr");
        // RIFF (Cubase 12, 4-byte words): tempo v2 and signature are read; events are not verified in that layout.
        CprBuilder r(false);
        r.begin("MTempoTrackEvent", 2); r.u32(1); r.f32(0.5f); r.u32(0); r.end();
        r.begin("MSignatureTrackEvent", 3); r.u32(1); r.u32(0); r.u16(4); r.u16(4);
        r.begin("FAttributes", 0); r.u32(2); r.attribute("Numerator", 4); r.attribute("Denominator", 4); r.end(); r.end();
        r.begin("MAudioTrackEvent", 1); r.begin("MListNode", 0); r.str("A"); r.event(0.0, 10.0, 0.0); r.end(); r.end(); r.end();
        const auto data = r.file();
        ProjectInfo old;
        requireOk(parse(data.getData(), data.getSize(), old));
        require(! old.wide && old.bpm() == 120.0 && old.numerator == 4 && old.tracks.size() == 1 && old.events.empty(), "RIFF: 120 BPM, 4/4, one track, no events imported");
        require(old.warnings.joinIntoString("|").contains(i18n::tr("Eventos de audio en un formato no verificado (versión o Cubase 12), omitidos: ") + "1"), "RIFF event reported");
        ProjectInfo bad;
        const char junk[] = "RIFF0000WAVEfmt ";
        require(parse(junk, sizeof(junk), bad).failed(), "A WAV is not a .cpr");
    });
    {
        // Optional integration case (read only; no audio decoded, nothing written next to the .cpr).
        const juce::File tom("/Applications/ARCHIVO FER/PRODUCCION MUSICAL/seguricopia tom pere mix fer/sesion con el tom-01.cpr");
        if (! tom.existsAsFile()) { suite.text += "SKIP Cubase tom-01 integration: file not present\n"; std::cout << "SKIP Cubase tom-01 integration" << std::endl; }
        else suite.run("Cubase tom-01 (real .cpr): 43 tracks, 99 clips verified against the WAVs, 187 events, 67 BPM, 2/4", [&]
        {
            using namespace io::cpr;
            ProjectInfo info;
            requireOk(readProject(tom, info));
            require(info.tracks.size() == 43 && info.clips.size() == 99 && info.events.size() == 187, "Counts " + juce::String((int) info.tracks.size()) + "/" + juce::String((int) info.clips.size()) + "/" + juce::String((int) info.events.size()));
            near(info.bpm(), 67.0, 0.0, "Tempo");
            require(info.numerator == 2 && info.denominator == 4 && info.tempoPoints == 1, "2/4, one tempo point");
            const auto resolution = resolveClips(info, tom);
            int verified = 0, renamed = 0;
            for (const auto& r : resolution) { verified += r.status == ClipStatus::verified; renamed += r.nameDiffers; }
            require(verified == 99 && renamed == 1, "Verified " + juce::String(verified) + ", renamed " + juce::String(renamed));
            for (const auto& e : info.events)
            {
                require(e.track >= 0 && e.clip >= 0, "Every event has a track and a clip");
                require(e.offsetSamples >= 0.0 && e.offsetSamples + e.lengthSamples <= (double) info.clips[(size_t) e.clip].frames + 0.5, "Event inside its file");
            }
        });
    }
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

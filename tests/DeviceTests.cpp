#include "DeviceTests.h"
#include "../src/AudioEngine.h"
#include "../src/ui/audio/AudioSettingsPanel.h"
#include <cmath>

namespace silente
{
namespace
{
class Device18 final:public juce::AudioIODevice
{
public:
    Device18():AudioIODevice("Simulated 18-channel interface","Test"){}
    juce::BigInteger inputs,outputs;
    juce::StringArray getOutputChannelNames() override{return names();}
    juce::StringArray getInputChannelNames() override{return names();}
    juce::Array<double> getAvailableSampleRates() override{return {48000,96000};}
    juce::Array<int> getAvailableBufferSizes() override{return {64,128,256,512};}
    int getDefaultBufferSize() override{return 128;}
    juce::String open(const juce::BigInteger& i,const juce::BigInteger& o,double,int)override{inputs=i;outputs=o;return {};}
    void close()override{}bool isOpen()override{return true;}void start(juce::AudioIODeviceCallback*)override{}void stop()override{}bool isPlaying()override{return true;}
    juce::String getLastError()override{return {};}
    int getCurrentBufferSizeSamples()override{return 128;}double getCurrentSampleRate()override{return 48000;}int getCurrentBitDepth()override{return 32;}
    juce::BigInteger getActiveInputChannels()const override{return inputs;}juce::BigInteger getActiveOutputChannels()const override{return outputs;}
    int getOutputLatencyInSamples()override{return 128;}int getInputLatencyInSamples()override{return 128;}
private:
    static juce::StringArray names(){juce::StringArray n;for(int i=1;i<=18;++i)n.add("Channel "+juce::String(i));return n;}
};
}
int runDeviceTests(const juce::File& output)
{
    output.createDirectory();juce::String report;int failures=0;
    const auto check=[&](bool ok,const juce::String& message){report+=(ok?"PASS: ":"FAIL: ")+message+"\n";if(!ok)++failures;};
    AudioEngine engine;engine.prepareForOffline(48000,128);engine.masterGain=1;
    auto track=engine.addAudioTrack("Input routing test");track->monitor=true;track->gain=1;
    std::array<std::array<float,128>,18> source;const float* input[18];
    for(int n=0;n<18;++n){source[(size_t)n].fill((n+1)*.01f);input[n]=source[(size_t)n].data();}
    float left[128]{},right[128]{};float* outputs[]{left,right};
    for(int channel=0;channel<18;++channel)
    {
        track->inputLeft=track->inputRight=channel;
        engine.audioDeviceIOCallbackWithContext(input,18,outputs,2,128,{});
        check(std::abs(left[127]-(channel+1)*.01f*std::sqrt(.5f))<1e-5&&std::abs(left[127]-right[127])<1e-6,"mono input "+juce::String(channel+1)+" maps to both monitor channels");
    }
    track->inputLeft=16;track->inputRight=17;engine.audioDeviceIOCallbackWithContext(input,18,outputs,2,128,{});
    check(std::abs(left[127]-.17f*std::sqrt(.5f))<1e-5&&std::abs(right[127]-.18f*std::sqrt(.5f))<1e-5,"stereo inputs 17-18 remain separate");
    Device18 fake;fake.inputs.setBit(17);fake.outputs.setRange(16,2,true);engine.audioDeviceAboutToStart(&fake);
    track->inputLeft=track->inputRight=17;const float* compact[]{input[17]};engine.audioDeviceIOCallbackWithContext(compact,1,outputs,2,128,{});
    check(std::abs(left[127]-.18f*std::sqrt(.5f))<1e-5,"physical input 18 maps correctly when it is the only enabled input");
    track->inputLeft=track->inputRight=0;engine.audioDeviceIOCallbackWithContext(compact,1,outputs,2,128,{});
    check(std::abs(left[127])<1e-6&&std::abs(right[127])<1e-6,"disabled input is silence, never another input");
    track->inputLeft=16;track->inputRight=17;auto saved=engine.saveProject(output.getChildFile("routing.silente"));AudioEngine restored;
    const auto loaded=restored.loadProject(output.getChildFile("routing.silente"));
    check(saved.wasOk()&&loaded.wasOk()&&restored.tracks.size()==1&&restored.tracks[0]->inputLeft==16&&restored.tracks[0]->inputRight==17,"input routing survives project save/load");
    output.getChildFile("inventory.json").replaceWithText(juce::JSON::toString(engine.devices.inventory()));
    {ui::AudioSettingsPanel panel(engine);panel.setSize(690,650);auto image=panel.createComponentSnapshot(panel.getLocalBounds());if(auto stream=output.getChildFile("audio-settings.png").createOutputStream())juce::PNGImageFormat().writeImageToStream(image,*stream);}
    output.getChildFile("devices.txt").replaceWithText(report);return failures==0?0:1;
}

namespace
{
class RateSwitchCheck final:private juce::Timer,private juce::AudioIODeviceCallback
{
public:
    RateSwitchCheck(juce::File f,juce::String in,juce::String out,std::function<void(int)> d):file(std::move(f)),done(std::move(d))
    {
        auto error=manager.initialise(1,2,nullptr,false);
        juce::AudioDeviceManager::AudioDeviceSetup setup;manager.getAudioDeviceSetup(setup);
        setup.inputDeviceName=in;setup.outputDeviceName=out;setup.useDefaultInputChannels=true;setup.useDefaultOutputChannels=true;setup.sampleRate=48000;
        if(error.isEmpty())error=manager.setAudioDeviceSetup(setup,true);
        log<<"open "<<in<<" + "<<out<<": "<<(error.isEmpty()?juce::String("ok"):error)<<"\n";
        if(error.isNotEmpty()){finish(1);return;}
        manager.addAudioCallback(this);startTimer(400);
    }
    ~RateSwitchCheck()override{stopTimer();manager.removeAudioCallback(this);}
private:
    juce::AudioDeviceManager manager;juce::File file;std::function<void(int)> done;juce::String log;int step=0;std::atomic<int> blocks{0};
    void timerCallback()override
    {
        if(step==12){stopTimer();auto* d=manager.getCurrentAudioDevice();log<<"final: "<<(d?juce::String(d->getCurrentSampleRate()):juce::String("none"))<<" Hz, blocks "<<blocks.load()<<"\n";finish(d&&blocks.load()>0?0:1);return;}
        juce::AudioDeviceManager::AudioDeviceSetup setup;manager.getAudioDeviceSetup(setup);setup.sampleRate=(++step%2==1||step==12)?96000:48000;if(step==12)setup.sampleRate=48000;
        const auto error=manager.setAudioDeviceSetup(setup,true);auto* d=manager.getCurrentAudioDevice();
        log<<"step "<<step<<" -> "<<setup.sampleRate<<": "<<(error.isEmpty()?juce::String("ok"):error)<<" now "<<(d?juce::String(d->getCurrentSampleRate()):juce::String("none"))<<"\n";
        file.replaceWithText(log);
    }
    void finish(int code){file.replaceWithText(log);juce::MessageManager::callAsync([d=done,code]{d(code);});}
    void audioDeviceIOCallbackWithContext(const float* const*,int,float* const* out,int channels,int samples,const juce::AudioIODeviceCallbackContext&)override{for(int c=0;c<channels;++c)if(out[c])juce::FloatVectorOperations::clear(out[c],samples);++blocks;}
    void audioDeviceAboutToStart(juce::AudioIODevice*)override{}
    void audioDeviceStopped()override{}
};
std::unique_ptr<RateSwitchCheck> rateCheck;
}
void startRateSwitchCheck(const juce::File& output,const juce::String& input,const juce::String& outputDevice,std::function<void(int)> done)
{
    output.getParentDirectory().createDirectory();
    rateCheck=std::make_unique<RateSwitchCheck>(output,input,outputDevice,[done](int code){rateCheck.reset();done(code);});
}

namespace
{
class PlaybackCheck final:private juce::Timer
{
public:
    PlaybackCheck(AudioEngine& e,juce::File w,juce::File o,std::function<void(int)> d):engine(e),output(std::move(o)),done(std::move(d))
    {
        auto r=engine.initialise();
        if(r.ok())r=engine.importAudio(w);
        auto* device=engine.deviceManager.getCurrentAudioDevice();
        log<<"init: "<<(r.ok()?juce::String("ok"):r.getErrorMessage())<<"\ndevice: "<<(device?device->getName()+" "+juce::String(device->getCurrentSampleRate())+" Hz buffer "+juce::String(device->getCurrentBufferSizeSamples())+" playing "+juce::String((int)device->isPlaying()):juce::String("none"))<<"\n";
        {juce::AudioDeviceManager::AudioDeviceSetup setup;engine.deviceManager.getAudioDeviceSetup(setup);log<<"setup in='"<<setup.inputDeviceName<<"' out='"<<setup.outputDeviceName<<"' inputs "<<setup.inputChannels.toString(2)<<" outputs "<<setup.outputChannels.toString(2)<<"\n";}
        if(r.failed()||!device){finish(1);return;}
        engine.masterGain=0.0f;underrunsAtStart=engine.audioUnderruns.load();engine.play();startTimer(250);
    }
private:
    AudioEngine& engine;juce::File output;std::function<void(int)> done;juce::String log;int ticks=0,underrunsAtStart=0;float maxPeak=0;
    void timerCallback()override
    {
        maxPeak=juce::jmax(maxPeak,engine.tracks.empty()?0.0f:engine.tracks[0]->peakL.load());
        log<<"t="<<(++ticks*0.25)<<" s position "<<engine.positionSeconds.load()<<" peak "<<maxPeak<<" underruns "<<(engine.audioUnderruns.load()-underrunsAtStart)<<" callbacks "<<engine.deviceCallbacks.load()<<"\n";
        if(ticks==12){stopTimer();engine.stop();const bool ok=maxPeak>0.01f&&std::abs(0.0)==0.0;log<<"result: "<<(ok?"audio rendered":"no audio")<<"\n";finish(ok?0:1);}
    }
    void finish(int code){output.getParentDirectory().createDirectory();output.replaceWithText(log);juce::MessageManager::callAsync([d=done,code]{d(code);});}
};
std::unique_ptr<PlaybackCheck> playbackCheck;
}
void startPlaybackCheck(AudioEngine& engine,const juce::File& wav,const juce::File& output,std::function<void(int)> done)
{
    playbackCheck=std::make_unique<PlaybackCheck>(engine,wav,output,[done](int code){playbackCheck.reset();done(code);});
}

namespace
{
class CaptureCheck final:private juce::Timer,private juce::AudioIODeviceCallback
{
public:
    CaptureCheck(AudioEngine& e,juce::File project,juce::File dir,double s,std::function<void(int)> d):engine(e),outputDir(std::move(dir)),seconds(s),done(std::move(d))
    {
        outputDir.createDirectory();
        auto r=engine.loadProject(project);
        if(r.ok())r=engine.initialise();
        auto* device=engine.deviceManager.getCurrentAudioDevice();
        if(r.failed()||!device){log<<"init failed: "<<r.getErrorMessage()<<"\n";finish(1);return;}
        rate=device->getCurrentSampleRate();
        log<<"device "<<device->getName()<<" "<<rate<<" Hz buffer "<<device->getCurrentBufferSizeSamples()<<"\n";
        capture.setSize(2,(int)(rate*(seconds+1.0)));capture.clear();
        engine.deviceManager.removeAudioCallback(&engine);engine.deviceManager.addAudioCallback(this);
        engine.seek(0);engine.play();startTimer(100);
    }
    ~CaptureCheck()override{stopTimer();engine.deviceManager.removeAudioCallback(this);}
private:
    AudioEngine& engine;juce::File outputDir;double seconds,rate=48000;std::function<void(int)> done;juce::String log;
    juce::AudioBuffer<float> capture;std::atomic<int> written{0},callbacks{0},minBlock{1<<30},maxBlock{0};
    void audioDeviceAboutToStart(juce::AudioIODevice* d)override{engine.audioDeviceAboutToStart(d);}
    void audioDeviceStopped()override{engine.audioDeviceStopped();}
    void audioDeviceIOCallbackWithContext(const float* const* in,int ins,float* const* out,int outs,int n,const juce::AudioIODeviceCallbackContext& c)override
    {
        engine.audioDeviceIOCallbackWithContext(in,ins,out,outs,n,c);++callbacks;minBlock=juce::jmin(minBlock.load(),n);maxBlock=juce::jmax(maxBlock.load(),n);
        const int at=written.load(),count=juce::jmin(n,capture.getNumSamples()-at);
        for(int ch=0;ch<2&&count>0;++ch)if(ch<outs&&out[ch])capture.copyFrom(ch,at,out[ch],count);
        if(count>0)written=at+count;
        for(int ch=0;ch<outs;++ch)if(out[ch])juce::FloatVectorOperations::clear(out[ch],n); // keep the speakers silent
    }
    void timerCallback()override
    {
        if(written.load()<(int)(rate*seconds))return;
        stopTimer();engine.stop();
        log<<"callbacks "<<callbacks.load()<<" block "<<minBlock.load()<<"-"<<maxBlock.load()<<" underruns "<<engine.audioUnderruns.load()<<" clipped "<<engine.clippedSamples.load()<<"\n";
        write(outputDir.getChildFile("realtime.wav"),capture,written.load());
        const auto offline=outputDir.getChildFile("offline.wav");auto r=engine.exportMix(offline,rate,32);
        log<<"offline export: "<<(r.ok()?juce::String("ok"):r.getErrorMessage())<<"\n";finish(r.ok()?0:1);
    }
    void write(const juce::File& f,const juce::AudioBuffer<float>& b,int n)
    {
        f.deleteFile();std::unique_ptr<juce::OutputStream> stream(f.createOutputStream());juce::WavAudioFormat wav;
        auto w=wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(2).withBitsPerSample(32).withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        if(w)w->writeFromAudioSampleBuffer(b,0,n);
    }
    void finish(int code){outputDir.getChildFile("capture.txt").replaceWithText(log);juce::MessageManager::callAsync([d=done,code]{d(code);});}
};
std::unique_ptr<CaptureCheck> captureCheck;
}
void startCaptureCheck(AudioEngine& engine,const juce::File& project,const juce::File& outputDir,double seconds,std::function<void(int)> done)
{
    captureCheck=std::make_unique<CaptureCheck>(engine,project,outputDir,seconds,[done](int code){captureCheck.reset();done(code);});
}
}

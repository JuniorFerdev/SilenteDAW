#include "IoTests.h"
#include "../src/AudioEngine.h"
#include "../src/io/AudioFileFormats.h"
#include "../src/io/TrackExport.h"
#include "../src/io/VideoAudio.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace silente
{
namespace
{
constexpr double twoPi = juce::MathConstants<double>::twoPi;
void require(bool ok, const juce::String& message) { if (! ok) throw std::runtime_error(message.toStdString()); }
void requireOk(const juce::Result& r) { require(r.wasOk(), r.getErrorMessage()); }

juce::AudioBuffer<float> tones(double rate, double seconds, double left, double right, float level)
{
    juce::AudioBuffer<float> b(2, (int) (rate * seconds));
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        b.setSample(0, i, level * (float) std::sin(twoPi * left * i / rate));
        b.setSample(1, i, level * (float) std::sin(twoPi * right * i / rate));
    }
    return b;
}

juce::Result writeFile(const juce::File& f, const juce::AudioBuffer<float>& b, io::EncodeSettings s)
{
    io::EncodedFileWriter writer;
    if (auto r = writer.open(f, s, b.getNumChannels()); r.failed()) return r;
    if (! writer.write(b, 0, b.getNumSamples())) return juce::Result::fail("write");
    return writer.finish();
}

struct Decoded { juce::AudioBuffer<float> audio; double rate = 0; };
Decoded readFile(juce::AudioFormatManager& formats, const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(f));
    require(reader != nullptr, "Cannot decode " + f.getFileName());
    Decoded d; d.rate = reader->sampleRate;
    d.audio.setSize((int) reader->numChannels, (int) reader->lengthInSamples);
    require(reader->read(&d.audio, 0, d.audio.getNumSamples(), 0, true, true), "Read failed " + f.getFileName());
    return d;
}

// Level of a sinusoid at `hz` over the middle of the signal (Blackman-Harris window).
double toneLevel(const juce::AudioBuffer<float>& b, int channel, double rate, double hz)
{
    const int n = juce::jmin(b.getNumSamples() / 2, (int) rate / 2), start = b.getNumSamples() / 4;
    double re = 0, im = 0, w = 0;
    for (int i = 0; i < n; ++i)
    {
        const double u = (double) i / (n - 1), win = 0.35875 - 0.48829 * std::cos(twoPi * u) + 0.14128 * std::cos(2 * twoPi * u) - 0.01168 * std::cos(3 * twoPi * u);
        const double v = b.getSample(channel, start + i) * win;
        re += v * std::cos(twoPi * hz * i / rate); im += v * std::sin(twoPi * hz * i / rate); w += win;
    }
    return 2.0 * std::sqrt(re * re + im * im) / w;
}

struct Runner
{
    juce::String report;
    int failures = 0, passed = 0;
    void run(const juce::String& name, const std::function<void()>& test)
    {
        juce::String error;
        try { test(); } catch (const std::exception& e) { error = e.what(); } catch (...) { error = "Unknown exception"; }
        const auto line = (error.isEmpty() ? juce::String("PASS ") : juce::String("FAIL ")) + name + (error.isEmpty() ? juce::String() : ": " + error);
        if (error.isEmpty()) ++passed; else ++failures;
        report << line << "\n"; std::cout << line << std::endl;
    }
};
}

int runIoTests(const juce::File& dir)
{
    dir.createDirectory();
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    Runner t;
    const double rate = 48000.0;
    const auto source = tones(rate, 2.0, 1000.0, 3000.0, 0.5f);

    t.run("WAV, AIFF and FLAC write and read back exactly", [&]
    {
        for (auto f : { io::FileFormat::wav, io::FileFormat::aiff, io::FileFormat::flac })
        {
            io::EncodeSettings s; s.format = f; s.sampleRate = rate; s.bitDepth = 24;
            const auto file = dir.getChildFile("exact" + io::extensionFor(f));
            requireOk(writeFile(file, source, s));
            const auto d = readFile(formats, file);
            require(d.rate == rate && d.audio.getNumSamples() == source.getNumSamples(), io::displayName(f) + " length/rate");
            float worst = 0;
            for (int c = 0; c < 2; ++c) for (int i = 0; i < source.getNumSamples(); ++i) worst = juce::jmax(worst, std::abs(d.audio.getSample(c, i) - source.getSample(c, i)));
            require(worst < 1.5e-7f, io::displayName(f) + " not exact at 24 bit: " + juce::String(worst));
        }
    });

    t.run("MP3 (LAME) and M4A/AAC encode and decode with level, length and pitch", [&]
    {
        for (auto f : { io::FileFormat::mp3, io::FileFormat::m4a })
        {
            io::EncodeSettings s; s.format = f; s.sampleRate = rate; s.kilobitsPerSecond = f == io::FileFormat::mp3 ? 320 : 256;
            const auto file = dir.getChildFile("lossy" + io::extensionFor(f));
            requireOk(writeFile(file, source, s));
            const auto d = readFile(formats, file);
            const double seconds = d.audio.getNumSamples() / d.rate;
            require(std::abs(seconds - 2.0) < 0.1, io::displayName(f) + " length " + juce::String(seconds));
            const double l = toneLevel(d.audio, 0, d.rate, 1000.0), r = toneLevel(d.audio, 1, d.rate, 3000.0);
            require(std::abs(juce::Decibels::gainToDecibels(l / 0.5)) < 0.5 && std::abs(juce::Decibels::gainToDecibels(r / 0.5)) < 0.5,
                    io::displayName(f) + " tone level " + juce::String(l) + " / " + juce::String(r));
            require(toneLevel(d.audio, 0, d.rate, 3000.0) < 0.01, io::displayName(f) + " channels mixed up");
        }
    });

    t.run("MP3 refuses 96 kHz instead of writing a broken file", [&]
    {
        io::EncodeSettings s; s.format = io::FileFormat::mp3; s.sampleRate = 96000.0;
        require(io::validate(s).failed(), "96 kHz MP3 accepted");
    });

    t.run("conversion 44.1 kHz WAV to 48 kHz FLAC keeps pitch and duration", [&]
    {
        const auto in = dir.getChildFile("tone-44100.wav");
        io::EncodeSettings s; s.format = io::FileFormat::wav; s.sampleRate = 44100.0;
        requireOk(writeFile(in, tones(44100.0, 1.0, 440.0, 440.0, 0.5f), s));
        const auto out = dir.getChildFile("tone-48000.flac");
        io::EncodeSettings target; target.format = io::FileFormat::flac; target.sampleRate = 48000.0; target.bitDepth = 24;
        requireOk(io::convertFile(formats, in, out, target));
        const auto d = readFile(formats, out);
        require(d.rate == 48000.0 && std::abs(d.audio.getNumSamples() - 48000) <= 2, "Duration changed: " + juce::String(d.audio.getNumSamples()));
        require(std::abs(toneLevel(d.audio, 0, 48000.0, 440.0) - 0.5) < 0.005, "Pitch or level changed");
    });

    t.run("audio extracted from a movie container and converted to WAV", [&]
    {
        const auto m4a = dir.getChildFile("lossy.m4a"), movie = dir.getChildFile("clip.mov"), out = dir.getChildFile("clip-audio.wav");
        require(m4a.existsAsFile(), "M4A fixture missing");
        requireOk(io::wrapInMovie(m4a, movie));
        require(io::isVideoFile(movie) && ! io::isVideoFile(m4a), "Video detection");
        io::EncodeSettings s; s.format = io::FileFormat::wav; s.sampleRate = rate; s.bitDepth = 24;
        requireOk(io::convertFile(formats, movie, out, s));
        const auto d = readFile(formats, out);
        require(std::abs(d.audio.getNumSamples() / rate - 2.0) < 0.1, "Extracted length");
        require(std::abs(juce::Decibels::gainToDecibels(toneLevel(d.audio, 0, rate, 1000.0) / 0.5)) < 0.5, "Extracted level");
    });

    t.run("export mix and separate channels; mute and solo restored", [&]
    {
        const auto left = dir.getChildFile("left.wav"), right = dir.getChildFile("right.wav");
        io::EncodeSettings s; s.format = io::FileFormat::wav; s.sampleRate = rate; s.bitDepth = 24;
        juce::AudioBuffer<float> dc(1, 48000);
        dc.clear(); for (int i = 0; i < dc.getNumSamples(); ++i) dc.setSample(0, i, 0.25f);
        requireOk(writeFile(left, dc, s));
        for (int i = 0; i < dc.getNumSamples(); ++i) dc.setSample(0, i, 0.5f);
        requireOk(writeFile(right, dc, s));
        AudioEngine engine; engine.prepareForOffline(rate, 512); engine.masterGain = 1.0f;
        requireOk(engine.importAudio(left)); requireOk(engine.importAudio(right));
        auto a = engine.tracks[0], b = engine.tracks[1];
        a->gain = 1.0f; a->pan = -1.0f; b->gain = 1.0f; b->pan = 1.0f; b->mute = true;
        for (auto& tr : engine.tracks) for (auto& c : tr->clips) { c.fadeIn = 0; c.fadeOut = 0; }
        io::ExportJob job; job.folder = dir.getChildFile("stems"); job.baseName = "Prueba"; job.settings = s; job.mix = true;
        job.trackIds.add(a->id); job.trackIds.add(b->id);
        juce::StringArray written;
        requireOk(io::exportChannels(engine, job, {}, written));
        require(written.size() == 3, "Expected 3 files, got " + juce::String(written.size()));
        const auto mix = readFile(formats, juce::File(written[0])), first = readFile(formats, juce::File(written[1])), second = readFile(formats, juce::File(written[2]));
        const int at = 24000;
        require(std::abs(mix.audio.getSample(0, at) - 0.25f) < 1e-4f && std::abs(mix.audio.getSample(1, at)) < 1e-4f, "Mix respects the muted track");
        require(std::abs(first.audio.getSample(0, at) - 0.25f) < 1e-4f && std::abs(first.audio.getSample(1, at)) < 1e-4f, "Channel 1 stem");
        require(std::abs(second.audio.getSample(1, at) - 0.5f) < 1e-4f && std::abs(second.audio.getSample(0, at)) < 1e-4f, "Channel 2 stem (muted track exported soloed)");
        require(b->mute.load() && ! b->solo.load() && ! a->solo.load(), "Mute/solo not restored");
        require(juce::File(written[2]).getFileName() == "Prueba - " + juce::File::createLegalFileName(b->name) + ".wav", "Stem name " + juce::File(written[2]).getFileName());
        job.settings.format = io::FileFormat::mp3; job.trackIds.clear(); written.clear();
        requireOk(io::exportChannels(engine, job, {}, written));
        require(juce::File(written[0]).hasFileExtension("mp3") && juce::File(written[0]).getSize() > 1000, "MP3 mix export");
    });

    dir.getChildFile("io-tests.txt").replaceWithText(t.report + juce::String(t.passed) + "/" + juce::String(t.passed + t.failures) + " io passed\n");
    std::cout << t.passed << "/" << (t.passed + t.failures) << " io passed" << std::endl;
    return t.failures;
}
}

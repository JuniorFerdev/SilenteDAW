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

    // One case per format and sample rate; inside, every bit depth or bit rate that format lists.
    for (int fi = 0; fi < io::fileFormatCount; ++fi)
        for (double r : io::supportedRates((io::FileFormat) fi))
            t.run(io::displayName((io::FileFormat) fi) + " at " + juce::String(r) + " Hz writes every offered depth/bit rate", [&, fi, r]
            {
                const auto f = (io::FileFormat) fi;
                const bool lossy = f == io::FileFormat::mp3 || f == io::FileFormat::m4a;
                const auto options = lossy ? io::supportedBitRates(f, r) : io::supportedBitDepths(f);
                require(! options.empty(), "No options offered");
                const auto matrixDir = dir.getChildFile("matrix"); matrixDir.createDirectory();
                const auto tone = tones(r, 0.5, 1000.0, 3000.0, 0.5f);
                for (int option : options)
                {
                    io::EncodeSettings s; s.format = f; s.sampleRate = r;
                    if (lossy) s.kilobitsPerSecond = option; else s.bitDepth = option;
                    const auto file = matrixDir.getChildFile("m" + io::extensionFor(f));
                    file.deleteFile();
                    requireOk(io::validate(s));
                    requireOk(writeFile(file, tone, s));
                    const auto d = readFile(formats, file);
                    require(std::abs(d.rate - r) < 0.5, "Rate " + juce::String(d.rate) + " option " + juce::String(option));
                    require(std::abs(d.audio.getNumSamples() / d.rate - 0.5) < (lossy ? 0.05 + 4000.0 / r : 0.001), "Length option " + juce::String(option));
                    require(file.getSize() > 200, "Empty file");
                }
            });

    t.run("every unsupported format/rate/depth/bit rate is refused without a partial file", [&]
    {
        const auto bad = dir.getChildFile("invalid"); bad.deleteRecursively(); bad.createDirectory();
        struct Case { io::FileFormat f; double rate; int bits, kbps; };
        const Case cases[] {
            { io::FileFormat::wav, 12000.0, 24, 0 }, { io::FileFormat::wav, 384000.0, 24, 0 }, { io::FileFormat::wav, 352800.0, 24, 0 }, { io::FileFormat::wav, 48000.0, 8, 0 }, { io::FileFormat::wav, 48000.0, 20, 0 },
            { io::FileFormat::aiff, 48000.0, 12, 0 }, { io::FileFormat::aiff, 48000.0, 32, 0 }, { io::FileFormat::aiff, 7999.0, 16, 0 },
            { io::FileFormat::flac, 48000.0, 32, 0 }, { io::FileFormat::flac, 384000.0, 24, 0 },
            { io::FileFormat::mp3, 88200.0, 24, 128 }, { io::FileFormat::mp3, 192000.0, 24, 128 }, { io::FileFormat::mp3, 44100.0, 24, 8 }, { io::FileFormat::mp3, 44100.0, 24, 321 },
            { io::FileFormat::mp3, 22050.0, 24, 320 }, { io::FileFormat::mp3, 11025.0, 24, 128 },
            { io::FileFormat::m4a, 96000.0, 24, 128 }, { io::FileFormat::m4a, 64000.0, 24, 128 }, { io::FileFormat::m4a, 22050.0, 24, 320 }, { io::FileFormat::m4a, 8000.0, 24, 128 },
            { io::FileFormat::m4a, 44100.0, 24, 64 }, { io::FileFormat::m4a, 22050.0, 24, 32 } };
        for (const auto& c : cases)
        {
            io::EncodeSettings s; s.format = c.f; s.sampleRate = c.rate; s.bitDepth = c.bits; s.kilobitsPerSecond = c.kbps;
            const auto file = bad.getChildFile("x" + io::extensionFor(c.f));
            const auto label = io::displayName(c.f) + " " + juce::String(c.rate) + " Hz " + juce::String(c.bits) + " bit " + juce::String(c.kbps) + " kbit/s";
            require(io::validate(s).failed(), label + " accepted by validate");
            io::EncodedFileWriter writer;
            require(writer.open(file, s, 2).failed(), label + " accepted by open");
            require(bad.getNumberOfChildFiles(juce::File::findFilesAndDirectories) == 0, label + " left a file behind");
        }
    });

    t.run("export cycle-marker ranges: one file per range with the right section; unique names and overwrite", [&]
    {
        const auto ramp = dir.getChildFile("ramp.wav");
        io::EncodeSettings s; s.format = io::FileFormat::wav; s.sampleRate = rate; s.bitDepth = 32;
        juce::AudioBuffer<float> r(1, 6 * 48000);
        for (int i = 0; i < r.getNumSamples(); ++i) r.setSample(0, i, 0.1f * (float) i / 48000.0f);
        requireOk(writeFile(ramp, r, s));
        AudioEngine engine; engine.prepareForOffline(rate, 512); engine.masterGain = 1.0f;
        requireOk(engine.importAudio(ramp));
        engine.tracks[0]->gain = 1.0f; engine.tracks[0]->pan = -1.0f;
        for (auto& tr : engine.tracks) for (auto& c : tr->clips) { c.fadeIn = 0; c.fadeOut = 0; }
        auto markers = engine.addTrack(TrackKind::marker);
        domain::Marker a; a.description = "A"; a.number = 1; a.position = 1.0; a.end = 2.0; a.cycle = true;
        domain::Marker b; b.number = 2; b.position = 3.5; b.end = 4.0; b.cycle = true;
        domain::Marker point; point.number = 3; point.position = 5.0; // not a cycle: ignored
        markers->data.markers = { b, point, a };
        const auto ranges = io::cycleMarkerRanges(engine);
        require(ranges.size() == 2 && ranges[0].name == "A" && ranges[1].name == "Ciclo 2" && ranges[0].start == 1.0 && ranges[1].end == 4.0, "Cycle ranges listing");
        io::ExportJob job; job.folder = dir.getChildFile("ranges"); job.folder.deleteRecursively(); job.baseName = "Prueba"; job.settings = s; job.mix = true; job.ranges = ranges;
        juce::StringArray written;
        requireOk(io::exportChannels(engine, job, {}, written));
        require(written.size() == 2, "Expected 2 files, got " + juce::String(written.size()));
        require(juce::File(written[0]).getFileName() == "Prueba - A.wav" && juce::File(written[1]).getFileName() == "Prueba - Ciclo 2.wav", "Names " + written.joinIntoString(","));
        const double starts[] { 1.0, 3.5 }, lengths[] { 1.0, 0.5 };
        for (int n = 0; n < 2; ++n)
        {
            const auto d = readFile(formats, juce::File(written[n]));
            require(std::abs(d.audio.getNumSamples() - lengths[n] * rate) <= 2, "Range " + juce::String(n) + " length " + juce::String(d.audio.getNumSamples()));
            const int at = 100;
            const float expected = 0.1f * (float) (starts[n] + at / rate);
            require(std::abs(d.audio.getSample(0, at) - expected) < 2e-3f, "Range " + juce::String(n) + " content " + juce::String(d.audio.getSample(0, at)) + " vs " + juce::String(expected));
        }
        require(io::plannedTargets(engine, job).size() == 2, "Planned targets");
        job.conflict = io::NameConflict::uniqueName; written.clear();
        requireOk(io::exportChannels(engine, job, {}, written));
        require(juce::File(written[0]).getFileName() == "Prueba - A 2.wav" && juce::File(written[1]).getFileName() == "Prueba - Ciclo 2 2.wav", "Unique names " + written.joinIntoString(","));
        job.conflict = io::NameConflict::overwrite; written.clear();
        requireOk(io::exportChannels(engine, job, {}, written));
        require(written[0].endsWith("Prueba - A.wav") && job.folder.getNumberOfChildFiles(juce::File::findFiles, "*.wav") == 4, "Overwrite kept the same names");
        job.ranges[1].end = job.ranges[1].start; written.clear();
        require(io::exportChannels(engine, job, {}, written).failed(), "Empty range accepted");
    });

    dir.getChildFile("io-tests.txt").replaceWithText(t.report + juce::String(t.passed) + "/" + juce::String(t.passed + t.failures) + " io passed\n");
    std::cout << t.passed << "/" << (t.passed + t.failures) << " io passed" << std::endl;
    return t.failures;
}
}

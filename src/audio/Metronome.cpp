#include "Metronome.h"
#include <cmath>

namespace silente::audio
{
namespace
{
constexpr double twoPi = juce::MathConstants<double>::twoPi;
// Accent, beat and subdivision: pitch ratio and level (0, -3.5 and -9 dB).
constexpr double pitchRatio[] { 1.25, 1.0, 0.84 };
constexpr double variantLevel[] { 1.0, 0.67, 0.355 };
}

std::vector<float> Metronome::synthesise(ClickSound sound, int variant, double sampleRate)
{
    const double pitch = pitchRatio[variant];
    const double length = sound == ClickSound::cowbell ? 0.22 : 0.12;
    std::vector<float> data((size_t) std::ceil(length * sampleRate), 0.0f);
    juce::uint32 noise = 0x9e3779b9u;
    const auto random = [&noise] { noise = noise * 1664525u + 1013904223u; return (double) (noise >> 8) / 8388608.0 - 1.0; };
    double phase = 0.0, phase2 = 0.0, band = 0.0, low = 0.0;
    for (size_t n = 0; n < data.size(); ++n)
    {
        const double t = (double) n / sampleRate;
        const double attack = juce::jmin(1.0, t / 0.0006);
        double value = 0.0;
        switch (sound)
        {
            case ClickSound::classic:
                value = std::sin(twoPi * 1320.0 * pitch * t) * std::exp(-t / 0.014);
                break;
            case ClickSound::woodblock:
            {
                // Two inharmonic modes with a slight downward pitch glide, like a struck block.
                const double f = 1050.0 * pitch * (1.0 + 0.06 * std::exp(-t / 0.004));
                phase += twoPi * f / sampleRate; phase2 += twoPi * f * 2.72 / sampleRate;
                value = (std::sin(phase) * std::exp(-t / 0.028) + 0.45 * std::sin(phase2) * std::exp(-t / 0.011));
                break;
            }
            case ClickSound::cowbell:
            {
                // Classic two-oscillator cowbell (540/800 Hz squares, few odd harmonics).
                double v = 0.0;
                for (int h = 1; h <= 7; h += 2) v += (std::sin(twoPi * 540.0 * pitch * h * t) + std::sin(twoPi * 800.0 * pitch * h * t)) / h;
                value = 0.5 * v * (0.7 * std::exp(-t / 0.012) + 0.3 * std::exp(-t / 0.09));
                break;
            }
            case ClickSound::digital:
            {
                double v = 0.0;
                for (int h = 1; h <= 9; h += 2) v += std::sin(twoPi * 1600.0 * pitch * h * t) / h;
                value = v * juce::jmax(0.0, 1.0 - t / 0.012);
                break;
            }
            case ClickSound::clave:
            {
                // Noise through a resonant band-pass (state-variable filter) plus the body tone.
                const double f = 2500.0 * pitch, q = 0.08, k = 2.0 * std::sin(juce::MathConstants<double>::pi * f / sampleRate);
                const double excitation = random() * std::exp(-t / 0.0015);
                low += k * band; band += k * (excitation - low - q * band);
                value = 0.6 * band + std::sin(twoPi * f * t) * std::exp(-t / 0.02);
                break;
            }
        }
        data[n] = (float) (value * attack);
    }
    float peak = 0.0f;
    for (auto v : data) peak = juce::jmax(peak, std::abs(v));
    const float scale = peak > 0.0f ? (float) (0.6 * variantLevel[variant] / peak) : 0.0f;
    for (auto& v : data) v *= scale;
    const size_t fade = juce::jmin(data.size(), (size_t) (0.004 * sampleRate));
    for (size_t i = 0; i < fade; ++i) data[data.size() - 1 - i] *= (float) i / (float) fade;
    return data;
}

void Metronome::prepare(double sampleRate)
{
    rate = sampleRate;
    for (int s = 0; s < clickSoundCount; ++s)
        for (int v = 0; v < 3; ++v)
            clicks[(size_t) s][(size_t) v] = synthesise((ClickSound) s, v, sampleRate);
    voice = previewVoice = {};
}

void Metronome::play(Voice& v, juce::AudioBuffer<float>& out, int offset, int samples) noexcept
{
    if (v.variant < 0) return;
    const auto& data = clicks[(size_t) juce::jlimit(0, clickSoundCount - 1, sound.load())][(size_t) v.variant];
    const float gain = juce::Decibels::decibelsToGain(juce::jlimit(-40.0f, 6.0f, levelDb.load()));
    const int count = juce::jmin(samples, (int) data.size() - v.position);
    for (int i = 0; i < count; ++i)
    {
        const float value = data[(size_t) (v.position + i)] * gain;
        out.addSample(0, offset + i, value);
        if (out.getNumChannels() > 1) out.addSample(1, offset + i, value);
    }
    v.position += juce::jmax(0, count);
    if (v.position >= (int) data.size()) v = {};
}

void Metronome::render(juce::AudioBuffer<float>& out, int offset, int samples, double startSeconds, double bpm, int beatsPerBar)
{
    const int division = juce::jlimit(1, 4, subdivision.load());
    const double ticksPerSecond = bpm / 60.0 * division;
    int done = 0;
    while (done < samples)
    {
        // Next grid line strictly after the current sample (or exactly on it).
        const double now = startSeconds + done / rate;
        const double tick = std::ceil(now * ticksPerSecond - 1.0e-9);
        const int at = done + juce::jmax(0, (int) std::ceil((tick / ticksPerSecond - now) * rate - 1.0e-6));
        const int until = juce::jmin(samples, at);
        play(voice, out, offset + done, until - done);
        if (at >= samples) break;
        const auto index = (juce::int64) tick;
        const auto beat = index / division;
        const bool onBeat = index % division == 0;
        const bool barStart = onBeat && beatsPerBar > 0 && ((beat % beatsPerBar) + beatsPerBar) % beatsPerBar == 0;
        voice = { ! onBeat ? subClick : (barStart && accent.load() ? accentClick : beatClick), 0 };
        done = at;
        // Consume at least one sample so the same grid line is not retriggered.
        play(voice, out, offset + done, 1);
        ++done;
    }
}

void Metronome::renderPreview(juce::AudioBuffer<float>& out, int samples)
{
    if (previewRequested.exchange(false)) previewVoice = { accentClick, 0 };
    play(previewVoice, out, 0, samples);
}

juce::Result Metronome::save(const juce::File& file) const
{
    juce::XmlElement xml("METRONOME");
    xml.setAttribute("sound", sound.load()); xml.setAttribute("subdivision", subdivision.load());
    xml.setAttribute("countInBars", countInBars.load()); xml.setAttribute("levelDb", (double) levelDb.load());
    xml.setAttribute("accent", accent.load()); xml.setAttribute("onlyWhenRecording", onlyWhenRecording.load());
    file.getParentDirectory().createDirectory();
    return xml.writeTo(file) ? juce::Result::ok() : juce::Result::fail(file.getFullPathName());
}

void Metronome::load(const juce::File& file)
{
    auto xml = juce::XmlDocument::parse(file);
    if (! xml || ! xml->hasTagName("METRONOME")) return;
    sound = juce::jlimit(0, clickSoundCount - 1, xml->getIntAttribute("sound", sound.load()));
    subdivision = juce::jlimit(1, 4, xml->getIntAttribute("subdivision", subdivision.load()));
    countInBars = juce::jlimit(0, 2, xml->getIntAttribute("countInBars", countInBars.load()));
    levelDb = (float) juce::jlimit(-40.0, 6.0, xml->getDoubleAttribute("levelDb", levelDb.load()));
    accent = xml->getBoolAttribute("accent", accent.load());
    onlyWhenRecording = xml->getBoolAttribute("onlyWhenRecording", onlyWhenRecording.load());
}
}

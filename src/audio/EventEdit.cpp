#include "EventEdit.h"
#include "../i18n/Translations.h"
#include <cmath>

namespace silente::audio
{
namespace
{
constexpr double edge = 1.0e-6;
double endOf(const AudioClip& c) { return c.startSeconds + c.durationSeconds; }

// Gain of the event at `local` seconds from its start (event gain, fade-in and fade-out).
float envelope(const AudioClip& c, double local)
{
    float fade = c.gain;
    if (c.fadeIn > 0.0) fade *= static_cast<float>(juce::jlimit(0.0, 1.0, local / c.fadeIn));
    if (c.fadeOut > 0.0) fade *= static_cast<float>(juce::jlimit(0.0, 1.0, (c.durationSeconds - local) / c.fadeOut));
    return fade;
}

// Adds (or, where `replace`, writes) the event into `out`, whose first sample is at `start`.
void renderInto(juce::AudioBuffer<float>& out, double start, double rate, const AudioClip& c, bool replace)
{
    const int first = static_cast<int>(std::llround((c.startSeconds - start) * rate));
    const int count = static_cast<int>(std::llround(c.durationSeconds * rate));
    const int sourceFirst = static_cast<int>(std::llround(c.offsetSeconds * c.sourceRate));
    for (int i = 0; i < count; ++i)
    {
        const int at = first + i, source = sourceFirst + i;
        if (at < 0 || at >= out.getNumSamples() || source < 0 || source >= c.audio->getNumSamples()) continue;
        const double local = i / rate;
        const float gain = envelope(c, local);
        // In a crossfade the lower event keeps sounding under the fade-in of the upper one.
        const bool mix = ! replace || (c.crossfade && local < c.fadeIn);
        for (int ch = 0; ch < out.getNumChannels(); ++ch)
        {
            const float value = c.audio->getSample(juce::jmin(ch, c.audio->getNumChannels() - 1), source) * gain;
            if (mix) out.addSample(ch, at, value); else out.setSample(ch, at, value);
        }
    }
}
}

int nextEvent(const std::vector<AudioClip>& clips, int index)
{
    if (! juce::isPositiveAndBelow(index, static_cast<int>(clips.size()))) return -1;
    const double start = clips[(size_t) index].startSeconds;
    int best = -1;
    for (int j = 0; j < static_cast<int>(clips.size()); ++j)
    {
        if (j == index) continue;
        const double s = clips[(size_t) j].startSeconds;
        if (s < start - edge || (std::abs(s - start) <= edge && j < index)) continue;
        if (best < 0 || s < clips[(size_t) best].startSeconds) best = j;
    }
    return best;
}

juce::Result glueEvents(std::vector<AudioClip>& clips, int index)
{
    const int next = nextEvent(clips, index);
    if (next < 0) return juce::Result::fail(i18n::tr("No hay un evento siguiente para pegar."));
    auto& a = clips[(size_t) index];
    const auto& b = clips[(size_t) next];
    if (! a.audio || ! b.audio) return juce::Result::fail(i18n::tr("Solo se pueden pegar eventos de audio."));
    if (a.audio == b.audio && std::abs(b.startSeconds - endOf(a)) <= edge && std::abs(b.offsetSeconds - (a.offsetSeconds + a.durationSeconds)) <= edge
        && std::abs(a.gain - b.gain) < 1.0e-6f && ! a.muted && ! b.muted)
    {
        // Two pieces of one recording that still touch: one event again.
        a.durationSeconds += b.durationSeconds; a.fadeOut = b.fadeOut;
        clips.erase(clips.begin() + next);
        return juce::Result::ok();
    }
    if (std::abs(a.sourceRate - b.sourceRate) > 1.0e-6) return juce::Result::fail(i18n::tr("Los eventos tienen frecuencias de muestreo distintas."));
    const double rate = a.sourceRate, start = a.startSeconds, end = juce::jmax(endOf(a), endOf(b));
    const int channels = juce::jmax(a.audio->getNumChannels(), b.audio->getNumChannels()) > 1 ? 2 : 1;
    auto audio = std::make_shared<juce::AudioBuffer<float>>(channels, static_cast<int>(std::llround((end - start) * rate)));
    audio->clear();
    const bool aOnTop = index > next;
    const auto& lower = aOnTop ? b : a;
    const auto& upper = aOnTop ? a : b;
    if (! lower.muted) renderInto(*audio, start, rate, lower, false);
    if (! upper.muted) renderInto(*audio, start, rate, upper, true);
    AudioClip glued;
    glued.name = a.name + " + " + b.name;
    glued.audio = audio; glued.sourceRate = rate;
    glued.startSeconds = start; glued.durationSeconds = end - start;
    glued.fadeIn = glued.fadeOut = 0.0; // the original fades are part of the audio now
    glued.colour = a.colour;
    const int first = juce::jmin(index, next), second = juce::jmax(index, next);
    clips.erase(clips.begin() + second);
    clips[(size_t) first] = std::move(glued);
    return juce::Result::ok();
}

juce::Result crossfadeEvents(std::vector<AudioClip>& clips, int index, double length)
{
    if (! juce::isPositiveAndBelow(index, static_cast<int>(clips.size()))) return juce::Result::fail(i18n::tr("Selecciona un evento."));
    int first = index, second = nextEvent(clips, index);
    if (second < 0)
    {   // No event after it: crossfade with the one before.
        for (int j = 0; j < static_cast<int>(clips.size()); ++j)
            if (j != index && nextEvent(clips, j) == index) first = j;
        second = index;
        if (first == index) return juce::Result::fail(i18n::tr("No hay un evento junto a este para el fundido cruzado."));
    }
    auto& a = clips[(size_t) first];
    auto& b = clips[(size_t) second];
    if (! a.audio || ! b.audio) return juce::Result::fail(i18n::tr("El fundido cruzado necesita dos eventos de audio."));
    if (endOf(b) <= endOf(a) + edge) return juce::Result::fail(i18n::tr("Un evento cubre al otro por completo."));
    double overlap = endOf(a) - b.startSeconds;
    if (std::abs(overlap) <= edge)
    {   // Touching: overlap them around the joint with the audio beyond each edge.
        const double after = static_cast<double>(a.audio->getNumSamples()) / a.sourceRate - (a.offsetSeconds + a.durationSeconds);
        const double extendA = juce::jlimit(0.0, length * 0.5, after), extendB = juce::jlimit(0.0, length * 0.5, b.offsetSeconds);
        a.durationSeconds += extendA;
        b.startSeconds -= extendB; b.offsetSeconds -= extendB; b.durationSeconds += extendB;
        overlap = extendA + extendB;
        if (overlap <= edge) return juce::Result::fail(i18n::tr("Los eventos no tienen audio más allá del borde para el fundido."));
    }
    else if (overlap < 0.0) return juce::Result::fail(i18n::tr("Los eventos no se tocan; júntalos para crear el fundido cruzado."));
    a.fadeOut = overlap; b.fadeIn = overlap; b.crossfade = true;
    if (second < first)
    {   // The later event must be on top of the earlier one.
        AudioClip moved = std::move(b);
        clips.erase(clips.begin() + second);
        clips.push_back(std::move(moved));
    }
    return juce::Result::ok();
}

juce::Result writeEventFile(AudioClip& clip, const juce::File& folder)
{
    if (! clip.audio) return juce::Result::fail(i18n::tr("El evento no tiene audio."));
    if (auto created = folder.createDirectory(); created.failed()) return created;
    auto file = folder.getNonexistentChildFile(juce::File::createLegalFileName(clip.name.substring(0, 60)), ".wav");
    std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
    if (! stream) return juce::Result::fail(i18n::tr("No se pudo crear ") + file.getFileName());
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(clip.sourceRate).withNumChannels(clip.audio->getNumChannels())
                                                  .withBitsPerSample(32).withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
    if (! writer || ! writer->writeFromAudioSampleBuffer(*clip.audio, 0, clip.audio->getNumSamples()))
        return juce::Result::fail(i18n::tr("No se pudo escribir ") + file.getFileName());
    clip.file = file;
    return juce::Result::ok();
}
}

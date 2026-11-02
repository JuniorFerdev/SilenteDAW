#include "RangeEdit.h"

namespace silente::audio
{
namespace
{
constexpr double edge = 1.0e-9;
double endOf(const AudioClip& c) { return c.startSeconds + c.durationSeconds; }
void clampFades(AudioClip& c)
{
    c.fadeIn = juce::jlimit(0.0, c.durationSeconds, c.fadeIn);
    c.fadeOut = juce::jlimit(0.0, c.durationSeconds - c.fadeIn, c.fadeOut);
}
// Keeps [from, to) of an event (offset follows the new start).
AudioClip slice(const AudioClip& c, double from, double to)
{
    AudioClip part = c;
    part.offsetSeconds += from - c.startSeconds;
    part.startSeconds = from;
    part.durationSeconds = to - from;
    if (from > c.startSeconds + edge) { part.fadeIn = AudioClip {}.fadeIn; part.crossfade = false; }
    if (to < endOf(c) - edge) part.fadeOut = AudioClip {}.fadeOut;
    clampFades(part);
    return part;
}
double beats(double seconds, double bpm) { return seconds * bpm / 60.0; }
}

void splitAt(std::vector<AudioClip>& clips, double t)
{
    std::vector<AudioClip> result;
    result.reserve(clips.size() + 1);
    for (const auto& c : clips)
    {
        if (c.startSeconds + edge < t && t < endOf(c) - edge)
        {
            result.push_back(slice(c, c.startSeconds, t));
            auto right = slice(c, t, endOf(c));
            right.id = juce::Uuid().toString();
            result.push_back(std::move(right));
        }
        else result.push_back(c);
    }
    clips = std::move(result);
}

void splitRange(std::vector<AudioClip>& clips, TimeRange r)
{
    splitAt(clips, r.start);
    splitAt(clips, r.end);
}

void deleteRange(std::vector<AudioClip>& clips, std::vector<MidiNote>& notes, TimeRange r, double bpm)
{
    if (r.length() <= edge) return;
    splitRange(clips, r);
    clips.erase(std::remove_if(clips.begin(), clips.end(), [&](const AudioClip& c) { return c.startSeconds >= r.start - edge && endOf(c) <= r.end + edge; }), clips.end());
    const double s = beats(r.start, bpm), e = beats(r.end, bpm);
    notes.erase(std::remove_if(notes.begin(), notes.end(), [&](const MidiNote& n) { return n.startBeats >= s - edge && n.startBeats < e - edge; }), notes.end());
    for (auto& n : notes) if (n.startBeats < s && n.startBeats + n.lengthBeats > s) n.lengthBeats = s - n.startBeats;
}

void deleteTime(std::vector<AudioClip>& clips, std::vector<MidiNote>& notes, TimeRange r, double bpm)
{
    if (r.length() <= edge) return;
    deleteRange(clips, notes, r, bpm);
    for (auto& c : clips) if (c.startSeconds >= r.end - edge) c.startSeconds -= r.length();
    const double e = beats(r.end, bpm), len = beats(r.length(), bpm);
    for (auto& n : notes) if (n.startBeats >= e - edge) n.startBeats -= len;
}

void insertSilence(std::vector<AudioClip>& clips, std::vector<MidiNote>& notes, TimeRange r, double bpm)
{
    if (r.length() <= edge) return;
    splitAt(clips, r.start);
    for (auto& c : clips) if (c.startSeconds >= r.start - edge) c.startSeconds += r.length();
    const double s = beats(r.start, bpm), len = beats(r.length(), bpm);
    for (auto& n : notes) if (n.startBeats >= s - edge) n.startBeats += len;
}

void truncate(std::vector<AudioClip>& clips, TimeRange r)
{
    for (auto& c : clips)
    {
        const bool crossesStart = c.startSeconds < r.start - edge && endOf(c) > r.start + edge;
        const bool crossesEnd = c.startSeconds < r.end - edge && endOf(c) > r.end + edge;
        if (crossesStart || crossesEnd) c = slice(c, juce::jmax(c.startSeconds, r.start), juce::jmin(endOf(c), r.end));
    }
}

void duplicateRange(std::vector<AudioClip>& clips, std::vector<MidiNote>& notes, TimeRange r, double bpm)
{
    if (r.length() <= edge) return;
    std::vector<AudioClip> copies;
    for (const auto& c : clips)
    {
        const double from = juce::jmax(c.startSeconds, r.start), to = juce::jmin(endOf(c), r.end);
        if (to <= from + edge) continue;
        auto copy = slice(c, from, to);
        copy.id = juce::Uuid().toString();
        copy.startSeconds += r.length();
        copies.push_back(std::move(copy));
    }
    for (auto& c : copies) clips.push_back(std::move(c)); // on top of what was there
    const double s = beats(r.start, bpm), e = beats(r.end, bpm), len = beats(r.length(), bpm);
    std::vector<MidiNote> extra;
    for (const auto& n : notes)
        if (n.startBeats >= s - edge && n.startBeats < e - edge)
        {
            MidiNote copy = n; copy.id = juce::Uuid().toString(); copy.startBeats += len;
            copy.lengthBeats = juce::jmin(copy.lengthBeats, e - n.startBeats);
            extra.push_back(copy);
        }
    notes.insert(notes.end(), extra.begin(), extra.end());
}

RangeContent copyRange(const std::vector<AudioClip>& clips, const std::vector<MidiNote>& notes, TimeRange r, double bpm)
{
    RangeContent content;
    for (const auto& c : clips)
    {
        const double from = juce::jmax(c.startSeconds, r.start), to = juce::jmin(endOf(c), r.end);
        if (to <= from + edge) continue;
        auto part = slice(c, from, to);
        part.startSeconds -= r.start;
        content.clips.push_back(std::move(part));
    }
    const double s = beats(r.start, bpm), e = beats(r.end, bpm);
    for (const auto& n : notes)
        if (n.startBeats >= s - edge && n.startBeats < e - edge)
        {
            MidiNote copy = n; copy.startBeats -= s; copy.lengthBeats = juce::jmin(copy.lengthBeats, e - n.startBeats);
            content.notes.push_back(copy);
        }
    return content;
}

void pasteContent(std::vector<AudioClip>& clips, std::vector<MidiNote>& notes, const RangeContent& content, double at, double bpm)
{
    for (auto c : content.clips) { c.id = juce::Uuid().toString(); c.startSeconds += at; clips.push_back(std::move(c)); }
    for (auto n : content.notes) { n.id = juce::Uuid().toString(); n.startBeats += beats(at, bpm); notes.push_back(n); }
}

void adjustFadesToRange(std::vector<AudioClip>& clips, TimeRange r)
{
    for (auto& c : clips)
    {
        if (! (c.startSeconds < r.end && endOf(c) > r.start)) continue;
        const bool coversStart = r.start <= c.startSeconds + 1.0e-6, coversEnd = r.end >= endOf(c) - 1.0e-6;
        if (coversStart && coversEnd) continue;
        if (coversStart) { c.fadeIn = r.end - c.startSeconds; c.crossfade = false; }
        else if (coversEnd) c.fadeOut = endOf(c) - r.start;
        else { c.fadeIn = r.start - c.startSeconds; c.fadeOut = endOf(c) - r.end; c.crossfade = false; }
        clampFades(c);
    }
}
}

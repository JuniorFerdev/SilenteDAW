#include "TempoMap.h"
#include <algorithm>
#include <cmath>

namespace silente::domain
{
namespace
{
constexpr double epsilon = 1.0e-9;
const SignatureEvent defaultSignatureEvent {};

// Seconds from the point (fromBeat, fromBpm) to `beat`, inside the segment that ends at `next`.
// A ramp is linear in beats: v(b) = v0 + k (b - b0), so t = 60 / k · ln(v / v0). Written with
// log1p so that a nearly flat ramp keeps full precision.
double segmentSeconds(double fromBeat, double fromBpm, const TempoEvent& next, double beat) noexcept
{
    const double d = beat - fromBeat;
    if (! next.ramp || next.bpm == fromBpm || next.beat <= fromBeat) return d * 60.0 / fromBpm;
    const double k = (next.bpm - fromBpm) / (next.beat - fromBeat);
    const double x = k * d / fromBpm;
    return 60.0 * d / fromBpm * (std::abs(x) < 1.0e-12 ? 1.0 : std::log1p(x) / x);
}

// Inverse of segmentSeconds: beats advanced after `seconds` from the segment start.
// v = v0 · e^(k t / 60) and b - b0 = (v - v0) / k, written with expm1.
double segmentBeats(double fromBeat, double fromBpm, const TempoEvent& next, double seconds) noexcept
{
    if (! next.ramp || next.bpm == fromBpm || next.beat <= fromBeat) return seconds * fromBpm / 60.0;
    const double k = (next.bpm - fromBpm) / (next.beat - fromBeat);
    const double y = k * seconds / 60.0;
    return fromBpm * seconds / 60.0 * (std::abs(y) < 1.0e-12 ? 1.0 : std::expm1(y) / y);
}
}

bool validDenominator(int d) noexcept { return d == 1 || d == 2 || d == 4 || d == 8 || d == 16 || d == 32; }

void TempoMap::normalise()
{
    std::erase_if(tempos, [](const TempoEvent& e) { return ! std::isfinite(e.beat) || ! std::isfinite(e.bpm) || e.beat <= 0.0; });
    for (auto& e : tempos) e.bpm = std::clamp(e.bpm, minimumBpm, maximumBpm);
    std::stable_sort(tempos.begin(), tempos.end(), [](const auto& a, const auto& b) { return a.beat < b.beat; });
    // Keep the last of each equal position.
    std::vector<TempoEvent> uniqueTempos;
    for (const auto& e : tempos)
        if (! uniqueTempos.empty() && uniqueTempos.back().beat == e.beat) uniqueTempos.back() = e;
        else uniqueTempos.push_back(e);
    tempos = std::move(uniqueTempos);

    std::erase_if(signatures, [](const SignatureEvent& s) { return s.bar < 0 || s.numerator < 1 || s.numerator > 64 || ! validDenominator(s.denominator); });
    std::stable_sort(signatures.begin(), signatures.end(), [](const auto& a, const auto& b) { return a.bar < b.bar; });
    std::vector<SignatureEvent> uniqueSignatures;
    for (const auto& s : signatures)
        if (! uniqueSignatures.empty() && uniqueSignatures.back().bar == s.bar) uniqueSignatures.back() = s;
        else uniqueSignatures.push_back(s);
    signatures = std::move(uniqueSignatures);
}

bool TempoView::defaultSignature() const noexcept
{
    if (map == nullptr) return true;
    return std::all_of(map->signatures.begin(), map->signatures.end(), [](const SignatureEvent& s) { return s.numerator == 4 && s.denominator == 4; });
}

double TempoView::bpmAt(double beats) const noexcept
{
    if (constantTempo() || beats <= 0.0) return bpm;
    double fromBeat = 0.0, fromBpm = bpm;
    for (const auto& e : map->tempos)
    {
        if (beats < e.beat)
            return e.ramp ? fromBpm + (e.bpm - fromBpm) * (beats - fromBeat) / (e.beat - fromBeat) : fromBpm;
        fromBeat = e.beat; fromBpm = e.bpm;
    }
    return fromBpm;
}

double TempoView::secondsAt(double beats) const noexcept
{
    if (constantTempo() || beats <= 0.0) return beats * 60.0 / bpm;
    double seconds = 0.0, fromBeat = 0.0, fromBpm = bpm;
    for (const auto& e : map->tempos)
    {
        if (beats <= e.beat) return seconds + segmentSeconds(fromBeat, fromBpm, e, beats);
        seconds += segmentSeconds(fromBeat, fromBpm, e, e.beat);
        fromBeat = e.beat; fromBpm = e.bpm;
    }
    return seconds + (beats - fromBeat) * 60.0 / fromBpm;
}

double TempoView::beatsAt(double seconds) const noexcept
{
    if (constantTempo() || seconds <= 0.0) return seconds * bpm / 60.0;
    double elapsed = 0.0, fromBeat = 0.0, fromBpm = bpm;
    for (const auto& e : map->tempos)
    {
        const double length = segmentSeconds(fromBeat, fromBpm, e, e.beat);
        if (seconds <= elapsed + length) return fromBeat + segmentBeats(fromBeat, fromBpm, e, seconds - elapsed);
        elapsed += length; fromBeat = e.beat; fromBpm = e.bpm;
    }
    return fromBeat + (seconds - elapsed) * fromBpm / 60.0;
}

double TempoView::beatsBetween(double fromSeconds, double toSeconds) const noexcept
{
    if (constantTempo()) return (toSeconds - fromSeconds) * bpm / 60.0;
    return beatsAt(toSeconds) - beatsAt(fromSeconds);
}

SignatureEvent TempoView::signatureAtBar(int bar) const noexcept
{
    SignatureEvent current = defaultSignatureEvent;
    if (map != nullptr)
        for (const auto& s : map->signatures)
        {
            if (s.bar > bar) break;
            current = s;
        }
    return current;
}

double TempoView::barStart(int bar) const noexcept
{
    double beats = 0.0;
    SignatureEvent current = defaultSignatureEvent;
    if (map != nullptr)
        for (const auto& s : map->signatures)
        {
            if (s.bar >= bar) break;
            beats += (s.bar - current.bar) * current.barBeats();
            current = s;
        }
    return beats + (bar - current.bar) * current.barBeats();
}

BarPosition TempoView::barAt(double beats) const noexcept
{
    double segmentStart = 0.0;
    SignatureEvent current = defaultSignatureEvent;
    if (map != nullptr)
        for (const auto& s : map->signatures)
        {
            const double segmentEnd = segmentStart + (s.bar - current.bar) * current.barBeats();
            if (beats + epsilon < segmentEnd) break;
            segmentStart = segmentEnd; current = s;
        }
    const double length = current.barBeats();
    const double count = std::floor((beats - segmentStart + epsilon) / length);
    BarPosition result;
    result.bar = current.bar + static_cast<int>(count);
    result.beatInBar = std::max(0.0, beats - (segmentStart + count * length));
    result.signature = current;
    return result;
}

double TempoView::beatsFromBarsBeats(double bar, double beat, double sixteenth, double tick) const noexcept
{
    if (defaultSignature()) return (bar - 1) * 4 + (beat - 1) + (sixteenth - 1) / 4 + tick / 480;
    const int index = static_cast<int>(std::floor(bar - 1));
    const auto signature = signatureAtBar(index);
    return barStart(index) + (bar - 1 - index) * signature.barBeats() + (beat - 1) * signature.unitBeats() + (sixteenth - 1) / 4 + tick / 480;
}
}

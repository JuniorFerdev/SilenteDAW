#pragma once
#include <array>
#include <cstddef>
#include <vector>

namespace silente::domain
{
// Track automation (Cubase «Automatización», manual pp. 900–926): one curve per parameter with
// points in quarter-note beats (so they follow the tempo map) and linear interpolation between
// them. Values are stored in the parameter's own unit, the same one the mixer uses:
//   volume  linear fader gain 0–4 (Track::gain; 1 = 0 dB, 4 ≈ +12 dB)
//   pan     −1 (left) … +1 (right) (Track::pan)
//   send    level in dB −60 … +6 (TrackSend::levelDb; −60 = off)
// Interpolation is linear in that unit (volume in gain, sends in dB).
constexpr int automationSendCount = 16;

enum class AutomationParameter { volume, pan, send };

struct AutomationTarget
{
    AutomationParameter parameter = AutomationParameter::volume;
    int send = 0; // slot 0–15 when parameter == send
    bool operator==(const AutomationTarget&) const = default;
};

struct AutomationPoint
{
    double beat = 0.0, value = 0.0;
    bool operator==(const AutomationPoint&) const = default;
};

// Points ascending by beat. Two points may share a beat (a vertical step); at that beat the
// later one applies. Before the first point the first value holds, after the last the last.
struct AutomationCurve
{
    std::vector<AutomationPoint> points;
    bool empty() const noexcept { return points.empty(); }
    bool operator==(const AutomationCurve&) const = default;
};

// Automation mode of a track (Cubase R/W buttons). Read: playback follows the curves that have
// points. Write: changing a control writes a point at the project cursor (only while the
// transport is stopped in this stage; live writing during playback is pending).
struct TrackAutomation
{
    bool read = false, write = false;
    AutomationCurve volume, pan;
    std::array<AutomationCurve, automationSendCount> sends;

    AutomationCurve& curve(AutomationTarget) noexcept;
    const AutomationCurve& curve(AutomationTarget) const noexcept;
    bool hasPoints() const noexcept;
    // Nothing to save: no points and both modes off.
    bool empty() const noexcept { return ! read && ! write && ! hasPoints(); }
    // Read mode on and this curve has points: the engine and the mixer follow the curve.
    bool reads(AutomationTarget t) const noexcept { return read && ! curve(t).empty(); }
    bool operator==(const TrackAutomation&) const = default;
};

double minimumValue(AutomationParameter) noexcept;
double maximumValue(AutomationParameter) noexcept;
double clampValue(AutomationParameter, double) noexcept;
bool validTarget(AutomationTarget) noexcept;

// Value at `beat`; `fallback` when the curve is empty. No allocation, no locks (audio thread).
double evaluate(const AutomationCurve&, double beat, double fallback) noexcept;

// Editing rules (message thread). Beats are clamped to >= 0 and values to the parameter range.
// insertPoint replaces a point at the same beat (within 1e-9) and returns the point's index.
int insertPoint(AutomationCurve&, AutomationParameter, AutomationPoint);
// Moves point `index`; it cannot pass its neighbours (Cubase keeps the point order). Returns
// false when the index is invalid.
bool movePoint(AutomationCurve&, AutomationParameter, int index, double beat, double value);
bool removePoint(AutomationCurve&, int index);
// Sorts, clamps and drops non-finite points (used after loading).
void normalise(AutomationCurve&, AutomationParameter);
}

#pragma once
#include <array>
#include <string>
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

// Trim (manual p. 919): a factor per track and level parameter (volume and each send) that scales
// the curve being read without rewriting it: value heard = curve × trim. Volume is multiplied in
// fader gain; a send's dB value is shifted by 20·log10(trim) (the same product in linear gain).
// Pan has no trim. 1 = no change; the range is 0 … 4 (−∞ … ≈ +12 dB).
constexpr double maximumTrim = 4.0;
constexpr std::array<double, automationSendCount> unitTrims() noexcept
{
    std::array<double, automationSendCount> t {};
    for (auto& v : t) v = 1.0;
    return t;
}

// Automation mode of a track (Cubase R/W buttons). Read: playback follows the curves that have
// points. Write: changing a control writes a point at the project cursor (only while the
// transport is stopped in this stage; live writing during playback is pending).
//
// Write style (panel «Automatización», manual p. 913): with Write on, `style` says how writing
// works. normal = a control change writes at the cursor (stopped transport); touch = writes only
// while the control is held and goes back to reading on release; latch = starts writing when the
// control is touched and keeps writing the last value until the transport stops; crossOver =
// like latch, but touching the control a second time and moving it across the curve that existed
// before the pass punches out at the crossing point, so the old curve continues from there
// without a jump (manual p. 918).
enum class AutomationWriteStyle { normal, touch, latch, crossOver };
// The modes of the panel; read/write/style are the stored form (old projects have no style).
enum class AutomationMode { off, read, write, touch, latch, crossOver };

struct TrackAutomation
{
    bool read = false, write = false;
    AutomationWriteStyle style = AutomationWriteStyle::normal;
    AutomationCurve volume, pan;
    std::array<AutomationCurve, automationSendCount> sends;
    double volumeTrim = 1.0;
    std::array<double, automationSendCount> sendTrims = unitTrims();

    AutomationCurve& curve(AutomationTarget) noexcept;
    const AutomationCurve& curve(AutomationTarget) const noexcept;
    bool hasPoints() const noexcept;
    // Trim of a level parameter (1 for pan or an invalid target). setTrim clamps; false for pan.
    double trim(AutomationTarget) const noexcept;
    bool setTrim(AutomationTarget, double factor) noexcept;
    bool hasTrim() const noexcept; // some trim differs from 1
    // Nothing to save: no points, both modes off and no trim.
    bool empty() const noexcept { return ! read && ! write && ! hasPoints() && ! hasTrim(); }
    // Read mode on and this curve has points: the engine and the mixer follow the curve.
    bool reads(AutomationTarget t) const noexcept { return read && ! curve(t).empty(); }
    // Curve value at `beat` with the trim applied (audio thread: no allocation, no locks).
    double readValue(AutomationTarget, double beat, double fallback) const noexcept;
    AutomationMode mode() const noexcept
    {
        if (write)
            switch (style)
            {
                case AutomationWriteStyle::touch: return AutomationMode::touch;
                case AutomationWriteStyle::latch: return AutomationMode::latch;
                case AutomationWriteStyle::crossOver: return AutomationMode::crossOver;
                case AutomationWriteStyle::normal: break;
            }
        return write ? AutomationMode::write : read ? AutomationMode::read : AutomationMode::off;
    }
    bool operator==(const TrackAutomation&) const = default;
};

// Control held (or latched) for live writing, message thread only (lives in AudioEngine).
struct AutomationGesture
{
    std::string trackId;
    AutomationTarget target;
    bool pressed = false, latched = false;
    double value = 0.0, lastBeat = -1.0;
    double startBeat = -1.0; // first beat written in this pass (for Fill «Hasta punch»)
    // Cross-Over: the curve before the pass (and the static value, used when it was empty); after
    // the second touch, the side of that curve the written value is on (−1/+1, 0 = not known yet).
    AutomationCurve original;
    double originalFallback = 0.0;
    bool secondTouch = false, punchedOut = false;
    int side = 0;
    bool active() const noexcept { return pressed || latched; }
};

// Last finished live pass (message thread only, not saved): the span written and its last value.
struct AutomationPass
{
    std::string trackId;
    AutomationTarget target;
    double fromBeat = -1.0, toBeat = -1.0, value = 0.0;
    bool valid() const noexcept { return ! trackId.empty() && fromBeat >= 0.0 && toBeat > fromBeat; }
};

// Fill options of the panel (manual pp. 921–923), applied here as actions on one curve.
enum class AutomationFill { toPunch, toStart, toEnd, loop, gaps };

double minimumValue(AutomationParameter) noexcept;
double maximumValue(AutomationParameter) noexcept;
double clampValue(AutomationParameter, double) noexcept;
bool validTarget(AutomationTarget) noexcept;
bool trimmable(AutomationTarget) noexcept; // volume and sends
double clampTrim(double) noexcept;         // 0 … maximumTrim; non-finite → 1
// value × trim in the parameter's unit, clamped to its range; the identical value when trim == 1.
double applyTrim(AutomationParameter, double value, double trim) noexcept;
// Inverse, so writing under a trim stores the curve value that sounds as `value`. Trim 0 cannot be
// inverted: the value is returned clamped.
double removeTrim(AutomationParameter, double value, double trim) noexcept;

// Value at `beat`; `fallback` when the curve is empty. No allocation, no locks (audio thread).
double evaluate(const AutomationCurve&, double beat, double fallback) noexcept;

// Editing rules (message thread). Beats are clamped to >= 0 and values to the parameter range.
// insertPoint replaces a point at the same beat (within 1e-9) and returns the point's index.
int insertPoint(AutomationCurve&, AutomationParameter, AutomationPoint);
// Moves point `index`; it cannot pass its neighbours (Cubase keeps the point order). Returns
// false when the index is invalid.
bool movePoint(AutomationCurve&, AutomationParameter, int index, double beat, double value);
bool removePoint(AutomationCurve&, int index);
// Live writing (touch/latch): replaces what lies between `fromBeat` (exclusive; pass a negative
// value for "nothing written before") and `beat` with `value` at `beat`. Consecutive equal values
// extend the previous point instead of adding one, so holding a control does not flood the curve.
void writeSpan(AutomationCurve&, AutomationParameter, double fromBeat, double beat, double value);
// Fill: replaces everything in [fromBeat, toBeat] with `value` (toBeat may be +infinity: up to
// the end of the project). The curve outside the span keeps sounding as before: vertical steps at
// the borders join the old values. Returns false when the span is empty or invalid.
bool fillRange(AutomationCurve&, AutomationParameter, double fromBeat, double toBeat, double value);
// Fill «Huecos»: Silente curves have no terminators, so the only virgin territory is before the
// first point and after the last one (where the end values are held). Both get `value`, joined to
// the existing points by vertical steps; an empty curve becomes a single point at beat 0.
void fillGaps(AutomationCurve&, AutomationParameter, double value);
// Sorts, clamps and drops non-finite points (used after loading).
void normalise(AutomationCurve&, AutomationParameter);
}

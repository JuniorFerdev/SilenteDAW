#pragma once
#include "../domain/Automation.h"
#include <cmath>

namespace silente::audio
{
// One mixer parameter across a render segment (audio thread: no allocation, locks or I/O).
// The value moves linearly from the one applied at the end of the previous segment (`last`) to
// the value at the end of this one, so playback through a linear curve is exact per sample and
// any jump (locate, cycle, R on/off, a moved fader) is smoothed over one segment instead of
// clicking. With no change, `from == to` and the caller keeps its constant-gain path.
struct ParameterRamp
{
    float from = 0.0f, to = 0.0f;
    bool constant() const noexcept { return from == to; }
};

// `last` is NaN before the first segment after preparing the track: then the ramp starts at the
// curve (or static) value at `beatFrom`, so an export begins exactly on the curve.
// While reading, the track's Trim for `target` scales the curve (TrackAutomation::readValue);
// trim 1 leaves the value bit-identical.
inline ParameterRamp automationRamp(float& last, const domain::TrackAutomation& automation, domain::AutomationTarget target, bool reading,
                                    double beatFrom, double beatTo, float staticValue) noexcept
{
    const float value = reading ? static_cast<float>(automation.readValue(target, beatTo, staticValue)) : staticValue;
    float from = last;
    if (std::isnan(from)) from = reading ? static_cast<float>(automation.readValue(target, beatFrom, staticValue)) : staticValue;
    last = value;
    return { from, value };
}
}

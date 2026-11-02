#pragma once
#include <cmath>

namespace silente::audio
{
// Fade shapes of the fade editor (Cubase manual pp. 401–410). `x` runs 0 → 1 through the fade
// (fade-in: silence → full level); a fade-out uses the same curve reversed.
enum class FadeShape { linear, equalPower, sCurve, fastStart, slowStart, count };

inline float fadeGain(int shape, double x)
{
    x = x < 0.0 ? 0.0 : x > 1.0 ? 1.0 : x;
    constexpr double halfPi = 1.57079632679489661923;
    switch (static_cast<FadeShape>(shape))
    {
        case FadeShape::equalPower: return static_cast<float>(std::sin(x * halfPi));   // constant energy in a crossfade
        case FadeShape::sCurve:     return static_cast<float>(x * x * (3.0 - 2.0 * x)); // slow at both ends
        case FadeShape::fastStart:  return static_cast<float>(1.0 - (1.0 - x) * (1.0 - x));
        case FadeShape::slowStart:  return static_cast<float>(x * x);
        default:                    return static_cast<float>(x);                       // linear: equal amplitude
    }
}

// Event gain at `local` seconds from its start: fade-in and fade-out with their shapes.
inline float fadeEnvelope(double local, double length, double fadeIn, int inShape, double fadeOut, int outShape)
{
    float gain = 1.0f;
    if (fadeIn > 0.0) gain *= fadeGain(inShape, local / fadeIn);
    if (fadeOut > 0.0) gain *= fadeGain(outShape, (length - local) / fadeOut);
    return gain;
}
}

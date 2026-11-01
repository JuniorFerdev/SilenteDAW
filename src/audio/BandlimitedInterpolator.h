#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace silente::audio
{
// Band-limited interpolation for clips whose sample rate differs from the device
// (J. O. Smith, "Digital Audio Resampling"): windowed sinc with 16 zero crossings and a
// Blackman-Harris window (~-92 dB sidelobes), tabulated 512 points per zero crossing.
// The cutoff scales with the rate ratio, so downsampling is anti-aliased too. Stateless and
// allocation-free: safe on the audio thread, any position, any seek.
class BandlimitedInterpolator
{
public:
    static constexpr int zeroCrossings = 16, resolution = 512;

    static const BandlimitedInterpolator& instance()
    {
        static const BandlimitedInterpolator table;
        return table;
    }

    // Cutoff for a source played at outputRate: 95 % of the lower Nyquist frequency.
    static double scaleFor(double sourceRate, double outputRate)
    {
        return 0.95 * std::min(1.0, outputRate / sourceRate);
    }

    float read(const float* data, int size, double position, double scale) const
    {
        const double reach = zeroCrossings / scale;
        const int first = std::max(0, static_cast<int>(std::ceil(position - reach)));
        const int last = std::min(size - 1, static_cast<int>(std::floor(position + reach)));
        const double step = scale * resolution;
        double sum = 0.0;
        for (int n = first; n <= last; ++n)
        {
            const double t = std::abs(position - n) * step;
            const int k = static_cast<int>(t);
            if (k >= limit) continue;
            sum += data[n] * (kernel[(size_t) k] + (t - k) * (kernel[(size_t) k + 1] - kernel[(size_t) k]));
        }
        return static_cast<float>(sum * scale);
    }

private:
    std::vector<double> kernel;
    int limit = 0;

    BandlimitedInterpolator()
    {
        constexpr double pi = 3.14159265358979323846;
        limit = zeroCrossings * resolution;
        kernel.resize((size_t) limit + 2, 0.0);
        for (int k = 0; k <= limit; ++k)
        {
            const double x = static_cast<double>(k) / resolution;
            const double sinc = k == 0 ? 1.0 : std::sin(pi * x) / (pi * x);
            const double u = 0.5 + x / (2.0 * zeroCrossings);
            const double window = 0.35875 - 0.48829 * std::cos(2 * pi * u) + 0.14128 * std::cos(4 * pi * u) - 0.01168 * std::cos(6 * pi * u);
            kernel[(size_t) k] = sinc * window;
        }
    }
};
}

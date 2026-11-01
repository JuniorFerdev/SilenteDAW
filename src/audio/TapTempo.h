#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace silente::audio
{
// Tap tempo estimator. Takes tap times in seconds and reports BPM from the median of the
// recent intervals (robust to a single sloppy tap). Ideas from Mixxx (reset after a pause,
// outlier rejection) and ArduinoTapTempo (a tap after a missed beat counts as two beats).
class TapTempo
{
public:
    static constexpr int maxIntervals = 12;

    void tap(double seconds)
    {
        if (last < 0.0) { last = seconds; taps = 1; return; }
        double interval = seconds - last;
        last = seconds;
        if (interval <= 0.0) return;
        const double reference = intervals.empty() ? 0.0 : median();
        if (interval > std::max(2.0, 2.5 * reference) || interval > 3.5)
        {
            // Long pause: start a new chain from this tap.
            intervals.clear(); rejected = 0; taps = 1; return;
        }
        if (! intervals.empty())
        {
            const double ratio = interval / reference;
            if (ratio > 1.75 && ratio < 2.3) interval *= 0.5; // missed beat
            else if (intervals.size() >= 3 && std::abs(ratio - 1.0) > 0.35)
            {
                // Outlier. Two in a row mean the player changed tempo: restart from here.
                if (++rejected < 2) return;
                intervals.clear();
            }
        }
        rejected = 0;
        intervals.push_back(interval);
        ++taps;
        if ((int) intervals.size() > maxIntervals) intervals.erase(intervals.begin());
    }

    void reset() { intervals.clear(); last = -1.0; taps = 0; rejected = 0; }
    bool hasTempo() const { return ! intervals.empty(); }
    int tapCount() const { return taps; }
    double bpm() const { return intervals.empty() ? 0.0 : std::clamp(60.0 / median(), 20.0, 300.0); }

    // Spread of the intervals as a percentage of the median (lower is steadier).
    double spreadPercent() const
    {
        if (intervals.size() < 2) return 0.0;
        const double m = median();
        double sum = 0.0;
        for (double v : intervals) sum += (v - m) * (v - m);
        return 100.0 * std::sqrt(sum / (double) intervals.size()) / m;
    }

private:
    std::vector<double> intervals;
    double last = -1.0;
    int taps = 0, rejected = 0;

    double median() const
    {
        auto sorted = intervals;
        std::sort(sorted.begin(), sorted.end());
        const size_t n = sorted.size();
        return n % 2 ? sorted[n / 2] : 0.5 * (sorted[n / 2 - 1] + sorted[n / 2]);
    }
};
}

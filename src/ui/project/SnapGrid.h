#pragma once
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

namespace silente::ui
{
// «Función Ajustar», «Tipo de ajuste», «Tipo de rejilla» and ruler time formats
// (Cubase manual pp. 71–72, 108–112).
enum class SnapType { grid, events, cursor, gridCursor, eventsCursor, eventsGridCursor };
enum class GridType { bar, beat, eighth, sixteenth, thirtySecond, adaptToZoom, ms10, ms100, ms1000 };
enum class TimeFormat { barsBeats, seconds, timecode, samples };

struct SnapGrid
{
    bool enabled = true;
    SnapType type = SnapType::grid;
    GridType grid = GridType::sixteenth;
    TimeFormat format = TimeFormat::barsBeats;

    static constexpr double magnetPixels = 10.0;

    bool usesGrid() const { return type == SnapType::grid || type == SnapType::gridCursor || type == SnapType::eventsGridCursor; }
    bool usesEvents() const { return type == SnapType::events || type == SnapType::eventsCursor || type == SnapType::eventsGridCursor; }
    bool usesCursor() const { return type == SnapType::cursor || type == SnapType::gridCursor || type == SnapType::eventsCursor || type == SnapType::eventsGridCursor; }

    // Grid step in seconds. «Adaptar a zoom»: the finest musical step at least 16 px wide.
    double unitSeconds(double tempo, double pixelsPerSecond) const
    {
        const double beat = 60.0 / juce::jlimit(20.0, 400.0, tempo);
        switch (grid)
        {
            case GridType::bar: return beat * 4.0;
            case GridType::beat: return beat;
            case GridType::eighth: return beat / 2.0;
            case GridType::sixteenth: return beat / 4.0;
            case GridType::thirtySecond: return beat / 8.0;
            case GridType::ms10: return 0.01;
            case GridType::ms100: return 0.1;
            case GridType::ms1000: return 1.0;
            case GridType::adaptToZoom: break;
        }
        double unit = beat * 4.0;
        while (unit / 2.0 * pixelsPerSecond >= 16.0 && unit > beat / 16.0) unit /= 2.0;
        return unit;
    }

    // Position after snapping. Grid types are absolute; events and the cursor are magnetic
    // within 10 px and win over the grid when they are closer.
    double snap(double seconds, double tempo, double pixelsPerSecond, const std::vector<double>& eventEdges, double cursor) const
    {
        if (! enabled) return juce::jmax(0.0, seconds);
        const double reach = magnetPixels / juce::jmax(1.0, pixelsPerSecond);
        double best = seconds, bestDistance = reach;
        bool magnetic = false;
        const auto consider = [&](double candidate) { const double d = std::abs(candidate - seconds); if (d <= bestDistance) { best = candidate; bestDistance = d; magnetic = true; } };
        if (usesCursor()) consider(cursor);
        if (usesEvents()) for (double edge : eventEdges) consider(edge);
        if (magnetic) return juce::jmax(0.0, best);
        if (usesGrid()) { const double unit = unitSeconds(tempo, pixelsPerSecond); return juce::jmax(0.0, std::round(seconds / unit) * unit); }
        return juce::jmax(0.0, seconds);
    }

    static juce::String formatTime(double seconds, TimeFormat f, double tempo, double sampleRate)
    {
        seconds = juce::jmax(0.0, seconds);
        switch (f)
        {
            case TimeFormat::seconds:
                return juce::String((int) (seconds / 3600.0)) + ":" + juce::String((int) std::fmod(seconds / 60.0, 60.0)).paddedLeft('0', 2) + ":"
                     + juce::String((int) std::fmod(seconds, 60.0)).paddedLeft('0', 2) + "." + juce::String((int) std::fmod(seconds * 1000.0, 1000.0)).paddedLeft('0', 3);
            case TimeFormat::timecode:
            {
                const int frames = (int) std::floor(seconds * 30.0 + 1.0e-9);
                return juce::String(frames / 108000).paddedLeft('0', 2) + ":" + juce::String((frames / 1800) % 60).paddedLeft('0', 2) + ":"
                     + juce::String((frames / 30) % 60).paddedLeft('0', 2) + ":" + juce::String(frames % 30).paddedLeft('0', 2);
            }
            case TimeFormat::samples: return juce::String((juce::int64) std::llround(seconds * sampleRate));
            case TimeFormat::barsBeats: break;
        }
        const double beats = seconds * juce::jlimit(20.0, 400.0, tempo) / 60.0;
        const int b = (int) std::floor(beats + 1.0e-9);
        const double sixteenths = (beats - b) * 4.0;
        const int six = (int) std::floor(sixteenths + 1.0e-9);
        return juce::String(b / 4 + 1) + ". " + juce::String(b % 4 + 1) + ". " + juce::String(six + 1) + ". " + juce::String((int) std::floor((sixteenths - six) * 120.0 + 1.0e-6));
    }
};
}

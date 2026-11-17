#pragma once
#include <cstddef>
#include <vector>

namespace silente::domain
{
// Tempo and time signature map (Cubase «Pista de tempo» / «Pista de tipo de compás», manual
// pp. 1494–1519). Musical positions are quarter-note beats from the project start; tempo is in
// quarter notes per minute. The tempo at beat 0 is the project tempo (AudioEngine::tempo) and is
// not stored here, so a project without events behaves exactly as with a single tempo.
constexpr double minimumBpm = 20.0, maximumBpm = 400.0;

// A tempo point. Jump («Salto»): the tempo becomes `bpm` at `beat`. Ramp («Rampa»): the tempo
// changes linearly (in beats) from the previous point to this one, then stays at `bpm`.
struct TempoEvent
{
    double beat = 0.0, bpm = 120.0;
    bool ramp = false;
    bool operator==(const TempoEvent&) const = default;
};

// A time signature change at the start of bar `bar` (0-based; bar 0 replaces the default 4/4).
struct SignatureEvent
{
    int bar = 0, numerator = 4, denominator = 4;
    double barBeats() const noexcept { return numerator * 4.0 / denominator; } // bar length in quarters
    double unitBeats() const noexcept { return 4.0 / denominator; }            // one counted beat
    bool operator==(const SignatureEvent&) const = default;
};

// Bar (0-based), quarter beats from the bar start and the signature in force.
struct BarPosition
{
    int bar = 0;
    double beatInBar = 0.0;
    SignatureEvent signature;
};

struct TempoMap
{
    std::vector<TempoEvent> tempos;          // beat > 0, ascending, unique
    std::vector<SignatureEvent> signatures;  // bar >= 0, ascending, unique
    bool empty() const noexcept { return tempos.empty() && signatures.empty(); }
    // Sorts, clamps BPM, drops invalid entries; on equal positions the last one wins.
    void normalise();
    bool operator==(const TempoMap&) const = default;
};

bool validDenominator(int) noexcept;

// Read-only view used by every conversion: the project tempo plus an optional map. It does not
// allocate, lock or perform I/O, so the audio thread may use it while it holds modelLock.
struct TempoView
{
    double bpm = 120.0;              // tempo at beat 0 (callers clamp to [minimumBpm, maximumBpm])
    const TempoMap* map = nullptr;   // nullptr = no events

    TempoView() = default;
    // Implicit from a BPM value: a single tempo in 4/4 (the behaviour before the tempo map).
    TempoView(double tempo, const TempoMap* events = nullptr) noexcept : bpm(tempo), map(events) {}

    bool constantTempo() const noexcept { return map == nullptr || map->tempos.empty(); }
    bool defaultSignature() const noexcept;
    // Single tempo and plain 4/4: every conversion uses the historical single-tempo formulas.
    bool isConstant() const noexcept { return constantTempo() && defaultSignature(); }

    double bpmAt(double beats) const noexcept;
    double secondsAt(double beats) const noexcept;
    double beatsAt(double seconds) const noexcept;
    // Beats elapsed between two times; with a single tempo it is (to - from) * bpm / 60.
    double beatsBetween(double fromSeconds, double toSeconds) const noexcept;

    SignatureEvent signatureAtBar(int bar) const noexcept;
    double barStart(int bar) const noexcept; // in beats
    BarPosition barAt(double beats) const noexcept;
    // Bar.beat.sixteenth.tick (1-based, ticks 0–119 per sixteenth, beats in signature units) to beats.
    double beatsFromBarsBeats(double bar, double beat, double sixteenth, double tick) const noexcept;
};
}

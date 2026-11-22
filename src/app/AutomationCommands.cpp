#include "AutomationCommands.h"
#include <cmath>
#include <limits>
#include <optional>

namespace silente::app
{
double staticValue(const Track& track, AutomationTarget target)
{
    switch (target.parameter)
    {
        case AutomationParameter::pan: return track.pan.load();
        case AutomationParameter::send: return domain::validTarget(target) ? (double) track.sends[(size_t) target.send].levelDb.load() : -60.0;
        case AutomationParameter::volume: break;
    }
    return track.gain.load();
}

double displayedValue(const AudioEngine& engine, const Track& track, AutomationTarget target)
{
    const double fallback = staticValue(track, target);
    if (! domain::validTarget(target) || ! track.automation.reads(target) || engine.automationSuspendRead.load()) return fallback;
    const double beat = engine.tempoView().beatsAt(engine.positionSeconds.load());
    return track.automation.readValue(target, beat, fallback);
}

double controlValue(const AudioEngine& engine, const Track& track, AutomationTarget target)
{
    return domain::removeTrim(target.parameter, displayedValue(engine, track, target), track.automation.trim(target));
}

namespace
{
using domain::AutomationGesture;
using domain::AutomationWriteStyle;
AutomationGesture* findGesture(AudioEngine& engine, const Track& track, AutomationTarget target)
{
    const auto id = track.id.toStdString();
    for (auto& g : engine.automationGestures)
        if (g.trackId == id && g.target == target) return &g;
    return nullptr;
}
void dropGestures(AudioEngine& engine, const Track& track)
{
    const auto id = track.id.toStdString();
    std::erase_if(engine.automationGestures, [&](const AutomationGesture& g) { return g.trackId == id; });
}
// Ends a live pass: remembers its span (Fill «Hasta punch») and forgets the gesture.
void finishGesture(AudioEngine& engine, size_t index)
{
    const auto& g = engine.automationGestures[index];
    if (g.startBeat >= 0.0 && g.lastBeat > g.startBeat) engine.automationLastPass = { g.trackId, g.target, g.startBeat, g.lastBeat, g.value };
    engine.automationGestures.erase(engine.automationGestures.begin() + (std::ptrdiff_t) index);
}
size_t indexOf(const AudioEngine& engine, const AutomationGesture& g) { return (size_t) (&g - engine.automationGestures.data()); }
bool liveStyle(const Track& track) { return track.automation.write && track.automation.style != AutomationWriteStyle::normal; }
bool keepsLastValue(const Track& track)
{
    return track.automation.style == AutomationWriteStyle::latch || track.automation.style == AutomationWriteStyle::crossOver;
}
// Writes `value` (curve unit, without trim) from the last written beat up to the cursor. In
// Cross-Over, after the second touch, crossing the curve that existed before the pass writes the
// crossing value and punches out (g.punchedOut; the caller finishes the gesture).
bool writeLive(AudioEngine& engine, Track& track, AutomationGesture& g, double value)
{
    const double beat = engine.tempoView().beatsAt(engine.positionSeconds.load());
    const double from = g.lastBeat >= 0.0 && g.lastBeat <= beat ? g.lastBeat : -1.0;
    if (track.automation.style == AutomationWriteStyle::crossOver && g.secondTouch)
    {
        const double old = domain::evaluate(g.original, beat, g.originalFallback);
        const int side = value > old ? 1 : value < old ? -1 : 0;
        if (side == 0 || (g.side != 0 && side != g.side)) { value = old; g.punchedOut = true; }
        else g.side = side;
    }
    {
        // Punch in (first point of the pass) on an existing curve: a vertical step keeps the
        // curve before the pass intact instead of ramping into the new value.
        auto& curve = track.automation.curve(g.target);
        const bool punchIn = from < 0.0 && ! curve.empty();
        const double before = punchIn ? domain::evaluate(curve, beat, value) : value;
        const juce::ScopedLock lock(engine.modelLock);
        domain::writeSpan(curve, g.target.parameter, from, beat, value);
        if (punchIn && before != domain::clampValue(g.target.parameter, value))
        {
            auto& p = curve.points;
            auto at = std::find_if(p.begin(), p.end(), [&](const domain::AutomationPoint& q) { return q.beat >= beat - 1.0e-9; });
            p.insert(at, { std::max(0.0, beat), before });
        }
    }
    if (g.startBeat < 0.0) g.startBeat = beat;
    g.lastBeat = beat; g.value = value;
    return true;
}
}

void setReadMode(AudioEngine& engine, Track& track, bool read)
{
    const juce::ScopedLock lock(engine.modelLock);
    track.automation.read = read;
    if (! read) track.automation.write = false;
    if (! track.automation.write) dropGestures(engine, track);
}

void setWriteMode(AudioEngine& engine, Track& track, bool write)
{
    const juce::ScopedLock lock(engine.modelLock);
    track.automation.write = write;
    track.automation.style = AutomationWriteStyle::normal;
    if (write) track.automation.read = true;
    else dropGestures(engine, track);
}

void setMode(AudioEngine& engine, Track& track, domain::AutomationMode mode)
{
    using domain::AutomationMode;
    const juce::ScopedLock lock(engine.modelLock);
    track.automation.read = mode != AutomationMode::off;
    track.automation.write = mode == AutomationMode::write || mode == AutomationMode::touch || mode == AutomationMode::latch || mode == AutomationMode::crossOver;
    track.automation.style = mode == AutomationMode::touch       ? AutomationWriteStyle::touch
                             : mode == AutomationMode::latch     ? AutomationWriteStyle::latch
                             : mode == AutomationMode::crossOver ? AutomationWriteStyle::crossOver
                                                                 : AutomationWriteStyle::normal;
    dropGestures(engine, track);
}

void setSuspendRead(AudioEngine& engine, bool suspended) { engine.automationSuspendRead = suspended; }
void setSuspendWrite(AudioEngine& engine, bool suspended) { engine.automationSuspendWrite = suspended; }

void beginGesture(AudioEngine& engine, Track& track, AutomationTarget target)
{
    if (! liveStyle(track) || ! domain::validTarget(target)) return;
    auto* g = findGesture(engine, track, target);
    if (! g)
    {
        engine.automationGestures.push_back({ track.id.toStdString(), target });
        g = &engine.automationGestures.back();
        g->value = NAN;
        if (track.automation.style == AutomationWriteStyle::crossOver)
        {
            g->original = track.automation.curve(target);
            g->originalFallback = staticValue(track, target);
        }
    }
    else if (g->latched)
    {
        // Touched again during a latched pass: the pass goes on; Cross-Over now watches the crossing.
        g->pressed = true;
        if (track.automation.style == AutomationWriteStyle::crossOver) { g->secondTouch = true; g->side = 0; }
        return;
    }
    g->pressed = true; g->lastBeat = -1.0;
}

void endGesture(AudioEngine& engine, Track& track, AutomationTarget target)
{
    auto* g = findGesture(engine, track, target);
    if (! g) return;
    g->pressed = false;
    if (keepsLastValue(track) && ! std::isnan(g->value)) { g->latched = true; return; }
    finishGesture(engine, indexOf(engine, *g));
}

void tickAutomation(AudioEngine& engine)
{
    const bool playing = engine.playing.load(), suspended = engine.automationSuspendWrite.load();
    for (size_t i = engine.automationGestures.size(); i-- > 0;)
    {
        auto& g = engine.automationGestures[i];
        Track* track = nullptr;
        for (auto& t : engine.tracks) if (t->id.toStdString() == g.trackId) { track = t.get(); break; }
        if (! track || ! liveStyle(*track)) { engine.automationGestures.erase(engine.automationGestures.begin() + (std::ptrdiff_t) i); continue; }
        if (! playing && ! g.pressed) { finishGesture(engine, i); continue; }
        if (playing && ! suspended && ! std::isnan(g.value))
        {
            writeLive(engine, *track, g, g.value);
            if (g.punchedOut) finishGesture(engine, i);
        }
    }
}

bool writeAtCursor(AudioEngine& engine, Track& track, AutomationTarget target, double value)
{
    if (! track.automation.write || engine.automationSuspendWrite.load() || ! domain::validTarget(target)) return false;
    // The control shows curve × trim: store the curve value that sounds as `value`.
    value = domain::removeTrim(target.parameter, value, track.automation.trim(target));
    if (liveStyle(track))
    {
        auto* g = findGesture(engine, track, target);
        if (! g || ! g->active()) return false;
        writeLive(engine, track, *g, value);
        if (g->punchedOut) finishGesture(engine, indexOf(engine, *g));
        return true;
    }
    if (engine.playing.load()) return false;
    const double beat = engine.tempoView().beatsAt(engine.positionSeconds.load());
    return addPoint(engine, track, target, beat, value) >= 0;
}

int addPoint(AudioEngine& engine, Track& track, AutomationTarget target, double beat, double value)
{
    if (! domain::validTarget(target)) return -1;
    const juce::ScopedLock lock(engine.modelLock);
    return domain::insertPoint(track.automation.curve(target), target.parameter, { beat, value });
}

bool movePoint(AudioEngine& engine, Track& track, AutomationTarget target, int index, double beat, double value)
{
    if (! domain::validTarget(target)) return false;
    const juce::ScopedLock lock(engine.modelLock);
    return domain::movePoint(track.automation.curve(target), target.parameter, index, beat, value);
}

bool removePoint(AudioEngine& engine, Track& track, AutomationTarget target, int index)
{
    if (! domain::validTarget(target)) return false;
    const juce::ScopedLock lock(engine.modelLock);
    return domain::removePoint(track.automation.curve(target), index);
}

bool setTrim(AudioEngine& engine, Track& track, AutomationTarget target, double factor)
{
    if (! domain::trimmable(target) || track.automation.trim(target) == domain::clampTrim(factor)) return false;
    const juce::ScopedLock lock(engine.modelLock);
    return track.automation.setTrim(target, factor);
}

namespace
{
// The curve after the Fill action, or nothing when the action does not apply or changes nothing.
std::optional<domain::AutomationCurve> filledCurve(const AudioEngine& engine, const Track& track, AutomationTarget target, domain::AutomationFill fill)
{
    using domain::AutomationFill;
    if (! domain::validTarget(target)) return std::nullopt;
    const auto tempo = engine.tempoView();
    const double cursor = tempo.beatsAt(engine.positionSeconds.load());
    double value = controlValue(engine, track, target), from = 0.0, to = 0.0;
    auto curve = track.automation.curve(target);
    switch (fill)
    {
        case AutomationFill::toPunch:
        {
            const auto& pass = engine.automationLastPass;
            if (! pass.valid() || pass.trackId != track.id.toStdString() || pass.target != target) return std::nullopt;
            from = pass.fromBeat; to = pass.toBeat; value = pass.value;
            break;
        }
        case AutomationFill::toStart: from = 0.0; to = cursor; break;
        case AutomationFill::toEnd: from = cursor; to = std::numeric_limits<double>::infinity(); break;
        case AutomationFill::loop: from = tempo.beatsAt(engine.loopStart.load()); to = tempo.beatsAt(engine.loopEnd.load()); break;
        case AutomationFill::gaps:
            domain::fillGaps(curve, target.parameter, value);
            if (curve == track.automation.curve(target)) return std::nullopt;
            return curve;
    }
    if (! domain::fillRange(curve, target.parameter, from, to, value) || curve == track.automation.curve(target)) return std::nullopt;
    return curve;
}
}

bool canFill(const AudioEngine& engine, const Track& track, AutomationTarget target, domain::AutomationFill fill)
{
    return filledCurve(engine, track, target, fill).has_value();
}

bool fill(AudioEngine& engine, Track& track, AutomationTarget target, domain::AutomationFill fill)
{
    auto curve = filledCurve(engine, track, target, fill);
    if (! curve) return false;
    const juce::ScopedLock lock(engine.modelLock);
    track.automation.curve(target) = std::move(*curve);
    return true;
}
}

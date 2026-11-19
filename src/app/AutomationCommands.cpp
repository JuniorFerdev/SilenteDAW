#include "AutomationCommands.h"
#include <cmath>

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
    return domain::evaluate(track.automation.curve(target), beat, fallback);
}

namespace
{
using domain::AutomationGesture;
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
bool liveStyle(const Track& track) { return track.automation.write && track.automation.style != domain::AutomationWriteStyle::normal; }
bool writeLive(AudioEngine& engine, Track& track, AutomationGesture& g, double value)
{
    const double beat = engine.tempoView().beatsAt(engine.positionSeconds.load());
    const double from = g.lastBeat >= 0.0 && g.lastBeat <= beat ? g.lastBeat : -1.0;
    {
        const juce::ScopedLock lock(engine.modelLock);
        domain::writeSpan(track.automation.curve(g.target), g.target.parameter, from, beat, value);
    }
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
    track.automation.style = domain::AutomationWriteStyle::normal;
    if (write) track.automation.read = true;
    else dropGestures(engine, track);
}

void setMode(AudioEngine& engine, Track& track, domain::AutomationMode mode)
{
    using domain::AutomationMode;
    using domain::AutomationWriteStyle;
    const juce::ScopedLock lock(engine.modelLock);
    track.automation.read = mode != AutomationMode::off;
    track.automation.write = mode == AutomationMode::write || mode == AutomationMode::touch || mode == AutomationMode::latch;
    track.automation.style = mode == AutomationMode::touch ? AutomationWriteStyle::touch : mode == AutomationMode::latch ? AutomationWriteStyle::latch : AutomationWriteStyle::normal;
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
    }
    g->pressed = true; g->lastBeat = -1.0;
}

void endGesture(AudioEngine& engine, Track& track, AutomationTarget target)
{
    auto* g = findGesture(engine, track, target);
    if (! g) return;
    g->pressed = false;
    if (track.automation.style == domain::AutomationWriteStyle::latch && ! std::isnan(g->value)) { g->latched = true; return; }
    engine.automationGestures.erase(engine.automationGestures.begin() + (g - engine.automationGestures.data()));
}

void tickAutomation(AudioEngine& engine)
{
    const bool playing = engine.playing.load(), suspended = engine.automationSuspendWrite.load();
    for (size_t i = engine.automationGestures.size(); i-- > 0;)
    {
        auto& g = engine.automationGestures[i];
        Track* track = nullptr;
        for (auto& t : engine.tracks) if (t->id.toStdString() == g.trackId) { track = t.get(); break; }
        if (! track || ! liveStyle(*track) || (! playing && ! g.pressed)) { engine.automationGestures.erase(engine.automationGestures.begin() + (std::ptrdiff_t) i); continue; }
        if (playing && ! suspended && ! std::isnan(g.value)) writeLive(engine, *track, g, g.value);
    }
}

bool writeAtCursor(AudioEngine& engine, Track& track, AutomationTarget target, double value)
{
    if (! track.automation.write || engine.automationSuspendWrite.load() || ! domain::validTarget(target)) return false;
    if (liveStyle(track))
    {
        auto* g = findGesture(engine, track, target);
        return g && g->active() && writeLive(engine, track, *g, value);
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
}

#include "AutomationCommands.h"

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
    if (! domain::validTarget(target) || ! track.automation.reads(target)) return fallback;
    const double beat = engine.tempoView().beatsAt(engine.positionSeconds.load());
    return domain::evaluate(track.automation.curve(target), beat, fallback);
}

void setReadMode(AudioEngine& engine, Track& track, bool read)
{
    const juce::ScopedLock lock(engine.modelLock);
    track.automation.read = read;
    if (! read) track.automation.write = false;
}

void setWriteMode(AudioEngine& engine, Track& track, bool write)
{
    const juce::ScopedLock lock(engine.modelLock);
    track.automation.write = write;
    if (write) track.automation.read = true;
}

bool writeAtCursor(AudioEngine& engine, Track& track, AutomationTarget target, double value)
{
    if (! track.automation.write || engine.playing.load() || ! domain::validTarget(target)) return false;
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

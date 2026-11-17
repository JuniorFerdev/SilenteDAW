#include "AutomationJson.h"
#include <cmath>

namespace silente::persistence
{
namespace
{
void set(juce::var& object, const char* name, juce::var value) { object.getDynamicObject()->setProperty(name, std::move(value)); }
double finite(const juce::var& object, const char* name, double fallback)
{
    const auto value = static_cast<double>(object.getProperty(name, fallback));
    return std::isfinite(value) ? value : fallback;
}
juce::var writePoints(const domain::AutomationCurve& curve)
{
    juce::Array<juce::var> points;
    for (const auto& p : curve.points)
    {
        juce::var value(new juce::DynamicObject());
        set(value, "beat", p.beat); set(value, "value", p.value); points.add(value);
    }
    return points;
}
domain::AutomationCurve readPoints(const juce::var& array, domain::AutomationParameter parameter)
{
    domain::AutomationCurve curve;
    if (auto* points = array.getArray())
        for (const auto& value : *points)
            curve.points.push_back({ finite(value, "beat", 0.0), finite(value, "value", domain::clampValue(parameter, NAN)) });
    domain::normalise(curve, parameter);
    return curve;
}
}

juce::var writeAutomation(const domain::TrackAutomation& automation)
{
    juce::var object(new juce::DynamicObject());
    set(object, "read", automation.read); set(object, "write", automation.write);
    if (! automation.volume.empty()) set(object, "volume", writePoints(automation.volume));
    if (! automation.pan.empty()) set(object, "pan", writePoints(automation.pan));
    juce::Array<juce::var> sends;
    for (int k = 0; k < domain::automationSendCount; ++k)
        if (! automation.sends[(size_t) k].empty())
        {
            juce::var send(new juce::DynamicObject());
            set(send, "slot", k); set(send, "points", writePoints(automation.sends[(size_t) k])); sends.add(send);
        }
    if (! sends.isEmpty()) set(object, "sends", sends);
    return object;
}

domain::TrackAutomation readAutomation(const juce::var& object)
{
    using domain::AutomationParameter;
    domain::TrackAutomation automation;
    if (! object.isObject()) return automation;
    automation.read = static_cast<bool>(object.getProperty("read", false));
    automation.write = static_cast<bool>(object.getProperty("write", false));
    automation.volume = readPoints(object["volume"], AutomationParameter::volume);
    automation.pan = readPoints(object["pan"], AutomationParameter::pan);
    if (auto* sends = object["sends"].getArray())
        for (const auto& send : *sends)
        {
            const int slot = static_cast<int>(send.getProperty("slot", -1));
            if (slot >= 0 && slot < domain::automationSendCount)
                automation.sends[(size_t) slot] = readPoints(send["points"], AutomationParameter::send);
        }
    return automation;
}
}

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
    // Write style only when it is not the default, so projects without touch/latch stay identical.
    if (automation.style == domain::AutomationWriteStyle::touch) set(object, "style", "touch");
    else if (automation.style == domain::AutomationWriteStyle::latch) set(object, "style", "latch");
    else if (automation.style == domain::AutomationWriteStyle::crossOver) set(object, "style", "crossover");
    // Trims only when not 1, for the same reason.
    if (automation.volumeTrim != 1.0) set(object, "volumeTrim", automation.volumeTrim);
    juce::Array<juce::var> trims;
    for (int k = 0; k < domain::automationSendCount; ++k)
        if (automation.sendTrims[(size_t) k] != 1.0)
        {
            juce::var trim(new juce::DynamicObject());
            set(trim, "slot", k); set(trim, "trim", automation.sendTrims[(size_t) k]); trims.add(trim);
        }
    if (! trims.isEmpty()) set(object, "sendTrims", trims);
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
    const auto style = object.getProperty("style", {}).toString();
    automation.style = style == "touch"       ? domain::AutomationWriteStyle::touch
                       : style == "latch"     ? domain::AutomationWriteStyle::latch
                       : style == "crossover" ? domain::AutomationWriteStyle::crossOver
                                              : domain::AutomationWriteStyle::normal;
    automation.volumeTrim = domain::clampTrim(finite(object, "volumeTrim", 1.0));
    if (auto* trims = object["sendTrims"].getArray())
        for (const auto& trim : *trims)
        {
            const int slot = static_cast<int>(trim.getProperty("slot", -1));
            if (slot >= 0 && slot < domain::automationSendCount)
                automation.sendTrims[(size_t) slot] = domain::clampTrim(finite(trim, "trim", 1.0));
        }
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

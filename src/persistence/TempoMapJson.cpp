#include "TempoMapJson.h"
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
}
juce::var writeTempoMap(const domain::TempoMap& map)
{
    juce::var object(new juce::DynamicObject());
    juce::Array<juce::var> tempos, signatures;
    for (const auto& e : map.tempos)
    {
        juce::var value(new juce::DynamicObject());
        set(value, "beat", e.beat); set(value, "bpm", e.bpm); set(value, "ramp", e.ramp); tempos.add(value);
    }
    for (const auto& s : map.signatures)
    {
        juce::var value(new juce::DynamicObject());
        set(value, "bar", s.bar); set(value, "numerator", s.numerator); set(value, "denominator", s.denominator); signatures.add(value);
    }
    set(object, "tempos", tempos); set(object, "signatures", signatures);
    return object;
}
domain::TempoMap readTempoMap(const juce::var& object)
{
    domain::TempoMap map;
    if (auto* tempos = object["tempos"].getArray())
        for (const auto& value : *tempos)
            map.tempos.push_back({ finite(value, "beat", -1.0), finite(value, "bpm", 120.0), static_cast<bool>(value.getProperty("ramp", false)) });
    if (auto* signatures = object["signatures"].getArray())
        for (const auto& value : *signatures)
            map.signatures.push_back({ static_cast<int>(value.getProperty("bar", -1)), static_cast<int>(value.getProperty("numerator", 4)), static_cast<int>(value.getProperty("denominator", 4)) });
    map.normalise();
    return map;
}
}

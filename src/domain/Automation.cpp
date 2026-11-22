#include "Automation.h"
#include <algorithm>
#include <cmath>

namespace silente::domain
{
namespace
{
constexpr double sameBeat = 1.0e-9;
}

AutomationCurve& TrackAutomation::curve(AutomationTarget t) noexcept
{
    if (t.parameter == AutomationParameter::pan) return pan;
    if (t.parameter == AutomationParameter::send) return sends[(size_t) std::clamp(t.send, 0, automationSendCount - 1)];
    return volume;
}

const AutomationCurve& TrackAutomation::curve(AutomationTarget t) const noexcept
{
    return const_cast<TrackAutomation&>(*this).curve(t);
}

bool TrackAutomation::hasPoints() const noexcept
{
    if (! volume.empty() || ! pan.empty()) return true;
    return std::any_of(sends.begin(), sends.end(), [](const auto& c) { return ! c.empty(); });
}

double TrackAutomation::trim(AutomationTarget t) const noexcept
{
    if (! trimmable(t)) return 1.0;
    return t.parameter == AutomationParameter::send ? sendTrims[(size_t) t.send] : volumeTrim;
}

bool TrackAutomation::setTrim(AutomationTarget t, double factor) noexcept
{
    if (! trimmable(t)) return false;
    (t.parameter == AutomationParameter::send ? sendTrims[(size_t) t.send] : volumeTrim) = clampTrim(factor);
    return true;
}

bool TrackAutomation::hasTrim() const noexcept
{
    return volumeTrim != 1.0 || std::any_of(sendTrims.begin(), sendTrims.end(), [](double v) { return v != 1.0; });
}

double TrackAutomation::readValue(AutomationTarget t, double beat, double fallback) const noexcept
{
    return applyTrim(t.parameter, evaluate(curve(t), beat, fallback), trim(t));
}

bool trimmable(AutomationTarget t) noexcept
{
    return validTarget(t) && t.parameter != AutomationParameter::pan;
}

double clampTrim(double v) noexcept
{
    return std::isfinite(v) ? std::clamp(v, 0.0, maximumTrim) : 1.0;
}

double applyTrim(AutomationParameter p, double value, double trim) noexcept
{
    if (trim == 1.0 || p == AutomationParameter::pan) return value;
    trim = clampTrim(trim);
    if (p == AutomationParameter::send)
    {
        if (trim <= 0.0 || value <= minimumValue(p)) return minimumValue(p);
        return clampValue(p, value + 20.0 * std::log10(trim));
    }
    return clampValue(p, value * trim);
}

double removeTrim(AutomationParameter p, double value, double trim) noexcept
{
    if (trim == 1.0 || p == AutomationParameter::pan) return value;
    trim = clampTrim(trim);
    if (trim <= 0.0) return clampValue(p, value);
    if (p == AutomationParameter::send) return value <= minimumValue(p) ? minimumValue(p) : clampValue(p, value - 20.0 * std::log10(trim));
    return clampValue(p, value / trim);
}

double minimumValue(AutomationParameter p) noexcept
{
    return p == AutomationParameter::pan ? -1.0 : p == AutomationParameter::send ? -60.0 : 0.0;
}

double maximumValue(AutomationParameter p) noexcept
{
    return p == AutomationParameter::pan ? 1.0 : p == AutomationParameter::send ? 6.0 : 4.0;
}

double clampValue(AutomationParameter p, double v) noexcept
{
    if (! std::isfinite(v)) v = p == AutomationParameter::volume ? 1.0 : 0.0;
    return std::clamp(v, minimumValue(p), maximumValue(p));
}

bool validTarget(AutomationTarget t) noexcept
{
    return t.parameter != AutomationParameter::send || (t.send >= 0 && t.send < automationSendCount);
}

double evaluate(const AutomationCurve& c, double beat, double fallback) noexcept
{
    const auto& p = c.points;
    if (p.empty()) return fallback;
    if (beat < p.front().beat) return p.front().value;
    // First point strictly after `beat`: the segment is [next - 1, next].
    const auto next = std::upper_bound(p.begin(), p.end(), beat, [](double b, const AutomationPoint& q) { return b < q.beat; });
    if (next == p.end()) return p.back().value;
    const auto& a = *(next - 1);
    const auto& b = *next;
    const double span = b.beat - a.beat;
    if (span <= 0.0) return b.value;
    return a.value + (b.value - a.value) * ((beat - a.beat) / span);
}

int insertPoint(AutomationCurve& c, AutomationParameter parameter, AutomationPoint point)
{
    point.beat = std::isfinite(point.beat) ? std::max(0.0, point.beat) : 0.0;
    point.value = clampValue(parameter, point.value);
    auto& p = c.points;
    for (size_t i = 0; i < p.size(); ++i)
        if (std::abs(p[i].beat - point.beat) <= sameBeat) { p[i].value = point.value; return (int) i; }
    const auto at = std::upper_bound(p.begin(), p.end(), point.beat, [](double b, const AutomationPoint& q) { return b < q.beat; });
    const auto index = std::distance(p.begin(), at); // before insert: it may reallocate
    p.insert(at, point);
    return (int) index;
}

bool movePoint(AutomationCurve& c, AutomationParameter parameter, int index, double beat, double value)
{
    auto& p = c.points;
    if (index < 0 || index >= (int) p.size()) return false;
    const double low = index > 0 ? p[(size_t) index - 1].beat : 0.0;
    const double high = index + 1 < (int) p.size() ? p[(size_t) index + 1].beat : 1.0e12;
    if (! std::isfinite(beat)) beat = p[(size_t) index].beat;
    p[(size_t) index] = { std::clamp(beat, low, high), clampValue(parameter, value) };
    return true;
}

bool removePoint(AutomationCurve& c, int index)
{
    if (index < 0 || index >= (int) c.points.size()) return false;
    c.points.erase(c.points.begin() + index);
    return true;
}

void normalise(AutomationCurve& c, AutomationParameter parameter)
{
    auto& p = c.points;
    p.erase(std::remove_if(p.begin(), p.end(), [](const AutomationPoint& q) { return ! std::isfinite(q.beat) || ! std::isfinite(q.value); }), p.end());
    for (auto& q : p) { q.beat = std::max(0.0, q.beat); q.value = clampValue(parameter, q.value); }
    std::stable_sort(p.begin(), p.end(), [](const AutomationPoint& a, const AutomationPoint& b) { return a.beat < b.beat; });
}

void writeSpan(AutomationCurve& curve, AutomationParameter parameter, double fromBeat, double beat, double value)
{
    beat = std::max(0.0, beat); value = clampValue(parameter, value);
    auto& pts = curve.points;
    if (fromBeat >= 0.0 && fromBeat < beat)
        std::erase_if(pts, [&](const AutomationPoint& p) { return p.beat > fromBeat + 1e-9 && p.beat <= beat + 1e-9; });
    else
        std::erase_if(pts, [&](const AutomationPoint& p) { return std::abs(p.beat - beat) <= 1e-9; });
    size_t at = 0;
    while (at < pts.size() && pts[at].beat < beat) ++at;
    if (at >= 2 && pts[at - 1].value == value && pts[at - 2].value == value && fromBeat >= 0.0) { pts[at - 1].beat = beat; return; }
    pts.insert(pts.begin() + (std::ptrdiff_t) at, { beat, value });
}

namespace
{
// Value just before `beat`: at a vertical step, the first point of the step.
double valueBefore(const AutomationCurve& c, double beat, double fallback) noexcept
{
    for (const auto& p : c.points)
        if (std::abs(p.beat - beat) <= sameBeat) return p.value;
    return evaluate(c, beat, fallback);
}
}

bool fillRange(AutomationCurve& c, AutomationParameter parameter, double fromBeat, double toBeat, double value)
{
    if (! std::isfinite(fromBeat) || std::isnan(toBeat)) return false;
    fromBeat = std::max(0.0, fromBeat);
    if (toBeat <= fromBeat + sameBeat) return false;
    value = clampValue(parameter, value);
    const bool wasEmpty = c.points.empty(), toEnd = std::isinf(toBeat);
    const double left = valueBefore(c, fromBeat, value), right = toEnd ? value : evaluate(c, toBeat, value);
    std::vector<AutomationPoint> result;
    result.reserve(c.points.size() + 4);
    for (const auto& p : c.points)
        if (p.beat < fromBeat - sameBeat) result.push_back(p);
    if (! wasEmpty && fromBeat > 0.0 && left != value) result.push_back({ fromBeat, left });
    result.push_back({ fromBeat, value });
    if (! toEnd)
    {
        result.push_back({ toBeat, value });
        if (! wasEmpty && right != value) result.push_back({ toBeat, right });
        for (const auto& p : c.points)
            if (p.beat > toBeat + sameBeat) result.push_back(p);
    }
    c.points = std::move(result);
    return true;
}

void fillGaps(AutomationCurve& c, AutomationParameter parameter, double value)
{
    value = clampValue(parameter, value);
    auto& p = c.points;
    if (p.empty()) { p.push_back({ 0.0, value }); return; }
    if (p.back().value != value) p.push_back({ p.back().beat, value });
    const AutomationPoint first = p.front();
    if (first.beat <= sameBeat) return;
    if (first.value != value) p.insert(p.begin(), { { 0.0, value }, { first.beat, value } });
    else p.insert(p.begin(), { 0.0, value });
}
}

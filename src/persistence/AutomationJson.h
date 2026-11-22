#pragma once
#include "../domain/Automation.h"
#include <juce_core/juce_core.h>

namespace silente::persistence
{
// Track JSON property "automation" (written only when TrackAutomation::empty() is false):
// { "read": bool, "write": bool, "volume": [{beat,value}], "pan": [{beat,value}],
//   "sends": [{ "slot": 0–15, "points": [{beat,value}] }] } — empty curves are omitted.
// Only when not the default: "style": "touch" | "latch" | "crossover", "volumeTrim": factor,
// "sendTrims": [{ "slot": 0–15, "trim": factor }].
// Older projects have no property and read back as no automation (trims 1, style normal).
juce::var writeAutomation(const domain::TrackAutomation&);
domain::TrackAutomation readAutomation(const juce::var&);
}

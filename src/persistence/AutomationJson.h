#pragma once
#include "../domain/Automation.h"
#include <juce_core/juce_core.h>

namespace silente::persistence
{
// Track JSON property "automation" (written only when TrackAutomation::empty() is false):
// { "read": bool, "write": bool, "volume": [{beat,value}], "pan": [{beat,value}],
//   "sends": [{ "slot": 0–15, "points": [{beat,value}] }] } — empty curves are omitted.
// Older projects have no property and read back as no automation.
juce::var writeAutomation(const domain::TrackAutomation&);
domain::TrackAutomation readAutomation(const juce::var&);
}

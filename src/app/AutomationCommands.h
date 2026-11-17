#pragma once
#include "../AudioEngine.h"

namespace silente::app
{
// Automation commands (message thread). Every edit of a curve or mode takes modelLock briefly;
// the caller records the undo step (beforeEdit) and the dirty flag.
using domain::AutomationParameter;
using domain::AutomationTarget;

// Static (non-automated) value of the parameter in the curve's unit: fader gain, pan −1…1, send dB.
double staticValue(const Track&, AutomationTarget);
// What the mixer shows: in Read mode with points, the curve at the project cursor (the fader
// follows playback); otherwise the static value.
double displayedValue(const AudioEngine&, const Track&, AutomationTarget);
// R/W buttons. Turning Write on also turns Read on (Cubase habit); turning Read off turns Write off.
void setReadMode(AudioEngine&, Track&, bool read);
void setWriteMode(AudioEngine&, Track&, bool write);
// Write mode with the transport stopped: a control change writes (or replaces) a point at the
// project cursor. Returns false when nothing was written (Write off, transport running —
// live writing during playback is pending — or invalid target).
bool writeAtCursor(AudioEngine&, Track&, AutomationTarget, double value);
// Lane edits. Return the affected point index (-1 / false on refusal).
int addPoint(AudioEngine&, Track&, AutomationTarget, double beat, double value);
bool movePoint(AudioEngine&, Track&, AutomationTarget, int index, double beat, double value);
bool removePoint(AudioEngine&, Track&, AutomationTarget, int index);
}

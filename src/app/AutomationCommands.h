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
// The five modes of the «Automatización» panel (F6). Off: neither read nor write. Read. Write
// (normal): a control change writes at the cursor with the transport stopped. Touch and Latch
// write live (also during playback) through beginGesture / writeAtCursor / endGesture. Replaces
// the old R/W pair without changing what it stores: old projects load as Off/Read/Write.
void setMode(AudioEngine&, Track&, domain::AutomationMode);
// Global «Suspender lectura» / «Suspender escritura»: not saved. Suspended read plays the static
// fader/pan/send values; suspended write writes nothing.
void setSuspendRead(AudioEngine&, bool);
void setSuspendWrite(AudioEngine&, bool);
// A control of the track is grabbed / released (slider drag start / end). In Touch mode writing
// lasts while the control is held and the track goes back to reading on release; in Latch mode
// it continues with the last value until the transport stops. Ignored in other modes.
void beginGesture(AudioEngine&, Track&, AutomationTarget);
void endGesture(AudioEngine&, Track&, AutomationTarget);
// Called by a message-thread timer: while playing, held and latched controls keep writing their
// last value at the cursor; when the transport stops, latched controls are released.
void tickAutomation(AudioEngine&);
// Writes (or replaces) a point at the project cursor for a control change. Normal Write: only with
// the transport stopped. Touch/Latch: only while the control is held or latched, also during
// playback. Returns false when nothing was written (not writing, write suspended, invalid target).
bool writeAtCursor(AudioEngine&, Track&, AutomationTarget, double value);
// Lane edits. Return the affected point index (-1 / false on refusal).
int addPoint(AudioEngine&, Track&, AutomationTarget, double beat, double value);
bool movePoint(AudioEngine&, Track&, AutomationTarget, int index, double beat, double value);
bool removePoint(AudioEngine&, Track&, AutomationTarget, int index);
}

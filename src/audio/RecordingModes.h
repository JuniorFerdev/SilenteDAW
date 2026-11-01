#pragma once
#include "../AudioEngine.h"
#include <vector>

namespace silente::audio
{
// «Modo grabación de audio» (Cubase manual p. 357): what happens to the events already on a
// track when a new recording overlaps them.
enum class AudioRecordMode
{
    keepHistory,          // Mantener historial: existing events stay (the new take plays on top)
    cycleHistoryReplace,  // Ciclo historial + reemplazar: existing events are cut; all cycle takes stay
    replace               // Reemplazar: existing events are cut; only the last take stays
};

// What Record does while already recording («Modos de grabación», p. 351).
enum class RecordRestartMode { punchInOut, reRecord };

// Where recording starts when Record is pressed in stop mode.
enum class RecordStartMode { cursor, leftLocator, selection };

// Removes the part of every clip that lies in [from, to): clips inside the range are deleted,
// clips crossing an edge are shortened and a clip spanning the whole range is split in two.
void cutRange(std::vector<AudioClip>& clips, double from, double to);

// Places the takes of one recording (one per cycle lap, in time order) on a track.
void placeTakes(std::vector<AudioClip>& clips, std::vector<AudioClip> takes, AudioRecordMode mode);
}

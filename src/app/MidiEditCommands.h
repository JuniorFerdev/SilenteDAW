#pragma once
#include "../AudioEngine.h"
#include "../domain/NoteEdit.h"
#include "../domain/Quantize.h"
#include <functional>

namespace silente::app
{
// MIDI editor commands (message thread) on the notes selected in a track. Each command takes
// modelLock, records ONE named undo step through `beforeEdit(name)` just before changing the
// model, and calls `changed` (dirty flag). They return the number of notes affected; 0 means
// nothing changed and no undo step was recorded. The piano roll, menus and key commands share them.
struct MidiEditContext
{
    AudioEngine& engine;
    int track = 0;
    domain::NoteSelection& selection;
    std::function<void(const juce::String& name)> beforeEdit;
    std::function<void()> changed;
};

// Moves the group by whole beats and semitones (clamped as a whole: start >= 0, pitch 0..127).
int moveSelection(const MidiEditContext&, double deltaBeats, int deltaPitch);
// Changes the length of every selected note by deltaBeats (each one at least 1/64 beat).
int resizeSelection(const MidiEditContext&, double deltaBeats);
// Removes the selected notes and clears the selection.
int deleteSelection(const MidiEditContext&);
// Quantizes the selected notes, or every note when nothing is selected.
int quantizeSelection(const MidiEditContext&, const domain::QuantizeSettings&);
// Copies the group right after its own span (Cubase Duplicate); the copies become the selection.
// `newAnchor` (optional) receives the index of the last copy.
int duplicateSelection(const MidiEditContext&, int* newAnchor = nullptr);
// Transposes by semitones, limiting the shift so the whole group stays within 0..127.
int transposeSelection(const MidiEditContext&, int semitones);

// Clipboard and nudge (subetapa 4). The clipboard holds copies whose starts are relative to the
// earliest copied note, so a paste keeps the intervals of the group. It lives on the message thread.
struct NoteClipboard
{
    std::vector<MidiNote> notes;
    bool empty() const { return notes.empty(); }
};
// Copies the selection into `clipboard` (no undo step). With nothing selected the clipboard is kept.
int copySelection(const MidiEditContext&, NoteClipboard& clipboard);
// Copies the selection and removes it in one undo step ("Cortar notas").
int cutSelection(const MidiEditContext&, NoteClipboard& clipboard);
// Pastes the clipboard with its earliest note at `atBeat` (clamped to >= 0) in one undo step;
// the pasted notes get new ids and become the selection. An empty clipboard does nothing.
// `newAnchor` (optional) receives the index of the last pasted note.
int pasteNotes(const MidiEditContext&, const NoteClipboard& clipboard, double atBeat, int* newAnchor = nullptr);
// Nudge: moves the selection by deltaBeats without snapping (the group stops at beat 0) in one
// undo step ("Empujar notas").
int nudgeSelection(const MidiEditContext&, double deltaBeats);

// Editor tools (subetapa 3). `targets` are explicit notes (the clicked one or the selection).
// Draw/double click: adds one note and makes it the only selected note; `newIndex` receives it.
int createNote(const MidiEditContext&, int pitch, double startBeats, double lengthBeats, int* newIndex = nullptr);
// Erase: removes the target notes and drops them from the selection.
int deleteNotes(const MidiEditContext&, const domain::NoteSelection& targets);
// Split: cuts the target notes that contain `atBeat`; the right parts of selected notes are selected too.
int splitNotesAt(const MidiEditContext&, const domain::NoteSelection& targets, double atBeat);
// Glue: joins the note `id` with the next note of the same pitch (Cubase Glue tool).
int glueNote(const MidiEditContext&, const juce::String& id);
// Mute: sets the mute state of the target notes; returns how many changed.
int setNotesMuted(const MidiEditContext&, const domain::NoteSelection& targets, bool muted);
}

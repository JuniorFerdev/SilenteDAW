#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace silente::app
{
// Default key commands of Cubase Pro 15 (Operation Manual, «Comandos de teclado por defecto»,
// pp. 1609–1628), with the same categories, names and keys. `id` links a command to the action
// Silente runs; commands Silente does not have yet are listed but not executed.
// The complete Tool category includes unassigned commands (keyCode == 0),
// audited against the installed Cubase 15.0.30 catalog; other categories are partial.
struct KeyCommand
{
    juce::String id, categoryEs, categoryEn, nameEs, nameEn;
    int keyCode = 0;
    int modifiers = 0;   // juce::ModifierKeys flags (command = ⌘, alt = ⌥, shift = ⇧)
    juce::String category() const;
    juce::String name() const;
};

const std::vector<KeyCommand>& keyCommands();
// The command for a key press (numeric keypad keys also answer to their laptop equivalents:
// «/» cycle, «*» record, «+»/«−» forward/rewind, «0» on the keypad stop).
const KeyCommand* findKeyCommand(const juce::KeyPress&);
juce::String describeKey(const KeyCommand&);
}

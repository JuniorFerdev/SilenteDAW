#pragma once
#include "../project/ProjectToolbox.h"

namespace silente::ui
{
// Key editor tools (Cubase Pro 15, p. 1233). IDs and number keys match the project tools.
enum MidiToolId { midiToolSelect = 1, midiToolSplit = 3, midiToolGlue = 4, midiToolErase = 5, midiToolMute = 7, midiToolDraw = 8 };
// In the manual's order: Select, Draw, Erase, Split, Glue, Mute.
std::vector<ProjectToolDefinition> midiToolDefinitions();
// Tool chosen by a key command (1/8/5/3/4/7, F9/F10), or 0 when the key is not a MIDI tool key.
int midiToolForKey(const juce::KeyPress&, int activeTool);

// Transpose palette (manual p. 1236): Up/Down a semitone, Up/Down More an octave.
struct MidiTransposeDefinition { juce::String componentId; theme::ToolIcon icon; juce::String name, description; int semitones; };
std::vector<MidiTransposeDefinition> midiTransposeDefinitions();

// Toolbar above the piano roll. It only shows and chooses the tool and asks for transpositions;
// the piano roll keeps the state and runs the commands.
class MidiToolBar final : public juce::Component
{
public:
    MidiToolBar();
    void setActiveTool(int tool);
    std::function<void(int)> chooseTool;
    // Transpose palette: called with +1, -1, +12 or -12 semitones.
    std::function<void(int)> transpose;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    std::vector<std::unique_ptr<juce::TextButton>> buttons,transposeButtons;
    std::vector<int> ids;
};
}

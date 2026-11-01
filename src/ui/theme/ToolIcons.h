#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace silente::ui::theme
{
enum class ToolIcon { select, scissors, erase, pencil, palette, rewind, stop, play, pause, record, cycle, click, undo, redo, snap, mixer, zoneChannel, zoneLeft, zoneLower, zoneRight, layout, filter, search, hub, exportAudio, punchIn, punchOut };
void drawToolIcon(juce::Graphics&, ToolIcon, juce::Rectangle<float>, juce::Colour);
void setButtonIcon(juce::TextButton&, ToolIcon);
}

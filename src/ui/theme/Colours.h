#pragma once
#include <juce_graphics/juce_graphics.h>

namespace silente::ui::theme
{
// Palette measured on the project window figure of the Cubase Pro 15 manual (p. 55), so the
// surfaces, separators and states read like the program Fer is used to.
inline const juce::Colour bg { 0xff1c1e1f }, panel { 0xff222528 }, raised { 0xff3c3f42 }, line { 0xff121415 };
inline const juce::Colour ink { 0xffd6d8d9 }, muted { 0xff9a9c9e }, accent { 0xff5aa2e0 };
inline const juce::Colour meterColour { 0xff22d3c5 };
inline const juce::Colour mint { 0xff3cd84a }, coral { 0xffe2453c }, meterWarning { 0xffe6c84a };
inline const juce::Colour muteColour { 0xffe2b33c }, soloColour { 0xffe2453c }, pianoWhite { 0xffb6becb }, keyboardWhite { 0xffb8c1d0 }, masterPanel { 0xff26292c };
inline const juce::Colour rulerBackground { 0xff1c1e1f }, gridBeat { 0xff36383b }, gridBar { 0xff4a4c4f }, eventArea { 0xff2b2c2f };
inline const juce::Colour cycleColour { 0xff8472e3 }, eventInk { 0xff101417 }, selectedTrack { 0xff8a8d8d };
// Active toggles (tools, tabs, zone buttons): light grey with dark text, as in Cubase.
inline const juce::Colour activeFill { 0xff8e8f90 }, activeInk { 0xff141618 };
// Faders and sliders: grey, as in Cubase (no accent colour on controls).
// Loaded insert slots are teal in Cubase.
inline const juce::Colour insertLoaded { 0xff1d8597 };
// Automation (Cubase habit): Read green, Write red; curve and points of the automation lane.
inline const juce::Colour automationRead { 0xff3fbf5c }, automationWrite { 0xffe2453c }, automationCurve { 0xffe6c84a }, automationLane { 0xff26282b };
inline const juce::Colour faderCap { 0xffb9bbbd }, faderGroove { 0xff0f1011 }, sliderFill { 0xffbfc1c3 }, knobRing { 0xffaeb0b2 };
}

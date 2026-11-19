#pragma once
#include "../../app/VideoCommands.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>

namespace silente::ui
{
// «Reproductor de video» (Cubase: Video Player, F8). Shows the picture of the topmost video track
// at the project cursor. It only follows the transport position: frames are decoded on its own
// worker thread (io::FrameReader), never on the audio thread, and the view keeps the last picture
// until the next one is ready.
class VideoViewer final : public juce::Component, private juce::Timer
{
public:
    explicit VideoViewer(AudioEngine&);
    ~VideoViewer() override;
    void paint(juce::Graphics&) override;
    void visibilityChanged() override;
    // Reads the cursor, asks for a new frame when the picture changes and shows decoded frames.
    void update();
    // Tests: updates until the frame for the current cursor is decoded (or `timeoutMs` passes).
    bool waitForFrame(int timeoutMs);
    const std::optional<app::VideoFrameTarget>& target() const noexcept { return wanted; }
    const juce::Image& picture() const noexcept { return shown; }
    bool showsMissingFile() const noexcept { return missing; }
private:
    class Decoder;
    AudioEngine& engine;
    std::unique_ptr<Decoder> decoder;
    std::optional<app::VideoFrameTarget> wanted, shownTarget;
    juce::Image shown;
    bool missing = false;
    void timerCallback() override { update(); }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VideoViewer)
};
}

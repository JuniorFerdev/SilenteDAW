#pragma once
#include <juce_graphics/juce_graphics.h>
#include <memory>

namespace silente::io
{
// Pictures for video tracks with macOS AVFoundation / ImageIO (no third-party code).
// None of this runs on the audio thread: probing happens on the message thread when importing
// and frames are decoded on the video viewer's own worker thread.
bool isStillImageFile(const juce::File&);  // PNG, JPEG
juce::String visualMediaWildcard();         // movies and still images for a video track

struct VisualMediaInfo
{
    bool still = false;
    double duration = 0.0;   // seconds (0 for still images)
    double frameRate = 0.0;  // nominal frames per second (0 if unknown or still)
    int width = 0, height = 0;
    bool hasAudio = false;
};
// Reads duration, size and frame rate. Fails for files without a picture.
juce::Result probeVisualMedia(const juce::File&, VisualMediaInfo&);

// Decodes pictures of one file. Not thread-safe: keep one reader per worker thread.
class FrameReader
{
public:
    FrameReader();
    ~FrameReader();
    // The picture at `sourceSeconds` (ignored for still images), scaled to fit `maxWidth` x
    // `maxHeight` keeping its proportions. Invalid image when the file is missing or unreadable.
    juce::Image frameAt(const juce::File&, double sourceSeconds, int maxWidth = 1280, int maxHeight = 720);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}

#pragma once
#include <juce_core/juce_core.h>

namespace silente::io
{
// Audio from video files with macOS AVFoundation (no third-party code).
bool isVideoFile(const juce::File&);
juce::String videoWildcard();
// Writes the first audio track of `video` (MP4, MOV, M4V…) to `m4a` without re-encoding when possible.
juce::Result extractAudioTrack(const juce::File& video, const juce::File& m4a);
// Test helper: wraps an audio file in a QuickTime movie container.
juce::Result wrapInMovie(const juce::File& audio, const juce::File& movie);
}

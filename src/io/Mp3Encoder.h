#pragma once
#include <juce_core/juce_core.h>
#include <vector>

namespace silente::io
{
// MP3 encoder on LAME 3.100 (third_party/lame-3.100, LGPL). Constant bit rate, joint stereo,
// LAME quality 2 (high). MP3 supports sample rates up to 48 kHz.
class Mp3Encoder
{
public:
    ~Mp3Encoder();
    juce::Result open(double sampleRate, int channels, int kilobitsPerSecond);
    // Samples in -1…1. `right` may be null for mono.
    bool encode(const float* left, const float* right, int samples, juce::OutputStream&);
    bool finish(juce::OutputStream&);
    static bool supportsRate(double rate);

private:
    void* lame = nullptr; // lame_global_flags*
    int channels = 2;
    std::vector<unsigned char> buffer;
};
}

#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>
#include <memory>

namespace silente::io
{
class Mp3Encoder;

enum class FileFormat { wav, aiff, flac, mp3, m4a };
constexpr int fileFormatCount = 5;

struct EncodeSettings
{
    FileFormat format = FileFormat::wav;
    double sampleRate = 48000.0;
    int bitDepth = 24;          // WAV/AIFF 16, 24 or 32 (32 = float); FLAC 16 or 24
    int kilobitsPerSecond = 320; // MP3 and M4A/AAC
};

juce::String extensionFor(FileFormat);
juce::String displayName(FileFormat);
juce::String readableWildcard(juce::AudioFormatManager&); // audio + video files Silente can open
juce::Result validate(const EncodeSettings&);

// Streams audio blocks into any supported format. WAV, AIFF and FLAC use JUCE writers, MP3 uses
// LAME and M4A/AAC is encoded by macOS (afconvert) from an intermediate WAV. Writes go to a
// temporary file that replaces the target only when finish() succeeds.
class EncodedFileWriter
{
public:
    EncodedFileWriter();
    ~EncodedFileWriter();
    juce::Result open(const juce::File& target, const EncodeSettings&, int channels);
    bool write(const juce::AudioBuffer<float>&, int start, int count);
    juce::Result finish();

private:
    juce::File target;
    EncodeSettings settings;
    int channels = 2;
    std::unique_ptr<juce::TemporaryFile> temporary;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    std::unique_ptr<juce::FileOutputStream> mp3Stream;
    std::unique_ptr<Mp3Encoder> mp3;
};

// Converts one file (audio or the audio track of a video) to `output` with `settings`,
// resampling with the band-limited interpolator when the rate changes. `progress` (0–1) may
// return false to cancel.
juce::Result convertFile(juce::AudioFormatManager&, const juce::File& input, const juce::File& output,
                         const EncodeSettings&, const std::function<bool(double)>& progress = {});
}

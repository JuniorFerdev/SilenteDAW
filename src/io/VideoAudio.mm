#include "VideoAudio.h"
#include "../i18n/Translations.h"
#import <AVFoundation/AVFoundation.h>

namespace silente::io
{
bool isVideoFile(const juce::File& file)
{
    return file.hasFileExtension("mp4;mov;m4v;mkv;avi;3gp;mpg;mpeg;webm");
}

juce::String videoWildcard() { return "*.mp4;*.mov;*.m4v;*.3gp"; }

namespace
{
// Runs an AVAssetExportSession synchronously (the caller is a worker or test thread).
juce::Result runExport(const juce::File& input, const juce::File& output, NSString* preset, AVFileType type, bool needsAudio)
{
    @autoreleasepool
    {
        NSURL* source = [NSURL fileURLWithPath:[NSString stringWithUTF8String:input.getFullPathName().toRawUTF8()]];
        NSURL* target = [NSURL fileURLWithPath:[NSString stringWithUTF8String:output.getFullPathName().toRawUTF8()]];
        AVURLAsset* asset = [AVURLAsset URLAssetWithURL:source options:nil];
        if (needsAudio && [[asset tracksWithMediaType:AVMediaTypeAudio] count] == 0)
            return juce::Result::fail(i18n::tr("El video no tiene pista de audio: ") + input.getFileName());
        AVAssetExportSession* session = [AVAssetExportSession exportSessionWithAsset:asset presetName:preset];
        if (session == nil) return juce::Result::fail(i18n::tr("macOS no puede leer este archivo: ") + input.getFileName());
        output.deleteFile();
        session.outputURL = target;
        session.outputFileType = type;
        dispatch_semaphore_t done = dispatch_semaphore_create(0);
        [session exportAsynchronouslyWithCompletionHandler:^{ dispatch_semaphore_signal(done); }];
        dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
        if (session.status != AVAssetExportSessionStatusCompleted)
        {
            NSString* reason = session.error ? session.error.localizedDescription : @"";
            return juce::Result::fail(i18n::tr("No se pudo extraer el audio: ") + juce::String::fromUTF8([reason UTF8String]));
        }
        return juce::Result::ok();
    }
}
}

juce::Result extractAudioTrack(const juce::File& video, const juce::File& m4a)
{
    return runExport(video, m4a, AVAssetExportPresetAppleM4A, AVFileTypeAppleM4A, true);
}

juce::Result wrapInMovie(const juce::File& audio, const juce::File& movie)
{
    return runExport(audio, movie, AVAssetExportPresetPassthrough, AVFileTypeQuickTimeMovie, true);
}
}

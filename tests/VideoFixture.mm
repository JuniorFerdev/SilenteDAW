#include "VideoFixture.h"
#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>

namespace silente::testing
{
juce::Result writeColourMovie(const juce::File& file, const std::vector<juce::Colour>& perSecond, int fps, int width, int height)
{
    @autoreleasepool
    {
        file.deleteFile();
        NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:file.getFullPathName().toRawUTF8()]];
        NSError* error = nil;
        AVAssetWriter* writer = [AVAssetWriter assetWriterWithURL:url fileType:AVFileTypeQuickTimeMovie error:&error];
        if (writer == nil) return juce::Result::fail("AVAssetWriter");
        NSDictionary* settings = @{ AVVideoCodecKey: AVVideoCodecTypeH264, AVVideoWidthKey: @(width), AVVideoHeightKey: @(height) };
        AVAssetWriterInput* input = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo outputSettings:settings];
        input.expectsMediaDataInRealTime = NO;
        NSDictionary* attributes = @{ (NSString*) kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
                                      (NSString*) kCVPixelBufferWidthKey: @(width), (NSString*) kCVPixelBufferHeightKey: @(height) };
        AVAssetWriterInputPixelBufferAdaptor* adaptor = [AVAssetWriterInputPixelBufferAdaptor assetWriterInputPixelBufferAdaptorWithAssetWriterInput:input sourcePixelBufferAttributes:attributes];
        if (! [writer canAddInput:input]) return juce::Result::fail("canAddInput");
        [writer addInput:input];
        if (! [writer startWriting]) return juce::Result::fail("startWriting");
        [writer startSessionAtSourceTime:kCMTimeZero];
        const int frames = (int) perSecond.size() * fps;
        for (int frame = 0; frame < frames; ++frame)
        {
            for (int guard = 0; ! input.readyForMoreMediaData && guard < 2000; ++guard) juce::Thread::sleep(1);
            CVPixelBufferRef buffer = nullptr;
            if (CVPixelBufferPoolCreatePixelBuffer(nullptr, adaptor.pixelBufferPool, &buffer) != kCVReturnSuccess || buffer == nullptr)
                return juce::Result::fail("pixel buffer");
            CVPixelBufferLockBaseAddress(buffer, 0);
            auto* base = static_cast<juce::uint8*>(CVPixelBufferGetBaseAddress(buffer));
            const size_t stride = CVPixelBufferGetBytesPerRow(buffer);
            const auto colour = perSecond[(size_t) (frame / fps)];
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x)
                {
                    const auto c = y < height / 4 ? juce::Colours::white : colour;
                    auto* p = base + (size_t) y * stride + (size_t) x * 4;
                    p[0] = c.getBlue(); p[1] = c.getGreen(); p[2] = c.getRed(); p[3] = 255;
                }
            CVPixelBufferUnlockBaseAddress(buffer, 0);
            const bool ok = [adaptor appendPixelBuffer:buffer withPresentationTime:CMTimeMake(frame, fps)];
            CVPixelBufferRelease(buffer);
            if (! ok) return juce::Result::fail("append frame");
        }
        [input markAsFinished];
        [writer endSessionAtSourceTime:CMTimeMake(frames, fps)];
        dispatch_semaphore_t done = dispatch_semaphore_create(0);
        [writer finishWritingWithCompletionHandler:^{ dispatch_semaphore_signal(done); }];
        dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
        return writer.status == AVAssetWriterStatusCompleted ? juce::Result::ok() : juce::Result::fail("finish writing");
    }
}
}

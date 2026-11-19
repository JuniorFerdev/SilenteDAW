#include "VideoFrames.h"
#include "VideoAudio.h"
#include "../i18n/Translations.h"
#import <AVFoundation/AVFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#include <cmath>

// Synchronous AVFoundation calls (tracksWithMediaType, duration, copyCGImageAtTime) are deprecated
// in recent SDKs but remain available back to macOS 12; callers are worker or message threads.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace silente::io
{
namespace
{
NSURL* urlOf(const juce::File& file)
{
    return [NSURL fileURLWithPath:[NSString stringWithUTF8String:file.getFullPathName().toRawUTF8()]];
}

juce::Image fitted(const juce::Image& image, int maxWidth, int maxHeight)
{
    if (! image.isValid() || (image.getWidth() <= maxWidth && image.getHeight() <= maxHeight)) return image;
    const double scale = juce::jmin((double) maxWidth / image.getWidth(), (double) maxHeight / image.getHeight());
    return image.rescaled(juce::jmax(1, (int) std::lround(image.getWidth() * scale)), juce::jmax(1, (int) std::lround(image.getHeight() * scale)),
                          juce::Graphics::mediumResamplingQuality);
}

// Copies a CGImage into a JUCE ARGB image (premultiplied BGRA in memory, first row = top).
juce::Image toJuceImage(CGImageRef cg)
{
    const int width = (int) CGImageGetWidth(cg), height = (int) CGImageGetHeight(cg);
    if (width <= 0 || height <= 0) return {};
    juce::Image image(juce::Image::ARGB, width, height, true, juce::SoftwareImageType());
    juce::Image::BitmapData pixels(image, juce::Image::BitmapData::writeOnly);
    CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef context = CGBitmapContextCreate(pixels.data, (size_t) width, (size_t) height, 8, (size_t) pixels.lineStride, space,
                                                 (uint32_t) kCGImageAlphaPremultipliedFirst | (uint32_t) kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(space);
    if (context == nullptr) return {};
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), cg);
    CGContextRelease(context);
    return image;
}
}

bool isStillImageFile(const juce::File& file) { return file.hasFileExtension("png;jpg;jpeg"); }

juce::String visualMediaWildcard() { return videoWildcard() + ";*.png;*.jpg;*.jpeg"; }

juce::Result probeVisualMedia(const juce::File& file, VisualMediaInfo& info)
{
    info = {};
    if (! file.existsAsFile()) return juce::Result::fail(i18n::tr("No se encuentra el archivo: ") + file.getFileName());
    if (isStillImageFile(file))
    {
        const auto image = juce::ImageFileFormat::loadFrom(file);
        if (! image.isValid()) return juce::Result::fail(i18n::tr("No se puede leer la imagen: ") + file.getFileName());
        info.still = true; info.width = image.getWidth(); info.height = image.getHeight();
        return juce::Result::ok();
    }
    if (! isVideoFile(file)) return juce::Result::fail(i18n::tr("Formato no admitido en una pista de video: ") + file.getFileName());
    @autoreleasepool
    {
        AVURLAsset* asset = [AVURLAsset URLAssetWithURL:urlOf(file) options:nil];
        NSArray<AVAssetTrack*>* pictures = [asset tracksWithMediaType:AVMediaTypeVideo];
        if ([pictures count] == 0) return juce::Result::fail(i18n::tr("El archivo no tiene imagen de video: ") + file.getFileName());
        AVAssetTrack* track = [pictures objectAtIndex:0];
        const CGSize size = CGSizeApplyAffineTransform(track.naturalSize, track.preferredTransform);
        info.width = (int) std::lround(std::abs(size.width)); info.height = (int) std::lround(std::abs(size.height));
        info.frameRate = std::isfinite(track.nominalFrameRate) && track.nominalFrameRate > 0 ? (double) track.nominalFrameRate : 0.0;
        info.duration = CMTimeGetSeconds(asset.duration);
        info.hasAudio = [[asset tracksWithMediaType:AVMediaTypeAudio] count] > 0;
        if (! std::isfinite(info.duration) || info.duration <= 0.0)
            return juce::Result::fail(i18n::tr("El video no tiene duración válida: ") + file.getFileName());
        return juce::Result::ok();
    }
}

struct FrameReader::Impl
{
    juce::String path;
    AVAssetImageGenerator* generator = nil; // retained (manual reference counting)
    juce::Image still;
    void reset()
    {
        if (generator != nil) [generator release];
        generator = nil; still = {}; path.clear();
    }
    ~Impl() { reset(); }
};

FrameReader::FrameReader() : impl(std::make_unique<Impl>()) {}
FrameReader::~FrameReader() = default;

juce::Image FrameReader::frameAt(const juce::File& file, double sourceSeconds, int maxWidth, int maxHeight)
{
    auto& s = *impl;
    if (s.path != file.getFullPathName())
    {
        s.reset();
        if (! file.existsAsFile()) return {};
        s.path = file.getFullPathName();
        if (isStillImageFile(file)) s.still = juce::ImageFileFormat::loadFrom(file);
        else
        {
            @autoreleasepool
            {
                AVURLAsset* asset = [AVURLAsset URLAssetWithURL:urlOf(file) options:nil];
                if ([[asset tracksWithMediaType:AVMediaTypeVideo] count] == 0) return {};
                s.generator = [[AVAssetImageGenerator alloc] initWithAsset:asset];
                s.generator.appliesPreferredTrackTransform = YES;
                // Exact frames: the viewer asks for frame starts, so zero tolerance keeps picture and cursor in step.
                s.generator.requestedTimeToleranceBefore = kCMTimeZero;
                s.generator.requestedTimeToleranceAfter = kCMTimeZero;
            }
        }
    }
    if (s.still.isValid()) return fitted(s.still, maxWidth, maxHeight);
    if (s.generator == nil) return {};
    @autoreleasepool
    {
        s.generator.maximumSize = CGSizeMake(maxWidth, maxHeight);
        NSError* error = nil;
        CGImageRef cg = [s.generator copyCGImageAtTime:CMTimeMakeWithSeconds(juce::jmax(0.0, sourceSeconds), 600) actualTime:nullptr error:&error];
        if (cg == nullptr) return {};
        auto image = toJuceImage(cg);
        CGImageRelease(cg);
        return image;
    }
}
}
#pragma clang diagnostic pop

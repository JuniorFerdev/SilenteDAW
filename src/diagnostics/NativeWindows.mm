#include "NativeWindows.h"
#import <AppKit/AppKit.h>

namespace silente::diagnostics
{
juce::StringArray nativeWindowTitles()
{
    juce::StringArray titles;
    for (NSWindow* window in [NSApp windows])
        if (window.isVisible)
            titles.add(juce::String::fromUTF8(window.title.UTF8String ? window.title.UTF8String : "") + " [" + juce::String::fromUTF8(NSStringFromClass([window class]).UTF8String) + "]");
    return titles;
}
}

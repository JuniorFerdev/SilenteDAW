#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace silente::ui
{
// Runs file work (export, conversion, video extraction) on a worker thread with a progress
// window and a Cancel button, then reports the result on the message thread. Self-deleting.
class BackgroundJob final : public juce::ThreadWithProgressWindow
{
public:
    using Work = std::function<juce::Result(const std::function<bool(double)>& progress)>;
    static void start(const juce::String& title, Work work, std::function<void(const juce::Result&)> done)
    {
        (new BackgroundJob(title, std::move(work), std::move(done)))->launchThread();
    }

private:
    Work work;
    std::function<void(const juce::Result&)> done;
    juce::Result result = juce::Result::ok();
    BackgroundJob(const juce::String& title, Work w, std::function<void(const juce::Result&)> d)
        : juce::ThreadWithProgressWindow(title, true, true), work(std::move(w)), done(std::move(d)) {}
    void run() override
    {
        result = work([this](double p) { setProgress(juce::jlimit(0.0, 1.0, p)); return ! threadShouldExit(); });
    }
    void threadComplete(bool cancelled) override
    {
        auto callback = std::move(done);
        const auto outcome = cancelled ? juce::Result::fail("cancel") : result;
        juce::MessageManager::callAsync([this] { delete this; });
        if (callback) callback(outcome);
    }
};
}

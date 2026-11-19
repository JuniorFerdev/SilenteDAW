#include "VideoViewer.h"
#include "../theme/Colours.h"
#include "../theme/Drawing.h"

namespace silente::ui
{
using namespace theme;

// Worker thread: only the newest request matters; older ones are dropped while it decodes.
class VideoViewer::Decoder final : public juce::Thread
{
public:
    Decoder() : juce::Thread(juce::String::fromUTF8("Silente · fotogramas de video")) { startThread(juce::Thread::Priority::low); }
    ~Decoder() override { signalThreadShouldExit(); notify(); stopThread(4000); }
    void request(const app::VideoFrameTarget& target, int width, int height)
    {
        { const juce::ScopedLock lock(guard); pending = target; pendingWidth = width; pendingHeight = height; hasPending = true; }
        notify();
    }
    // The last decoded frame and the request it answers.
    bool result(app::VideoFrameTarget& target, juce::Image& image)
    {
        const juce::ScopedLock lock(guard);
        if (! hasResult) return false;
        target = done; image = doneImage; return true;
    }
    void run() override
    {
        io::FrameReader reader;
        while (! threadShouldExit())
        {
            app::VideoFrameTarget next; int width = 0, height = 0;
            {
                const juce::ScopedLock lock(guard);
                if (hasPending) { next = pending; width = pendingWidth; height = pendingHeight; hasPending = false; }
                else width = -1;
            }
            if (width < 0) { wait(-1); continue; }
            auto image = reader.frameAt(juce::File(next.path), next.sourceSeconds, juce::jmax(16, width), juce::jmax(16, height));
            const juce::ScopedLock lock(guard);
            done = next; doneImage = image; hasResult = true;
        }
    }
private:
    juce::CriticalSection guard; // decoder thread <-> message thread (never the audio thread)
    app::VideoFrameTarget pending, done;
    juce::Image doneImage;
    int pendingWidth = 0, pendingHeight = 0;
    bool hasPending = false, hasResult = false;
};

VideoViewer::VideoViewer(AudioEngine& e) : engine(e), decoder(std::make_unique<Decoder>())
{
    setComponentID("video-viewer");
    setOpaque(true);
}

VideoViewer::~VideoViewer() { stopTimer(); }

void VideoViewer::visibilityChanged()
{
    if (isShowing()) { startTimerHz(30); update(); }
    else stopTimer();
}

void VideoViewer::update()
{
    const auto target = app::videoFrameAt(engine, engine.positionSeconds.load());
    if (target != wanted)
    {
        wanted = target;
        if (target)
        {   // Decode at the window size in physical pixels (Retina) for a sharp picture.
            const float scale = juce::jmax(1.0f, juce::Component::getApproximateScaleFactorForComponent(this));
            decoder->request(*target, juce::roundToInt((float) juce::jmax(160, getWidth()) * scale), juce::roundToInt((float) juce::jmax(90, getHeight()) * scale));
        }
        else { shown = {}; shownTarget.reset(); missing = false; repaint(); }
    }
    app::VideoFrameTarget done; juce::Image image;
    if (wanted && decoder->result(done, image) && done == *wanted && (! shownTarget || ! (*shownTarget == done)))
    {
        shownTarget = done; missing = ! image.isValid();
        shown = image;
        repaint();
    }
}

bool VideoViewer::waitForFrame(int timeoutMs)
{
    const auto until = juce::Time::getMillisecondCounter() + (juce::uint32) juce::jmax(0, timeoutMs);
    for (;;)
    {
        update();
        if (! wanted || (shownTarget && *shownTarget == *wanted)) return true;
        if (juce::Time::getMillisecondCounter() >= until) return false;
        juce::Thread::sleep(5);
    }
}

void VideoViewer::paint(juce::Graphics& g)
{
    g.fillAll(videoBackground);
    const auto area = getLocalBounds();
    if (wanted && shown.isValid() && ! missing)
    {
        g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
        g.drawImage(shown, area.toFloat(), juce::RectanglePlacement::centred);
    }
    if (! wanted) text(g, i18n::tr("Sin video en la posición del cursor"), area, 13, muted, juce::Justification::centred);
    else if (missing) text(g, i18n::tr("Archivo de video ausente: ") + juce::File::createFileWithoutCheckingPath(wanted->path).getFileName(),
                           area.reduced(12), 13, coral, juce::Justification::centred);
}
}

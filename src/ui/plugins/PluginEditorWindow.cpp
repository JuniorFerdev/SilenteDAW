#include "PluginEditorWindow.h"
#include "../theme/Colours.h"

namespace silente::ui
{
// The editor's limits plus the window frame (as in JUCE's AudioPluginHost).
class PluginEditorWindow::Constrainer final : public juce::BorderedComponentBoundsConstrainer
{
public:
    explicit Constrainer(juce::DocumentWindow& w) : window(w) {}
    juce::ComponentBoundsConstrainer* getWrappedConstrainer() const override
    {
        auto* editor = dynamic_cast<juce::AudioProcessorEditor*>(window.getContentComponent());
        return editor != nullptr ? editor->getConstrainer() : nullptr;
    }
    juce::BorderSize<int> getAdditionalBorder() const override
    {
        juce::BorderSize<int> frame;
        if (auto* peer = window.getPeer())
            if (const auto size = peer->getFrameSizeIfPresent()) frame = *size;
        return frame.addedTo(window.getContentComponentBorder());
    }
private:
    juce::DocumentWindow& window;
};

PluginEditorWindow::PluginEditorWindow(juce::AudioProcessor& processor, const juce::String& title)
    : juce::DocumentWindow(title, theme::panel, juce::DocumentWindow::closeButton), constrainer(std::make_unique<Constrainer>(*this))
{
    setUsingNativeTitleBar(true);
    setSize(400, 300);
    juce::AudioProcessorEditor* editor = processor.hasEditor() ? processor.createEditorIfNeeded() : nullptr;
    if (editor == nullptr)
    {
        auto* generic = new juce::GenericAudioProcessorEditor(processor);
        generic->setResizeLimits(200, 300, 1000, 10000);
        editor = generic;
    }
    setContentOwned(editor, true); // the window follows the editor's size from now on
    setResizable(editor->isResizable(), false);
    setConstrainer(constrainer.get());
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        setTopLeftPosition(display->userArea.getCentreX() - getWidth() / 2, display->userArea.getY() + 80);
    setVisible(true);
    toFront(true);
}

PluginEditorWindow::~PluginEditorWindow()
{
    clearContentComponent();
    setConstrainer(nullptr);
}
}

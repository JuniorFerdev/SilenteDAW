#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

namespace silente::ui
{
// Window for a plugin's own editor (Waves, etc.). The editor is a native view: the window is
// never scaled with Silente's interface size (115 %…), its size comes from the editor and the
// editor's own size limits are kept. Scaling it made the window and the plugin view fight over
// the size: collapsed window, flicker, impossible to move.
class PluginEditorWindow : public juce::DocumentWindow
{
public:
    PluginEditorWindow(juce::AudioProcessor&, const juce::String& title);
    ~PluginEditorWindow() override;
    void closeButtonPressed() override { setVisible(false); }
    float getDesktopScaleFactor() const override { return 1.0f; }

private:
    class Constrainer;
    std::unique_ptr<Constrainer> constrainer;
};
}

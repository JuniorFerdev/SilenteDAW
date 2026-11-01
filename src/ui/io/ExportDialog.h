#pragma once
#include "FormatSelector.h"
#include "../../AudioEngine.h"
#include "../theme/SilenteLookAndFeel.h"

namespace silente::ui
{
// «Exportar mezcla de audio» (Cubase manual, p. 1527): channel selection (Stereo Out mix and any
// tracks, one file per channel), file name and location, file format and audio engine output.
class ExportDialog final : public juce::Component
{
public:
    ExportDialog(AudioEngine&, int selectedTrack);
    ~ExportDialog() override { setLookAndFeel(nullptr); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    AudioEngine& engine;
    theme::SilenteLookAndFeel look;
    FormatSelector format;
    juce::TextEditor name;
    juce::TextButton chooseFolder, all, none, selectedOnly, exportButton;
    juce::Label folderLabel, nameLabel, status;
    juce::File folder;
    std::vector<std::pair<juce::String, juce::String>> channels; // id ("" = mix) and name
    std::vector<bool> checked;
    int selectedTrack;
    juce::Rectangle<int> listArea;
    std::unique_ptr<juce::FileChooser> chooser;
    static constexpr int rowHeight = 24;
    void startExport();
};
}

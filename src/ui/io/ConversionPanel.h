#pragma once
#include "FormatSelector.h"
#include "../theme/SilenteLookAndFeel.h"

namespace silente::ui
{
// «Conversión de formatos»: converts audio files between WAV, AIFF, FLAC, MP3 and M4A/AAC
// (any rate), and extracts the audio of video files (MP4, MOV, M4V…). Batch, with progress.
class ConversionPanel final : public juce::Component
{
public:
    explicit ConversionPanel(juce::AudioFormatManager&);
    ~ConversionPanel() override { setLookAndFeel(nullptr); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void addFiles(bool videoOnly);

private:
    juce::AudioFormatManager& formats;
    theme::SilenteLookAndFeel look;
    FormatSelector format;
    juce::TextButton addButton, addVideo, removeButton, clearButton, chooseFolder, convertButton;
    juce::ToggleButton sameFolder;
    juce::Label folderLabel, status;
    juce::File folder;
    struct Item { juce::File file; juce::String state; bool ok = false; };
    std::vector<Item> items;
    int selectedRow = -1;
    juce::Rectangle<int> listArea;
    std::unique_ptr<juce::FileChooser> chooser;
    static constexpr int rowHeight = 24;
    void startConversion();
};
}

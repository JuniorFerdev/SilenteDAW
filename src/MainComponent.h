#pragma once
#include "AudioEngine.h"
#include "PluginManager.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace silente
{
class MainComponent final : public juce::Component,
                            public juce::FileDragAndDropTarget,
                            private juce::Timer
{
public:
    MainComponent(AudioEngine&, PluginManager&);
    ~MainComponent() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;
    // Folder for the window layout and transport options (~/Library/Silente). Test and capture
    // runs point it elsewhere so they never overwrite the user's own settings.
    static juce::File settingsFolder();
    static void useSettingsFolder(const juce::File&);
    void newProject();
    void openProject();
    void saveProject();
    void importFiles();
    void exportAudio();
    void showAudioSettings();
    void showSettings();
    void fitSettingsToScreen();
    std::function<void(float)> onInterfaceScaleChange;
    void showMixer();
    void toggleMetronome();
    void showMixConsole();
    void showConversion(bool extractFromVideo);
    void showKeyCommands();
    juce::PopupMenu transportMenu();
    void transportMenuItem(int id);
    juce::PopupMenu audioMenu();          // Audio › Fundidos (items 200–219)
    void audioMenuItem(int id);
    juce::PopupMenu rangeMenu();          // Edición › Rango (items 71–77)
    void rangeMenuItem(int id);
    bool runCommand(const juce::String& keyCommandId);
    juce::StringArray editHistory() const; // names of the undoable edits, oldest first
    float projectXForTime(double seconds) const; // Event-display geometry at the current horizontal zoom.
    void showMetronomeSettings();
    void showTapTempo();
    bool metronomeOn() const;
    void addAudioTrack();
    void addMidiTrack();
    void addTrack(TrackKind);
    void showPlugins();
    void refreshLanguage();
    void requestClose(std::function<void()> onReady);
private:
    struct Impl;
    std::unique_ptr<Impl> ui;
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
}

#pragma once
#include "AudioEngine.h"
#include "PluginManager.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <optional>

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
    // Ruta de un archivo elegido en «Abrir proyecto»: si es .cpr entra en el flujo de importación (true); si no, false.
    bool openProjectFile(const juce::File&);
    std::function<void(const juce::File&)> cubaseImportOverride; // pruebas: sustituye el selector de destino
    void saveProject();
    void importFiles();
    void importCubaseProject(); // Archivo › «Importar proyecto Cubase (.cpr)…» (io/CprImport)
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
    int trackHeightOf(int trackIndex) const; // Height in pixels of a track row in the event display.
    float waveZoomValue() const; // Zoom de forma de onda de todas las pistas (1–16).
    float projectXForTime(double seconds) const; // Event-display geometry at the current horizontal zoom.
    void showMetronomeSettings();
    void showTapTempo();
    void showTempoMap(); // «Mapa de tempo y de compás» window (tempo and signature events)
    void showPool(); // ventana «Pool de medios» (solo lectura y reasignación de rutas)
    void showVideoPlayer(); // «Reproductor de video» (F8): picture of the video track at the cursor
    void importVideoOrImage(); // Archivo › «Importar video o imagen en pista de video…» (file chooser)
    // Places movies (mp4/mov/m4v) and images (png/jpg) on the selected or first video track, at the
    // cursor; creates a video track when there is none. Undoable. Fails when nothing was imported.
    juce::Result importVisualMedia(const juce::StringArray& paths);
    bool metronomeOn() const;
    void addAudioTrack();
    void addMidiTrack();
    void addTrack(TrackKind);
    void showAddTrack(std::optional<TrackKind> initial={});
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

#include "i18n/Translations.h"
#pragma once
#include "AudioEngine.h"
#include "plugins/PluginSelection.h"

namespace silente
{
class PluginManager final
{
public:
    explicit PluginManager(AudioEngine&);
    ~PluginManager();
    void showBrowser(int trackIndex);
    void showEffectSelector(int trackIndex,int effectIndex=-1,juce::Component* anchor=nullptr);
    void showInstrumentSelector(int trackIndex=-1,juce::Component* anchor=nullptr);
    void showEffectEditor(int trackIndex,int effectIndex,juce::Component* anchor=nullptr);
    void showEffectSlot(int trackIndex,int effectIndex,juce::Component* anchor=nullptr);
    void showInstrument(int trackIndex,juce::Component* anchor=nullptr);
    void toggleBypass(int trackIndex,int slotIndex);
    void removeSlot(int trackIndex,int slotIndex);
    void showEditor(int trackIndex, int slotIndex);
    void closeEditors();
    void refreshLanguage();
    void scanPlugins();
    void stopScan();
    bool isScanning() const;
    juce::String status() const;
    void insertPlugin(int trackIndex, const juce::PluginDescription&);
    void saveCatalog();
    static int enumerateInChild(const juce::String& format, const juce::File& destination);
    static int scanInChild(const juce::String& format, const juce::String& identifier, const juce::File& destination);
    static juce::File settingsDirectory();
    std::function<void()> onChange;
    std::function<void(int)> onTrackCreated;
private:
    class Scanner;
    class BrowserWindow;
    class EditorWindow;
    struct SelectionTarget
    {
        std::shared_ptr<Track> track;
        std::shared_ptr<PluginSlot> replacing;
        PluginKind kind=PluginKind::effect;
        bool newInstrumentTrack=false;
        unsigned generation=0;
    };
    void openSelector(SelectionTarget,juce::Component* anchor);
    void loadSelection(SelectionTarget,const juce::PluginDescription&);
    void closeEditorFor(const std::shared_ptr<PluginSlot>&);
    unsigned generation=0;
    AudioEngine& engine;
    std::unique_ptr<Scanner> scanner;
    std::unique_ptr<BrowserWindow> browser;
    juce::OwnedArray<EditorWindow> editors;
    std::shared_ptr<int> lifetime = std::make_shared<int>(1);
    mutable juce::CriticalSection statusLock;
    juce::String statusText { i18n::tr("Busca tus instrumentos y efectos VST3 / Audio Units.") };
    void setStatus(const juce::String&);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginManager)
};
}

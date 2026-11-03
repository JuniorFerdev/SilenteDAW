#include "ui/plugins/PluginEditorWindow.h"
#include "i18n/Translations.h"
#include "PluginManager.h"
#include "i18n/UiTranslation.h"
#include "ui/theme/Colours.h"
#include "ui/plugins/PluginSelector.h"

namespace silente
{
using namespace juce;
namespace
{
const uint32 panel = ui::theme::panel.getARGB();
#if defined(__x86_64__)
constexpr const char* catalogueName="Plugins-Intel.xml";
#else
constexpr const char* catalogueName="Plugins.xml";
#endif
}

class PluginManager::Scanner final : public Thread
{
public:
    explicit Scanner(PluginManager& owner) : Thread(juce::String::fromUTF8("Silente · explorar plugins")), manager(owner) {}
    ~Scanner() override { signalThreadShouldExit(); stopThread(3000); }
    void run() override
    {
        AudioPluginFormatManager formats;
        addDefaultFormatsToManager(formats);
        struct Candidate { String format, identifier; };
        std::vector<Candidate> candidates;
        auto directory = PluginManager::settingsDirectory().getChildFile("Scan");
        directory.createDirectory();
        int index = 0, failed = 0;
        for (auto* format : formats.getFormats())
        {
            if (threadShouldExit()) break;
            manager.setStatus(i18n::tr("Buscando ubicaciones ") + format->getName() + juce::String::fromUTF8("…"));
            auto output = directory.getChildFile(Uuid().toString() + ".xml");
            ChildProcess process;
            StringArray args { File::getSpecialLocation(File::currentExecutableFile).getFullPathName(), "--enumerate-plugins", format->getName(), output.getFullPathName() };
            bool success = process.start(args, 0);
            const auto started = Time::getMillisecondCounterHiRes();
            if (success)
            {
                while (process.isRunning() && !threadShouldExit() && Time::getMillisecondCounterHiRes() - started < 60000.0) wait(50);
                if (process.isRunning()) { process.kill(); success = false; }
                else success = process.getExitCode() == 0;
            }
            if (success)
            {
                if (auto xml = XmlDocument::parse(output))
                    for (auto* item : xml->getChildIterator()) candidates.push_back({ format->getName(), item->getStringAttribute("identifier") });
                else ++failed;
            }
            else ++failed;
            output.deleteFile();
        }
        // Deliver built-in AU and individual VST3 results before large vendor shells.
        std::stable_sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b)
        {
            const auto priority=[](const Candidate& c){if(c.identifier.endsWith(",appl"))return 0;if(c.identifier.containsIgnoreCase("WaveShell")||c.identifier.endsWith(",ksWV"))return 2;return 1;};
            return priority(a)<priority(b);
        });
        for (const auto& candidate : candidates)
        {
            if (threadShouldExit()) break;
            ++index;
            manager.setStatus(i18n::tr("Explorando ") + String(index) + "/" + String(candidates.size()) + juce::String::fromUTF8(" · ") + File(candidate.identifier).getFileName());
            auto output = directory.getChildFile(Uuid().toString() + ".xml");
            ChildProcess process;
            StringArray args { File::getSpecialLocation(File::currentExecutableFile).getFullPathName(), "--scan-plugin", candidate.format, candidate.identifier, output.getFullPathName() };
            bool success = process.start(args, 0);
            auto start = Time::getMillisecondCounterHiRes();
            if (success)
            {
                while (process.isRunning() && !threadShouldExit() && Time::getMillisecondCounterHiRes() - start < 60000.0)
                    wait(50);
                if (process.isRunning()) { process.kill(); success = false; }
                else success = process.getExitCode() == 0;
            }
            if (success)
            {
                if (auto xml = XmlDocument::parse(output))
                {
                    Array<PluginDescription> descriptions;
                    for (auto* item : xml->getChildIterator())
                    {
                        PluginDescription description;
                        if (description.loadFromXml(*item)) descriptions.add(description);
                    }
                    std::weak_ptr<int> weak = manager.lifetime;
                    auto* target = &manager;
                    MessageManager::callAsync([weak, target, descriptions]
                    {
                        if (!weak.lock()) return;
                        for (const auto& description : descriptions) target->engine.knownPlugins.addType(description);
                        target->saveCatalog();

                    });
                }
                else ++failed;
            }
            else
            {
                ++failed;
                directory.getChildFile("failures.log").appendText(Time::getCurrentTime().toISO8601(true) + " | " + candidate.format + " | " + candidate.identifier + i18n::tr(" | no completó el análisis\n"));
            }
            output.deleteFile();
        }
        manager.setStatus(threadShouldExit() ? i18n::tr("Exploración detenida. Los plugins encontrados están disponibles.")
                          : i18n::tr("Exploración finalizada · ") + String(index) + i18n::tr(" ubicaciones · ") + String(failed) + i18n::tr(" sin respuesta o incompatibles."));
    }
private:
    PluginManager& manager;
};

class PluginManager::BrowserWindow final : public DocumentWindow
{
public:
    explicit BrowserWindow(Component* content,Component* anchor) : DocumentWindow(i18n::tr("Silente · Plugins"),Colour(panel),DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);setContentOwned(content,true);setResizable(true,false);
        setResizeLimits(510,440,900,900);centreWithSize(540,540);
        if(anchor)
        {
            auto* display=Desktop::getInstance().getDisplays().getDisplayForRect(anchor->getScreenBounds());
            auto area=display?display->userArea:Desktop::getInstance().getDisplays().getTotalBounds(true);
            auto position=anchor->getScreenBounds().getBottomLeft();
            setBounds(getBounds().withPosition(position).constrainedWithin(area));
        }
        setVisible(true);toFront(true);
    }
    void closeButtonPressed() override { setVisible(false); }
};

class PluginManager::EditorWindow final : public ui::PluginEditorWindow
{
public:
    std::shared_ptr<PluginSlot> slot;
    EditorWindow(std::shared_ptr<PluginSlot> item,const String& context)
        : ui::PluginEditorWindow(*item->processor, context + juce::String::fromUTF8(" · ") + item->description.name), slot(std::move(item)) {}
};

File PluginManager::settingsDirectory()
{
    auto dir = File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Silente");
    dir.createDirectory(); return dir;
}
PluginManager::PluginManager(AudioEngine& e) : engine(e)
{
    if (auto xml = XmlDocument::parse(settingsDirectory().getChildFile(catalogueName))) engine.knownPlugins.recreateFromXml(*xml);
}
PluginManager::~PluginManager() { lifetime.reset(); scanner.reset(); browser.reset(); closeEditors(); saveCatalog(); }
void PluginManager::setStatus(const String& value) { const ScopedLock lock(statusLock); statusText = value; }
String PluginManager::status() const { const ScopedLock lock(statusLock); return statusText; }
bool PluginManager::isScanning() const { return scanner != nullptr && scanner->isThreadRunning(); }
void PluginManager::showBrowser(int trackIndex)
{
    if(trackIndex<0)showInstrumentSelector();
    else showEffectSelector(trackIndex);
}
void PluginManager::scanPlugins()
{
    if (isScanning()) return;
    scanner = std::make_unique<Scanner>(*this); scanner->startThread();
}
void PluginManager::stopScan() { if (scanner != nullptr) scanner->signalThreadShouldExit(); }
void PluginManager::saveCatalog()
{
    if (auto xml = engine.knownPlugins.createXml()) xml->writeTo(settingsDirectory().getChildFile(catalogueName));
}
void PluginManager::refreshLanguage()
{
    if(!isScanning())setStatus(i18n::tr("Busca tus instrumentos y efectos VST3 / Audio Units."));
    if(browser){browser->setName(i18n::tr("Silente · Plugins"));i18n::translateControls(*browser);}
}
void PluginManager::closeEditors() { ++generation;editors.clear();if(browser)browser->setVisible(false); }
void PluginManager::closeEditorFor(const std::shared_ptr<PluginSlot>& slot)
{
    for(int n=editors.size()-1;n>=0;--n)if(editors[n]->slot==slot)editors.remove(n);
}
void PluginManager::showEditor(int trackIndex, int slotIndex)
{
    std::shared_ptr<PluginSlot> slot;
    {
        const ScopedLock lock(engine.modelLock);
        if (!isPositiveAndBelow(trackIndex, static_cast<int>(engine.tracks.size()))) return;
        const auto& plugins = engine.tracks[static_cast<size_t>(trackIndex)]->plugins;
        if (!isPositiveAndBelow(slotIndex, static_cast<int>(plugins.size()))) return;
        slot = plugins[static_cast<size_t>(slotIndex)];
    }
    if (!slot->processor) { setStatus(slot->error); return; }
    for (auto* editor : editors) if (editor->slot == slot) { editor->setVisible(true); editor->toFront(true); return; }
    String context;
    {const ScopedLock lock(engine.modelLock);if(isPositiveAndBelow(trackIndex,static_cast<int>(engine.tracks.size())))context=engine.tracks[(size_t)trackIndex]->name;}
    editors.add(new EditorWindow(slot,context));
}
void PluginManager::openSelector(SelectionTarget target,Component* anchor)
{
    target.generation=++generation;
    const auto channel=target.track?target.track->name:i18n::tr("Nueva pista de instrumento");
    auto* selector=new ui::PluginSelector(engine.knownPlugins,target.kind,channel,settingsDirectory().getChildFile("PluginFavourites.json"));
    const std::weak_ptr<int> weak=lifetime;
    selector->onChoose=[this,weak,target](const PluginDescription& description)
    {
        if(!weak.lock()||target.generation!=generation)return;
        if(browser)browser->setVisible(false);
        loadSelection(target,description);
    };
    selector->onScan=[this]{scanPlugins();};selector->onStop=[this]{stopScan();};
    selector->scanning=[this]{return isScanning();};selector->scanStatus=[this]{return status();};
    browser=std::make_unique<BrowserWindow>(selector,anchor);selector->focusSearch();
}
void PluginManager::showEffectSelector(int trackIndex,int effectIndex,Component* anchor)
{
    SelectionTarget target;target.kind=PluginKind::effect;
    {const ScopedLock lock(engine.modelLock);
     if(!isPositiveAndBelow(trackIndex,static_cast<int>(engine.tracks.size())))return;
     target.track=engine.tracks[(size_t)trackIndex];
     int slot=effectSlotIndex(*target.track,effectIndex);if(slot>=0)target.replacing=target.track->plugins[(size_t)slot];}
    openSelector(target,anchor);
}
void PluginManager::showInstrumentSelector(int trackIndex,Component* anchor)
{
    SelectionTarget target;target.kind=PluginKind::instrument;target.newInstrumentTrack=trackIndex<0;
    {const ScopedLock lock(engine.modelLock);
     if(trackIndex>=0){if(!isPositiveAndBelow(trackIndex,static_cast<int>(engine.tracks.size())))return;
       target.track=engine.tracks[(size_t)trackIndex];if(!target.track->isMidi)return;
       const int slot=instrumentSlotIndex(*target.track);if(slot>=0)target.replacing=target.track->plugins[(size_t)slot];}}
    openSelector(target,anchor);
}
void PluginManager::toggleBypass(int trackIndex,int slotIndex)
{
    {const ScopedLock lock(engine.modelLock);if(!isPositiveAndBelow(trackIndex,static_cast<int>(engine.tracks.size())))return;
     auto& slots=engine.tracks[(size_t)trackIndex]->plugins;if(!isPositiveAndBelow(slotIndex,static_cast<int>(slots.size())))return;
     slots[(size_t)slotIndex]->bypassed=!slots[(size_t)slotIndex]->bypassed.load();}
    if(onChange)onChange();
}
void PluginManager::removeSlot(int trackIndex,int slotIndex)
{
    {const ScopedLock lock(engine.modelLock);if(!isPositiveAndBelow(trackIndex,static_cast<int>(engine.tracks.size())))return;
     auto& slots=engine.tracks[(size_t)trackIndex]->plugins;if(!isPositiveAndBelow(slotIndex,static_cast<int>(slots.size())))return;
     closeEditorFor(slots[(size_t)slotIndex]);engine.removePlugin(trackIndex,slotIndex);}
    if(onChange)onChange();
}
void PluginManager::showEffectEditor(int trackIndex,int effectIndex,Component* anchor)
{
    int slot=-1;{const ScopedLock lock(engine.modelLock);if(isPositiveAndBelow(trackIndex,static_cast<int>(engine.tracks.size())))slot=effectSlotIndex(*engine.tracks[(size_t)trackIndex],effectIndex);}
    if(slot>=0)showEditor(trackIndex,slot);else showEffectSelector(trackIndex,effectIndex,anchor);
}
void PluginManager::showEffectSlot(int trackIndex,int effectIndex,Component* anchor)
{
    std::shared_ptr<Track> track;std::shared_ptr<PluginSlot> slot;
    {const ScopedLock lock(engine.modelLock);if(!isPositiveAndBelow(trackIndex,static_cast<int>(engine.tracks.size())))return;
     track=engine.tracks[(size_t)trackIndex];const int index=effectSlotIndex(*track,effectIndex);if(index>=0)slot=track->plugins[(size_t)index];}
    if(!slot){showEffectSelector(trackIndex,effectIndex,anchor);return;}
    PopupMenu menu;menu.addSectionHeader(slot->description.name);menu.addItem(1,i18n::tr("Abrir editor"));menu.addItem(2,i18n::tr("Sustituir plugin…"));
    menu.addItem(3,i18n::tr("Bypass del inserto"),true,slot->bypassed);menu.addSeparator();menu.addItem(4,i18n::tr("Quitar plugin"));
    const std::weak_ptr<int> weak=lifetime;Component::SafePointer<Component> safe=anchor;
    menu.showMenuAsync(PopupMenu::Options().withTargetComponent(anchor),[this,weak,track,slot,safe](int result)
    {
        if(!weak.lock()||result==0)return;int t=-1,n=-1;
        {const ScopedLock lock(engine.modelLock);for(int i=0;i<static_cast<int>(engine.tracks.size());++i)if(engine.tracks[(size_t)i]==track)t=i;
         for(int i=0;i<static_cast<int>(track->plugins.size());++i)if(track->plugins[(size_t)i]==slot)n=i;}
        if(t<0||n<0)return;
        if(result==1)showEditor(t,n);
        else if(result==2){SelectionTarget target;target.track=track;target.replacing=slot;openSelector(target,safe.getComponent());}
        else if(result==3)toggleBypass(t,n);else if(result==4)removeSlot(t,n);
    });
}
void PluginManager::showInstrument(int trackIndex,Component* anchor)
{
    int slot=-1;{const ScopedLock lock(engine.modelLock);if(isPositiveAndBelow(trackIndex,static_cast<int>(engine.tracks.size())))slot=instrumentSlotIndex(*engine.tracks[(size_t)trackIndex]);}
    if(slot>=0)showEditor(trackIndex,slot);else showInstrumentSelector(trackIndex,anchor);
}
void PluginManager::insertPlugin(int trackIndex,const PluginDescription& description)
{
    SelectionTarget target;target.kind=description.isInstrument?PluginKind::instrument:PluginKind::effect;target.generation=++generation;
    {const ScopedLock lock(engine.modelLock);if(!isPositiveAndBelow(trackIndex,static_cast<int>(engine.tracks.size())))return;
     target.track=engine.tracks[(size_t)trackIndex];
     if(description.isInstrument){const int slot=instrumentSlotIndex(*target.track);if(slot>=0)target.replacing=target.track->plugins[(size_t)slot];}}
    loadSelection(target,description);
}
void PluginManager::loadSelection(SelectionTarget target,const PluginDescription& description)
{
    if(target.generation!=generation||description.isInstrument!=(target.kind==PluginKind::instrument))return;
    if(description.fileOrIdentifier=="silente:internal-synth")
    {
        int index=-1;
        {const ScopedLock lock(engine.modelLock);
         if(target.newInstrumentTrack){engine.addMidiTrack();index=static_cast<int>(engine.tracks.size())-1;}
         else for(int n=0;n<static_cast<int>(engine.tracks.size());++n)if(engine.tracks[(size_t)n]==target.track)index=n;
         if(index<0)return;
         if(target.replacing){auto& slots=engine.tracks[(size_t)index]->plugins;for(int n=0;n<static_cast<int>(slots.size());++n)if(slots[(size_t)n]==target.replacing){closeEditorFor(target.replacing);engine.removePlugin(index,n);break;}}}
        setStatus(i18n::tr("Sintetizador interno"));if(target.newInstrumentTrack&&onTrackCreated)onTrackCreated(index);if(onChange)onChange();return;
    }
    setStatus(i18n::tr("Cargando ")+description.name+String::fromUTF8("…"));
    const std::weak_ptr<int> weak=lifetime;
    engine.pluginFormats.createPluginInstanceAsync(description,engine.currentSampleRate(),engine.currentBlockSize(),
        [this,weak,target,description](std::unique_ptr<AudioPluginInstance> plugin,const String& error)
        {
            if(!weak.lock()||target.generation!=generation)return;
            if(!plugin){setStatus(i18n::tr("No se pudo cargar ")+description.name);
                AlertWindow::showMessageBoxAsync(MessageBoxIconType::WarningIcon,i18n::tr("No se pudo cargar el plugin"),error+i18n::tr("\n\nComprueba su versión para Apple Silicon y su activación."));return;}
            int trackIndex=-1,replace=-1,added=-1;
            Result result=Result::ok();
            {const ScopedLock lock(engine.modelLock);
            if(target.newInstrumentTrack){engine.addMidiTrack(description.name);trackIndex=static_cast<int>(engine.tracks.size())-1;}
            else for(int i=0;i<static_cast<int>(engine.tracks.size());++i)if(engine.tracks[(size_t)i]==target.track)trackIndex=i;
            if(trackIndex<0)return;
            auto& slots=engine.tracks[(size_t)trackIndex]->plugins;
            if(target.replacing){for(int i=0;i<static_cast<int>(slots.size());++i)if(slots[(size_t)i]==target.replacing)replace=i;if(replace<0)return;}
            result=engine.addPlugin(trackIndex,std::move(plugin),description,replace);
            setStatus(result.wasOk()?description.name+i18n::tr(" añadido al canal."):result.getErrorMessage());
            if(result.failed())
            {
                if(target.newInstrumentTrack)engine.removeTrack(trackIndex);
            }
            else added=replace>=0?replace:description.isInstrument?0:static_cast<int>(slots.size())-1;
            }
            if(result.failed()){AlertWindow::showMessageBoxAsync(MessageBoxIconType::WarningIcon,i18n::tr("No se pudo cargar el plugin"),result.getErrorMessage());return;}
            if(target.replacing)closeEditorFor(target.replacing);
            if(target.newInstrumentTrack&&onTrackCreated)onTrackCreated(trackIndex);
            showEditor(trackIndex,added);if(onChange)onChange();
        });
}
int PluginManager::enumerateInChild(const String& formatName, const File& destination)
{
    AudioPluginFormatManager formats; addDefaultFormatsToManager(formats);
    for (auto* format : formats.getFormats())
    {
        if (format->getName() != formatName) continue;
        auto identifiers = format->searchPathsForPlugins(format->getDefaultLocationsToSearch(), true, false);
        identifiers.removeDuplicates(false);
        XmlElement root("SILENTE_CANDIDATES");
        for (const auto& identifier : identifiers) root.createNewChildElement("plugin")->setAttribute("identifier", identifier);
        return root.writeTo(destination) ? 0 : 3;
    }
    return 2;
}
int PluginManager::scanInChild(const String& formatName, const String& identifier, const File& destination)
{
    AudioPluginFormatManager formats; addDefaultFormatsToManager(formats);
    for (auto* format : formats.getFormats())
    {
        if (format->getName() != formatName) continue;
        OwnedArray<PluginDescription> descriptions; format->findAllTypesForFile(descriptions, identifier);
        XmlElement root("SILENTE_SCAN");
        for (auto* d : descriptions) root.addChildElement(d->createXml().release());
        return root.writeTo(destination) ? 0 : 3;
    }
    return 2;
}
}

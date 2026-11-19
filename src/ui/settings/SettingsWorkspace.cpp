#include "SettingsWorkspace.h"
#include "../audio/AudioSettingsPanel.h"
#include "../keys/KeyCommandsPanel.h"
#include "../metronome/MetronomePanel.h"
#include "../theme/Drawing.h"
#include "../../app/KeyCommands.h"
#include "../../app/Preferences.h"
#include "../../i18n/Translations.h"
#include "../../plugins/PluginImport.h"

namespace silente::ui
{
namespace
{
juce::String title(SettingsWorkspace::Page p)
{
    switch (p)
    {
        case SettingsWorkspace::Page::audio: return i18n::tr("Audio y MIDI");
        case SettingsWorkspace::Page::appearance: return i18n::tr("Apariencia");
        case SettingsWorkspace::Page::transport: return i18n::tr("Transporte");
        case SettingsWorkspace::Page::metronome: return i18n::tr("Metrónomo");
        case SettingsWorkspace::Page::keys: return i18n::tr("Comandos de teclado");
        case SettingsWorkspace::Page::editing: return i18n::tr("Opciones de edición");
        case SettingsWorkspace::Page::plugins: return i18n::tr("Plugins");
    }
    return {};
}
juce::String normalised(juce::String s)
{
    s = s.toLowerCase();
    return s.replaceCharacters(juce::String::fromUTF8("áéíóúüñ"), "aeiouun");
}
juce::Component* findControl(juce::Component& root, const juce::String& id)
{
    if (root.getComponentID() == id) return &root;
    for (auto* child : root.getChildren()) if (auto* found = findControl(*child, id)) return found;
    return nullptr;
}

class EditingPanel final : public juce::Component
{
public:
    EditingPanel()
    {
        addAndMakeVisible(toolbox);addAndMakeVisible(hint);addAndMakeVisible(status);
        toolbox.setComponentID("settings-show-toolbox");
        toolbox.setButtonText(i18n::tr("Mostrar caja de herramientas con clic derecho"));
        toolbox.setToggleState(app::showToolboxOnRightClick(),juce::dontSendNotification);
        hint.setText(i18n::tr("En la ventana de proyecto, una tecla modificadora alterna la caja de herramientas y el menú contextual."),juce::dontSendNotification);
        hint.setColour(juce::Label::textColourId,theme::muted);
        toolbox.onClick=[this]
        {
            const auto result=app::saveShowToolboxOnRightClick(toolbox.getToggleState());
            status.setText(result.failed()?result.getErrorMessage():juce::String(),juce::dontSendNotification);
            if(result.failed())toolbox.setToggleState(app::showToolboxOnRightClick(),juce::dontSendNotification);
        };
        setSize(620,200);
    }
    void resized() override
    {
        toolbox.setBounds(20,20,getWidth()-40,40);hint.setBounds(20,72,getWidth()-40,68);status.setBounds(20,150,getWidth()-40,40);
    }
private:
    juce::ToggleButton toolbox;
    juce::Label hint,status;
};

// Extra plug-in folder (manual: Plug-in Manager, VST 3 plug-in paths). The scan itself runs in
// the existing PluginManager child processes; this page only stores the folder and lists bundles.
class PluginsPanel final : public juce::Component
{
public:
    explicit PluginsPanel(SettingsHooks& h) : hooks(h)
    {
        for(auto* c:std::initializer_list<juce::Component*>{&label,&path,&choose,&clear,&scan,&summary,&hint})addAndMakeVisible(c);
        label.setText(i18n::tr("Carpeta adicional de plugins (VST3 y AU)"),juce::dontSendNotification);
        path.setComponentID("settings-plugin-folder-path");path.setColour(juce::Label::textColourId,theme::ink);
        choose.setComponentID("settings-plugin-folder");choose.setButtonText(i18n::tr("Elegir carpeta…"));
        clear.setComponentID("settings-plugin-folder-clear");clear.setButtonText(i18n::tr("Quitar carpeta"));
        scan.setComponentID("settings-plugin-scan");scan.setButtonText(i18n::tr("Buscar plugins"));scan.setEnabled((bool)hooks.scanPlugins);
        summary.setComponentID("settings-plugin-summary");summary.setColour(juce::Label::textColourId,theme::muted);
        hint.setColour(juce::Label::textColourId,theme::muted);
        hint.setText(i18n::tr("Silente explora las carpetas estándar de macOS y esta carpeta. Los VST3 se cargan desde cualquier ruta; macOS solo registra los Audio Units instalados en Plug-Ins/Components. AAX (Pro Tools), VST2 y CLAP no son compatibles."),juce::dontSendNotification);
        choose.onClick=[this]
        {
            chooser=std::make_unique<juce::FileChooser>(i18n::tr("Elegir carpeta de plugins"),app::pluginSearchFolder());
            const juce::Component::SafePointer<PluginsPanel> safe(this);
            chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe](const juce::FileChooser& fc)
            {
                if(safe!=nullptr&&fc.getResult()!=juce::File())safe->setFolder(fc.getResult());
            });
        };
        clear.onClick=[this]{setFolder({});};
        scan.onClick=[this]{if(hooks.scanPlugins)hooks.scanPlugins();};
        refresh();setSize(620,300);
    }
    // Also the entry point for tests: persists the folder and refreshes the inventory.
    void setFolder(const juce::File& folder)
    {
        const auto result=app::savePluginSearchFolder(folder);
        refresh();if(result.failed())summary.setText(result.getErrorMessage(),juce::dontSendNotification);
    }
    void resized() override
    {
        auto r=getLocalBounds().reduced(20);label.setBounds(r.removeFromTop(26));path.setBounds(r.removeFromTop(26));r.removeFromTop(6);
        auto row=r.removeFromTop(28);choose.setBounds(row.removeFromLeft(150));row.removeFromLeft(8);clear.setBounds(row.removeFromLeft(130));row.removeFromLeft(8);scan.setBounds(row.removeFromLeft(140));
        r.removeFromTop(10);summary.setBounds(r.removeFromTop(40));r.removeFromTop(6);hint.setBounds(r.removeFromTop(90));
    }
private:
    SettingsHooks& hooks;
    juce::Label label,path,summary,hint;
    juce::TextButton choose,clear,scan;
    std::unique_ptr<juce::FileChooser> chooser;
    void refresh()
    {
        const auto folder=app::pluginSearchFolder();
        path.setText(folder==juce::File()?i18n::tr("Sin carpeta adicional"):folder.getFullPathName(),juce::dontSendNotification);
        clear.setEnabled(folder!=juce::File());
        if(folder==juce::File()){summary.setText({},juce::dontSendNotification);return;}
        const auto found=plugins::inventoryFolder(folder);const auto sep=juce::String::fromUTF8(" · ");
        summary.setText(juce::String(found.vst3.size())+" VST3"+sep+juce::String(found.audioUnits.size())+i18n::tr(" AU sin registrar en macOS")
                        +sep+juce::String(found.unsupported())+i18n::tr(" AAX/VST2/CLAP no compatibles"),juce::dontSendNotification);
    }
};

class AppearancePanel final : public juce::Component, private juce::Timer
{
public:
    explicit AppearancePanel(SettingsHooks& h) : hooks(h)
    {
        addAndMakeVisible(label); addAndMakeVisible(scale);
        label.setText(i18n::tr("Tamaño de la interfaz y de la letra"), juce::dontSendNotification);
        scale.setComponentID("settings-scale");
        scale.addItemList({ "100 %", "115 %", "125 %", "140 %", "150 %" }, 1);
        scale.setEnabled((bool) hooks.writeScale);
        timerCallback();
        scale.onChange = [this] { if (hooks.writeScale && scale.getSelectedId() > 0) hooks.writeScale(values[scale.getSelectedId() - 1]); };
        setSize(620,180); startTimer(500);
    }
    void resized() override { label.setBounds(20,20,getWidth()-40,28); scale.setBounds(20,60,180,28); }
private:
    SettingsHooks& hooks;
    juce::Label label;
    juce::ComboBox scale;
    static constexpr float values[] { 1.0f,1.15f,1.25f,1.4f,1.5f };
    void timerCallback() override
    {
        const float current = hooks.readScale ? hooks.readScale() : 1.15f;
        int closest = 0;
        for (int n = 1; n < 5; ++n) if (std::abs(current-values[n]) < std::abs(current-values[closest])) closest=n;
        scale.setSelectedId(closest+1,juce::dontSendNotification);
    }
};

class TransportPanel final : public juce::Component, private juce::Timer
{
public:
    explicit TransportPanel(SettingsHooks& h) : hooks(h)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &startLabel,&start,&returnToStart,&pre,&post,&autoScroll,&stationary,&preBars,&postBars }) addAndMakeVisible(c);
        startLabel.setText(i18n::tr("Modo de inicio"),juce::dontSendNotification);
        start.addItemList({i18n::tr("Cursor"),i18n::tr("Inicio del ciclo"),i18n::tr("Inicio de la selección"),i18n::tr("Selección o inicio del ciclo")},1);
        start.setComponentID("settings-start-mode");
        start.onChange=[this]{edit([this](auto& s){s.startMode=start.getSelectedId()-1;});};
        const auto toggle=[this](juce::ToggleButton& b,const juce::String& name,const char* id, bool TransportPreferences::*field)
        {
            b.setButtonText(name);b.setComponentID(id);
            b.onClick=[this,&b,field]{edit([&b,field](auto& s){s.*field=b.getToggleState();});};
        };
        toggle(returnToStart,i18n::tr("Volver a la posición de inicio al parar"),"settings-return-start",&TransportPreferences::returnToStart);
        toggle(pre,i18n::tr("Activar pre-roll"),"settings-pre-roll",&TransportPreferences::preRoll);
        toggle(post,i18n::tr("Activar post-roll"),"settings-post-roll",&TransportPreferences::postRoll);
        toggle(autoScroll,i18n::tr("Desplazamiento automático"),"settings-auto-scroll",&TransportPreferences::autoScroll);
        toggle(stationary,i18n::tr("Cursor fijo"),"settings-stationary",&TransportPreferences::stationary);
        autoScroll.onClick=[this]{edit([this](auto& s){s.autoScroll=autoScroll.getToggleState();if(!s.autoScroll)s.stationary=false;});};
        stationary.onClick=[this]{edit([this](auto& s){s.stationary=stationary.getToggleState();if(s.stationary)s.autoScroll=true;});};
        for(auto* slider:{&preBars,&postBars})
        {
            slider->setSliderStyle(juce::Slider::IncDecButtons);slider->setTextBoxStyle(juce::Slider::TextBoxLeft,false,90,26);
            slider->setRange(0.5,4.0,0.5);slider->setTextValueSuffix(" "+i18n::tr("Compases"));
        }
        preBars.setComponentID("settings-pre-bars");postBars.setComponentID("settings-post-bars");
        preBars.onValueChange=[this]{edit([this](auto& s){s.preRollBars=preBars.getValue();});};
        postBars.onValueChange=[this]{edit([this](auto& s){s.postRollBars=postBars.getValue();});};
        timerCallback();setSize(620,340);startTimer(250);
    }
    void resized() override
    {
        auto r=getLocalBounds().reduced(20);startLabel.setBounds(r.removeFromTop(26));start.setBounds(r.removeFromTop(28));r.removeFromTop(12);
        returnToStart.setBounds(r.removeFromTop(30));
        for(auto pair:{std::pair<juce::ToggleButton*,juce::Slider*>{&pre,&preBars},{&post,&postBars}})
        {auto row=r.removeFromTop(36);pair.second->setBounds(row.removeFromRight(170));pair.first->setBounds(row);}
        autoScroll.setBounds(r.removeFromTop(32));stationary.setBounds(r.removeFromTop(32));
    }
private:
    SettingsHooks& hooks;
    juce::Label startLabel;
    juce::ComboBox start;
    juce::ToggleButton returnToStart,pre,post,autoScroll,stationary;
    juce::Slider preBars,postBars;
    void edit(const std::function<void(TransportPreferences&)>& f)
    {
        if(!hooks.readTransport||!hooks.writeTransport)return;
        auto state=hooks.readTransport();f(state);hooks.writeTransport(state);timerCallback();
    }
    void timerCallback() override
    {
        if(!hooks.readTransport)return;
        const auto s=hooks.readTransport();start.setSelectedId(s.startMode+1,juce::dontSendNotification);
        const auto set=[](juce::ToggleButton& b,bool value){b.setToggleState(value,juce::dontSendNotification);};
        set(returnToStart,s.returnToStart);set(pre,s.preRoll);set(post,s.postRoll);set(autoScroll,s.autoScroll);set(stationary,s.stationary);
        if(!preBars.isMouseButtonDown())preBars.setValue(s.preRollBars,juce::dontSendNotification);
        if(!postBars.isMouseButtonDown())postBars.setValue(s.postRollBars,juce::dontSendNotification);
    }
};
}

SettingsWorkspace::SettingsWorkspace(AudioEngine& e, SettingsHooks h) : engine(e), hooks(std::move(h)), navigation("settings-navigation",this)
{
    setLookAndFeel(&look);setComponentID("settings-workspace");
    for(auto* c:std::initializer_list<juce::Component*>{&search,&navigation,&heading,&hint,&viewport,&empty})addAndMakeVisible(c);
    search.setComponentID("settings-search");search.setTextToShowWhenEmpty(i18n::tr("Buscar en todos los ajustes…"),theme::muted);
    search.onTextChange=[this]{updateSearch();};navigation.setRowHeight(46);navigation.setColour(juce::ListBox::backgroundColourId,theme::bg);
    search.onReturnKey=[this]{chooseResult(juce::jmax(0,navigation.getSelectedRow()));};
    search.onEscapeKey=[this]{setSearch({});};
    heading.setFont(juce::Font(juce::FontOptions(18.0f)));empty.setText(i18n::tr("No hay ajustes que coincidan con la búsqueda."),juce::dontSendNotification);
    hint.setText(i18n::tr("Los cambios se aplican mientras trabajas en el proyecto."),juce::dontSendNotification);hint.setColour(juce::Label::textColourId,theme::muted);
    viewport.setScrollBarsShown(true,false);viewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
    buildIndex();setSize(960,730);updateSearch();
}
SettingsWorkspace::~SettingsWorkspace() { navigation.setModel(nullptr);viewport.setViewedComponent(nullptr,false);setLookAndFeel(nullptr); }

void SettingsWorkspace::buildIndex()
{
    entries.clear();
    for(auto p:{Page::audio,Page::appearance,Page::transport,Page::metronome,Page::keys,Page::editing,Page::plugins})entries.push_back({p,title(p),{},{},{}});
    const auto add=[this](Page p,const juce::String& label,const juce::String& keywords,const char* control){entries.push_back({p,label,title(p)+" "+keywords,control,{}});};
    add(Page::audio,i18n::tr("Dispositivos de audio y MIDI"),i18n::tr("Entrada salida frecuencia búfer latencia canales MIDI")+" buffer","audio-device-selector");
    add(Page::audio,i18n::tr("Guardar configuración"),{},"audio-save");
    add(Page::appearance,i18n::tr("Tamaño de la interfaz y de la letra"),i18n::tr("Escala resolución tamaño fuente texto"),"settings-scale");
    add(Page::editing,i18n::tr("Mostrar caja de herramientas con clic derecho"),i18n::tr("Menú contextual herramientas ratón"),"settings-show-toolbox");
    add(Page::plugins,i18n::tr("Carpeta adicional de plugins (VST3 y AU)"),i18n::tr("Importar VST3 Audio Units ruta explorar")+" vst au","settings-plugin-folder");
    add(Page::transport,i18n::tr("Modo de inicio"),{},"settings-start-mode");
    add(Page::transport,i18n::tr("Volver a la posición de inicio al parar"),{},"settings-return-start");
    add(Page::transport,i18n::tr("Activar pre-roll"),i18n::tr("Compases"),"settings-pre-roll");
    add(Page::transport,i18n::tr("Activar post-roll"),i18n::tr("Compases"),"settings-post-roll");
    add(Page::transport,i18n::tr("Desplazamiento automático"),{},"settings-auto-scroll");
    add(Page::transport,i18n::tr("Cursor fijo"),{},"settings-stationary");
    add(Page::metronome,i18n::tr("Sonido"),i18n::tr("Clásico Bloque de madera Cencerro Digital Clave"),"metronome-sound");
    add(Page::metronome,i18n::tr("Subdivisión"),i18n::tr("Negras Corcheas Tresillos Semicorcheas"),"metronome-subdivision");
    add(Page::metronome,i18n::tr("Volumen del clic"),{},"metronome-level");
    add(Page::metronome,i18n::tr("Precuenta al grabar"),{},"metronome-count-in");
    add(Page::metronome,i18n::tr("Acentuar el primer tiempo del compás"),{},"metronome-accent");
    add(Page::metronome,i18n::tr("Sonar solo al grabar"),{},"metronome-record-only");
    for(const auto& command:app::keyCommands())entries.push_back({Page::keys,command.name(),title(Page::keys)+" "+command.category()+" "+app::describeKey(command),{},command.name()});
}

void SettingsWorkspace::setSearch(const juce::String& text) { search.setText(text,false);updateSearch(); }
void SettingsWorkspace::updateSearch()
{
    const auto query=normalised(search.getText().trim());juce::StringArray words;words.addTokens(query," \t\r\n",{});words.removeEmptyStrings();
    matches.clear();
    for(size_t n=0;n<entries.size();++n)
    {
        if(query.isEmpty()){if(n<pageCount)matches.push_back((int)n);continue;}
        const auto haystack=normalised(entries[n].title+" "+entries[n].keywords);
        bool match=true;for(const auto& word:words)if(!haystack.contains(word)){match=false;break;}
        if(match)matches.push_back((int)n);
    }
    navigation.deselectAllRows();navigation.updateContent();empty.setVisible(matches.empty());viewport.setVisible(!matches.empty());
    if(!matches.empty())chooseResult(query.isEmpty()?(int)selectedPage:0);
    repaint();
}
juce::String SettingsWorkspace::resultText(int row) const
{
    return juce::isPositiveAndBelow(row,matchingCount())?entries[(size_t)matches[(size_t)row]].title:juce::String();
}
void SettingsWorkspace::chooseResult(int row)
{
    if(!juce::isPositiveAndBelow(row,matchingCount()))return;
    const auto entry=entries[(size_t)matches[(size_t)row]];
    showPage(entry.page);
    if(auto* keys=dynamic_cast<KeyCommandsPanel*>(&page(entry.page)))keys->setFilter(entry.command);
    revealControl(entry.control);
    navigation.selectRow(row,false,false);
}
void SettingsWorkspace::selectedRowsChanged(int row)
{
    if(juce::isPositiveAndBelow(row,matchingCount()))
    {
        const auto entry=entries[(size_t)matches[(size_t)row]];showPage(entry.page);
        if(auto* keys=dynamic_cast<KeyCommandsPanel*>(&page(entry.page)))keys->setFilter(entry.command);
        revealControl(entry.control);
    }
}
juce::Component& SettingsWorkspace::page(Page p)
{
    auto& result=pages[(size_t)p];
    if(!result)
    {
        switch(p)
        {
            case Page::audio: result=std::make_unique<AudioSettingsPanel>(engine);break;
            case Page::appearance: result=std::make_unique<AppearancePanel>(hooks);break;
            case Page::transport: result=std::make_unique<TransportPanel>(hooks);break;
            case Page::metronome: result=std::make_unique<MetronomePanel>(engine);break;
            case Page::keys: result=std::make_unique<KeyCommandsPanel>(hooks.keyAvailable?hooks.keyAvailable:[](const juce::String&){return false;});break;
            case Page::editing: result=std::make_unique<EditingPanel>();break;
            case Page::plugins: result=std::make_unique<PluginsPanel>(hooks);break;
        }
    }
    return *result;
}
void SettingsWorkspace::showPage(Page p)
{
    selectedPage=p;currentControl.clear();heading.setText(title(p),juce::dontSendNotification);
    viewport.setViewedComponent(&page(p),false);viewport.setViewPosition(0,0);resized();
    if(search.getText().trim().isEmpty())navigation.selectRow((int)p,false,false);
}
void SettingsWorkspace::revealControl(const juce::String& id)
{
    currentControl=id;
    if(id.isNotEmpty())if(auto* control=findControl(page(selectedPage),id))
    {
        const auto area=page(selectedPage).getLocalArea(control,control->getLocalBounds());
        const auto visible=viewport.getViewArea();
        if(area.getY()<visible.getY())viewport.setViewPosition(0,juce::jmax(0,area.getY()-20));
        else if(area.getBottom()>visible.getBottom())
            viewport.setViewPosition(0,juce::jmax(0,juce::jmin(area.getY()-20,area.getBottom()-visible.getHeight()+20)));
    }
    repaint();
}
void SettingsWorkspace::refreshLanguage()
{
    const auto previous=selectedPage;
    navigation.deselectAllRows();viewport.setViewedComponent(nullptr,false);
    for(auto& panel:pages)panel.reset();
    search.setTextToShowWhenEmpty(i18n::tr("Buscar en todos los ajustes…"),theme::muted);
    hint.setText(i18n::tr("Los cambios se aplican mientras trabajas en el proyecto."),juce::dontSendNotification);
    empty.setText(i18n::tr("No hay ajustes que coincidan con la búsqueda."),juce::dontSendNotification);
    buildIndex();updateSearch();if(search.getText().trim().isEmpty())showPage(previous);
}
void SettingsWorkspace::resized()
{
    auto r=getLocalBounds().reduced(16);search.setBounds(r.removeFromTop(32));r.removeFromTop(12);
    hint.setBounds(r.removeFromBottom(28));navigation.setBounds(r.removeFromLeft(235));r.removeFromLeft(18);
    heading.setBounds(r.removeFromTop(36));r.removeFromTop(8);viewport.setBounds(r);empty.setBounds(r.withHeight(80));
    if(auto* viewed=viewport.getViewedComponent())viewed->setSize(juce::jmax(280,viewport.getWidth()-viewport.getScrollBarThickness()),viewed->getHeight());
}
void SettingsWorkspace::paint(juce::Graphics& g) { g.fillAll(theme::panel); }
void SettingsWorkspace::paintOverChildren(juce::Graphics& g)
{
    if(currentControl.isNotEmpty()&&viewport.isVisible())if(auto* control=findControl(page(selectedPage),currentControl))
    {
        g.saveState();g.reduceClipRegion(viewport.getBounds());g.setColour(theme::accent);
        g.drawRoundedRectangle(getLocalArea(control,control->getLocalBounds()).toFloat().expanded(2),3,1.5f);g.restoreState();
    }
}
void SettingsWorkspace::paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected)
{
    if(!juce::isPositiveAndBelow(row,matchingCount()))return;
    const auto& entry=entries[(size_t)matches[(size_t)row]];
    if(selected){g.setColour(theme::raised);g.fillRect(0,0,width,height);}
    theme::text(g,entry.title,{10,2,width-20,23},12,theme::ink);
    theme::text(g,title(entry.page),{10,24,width-20,18},10,theme::muted);
}
bool SettingsWorkspace::keyPressed(const juce::KeyPress& key)
{
    if(dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()))return false;
    return key.getKeyCode()==juce::KeyPress::spaceKey && hooks.key && hooks.key(key);
}
}

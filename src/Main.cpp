#include "i18n/Translations.h"
#include "AudioEngine.h"
#include "ui/theme/Colours.h"
#include "MainComponent.h"
#include "ui/metronome/MetronomePanel.h"
#include "PluginManager.h"
#include "../tests/EngineTests.h"
#include "../tests/LocalisationUiTests.h"
#include "../tests/WorkflowUiTests.h"
#include "../tests/DeviceTests.h"
#include "../tests/IoTests.h"
#include <iostream>

namespace silente
{
// En el binario universal cada mitad se compila aparte: la x86_64 es la que corre bajo Rosetta.
static juce::String mainWindowTitle()
{
    auto title=i18n::tr("Silente · Audio Workstation");
#if defined(__x86_64__)
    title<<" · Intel (Rosetta)";
#endif
    return title;
}

// «Ventana › Tamaño de la interfaz»: scales every window, text included. Saved per user.
constexpr float interfaceScales[] { 1.0f, 1.15f, 1.25f, 1.4f, 1.5f };
static juce::File interfaceFile() { return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Silente/interface.xml"); }
static float loadInterfaceScale()
{
    if (auto xml = juce::XmlDocument::parse(interfaceFile())) return juce::jlimit(1.0f, 1.5f, (float) xml->getDoubleAttribute("scale", 1.15));
    return 1.15f; // larger than JUCE's default: Fer found the text too small
}

class SilenteApplication final : public juce::JUCEApplication, private juce::MenuBarModel
{
public:
    const juce::String getApplicationName() override { return "Silente"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void initialise(const juce::String&) override
    {
        auto args = getCommandLineParameterArray();
        const int languageIndex=args.indexOf("--language");
        const auto languageCode=languageIndex>=0&&args.size()>languageIndex+1?args[languageIndex+1]:juce::String();
        i18n::initialise(PluginManager::settingsDirectory(),languageCode);
        if(args.contains("--check-locales"))
        {
            const int index=args.indexOf("--check-locales");
            auto directory=args.size()>index+1?juce::File(args[index+1]):juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Silente-locales");
            setApplicationReturnValue(i18n::checkCatalogs(directory));quit();return;
        }
        if (args.size() >= 3 && args[0] == "--enumerate-plugins")
        {
            setApplicationReturnValue(PluginManager::enumerateInChild(args[1], juce::File(args[2]))); quit(); return;
        }
        if (args.size() >= 4 && args[0] == "--scan-plugin")
        {
            setApplicationReturnValue(PluginManager::scanInChild(args[1], args[2], juce::File(args[3]))); quit(); return;
        }
        if(args.contains("--check-devices"))
        {
            const int index=args.indexOf("--check-devices");setApplicationReturnValue(runDeviceTests(juce::File(args.size()>index+1?args[index+1]:"/tmp/Silente-devices")));quit();return;
        }
        if(args.size()>=4&&args[0]=="--check-rate-switch")
        {
            startRateSwitchCheck(juce::File(args[1]),args[2],args[3],[this](int code){setApplicationReturnValue(code);quit();});return;
        }
        if(args.contains("--audio-inventory"))
        {
            const int index=args.indexOf("--audio-inventory");juce::AudioDeviceManager manager;DeviceSettings settings(manager);
            const auto output=juce::File(args.size()>index+1?args[index+1]:"/tmp/Silente-audio.json");output.getParentDirectory().createDirectory();
            output.replaceWithText(juce::JSON::toString(settings.inventory()));quit();return;
        }
        if(args.size()>=3&&args[0]=="--probe-plugin")
        {
            const auto output=juce::File(args[2]);juce::PluginDescription description;
            auto xml=juce::XmlDocument::parse(juce::File(args[1]));
            if(!xml||!xml->getChildByName("PLUGIN")||!description.loadFromXml(*xml->getChildByName("PLUGIN"))){setApplicationReturnValue(2);quit();return;}
            probeFormats=std::make_unique<juce::AudioPluginFormatManager>();juce::addDefaultFormatsToManager(*probeFormats);
            juce::DynamicObject::Ptr report=new juce::DynamicObject();report->setProperty("plugin",description.name);report->setProperty("format",description.pluginFormatName);report->setProperty("version",description.version);
#if defined(__x86_64__)
            report->setProperty("hostArchitecture","x86_64");
#else
            report->setProperty("hostArchitecture","arm64");
#endif
            report->setProperty("licenseVerified",false);report->setProperty("status","creating");output.replaceWithText(juce::JSON::toString(juce::var(report.get())));
            probeFormats->createPluginInstanceAsync(description,48000,512,[this,output,report](std::unique_ptr<juce::AudioPluginInstance> plugin,const juce::String& error)
            {
                report->setProperty("status",plugin?"created":"error");report->setProperty("error",error);
                if(plugin){report->setProperty("inputs",plugin->getTotalNumInputChannels());report->setProperty("outputs",plugin->getTotalNumOutputChannels());}
                output.replaceWithText(juce::JSON::toString(juce::var(report.get())));setApplicationReturnValue(plugin?0:1);quit();
            });
            juce::Timer::callAfterDelay(20000,[this,output,report]{report->setProperty("status","timeout");output.replaceWithText(juce::JSON::toString(juce::var(report.get())));setApplicationReturnValue(3);quit();});return;
        }
        if (args.contains("--self-test"))
        {
            const auto index = args.indexOf("--self-test");
            auto dir = args.size() > index + 1 ? juce::File(args[index + 1]) : juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Silente-tests");
            setApplicationReturnValue(runEngineTests(dir) + runIoTests(dir.getChildFile("io"))); quit(); return;
        }
        if (! args.contains("--check-workflow") && ! args.contains("--screenshot")) juce::Desktop::getInstance().setGlobalScaleFactor(loadInterfaceScale());
        engine = std::make_unique<AudioEngine>();
        engine->click.load(ui::MetronomePanel::settingsFile());
        if(args.size()>=4&&args[0]=="--render-project")
        {
            auto r=engine->loadProject(juce::File(args[1]));if(r.ok())r=engine->exportMix(juce::File(args[2]),args[3].getDoubleValue(),32);
            std::cout<<(r.ok()?"ok":r.getErrorMessage().toStdString())<<std::endl;setApplicationReturnValue(r.ok()?0:1);quit();return;
        }
        if(args.size()>=4&&args[0]=="--capture-playback")
        {
            startCaptureCheck(*engine,juce::File(args[1]),juce::File(args[2]),args[3].getDoubleValue(),[this](int code){setApplicationReturnValue(code);quit();});return;
        }
        if(args.size()>=3&&args[0]=="--check-playback")
        {
            startPlaybackCheck(*engine,juce::File(args[1]),juce::File(args[2]),[this](int code){setApplicationReturnValue(code);quit();});return;
        }
        plugins = std::make_unique<PluginManager>(*engine);
        const bool screenshot = args.contains("--screenshot");
        if (screenshot || args.contains("--check-workflow"))
        {
            auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Silente-test-settings");
            folder.deleteRecursively(); MainComponent::useSettingsFolder(folder);
        }
        juce::Result audioResult = juce::Result::ok();
        if (!screenshot&&!args.contains("--check-workflow")) audioResult = engine->initialise();
        engine->clear();
        if(args.contains("--demo")||args.contains("--check-language-switch"))engine->createDemo();
        for (const auto& argument : args)
        {
            if (argument.endsWithIgnoreCase(".silente") && juce::File(argument).existsAsFile())
            {
                auto result = engine->loadProject(juce::File(argument));
                if (result.failed()) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, i18n::tr("Abrir proyecto"), result.getErrorMessage());
                break;
            }
        }
        window = std::make_unique<MainWindow>(*engine, *plugins);
        juce::MenuBarModel::setMacMainMenu(this);
        i18n::setLanguageChangedCallback([this]
        {
            if(window){window->content().refreshLanguage();window->setName(mainWindowTitle());}
            if(plugins)plugins->refreshLanguage();
            menuItemsChanged();
        });
        if (audioResult.failed()) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, i18n::tr("Configuración de audio"), audioResult.getErrorMessage());
        if(args.size()>=4&&args[0]=="--ui-playback-check")
        {
            // Plays the project with the real window open (master muted) and reports dropped blocks.
            engine->masterGain=0.0f;engine->seek(0);engine->play();const auto out=juce::File(args[2]);const int callbacksBefore=engine->deviceCallbacks.load(),underrunsBefore=engine->audioUnderruns.load();
            juce::Timer::callAfterDelay((int)(args[3].getDoubleValue()*1000),[this,out,callbacksBefore,underrunsBefore]
            {
                const int underruns=engine->audioUnderruns.load()-underrunsBefore,callbacks=engine->deviceCallbacks.load()-callbacksBefore;
                out.replaceWithText("position "+juce::String(engine->positionSeconds.load())+"\ncallbacks "+juce::String(callbacks)+"\nunderruns "+juce::String(underruns)+"\ndropped "+juce::String(callbacks>0?100.0*underruns/callbacks:0.0,1)+" %\n");
                engine->stop();setApplicationReturnValue(underruns==0?0:1);quit();
            });
        }
        if(args.contains("--check-workflow"))
        {
            const int index=args.indexOf("--check-workflow"),fixtureIndex=args.indexOf("--plugin-fixture");
            const auto output=juce::File(args.size()>index+1?args[index+1]:"/tmp/Silente-workflow");
            const auto fixture=fixtureIndex>=0&&args.size()>fixtureIndex+1?juce::File(args[fixtureIndex+1]):juce::File();
            juce::Timer::callAfterDelay(300,[this,output,fixture]{runWorkflowChecks(window->content(),*engine,*plugins,output,fixture,[this](int code){setApplicationReturnValue(code);quit();});});
        }
        if (screenshot)
        {
            auto index = args.indexOf("--screenshot");
            auto output = juce::File(args.size() > index + 1 ? args[index + 1] : "/tmp/Silente.png");
            const bool checkSwitch=args.contains("--check-language-switch");
            juce::Timer::callAfterDelay(1000, [this, output, checkSwitch]
            {
                if(checkSwitch&&window)setApplicationReturnValue(runLanguageSwitchChecks(window->content(),*engine,output.getParentDirectory()));
                if (window)
                {
                    auto* content = window->getContentComponent();
                    auto image = content->createComponentSnapshot(content->getLocalBounds(), true, 1.0f);
                    output.getParentDirectory().createDirectory();
                    if (auto stream = output.createOutputStream()) { stream->setPosition(0); stream->truncate(); juce::PNGImageFormat().writeImageToStream(image, *stream); }
                }
                quit();
            });
        }
    }
    void shutdown() override
    {
        i18n::setLanguageChangedCallback({});
        juce::MenuBarModel::setMacMainMenu(nullptr);
        if (engine) engine->shutdown();
        window.reset(); plugins.reset(); engine.reset();
    }
    void systemRequestedQuit() override
    {
        if (window) window->content().requestClose([this] { quit(); }); else quit();
    }
    void anotherInstanceStarted(const juce::String&) override {}
private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow(AudioEngine& engine, PluginManager& plugins)
            : DocumentWindow(mainWindowTitle(), ui::theme::bg, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true); setContentOwned(new MainComponent(engine, plugins), true);
            setResizable(true, false); setResizeLimits(960, 600, 3840, 2160);
            fitToScreen(); setVisible(true);
        }
        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
        MainComponent& content() { return *static_cast<MainComponent*>(getContentComponent()); }
        // 1440 × 900 when it fits; smaller on screens that are full at the chosen interface size.
        void fitToScreen()
        {
            auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
            centreWithSize(juce::jmin(1440, area.getWidth() - 16), juce::jmin(900, area.getHeight() - 16));
        }
    };
    std::unique_ptr<AudioEngine> engine;
    std::unique_ptr<PluginManager> plugins;
    std::unique_ptr<juce::AudioPluginFormatManager> probeFormats;
    std::unique_ptr<MainWindow> window;
    juce::StringArray getMenuBarNames() override { return { i18n::tr("Archivo"), i18n::tr("Edición"), i18n::tr("Pista"), i18n::tr("Audio"), i18n::tr("Transporte"), i18n::tr("Estudio"), i18n::tr("Ventana"), i18n::tr("Idioma"), i18n::tr("Ayuda") }; }
    juce::PopupMenu getMenuForIndex(int index, const juce::String&) override
    {
        juce::PopupMenu menu;
        if (index == 0)
        {
            menu.addItem(1,i18n::tr("Nuevo proyecto")); menu.addItem(2,i18n::tr("Abrir proyecto…  ⌘O")); menu.addItem(3,i18n::tr("Guardar proyecto…  ⌘S"));
            menu.addSeparator(); menu.addItem(4,i18n::tr("Importar audio / MIDI / video…  ⌥⌘I")); menu.addItem(5,i18n::tr("Exportar mezcla de audio…"));
            menu.addSeparator(); menu.addItem(6,i18n::tr("Conversión de formatos…")); menu.addItem(7,i18n::tr("Extraer audio de video…"));
        }
        if (index == 2) { menu.addItem(10,i18n::tr("Añadir pista de audio")); menu.addItem(11,i18n::tr("Añadir instrumento MIDI")); menu.addItem(12,i18n::tr("Añadir pista MIDI")); menu.addSeparator(); menu.addItem(13,i18n::tr("Añadir pista de carpeta")); menu.addItem(14,i18n::tr("Añadir pista de FX")); menu.addItem(15,i18n::tr("Añadir pista de grupo")); }
        if (index == 3 && window) return window->content().audioMenu();
        if (index == 4 && window) return window->content().transportMenu();
        if (index == 5) { menu.addItem(20,i18n::tr("MixConsole  F3")); menu.addItem(27,i18n::tr("MixConsole en zona inferior  ⌥F3")); menu.addItem(21,i18n::tr("Instrumentos y efectos…")); menu.addItem(22,i18n::tr("Dispositivos de audio y MIDI…")); menu.addItem(23,i18n::tr("Apagar todas las notas")); menu.addSeparator(); menu.addItem(24,i18n::tr("Metrónomo  C"),true,window&&window->content().metronomeOn()); menu.addItem(25,i18n::tr("Ajustes del metrónomo…")); menu.addItem(26,i18n::tr("Calculadora de tempo (Tap)…")); }
        if (index == 8) { menu.addItem(30,i18n::tr("Acerca de Silente")); menu.addItem(31,i18n::tr("Guía rápida")); }
        if (index == 1)
        {
            // Edición: the same actions as the Cubase key commands.
            menu.addItem(60,i18n::tr("Deshacer  ⌘Z"));menu.addItem(61,i18n::tr("Rehacer  ⇧⌘Z"));menu.addSeparator();
            menu.addItem(62,i18n::tr("Cortar  ⌘X"));menu.addItem(63,i18n::tr("Copiar  ⌘C"));menu.addItem(64,i18n::tr("Pegar  ⌘V"));menu.addItem(65,i18n::tr("Pegar al origen  ⌥V"));
            menu.addItem(66,i18n::tr("Duplicar  ⌘D"));menu.addItem(67,i18n::tr("Suprimir"));menu.addItem(68,i18n::tr("Dividir en el cursor  ⌥X"));
            if (window) menu.addSubMenu(i18n::tr("Rango"), window->content().rangeMenu());
            menu.addSeparator();
            menu.addItem(70,i18n::tr("Historial…"));menu.addItem(69,i18n::tr("Comandos de teclado…"));
        }
        if (index == 6)
        {
            juce::PopupMenu sizes;const float current=juce::Desktop::getInstance().getGlobalScaleFactor();
            for(int n=0;n<(int)std::size(interfaceScales);++n)sizes.addItem(50+n,juce::String(juce::roundToInt(interfaceScales[n]*100))+" %",true,std::abs(current-interfaceScales[n])<0.01f);
            menu.addSubMenu(i18n::tr("Tamaño de la interfaz y de la letra"),sizes);
        }
        if(index==7){menu.addItem(40,juce::String::fromUTF8("Español"),true,i18n::language()==i18n::Language::spanish);menu.addItem(41,"English",true,i18n::language()==i18n::Language::english);}
        return menu;
    }
    void setInterfaceScale(float scale)
    {
        juce::Desktop::getInstance().setGlobalScaleFactor(scale);
        juce::XmlElement xml("INTERFACE"); xml.setAttribute("scale", (double) scale);
        interfaceFile().getParentDirectory().createDirectory(); xml.writeTo(interfaceFile());
        if (window) window->fitToScreen();
        menuItemsChanged();
    }
    void menuItemSelected(int item, int) override
    {
        if (!window) return;
        auto& c = window->content();
        if (item >= 100 && item < 200) { c.transportMenuItem(item); return; }
        if (item >= 200 && item < 220) { c.audioMenuItem(item); return; }
        if (item >= 71 && item <= 77) { c.rangeMenuItem(item); return; }
        switch (item)
        {
            case 1: c.newProject(); break; case 2: c.openProject(); break; case 3: c.saveProject(); break;
            case 4: c.importFiles(); break; case 5: c.exportAudio(); break;
            case 6: c.showConversion(false); break; case 7: c.showConversion(true); break;
            case 10: c.addAudioTrack(); break; case 11: c.addMidiTrack(); break;
            case 12: c.addTrack(TrackKind::midi); break; case 13: c.addTrack(TrackKind::folder); break;
            case 14: c.addTrack(TrackKind::fx); break; case 15: c.addTrack(TrackKind::group); break;
            case 20: c.showMixConsole(); break; case 27: c.showMixer(); break; case 21: c.showPlugins(); break; case 22: c.showAudioSettings(); break;
            case 23: engine->panic(); break;
            case 24: c.toggleMetronome(); break; case 25: c.showMetronomeSettings(); break; case 26: c.showTapTempo(); break;
            case 40: i18n::setLanguage(i18n::Language::spanish);break;
            case 50: case 51: case 52: case 53: case 54: setInterfaceScale(interfaceScales[item-50]); break;
            case 60: c.runCommand("undo"); break; case 61: c.runCommand("redo"); break; case 62: c.runCommand("cut"); break; case 63: c.runCommand("copy"); break;
            case 64: c.runCommand("paste"); break; case 65: c.runCommand("pasteOrigin"); break; case 66: c.runCommand("duplicate"); break; case 67: c.runCommand("delete"); break;
            case 68: c.runCommand("splitAtCursor"); break; case 69: c.showKeyCommands(); break; case 70: c.runCommand("history"); break;
            case 41: i18n::setLanguage(i18n::Language::english);break;
            case 30: juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, i18n::tr("Silente 0.1 · Audio Workstation"), i18n::tr("Un espacio para escuchar, crear y mezclar.\n\nC++20 · Core Audio · JUCE\nVST3 y Audio Units · macOS Apple Silicon\n\nPrimera versión de desarrollo. Consulta las limitaciones y las pruebas en el proyecto de Visual Studio Code.\n\nJUCE bajo AGPLv3. VST es una marca de Steinberg Media Technologies GmbH.")); break;
            case 31: juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, i18n::tr("Tu primera sesión"), i18n::tr("1. Importa WAV, AIFF, FLAC, MP3 o MIDI desde Archivo, o arrastra audio a la sesión.\n2. Espacio reproduce y pausa. Ajusta volumen, paneo, Mute y Solo por canal.\n3. Usa el mezclador o selecciona una pista MIDI para editar sus notas.\n4. Explora tus plugins y añádelos al canal seleccionado.\n5. Guarda el proyecto .silente y exporta tu mezcla WAV.\n\nLa sesión inicial está vacía. Puedes abrir la sesión de ejemplo desde Medios.")); break;
            default: break;
        }
    }
};
}
START_JUCE_APPLICATION(silente::SilenteApplication)

#include "SettingsUiTests.h"
#include "../src/MainComponent.h"
#include "../src/app/Preferences.h"
#include "../src/app/KeyCommands.h"
#include "../src/ui/settings/SettingsWorkspace.h"
#include "../src/ui/keys/KeyCommandsPanel.h"
#include "../src/ui/metronome/MetronomePanel.h"

namespace silente
{
namespace
{
juce::Component* find(juce::Component& c,const juce::String& id)
{
    if(c.getComponentID()==id)return &c;
    for(auto* child:c.getChildren())if(auto* result=find(*child,id))return result;
    return nullptr;
}
void capture(juce::Component& c,const juce::File& file)
{
    if(auto stream=file.createOutputStream()){stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()),*stream);}
}
}
void runSettingsUiChecks(MainComponent& view,AudioEngine& engine,const juce::File& output,const std::function<void(bool,const juce::String&)>& expect)
{
    using namespace juce;
    using Page=ui::SettingsWorkspace::Page;
    const auto preferences=app::preferencesDirectory();
    const auto originalLanguage=i18n::language();
    const auto isolated=output.getChildFile("settings-preferences");isolated.createDirectory();
    AudioEngine isolatedEngine;
    isolatedEngine.projectName="Guardar";
    ui::TransportPreferences transport;
    float scale=1.15f;int transportWrites=0,scaleWrites=0;
    ui::SettingsHooks hooks;
    hooks.readTransport=[&]{return transport;};hooks.writeTransport=[&](const auto& state){transport=state;++transportWrites;};
    hooks.readScale=[&]{return scale;};hooks.writeScale=[&](float value){scale=value;++scaleWrites;expect(app::saveInterfaceScale(value).wasOk(),"Settings scale writes to the isolated preferences directory");};
    hooks.keyAvailable=[](const String& id){return id=="metronome"||id=="locatorsToSelection";};
    app::usePreferencesDirectory(isolated);
    {
        ui::SettingsWorkspace workspace(isolatedEngine,hooks);workspace.setSize(960,730);
        expect(workspace.matchingCount()==6,"Settings lists six existing categories with an empty search");
        workspace.setSearch(String::fromUTF8("METRONOMO precuenta"));
        expect(workspace.matchingCount()==1&&workspace.currentPage()==Page::metronome,"Settings search ignores case and accents and combines words across category and control");
        if(auto* count=dynamic_cast<ComboBox*>(find(workspace,"metronome-count-in")))count->setSelectedId(3,sendNotificationSync);
        else expect(false,"Count-in control found by global search");
        expect(isolatedEngine.click.countInBars.load()==2,"Search result edits the actual metronome count-in");
        if(auto* level=dynamic_cast<Slider*>(find(workspace,"metronome-level")))level->setValue(-15.0,sendNotificationSync);
        capture(workspace,output.getChildFile("settings-search.png"));

        workspace.setSearch("frecuencia");
        expect(workspace.currentPage()==Page::audio&&find(workspace,"audio-device-selector"),"Audio frequency search opens the existing device selector");
        capture(workspace,output.getChildFile("settings-audio.png"));
        workspace.setSearch("xx-no-setting-1749");
        expect(workspace.matchingCount()==0,"Unknown setting gives an empty result without opening a fabricated page");
        workspace.chooseResult(-1);workspace.chooseResult(100000);

        workspace.setSearch("apariencia letra");
        if(auto* control=dynamic_cast<ComboBox*>(find(workspace,"settings-scale")))control->setSelectedId(4,sendNotificationSync);
        else expect(false,"Interface size control exists");
        expect(scaleWrites==1&&std::abs(scale-1.4f)<0.001f&&std::abs(app::loadInterfaceScale()-1.4f)<0.001f,"Appearance selection applies the scale callback and persists 140 percent");
        expect(app::saveInterfaceScale(3.0f).failed()&&std::abs(app::loadInterfaceScale()-1.4f)<0.001f,"Invalid interface scale cannot overwrite the saved setting");
        capture(workspace,output.getChildFile("settings-appearance.png"));

        workspace.setSearch("transporte cursor fijo");
        if(auto* fixed=dynamic_cast<ToggleButton*>(find(workspace,"settings-stationary")))
        {
            fixed->setToggleState(true,dontSendNotification);fixed->onClick();
            expect(transport.stationary&&transport.autoScroll&&transportWrites==1,"Stationary cursor updates real transport hooks and enables auto-scroll");
        }
        else expect(false,"Stationary cursor control exists");
        capture(workspace,output.getChildFile("settings-transport.png"));
        for(const auto& command:app::keyCommands())if(command.id=="locatorsToSelection")
        {
            workspace.setSearch(command.name());
            bool found=false;
            for(int row=0;row<workspace.matchingCount();++row)if(workspace.resultText(row)==command.name()){workspace.chooseResult(row);found=true;break;}
            auto* keys=dynamic_cast<ui::KeyCommandsPanel*>(find(workspace,"key-commands-search")?find(workspace,"key-commands-search")->getParentComponent():nullptr);
            expect(found&&workspace.currentPage()==Page::keys&&keys&&keys->matchingCount()>0,"Global search indexes command names and filters the embedded key-command reference");
            capture(workspace,output.getChildFile("settings-keys.png"));
        }
        workspace.setSearch({});workspace.showPage(Page::metronome);
        i18n::setLanguage(i18n::Language::english,false);workspace.refreshLanguage();workspace.setSearch("metronome count");
        expect(workspace.matchingCount()>0&&workspace.currentPage()==Page::metronome&&isolatedEngine.click.countInBars.load()==2&&isolatedEngine.projectName=="Guardar","Settings translate and rebuild the search index without changing project data or metronome settings");
        i18n::setLanguage(originalLanguage,false);workspace.refreshLanguage();
        workspace.setSearch({});workspace.setSize(760,540);
        workspace.setSearch("caja herramientas");
        if(auto* toggle=dynamic_cast<ToggleButton*>(find(workspace,"settings-show-toolbox")))
        {
            toggle->setToggleState(false,dontSendNotification);toggle->onClick();expect(!app::showToolboxOnRightClick(),"Editing preferences search edits the persisted toolbox switch");
        }
        else expect(false,"Editing preferences page exists");
        workspace.setSearch({});
        for(auto p:{Page::audio,Page::appearance,Page::transport,Page::metronome,Page::editing,Page::keys})workspace.showPage(p);
        expect(workspace.currentPage()==Page::keys,"Settings pages remain navigable at the minimum window size");
    }
    audio::Metronome loaded;loaded.load(isolated.getChildFile("metronome.xml"));
    expect(loaded.countInBars.load()==2&&std::abs(loaded.levelDb.load()+15.0f)<0.001f,"Metronome count-in and typed level survive leaving the settings workspace");
    app::usePreferencesDirectory(preferences);
    view.showSettings();
    ui::SettingsWorkspace* integrated=nullptr;
    for(int n=0;n<Desktop::getInstance().getNumComponents();++n)
        if(auto* found=dynamic_cast<ui::SettingsWorkspace*>(find(*Desktop::getInstance().getComponent(n),"settings-workspace")))integrated=found;
    expect(integrated&&!integrated->isCurrentlyBlockedByAnotherModalComponent()&&!view.isCurrentlyBlockedByAnotherModalComponent(),"Settings is a non-modal window and leaves the project available");
    if(integrated)
    {
        const auto tracks=engine.tracks.size();const auto name=engine.projectName;
        view.addTrack(TrackKind::audio);
        expect(engine.tracks.size()==tracks+1&&engine.projectName==name,"Adding a track works while Settings remains open");view.runCommand("undo");
        integrated->setSearch(String::fromUTF8("Volver a la posición"));
        if(auto* toggle=dynamic_cast<ToggleButton*>(find(*integrated,"settings-return-start")))
        {
            const bool before=toggle->getToggleState();toggle->setToggleState(!before,dontSendNotification);toggle->onClick();
            const auto xml=XmlDocument::parse(preferences.getChildFile("transport.xml"));
            expect(xml&&xml->getBoolAttribute("returnToStart")==!before,"Integrated transport preference persists through the existing project coordinator");
            toggle->setToggleState(before,dontSendNotification);toggle->onClick();
        }
        view.showAudioSettings();const auto* audioPage=integrated;
        view.showKeyCommands();expect(audioPage==integrated&&integrated->currentPage()==Page::keys,"Audio and Key Commands menu entries reuse the same settings window");
        if(auto* window=dynamic_cast<DocumentWindow*>(integrated->getTopLevelComponent()))
        {
            window->closeButtonPressed();expect(!window->isVisible(),"Settings window closes without closing the project");
            view.showSettings();expect(window->isVisible()&&integrated->currentPage()==Page::appearance,"Settings reopens after close with its panels available");window->closeButtonPressed();
        }
    }
}
}

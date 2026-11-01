#include "LocalisationUiTests.h"
#include "../src/MainComponent.h"
#include "../src/i18n/Translations.h"

namespace silente
{
namespace
{
bool hasButton(juce::Component& component,const juce::String& caption)
{
    if(auto* button=dynamic_cast<juce::TextButton*>(&component))if(button->getButtonText()==caption)return true;
    for(auto* child:component.getChildren())if(hasButton(*child,caption))return true;
    return false;
}
void capture(juce::Component& component,const juce::File& file)
{
    auto image=component.createComponentSnapshot(component.getLocalBounds());
    if(auto stream=file.createOutputStream()){stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(image,*stream);}
}
}
int runLanguageSwitchChecks(MainComponent& view,AudioEngine& engine,const juce::File& directory)
{
    directory.createDirectory();juce::String failures;
    engine.projectName="Archivo"; // Deliberately matches a translation key: user data must not change.
    const auto tracks=engine.tracks.size();
    auto first=engine.tracks.empty()?nullptr:engine.tracks.front();
    if(first)first->name="Guardar";
    const auto colour=first?first->colour:juce::Colours::black;
    const auto notes=first?first->notes.size():0;
    const double bpm=engine.tempo;
    i18n::setLanguage(i18n::Language::english,false);view.refreshLanguage();
    if(!hasButton(view,"Import")||!hasButton(view,"Visibility")||!hasButton(view,"Track colour"))failures+="English controls did not refresh\n";
    if(engine.projectName!="Archivo"||engine.tracks.size()!=tracks||engine.tempo!=bpm||(first&&(first->name!="Guardar"||first->colour!=colour||first->notes.size()!=notes)))failures+="Project changed during language switch\n";
    capture(view,directory.getChildFile("runtime-en.png"));
    i18n::setLanguage(i18n::Language::spanish,false);view.refreshLanguage();
    if(!hasButton(view,"Importar")||!hasButton(view,"Visibilidad")||!hasButton(view,"Color de pista"))failures+="Spanish controls did not refresh\n";
    if(engine.projectName!="Archivo"||(first&&first->name!="Guardar"))failures+="User names changed during return to Spanish\n";
    capture(view,directory.getChildFile("runtime-es.png"));
    directory.getChildFile("language-switch.txt").replaceWithText(failures.isEmpty()?"PASS: ES -> EN -> ES controls; project name, track name, colour, notes, tempo and track count preserved.\n":failures);
    return failures.isEmpty()?0:1;
}
}

#include "Translations.h"
#include "LocalisationData.h"
#include <atomic>
#include <map>

namespace silente::i18n
{
namespace
{
using Dictionary = std::map<juce::String, juce::String>;
struct Catalogs
{
    Dictionary es, en, aliases;
    static Dictionary read(const char* data, int size)
    {
        Dictionary result;
        auto parsed=juce::JSON::parse(juce::String::fromUTF8(data,size));
        if(auto* object=parsed.getDynamicObject())for(const auto& property:object->getProperties())result.emplace(property.name.toString(),property.value.toString());
        return result;
    }
    Catalogs() : es(read(SilenteLocales::es_json,SilenteLocales::es_jsonSize)), en(read(SilenteLocales::en_json,SilenteLocales::en_jsonSize))
    {
        for(const auto& [key,value]:es)aliases.try_emplace(value,key);
        for(const auto& [key,value]:en)aliases.try_emplace(value,key);
    }
};
const Catalogs& catalogs(){static const Catalogs value;return value;}
std::atomic<Language> current{Language::spanish};
juce::File preferenceFile;
std::function<void()> changed;
void installFrameworkTranslations()
{
    auto& entries=language()==Language::spanish?catalogs().es:catalogs().en;
    juce::String mapping=language()==Language::spanish?"language: Spanish\ncountries: es\n":"language: English\ncountries: us gb\n";
    for(const auto& [key,value]:entries)mapping+=juce::JSON::toString(juce::var(key),true)+" = "+juce::JSON::toString(juce::var(value),true)+"\n";
    juce::LocalisedStrings::setCurrentMappings(new juce::LocalisedStrings(mapping,false));
}
}
Language language(){return current.load();}
juce::String tr(const char* source){return tr(juce::String::fromUTF8(source));}
juce::String tr(const juce::String& source)
{
    const auto& data=catalogs();const auto& dictionary=language()==Language::spanish?data.es:data.en;
    if(auto found=dictionary.find(source);found!=dictionary.end())return found->second;
    if(auto alias=data.aliases.find(source);alias!=data.aliases.end())if(auto found=dictionary.find(alias->second);found!=dictionary.end())return found->second;
    return source;
}
void setLanguageChangedCallback(std::function<void()> callback){changed=std::move(callback);}
void initialise(const juce::File& directory,const juce::String& overrideCode)
{
    preferenceFile=directory.getChildFile("language.json");
    auto code=overrideCode;
    if(code.isEmpty()&&preferenceFile.existsAsFile())code=juce::JSON::parse(preferenceFile.loadFileAsString())["language"].toString();
    current=code=="en"?Language::english:Language::spanish;
    installFrameworkTranslations();
}
void setLanguage(Language value,bool savePreference)
{
    const bool different=current.exchange(value)!=value;
    installFrameworkTranslations();
    if(savePreference&&preferenceFile!=juce::File())
    {
        preferenceFile.getParentDirectory().createDirectory();
        juce::DynamicObject::Ptr object=new juce::DynamicObject();object->setProperty("language",value==Language::spanish?"es":"en");
        juce::TemporaryFile temporary(preferenceFile);
        if(temporary.getFile().replaceWithText(juce::JSON::toString(juce::var(object.get()))))temporary.overwriteTargetFileWithTemporary();
    }
    if(different&&changed)changed();
}
int checkCatalogs(const juce::File& directory)
{
    directory.createDirectory();juce::String errors;const auto& data=catalogs();
    for(const auto& [key,value]:data.es)if(value.isEmpty()||!data.en.count(key)||data.en.at(key).isEmpty())errors+="Missing English entry: "+key+"\n";
    for(const auto& [key,value]:data.en)if(value.isEmpty()||!data.es.count(key))errors+="Missing Spanish entry: "+key+"\n";
    current=Language::english;if(tr("Archivo")!="File")errors+="English lookup failed\n";
    current=Language::spanish;if(tr("Mute")!=juce::String::fromUTF8("Silenciar"))errors+="Spanish lookup failed\n";
    current=Language::english;if(tr(juce::String::fromUTF8("Silenciar"))!="Mute")errors+="Runtime reverse lookup failed\n";
    const auto originalFile=preferenceFile;
    const auto isolated=directory.getChildFile("preferences");
    initialise(isolated,"es");
    if(juce::translate("Cancel")!=juce::String::fromUTF8("Cancelar"))errors+="Spanish framework translation failed\n";
    setLanguage(Language::english,true);initialise(isolated);
    if(language()!=Language::english||juce::translate("Cancel")!="Cancel")errors+="English preference or framework failed\n";
    setLanguage(Language::spanish,true);initialise(isolated);
    if(language()!=Language::spanish)errors+="Spanish preference failed\n";
    preferenceFile=originalFile;
    directory.getChildFile("localisation.txt").replaceWithText(errors.isEmpty()?"PASS: "+juce::String(data.es.size())+" entries per language; lookups and reverse lookup.\n":errors);
    return errors.isEmpty()?0:1;
}
}

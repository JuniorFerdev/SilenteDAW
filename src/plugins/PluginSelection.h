#pragma once
#include "../AudioEngine.h"

namespace silente
{
enum class PluginKind { effect, instrument };
inline juce::PluginDescription internalInstrumentDescription()
{
    juce::PluginDescription result;result.name=i18n::tr("Sintetizador interno");result.manufacturerName="Silente";result.category=i18n::tr("Instrumentos");result.pluginFormatName="Silente";result.fileOrIdentifier="silente:internal-synth";result.isInstrument=true;return result;
}

// Instruments live before the serial effect chain. UI insert numbers count effects only.
inline int effectSlotIndex(const Track& track,int effectIndex)
{
    int effect=0;
    for(int n=0;n<static_cast<int>(track.plugins.size());++n)
        if(!track.plugins[static_cast<size_t>(n)]->description.isInstrument&&effect++==effectIndex)return n;
    return -1;
}
inline int instrumentSlotIndex(const Track& track)
{
    for(int n=0;n<static_cast<int>(track.plugins.size());++n)
        if(track.plugins[static_cast<size_t>(n)]->description.isInstrument)return n;
    return -1;
}
inline bool matchesPlugin(const juce::PluginDescription& item,PluginKind kind,const juce::String& query,const juce::String& format)
{
    if(item.isInstrument!=(kind==PluginKind::instrument))return false;
    if(format.isNotEmpty()&&item.pluginFormatName!=format)return false;
    const auto haystack=item.name+" "+item.manufacturerName+" "+item.category+" "+item.pluginFormatName;
    for(const auto& word:juce::StringArray::fromTokens(query," ",""))if(!haystack.containsIgnoreCase(word))return false;
    return true;
}
}

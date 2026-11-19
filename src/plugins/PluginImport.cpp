#include "PluginImport.h"
#include "../i18n/Translations.h"

namespace silente::plugins
{
namespace
{
bool isBundle(const juce::File& f)
{
    for (auto* extension : { ".vst3", ".component", ".appex", ".aaxplugin", ".vst", ".clap" })
        if (f.hasFileExtension(extension)) return true;
    return false;
}
void walk(const juce::File& folder, FolderInventory& result, int depth)
{
    if (depth > 4) return;
    for (const auto& entry : juce::RangedDirectoryIterator(folder, false, "*", juce::File::findFilesAndDirectories))
    {
        const auto f = entry.getFile();
        if (f.hasFileExtension(".vst3")) result.vst3.add(f.getFullPathName());
        else if (f.hasFileExtension(".component") || f.hasFileExtension(".appex")) result.audioUnits.add(f.getFullPathName());
        else if (f.hasFileExtension(".aaxplugin")) result.aax.add(f.getFullPathName());
        else if (f.hasFileExtension(".vst")) result.vst2.add(f.getFullPathName());
        else if (f.hasFileExtension(".clap")) result.clap.add(f.getFullPathName());
        else if (entry.isDirectory() && ! entry.isHidden() && ! isBundle(f)) walk(f, result, depth + 1);
    }
}
}

FolderInventory inventoryFolder(const juce::File& folder)
{
    FolderInventory result;
    if (folder != juce::File() && folder.isDirectory()) walk(folder, result, 0);
    return result;
}

Origin originOf(const juce::PluginDescription& d, const juce::File& importFolder)
{
    if (d.fileOrIdentifier.startsWith("silente:")) return Origin::internal;
    if (d.pluginFormatName == "AudioUnit" && ! juce::File::isAbsolutePath(d.fileOrIdentifier)) return Origin::audioUnitRegistry;
    if (! juce::File::isAbsolutePath(d.fileOrIdentifier)) return Origin::other;
    const juce::File file(d.fileOrIdentifier);
    if (importFolder != juce::File() && file.isAChildOf(importFolder)) return Origin::imported;
    if (file.isAChildOf(juce::File("/Library/Audio/Plug-Ins"))) return Origin::system;
    if (file.isAChildOf(juce::File("~/Library/Audio/Plug-Ins"))) return Origin::user;
    return Origin::other;
}

juce::String describeOrigin(const juce::PluginDescription& d, const juce::File& importFolder)
{
    const auto sep = juce::String::fromUTF8(" · ");
    switch (originOf(d, importFolder))
    {
        case Origin::internal: return i18n::tr("Integrado en Silente");
        case Origin::audioUnitRegistry: return i18n::tr("Audio Units registrados en macOS");
        case Origin::imported: return i18n::tr("Importado") + sep + d.fileOrIdentifier;
        case Origin::system: return i18n::tr("Sistema") + sep + d.fileOrIdentifier;
        case Origin::user: return i18n::tr("Usuario") + sep + d.fileOrIdentifier;
        case Origin::other: break;
    }
    return d.fileOrIdentifier;
}
}

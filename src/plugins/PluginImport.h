#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace silente::plugins
{
// Import of plugins from a user-chosen folder outside the standard macOS locations.
// Only inspects the file system: it never loads a plugin. See docs/PLUGIN_IMPORTACION.md.
struct FolderInventory
{
    juce::StringArray vst3;         // loadable: VST3 hosting accepts any path
    juce::StringArray audioUnits;   // found, but macOS only registers AU in Components folders
    juce::StringArray aax;          // Pro Tools: hosting requires Avid's SDK and licence
    juce::StringArray vst2;         // VST2 hosting requires the discontinued Steinberg VST2 SDK
    juce::StringArray clap;         // JUCE 8 does not host CLAP
    int unsupported() const { return aax.size() + vst2.size() + clap.size(); }
};

// Walks the folder up to a few levels without descending into plugin bundles.
FolderInventory inventoryFolder(const juce::File& folder);

enum class Origin { internal, system, user, imported, audioUnitRegistry, other };

// Classifies where a catalogued plugin comes from. `importFolder` may be juce::File().
Origin originOf(const juce::PluginDescription&, const juce::File& importFolder);

// Short, translated text for the catalogue: origin plus the bundle path when there is one.
juce::String describeOrigin(const juce::PluginDescription&, const juce::File& importFolder);
}

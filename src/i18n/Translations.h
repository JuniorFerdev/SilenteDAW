#pragma once
#include <juce_core/juce_core.h>
#include <functional>

namespace silente::i18n
{
enum class Language { spanish, english };
juce::String tr(const char* utf8Source);
juce::String tr(const juce::String& source);
Language language();
void initialise(const juce::File& preferenceDirectory, const juce::String& overrideCode = {});
void setLanguage(Language, bool savePreference = true);
void setLanguageChangedCallback(std::function<void()>);
int checkCatalogs(const juce::File& outputDirectory);
}

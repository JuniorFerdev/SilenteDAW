#include "Preferences.h"
#include "../i18n/Translations.h"
#include <cmath>

namespace silente::app
{
namespace { juce::File overrideDirectory; }
juce::File preferencesDirectory()
{
    return overrideDirectory != juce::File() ? overrideDirectory
        : juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Silente");
}
void usePreferencesDirectory(const juce::File& folder) { overrideDirectory = folder; }
float loadInterfaceScale()
{
    if (auto xml = juce::XmlDocument::parse(preferencesDirectory().getChildFile("interface.xml")))
    {
        const auto scale = xml->getDoubleAttribute("scale", 1.15);
        if (std::isfinite(scale)) return juce::jlimit(1.0f, 1.5f, (float) scale);
    }
    return 1.15f;
}
juce::Result saveInterfaceScale(float scale)
{
    if (! std::isfinite(scale) || scale < 1.0f || scale > 1.5f) return juce::Result::fail(i18n::tr("Tamaño de interfaz no válido."));
    const auto folder = preferencesDirectory();
    const auto result = folder.createDirectory();
    if (result.failed()) return result;
    juce::XmlElement xml("INTERFACE"); xml.setAttribute("scale", (double) scale);
    return xml.writeTo(folder.getChildFile("interface.xml")) ? juce::Result::ok() : juce::Result::fail(i18n::tr("No se pudo guardar el tamaño de la interfaz."));
}
}

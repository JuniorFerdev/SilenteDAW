#include "UiTranslation.h"
#include "Translations.h"

namespace silente::i18n
{
void translateControls(juce::Component& component)
{
    if(auto* button=dynamic_cast<juce::TextButton*>(&component))
    {
        const juce::Identifier key("silenteSourceCaption");
        if(!button->getProperties().contains(key))button->getProperties().set(key,button->getButtonText());
        button->setButtonText(tr(button->getProperties()[key].toString()));
    }
    if(auto* client=dynamic_cast<juce::SettableTooltipClient*>(&component))
    {
        const juce::Identifier key("silenteSourceTooltip");
        if(!component.getProperties().contains(key))component.getProperties().set(key,client->getTooltip());
        client->setTooltip(tr(component.getProperties()[key].toString()));
    }
    for(auto* child:component.getChildren())translateControls(*child);
    component.repaint();
}
}

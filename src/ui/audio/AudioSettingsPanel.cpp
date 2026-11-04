#include "AudioSettingsPanel.h"
#include "../../i18n/Translations.h"

namespace silente::ui
{
AudioSettingsPanel::AudioSettingsPanel(AudioEngine& e):engine(e),selector(e.deviceManager,0,DeviceSettings::channelLimit,0,DeviceSettings::channelLimit,true,true,true,false)
{
    selector.setComponentID("audio-device-selector");save.setComponentID("audio-save");rescan.setComponentID("audio-rescan");
    setLookAndFeel(&look);for(auto* c:std::initializer_list<juce::Component*>{&selector,&status,&hint,&save,&rescan})addAndMakeVisible(c);
    hint.setText(i18n::tr("Hasta 18 entradas y 18 salidas. Activa los canales que usarás. La mezcla sale por los dos primeros canales de salida activados."),juce::dontSendNotification);
    hint.setColour(juce::Label::textColourId,theme::muted);hint.setFont(juce::Font(juce::FontOptions(12)));
    save.setButtonText(i18n::tr("Guardar configuración"));rescan.setButtonText(i18n::tr("Actualizar dispositivos"));
    save.onClick=[this]{const auto result=engine.devices.save();status.setText(result.wasOk()?i18n::tr("Configuración guardada."):result.getErrorMessage(),juce::dontSendNotification);};
    rescan.onClick=[this]{engine.devices.rescan();timerCallback();};
    setSize(690,650);timerCallback();startTimer(1000);
}
void AudioSettingsPanel::resized()
{
    auto r=getLocalBounds().reduced(16);hint.setBounds(r.removeFromTop(44));status.setBounds(r.removeFromBottom(55));auto buttons=r.removeFromBottom(34);save.setBounds(buttons.removeFromRight(190));rescan.setBounds(buttons.removeFromLeft(190));r.removeFromBottom(8);selector.setBounds(r);
}
void AudioSettingsPanel::paint(juce::Graphics& g){g.fillAll(theme::panel);}
void AudioSettingsPanel::timerCallback(){if(!engine.recording)status.setText(engine.devices.summary(),juce::dontSendNotification);selector.setEnabled(!engine.recording);save.setEnabled(!engine.recording);}
}

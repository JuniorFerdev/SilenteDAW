#include "DeviceSettings.h"
#include "../i18n/Translations.h"

namespace silente
{
DeviceSettings::DeviceSettings(juce::AudioDeviceManager& m):manager(m),preferences(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Silente/audio-device.xml")){manager.addChangeListener(this);}
DeviceSettings::~DeviceSettings(){manager.removeChangeListener(this);}
void DeviceSettings::changeListenerCallback(juce::ChangeBroadcaster*){dropUnusedInputDevice();}
void DeviceSettings::dropUnusedInputDevice()
{
    auto setup=manager.getAudioDeviceSetup();
    if(setup.inputDeviceName.isEmpty()||setup.inputDeviceName==setup.outputDeviceName||!setup.inputChannels.isZero()||setup.useDefaultInputChannels)return;
    setup.inputDeviceName={};
    manager.setAudioDeviceSetup(setup,true);
}
juce::String DeviceSettings::initialise()
{
    auto saved=juce::XmlDocument::parse(preferences);
    juce::AudioDeviceManager::AudioDeviceSetup initial;
    initial.useDefaultInputChannels=false; // Input is enabled explicitly in Audio settings.
    auto error=manager.initialise(channelLimit,channelLimit,saved.get(),true,{},saved?nullptr:&initial);
    if(error.isEmpty())dropUnusedInputDevice();
    return error;
}
juce::Result DeviceSettings::save()
{
    auto xml=manager.createStateXml();
    if(!xml)return juce::Result::fail(i18n::tr("No hay configuración de audio para guardar."));
    preferences.getParentDirectory().createDirectory();juce::TemporaryFile temp(preferences);
    if(!xml->writeTo(temp.getFile())||!temp.overwriteTargetFileWithTemporary())return juce::Result::fail(i18n::tr("No se pudo guardar la configuración de audio."));
    return juce::Result::ok();
}
void DeviceSettings::rescan(){for(auto* type:manager.getAvailableDeviceTypes())type->scanForDevices();}
juce::StringArray DeviceSettings::inputNames() const
{
    if(auto* device=manager.getCurrentAudioDevice())return device->getInputChannelNames();
    return {};
}
juce::String DeviceSettings::summary() const
{
    auto* device=manager.getCurrentAudioDevice();if(!device)return i18n::tr("Sin dispositivo de audio activo");
    auto setup=manager.getAudioDeviceSetup();
    return setup.outputDeviceName+" | "+juce::String(device->getCurrentSampleRate()/1000.0,1)+" kHz | "+juce::String(device->getCurrentBufferSizeSamples())+i18n::tr(" muestras")+"\n"
        +i18n::tr("Entradas activas: ")+juce::String(device->getActiveInputChannels().countNumberOfSetBits())+" / "+juce::String(device->getInputChannelNames().size())
        +i18n::tr(" · Salidas activas: ")+juce::String(device->getActiveOutputChannels().countNumberOfSetBits())+" / "+juce::String(device->getOutputChannelNames().size());
}
juce::var DeviceSettings::inventory()
{
    juce::Array<juce::var> rows;
    for(auto* type:manager.getAvailableDeviceTypes())
    {
        type->scanForDevices();
        for(const bool input:{false,true})for(const auto& name:type->getDeviceNames(input))
        {
            juce::DynamicObject::Ptr row=new juce::DynamicObject();row->setProperty("type",type->getTypeName());row->setProperty("name",name);row->setProperty("input",input);
            std::unique_ptr<juce::AudioIODevice> device(type->createDevice(input?juce::String():name,input?name:juce::String()));
            if(device){row->setProperty("inputs",juce::var(device->getInputChannelNames()));row->setProperty("outputs",juce::var(device->getOutputChannelNames()));
                juce::Array<juce::var> rates,buffers;for(auto rate:device->getAvailableSampleRates())rates.add(rate);for(auto size:device->getAvailableBufferSizes())buffers.add(size);
                row->setProperty("rates",rates);row->setProperty("buffers",buffers);}
            rows.add(juce::var(row.get()));
        }
    }
    return juce::var(rows);
}
}

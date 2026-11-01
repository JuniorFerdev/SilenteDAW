#include "MetronomePanel.h"
#include "../../i18n/Translations.h"

namespace silente::ui
{
juce::File MetronomePanel::settingsFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Silente/metronome.xml");
}

MetronomePanel::MetronomePanel(AudioEngine& e) : engine(e)
{
    setLookAndFeel(&look);
    for (auto* c : std::initializer_list<juce::Component*> { &enabled, &sound, &subdivision, &countIn, &level, &accent, &onlyRecording, &preview, &soundLabel, &subdivisionLabel, &levelLabel, &countInLabel })
        addAndMakeVisible(c);
    auto& click = engine.click;
    enabled.setButtonText(i18n::tr("Metrónomo activado  (C)"));
    enabled.setToggleState(engine.metronome.load(), juce::dontSendNotification);
    enabled.onClick = [this] { engine.metronome = enabled.getToggleState(); };

    soundLabel.setText(i18n::tr("Sonido"), juce::dontSendNotification);
    sound.addItemList({ i18n::tr("Clásico"), i18n::tr("Bloque de madera"), i18n::tr("Cencerro"), i18n::tr("Digital"), i18n::tr("Clave") }, 1);
    sound.setSelectedItemIndex(click.sound.load(), juce::dontSendNotification);
    sound.onChange = [this] { engine.click.sound = sound.getSelectedItemIndex(); engine.click.requestPreview(); store(); };

    subdivisionLabel.setText(i18n::tr("Subdivisión"), juce::dontSendNotification);
    subdivision.addItemList({ i18n::tr("Negras"), i18n::tr("Corcheas"), i18n::tr("Tresillos"), i18n::tr("Semicorcheas") }, 1);
    subdivision.setSelectedItemIndex(click.subdivision.load() - 1, juce::dontSendNotification);
    subdivision.onChange = [this] { engine.click.subdivision = subdivision.getSelectedItemIndex() + 1; store(); };

    countInLabel.setText(i18n::tr("Precuenta al grabar"), juce::dontSendNotification);
    countIn.addItemList({ i18n::tr("Sin precuenta"), i18n::tr("1 compás"), i18n::tr("2 compases") }, 1);
    countIn.setSelectedItemIndex(click.countInBars.load(), juce::dontSendNotification);
    countIn.onChange = [this] { engine.click.countInBars = countIn.getSelectedItemIndex(); store(); };

    levelLabel.setText(i18n::tr("Volumen del clic"), juce::dontSendNotification);
    level.setSliderStyle(juce::Slider::LinearHorizontal);
    level.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 24);
    level.setRange(-40.0, 6.0, 0.5); level.setTextValueSuffix(" dB");
    level.setValue(click.levelDb.load(), juce::dontSendNotification);
    level.onValueChange = [this] { engine.click.levelDb = (float) level.getValue(); };
    level.onDragEnd = [this] { engine.click.requestPreview(); store(); };

    accent.setButtonText(i18n::tr("Acentuar el primer tiempo del compás"));
    accent.setToggleState(click.accent.load(), juce::dontSendNotification);
    accent.onClick = [this] { engine.click.accent = accent.getToggleState(); store(); };
    onlyRecording.setButtonText(i18n::tr("Sonar solo al grabar"));
    onlyRecording.setToggleState(click.onlyWhenRecording.load(), juce::dontSendNotification);
    onlyRecording.onClick = [this] { engine.click.onlyWhenRecording = onlyRecording.getToggleState(); store(); };

    preview.setButtonText(i18n::tr("Probar sonido"));
    preview.onClick = [this] { engine.click.requestPreview(); };
    for (auto* l : { &soundLabel, &subdivisionLabel, &levelLabel, &countInLabel }) l->setColour(juce::Label::textColourId, theme::muted);
    setSize(460, 330);
}

void MetronomePanel::store() { engine.click.save(settingsFile()); }

void MetronomePanel::resized()
{
    auto r = getLocalBounds().reduced(18);
    enabled.setBounds(r.removeFromTop(28)); r.removeFromTop(10);
    const auto row = [&r](juce::Label& label, juce::Component& control)
    {
        auto line = r.removeFromTop(30); r.removeFromTop(8);
        label.setBounds(line.removeFromLeft(160)); control.setBounds(line);
    };
    row(soundLabel, sound); row(subdivisionLabel, subdivision); row(levelLabel, level); row(countInLabel, countIn);
    accent.setBounds(r.removeFromTop(26)); onlyRecording.setBounds(r.removeFromTop(26)); r.removeFromTop(8);
    preview.setBounds(r.removeFromTop(30).removeFromRight(150));
}

void MetronomePanel::paint(juce::Graphics& g) { g.fillAll(theme::panel); }
}

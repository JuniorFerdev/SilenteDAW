#pragma once
#include "../../app/TrackCreation.h"
#include "../theme/SilenteLookAndFeel.h"
namespace silente::ui
{
class AddTrackDialog final : public juce::Component
{
public:
    AddTrackDialog();
    ~AddTrackDialog() override;
    std::function<juce::Result(const app::TrackCreation&)> create;
    std::function<void()> close,audioSettings;
    void selectType(TrackKind);
    void updateOptions(const juce::StringArray&,const std::vector<std::pair<juce::String,juce::String>>&,const juce::Array<juce::PluginDescription>&,int markerCount);
    void refreshLanguage();
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void submit();
private:
    class TypeButton;
    std::vector<std::unique_ptr<TypeButton>> types;
    class Expander final : public juce::TextButton {void paintButton(juce::Graphics&,bool,bool) override;};
    Expander more;juce::TextButton add,connections;
    juce::ToggleButton keep;
    juce::TextEditor name;
    juce::Slider count;
    juce::ComboBox input,configuration,output,plugin,format;
    juce::Label error;
    TrackKind kind=TrackKind::audio;
    bool expanded=true,updating=false;
    juce::StringArray inputNames;
    juce::String outputId;
    std::vector<std::pair<juce::String,juce::String>> outputOptions;
    std::vector<juce::PluginDescription> pluginOptions;
    juce::Array<juce::PluginDescription> plugins;
    int markers=0,fieldsTop=0,nameTop=0,footerTop=0;
    void rebuildFields();
    void rebuildInputs();
};
class AddTrackWindow final : public juce::DocumentWindow
{
public:
    AddTrackWindow();
    ~AddTrackWindow() override {setLookAndFeel(nullptr);}
    AddTrackDialog& dialog(){return *body;}
    void closeButtonPressed() override {setVisible(false);}
    void present(juce::Component& parent);
private:
    theme::SilenteLookAndFeel look;
    AddTrackDialog* body=nullptr;
};
}

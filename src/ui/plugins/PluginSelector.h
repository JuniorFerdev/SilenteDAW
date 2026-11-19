#pragma once
#include "../../plugins/PluginSelection.h"
#include <set>
#include "../theme/SilenteLookAndFeel.h"

namespace silente::ui
{
class PluginSelector final : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    PluginSelector(juce::KnownPluginList&,PluginKind,const juce::String& target,const juce::File& favouritesFile);
    ~PluginSelector() override { setLookAndFeel(nullptr); }
    std::function<void(const juce::PluginDescription&)> onChoose;
    std::function<void()> onScan,onStop;
    std::function<bool()> scanning;
    std::function<juce::String()> scanStatus;
    void paint(juce::Graphics&) override;
    void resized() override;
    void focusSearch();
    int rowCount() const { return static_cast<int>(rows.size()); }
    juce::String originText(int row) const;
private:
    struct Row { juce::String group; juce::PluginDescription plugin; bool heading=false; int count=0; };
    theme::SilenteLookAndFeel look;
    juce::KnownPluginList& catalog;
    PluginKind kind;
    juce::String target;
    juce::File favouriteFile;
    juce::Label title,channel,status;
    juce::TextEditor search;
    juce::ComboBox format,grouping;
    juce::TextButton favourites,scan,choose;
    juce::ListBox list;
    std::vector<Row> rows;
    std::set<juce::String> favouriteIds,collapsed;
    juce::File importFolder;
    int lastCount=-1,lastLanguage=-1;
    void rebuild();
    void translate();
    void accept();
    void timerCallback() override;
    int getNumRows() override;
    juce::String getTooltipForRow(int) override;
    void paintListBoxItem(int,juce::Graphics&,int,int,bool) override;
    void listBoxItemClicked(int,const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked(int,const juce::MouseEvent&) override;
    void returnKeyPressed(int) override;
    void selectedRowsChanged(int) override;
};
}

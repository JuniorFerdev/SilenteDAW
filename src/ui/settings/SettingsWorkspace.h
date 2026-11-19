#pragma once
#include "../../AudioEngine.h"
#include "../theme/SilenteLookAndFeel.h"

namespace silente::ui
{
struct TransportPreferences
{
    int startMode = 0;
    bool returnToStart = false, preRoll = false, postRoll = false, autoScroll = true, stationary = false;
    double preRollBars = 1.0, postRollBars = 1.0;
};
struct SettingsHooks
{
    std::function<TransportPreferences()> readTransport;
    std::function<void(const TransportPreferences&)> writeTransport;
    std::function<bool(const juce::String&)> keyAvailable;
    std::function<float()> readScale;
    std::function<void(float)> writeScale;
    std::function<bool(const juce::KeyPress&)> key;
    std::function<void()> scanPlugins;          // starts the existing out-of-process scanner
};

// Search indexes real settings and command names; a result opens its page and reveals its
// control. Pages are cached, so navigating never discards an active edit or duplicates devices.
class SettingsWorkspace final : public juce::Component, private juce::ListBoxModel
{
public:
    enum class Page { audio, appearance, transport, metronome, keys, editing, plugins };
    static constexpr int pageCount = 7;
    SettingsWorkspace(AudioEngine&, SettingsHooks);
    ~SettingsWorkspace() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void refreshLanguage();
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void showPage(Page);
    void setSearch(const juce::String&);
    int matchingCount() const { return (int) matches.size(); }
    void chooseResult(int);
    Page currentPage() const { return selectedPage; }
    juce::String resultText(int) const;

private:
    struct Entry { Page page; juce::String title, keywords, control, command; };
    AudioEngine& engine;
    SettingsHooks hooks;
    theme::SilenteLookAndFeel look;
    juce::TextEditor search;
    juce::ListBox navigation;
    juce::Label heading, hint, empty;
    juce::Viewport viewport;
    std::array<std::unique_ptr<juce::Component>, pageCount> pages;
    std::vector<Entry> entries;
    std::vector<int> matches;
    Page selectedPage = Page::appearance;
    juce::String currentControl;
    void buildIndex();
    void updateSearch();
    juce::Component& page(Page);
    void revealControl(const juce::String&);
    int getNumRows() override { return matchingCount(); }
    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override;
    void selectedRowsChanged(int) override;
};
}

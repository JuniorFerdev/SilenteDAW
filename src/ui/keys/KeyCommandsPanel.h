#pragma once
#include "../../app/KeyCommands.h"
#include "../theme/SilenteLookAndFeel.h"
#include <functional>

namespace silente::ui
{
// «Comandos de teclado» (Edición menu, manual p. 1607): reference of Cubase commands,
// their assigned/unassigned keys, and whether Silente runs them. The Tool category
// is complete against the installed 15.0.30 catalog; the other categories are partial.
class KeyCommandsPanel final : public juce::Component
{
public:
    explicit KeyCommandsPanel(std::function<bool(const juce::String&)> available);
    ~KeyCommandsPanel() override { setLookAndFeel(nullptr); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    int matchingCount() const { return (int) rows.size(); }
    void setFilter(const juce::String& text) { search.setText(text, false); update(); }

private:
    std::function<bool(const juce::String&)> available;
    theme::SilenteLookAndFeel look;
    juce::TextEditor search;
    std::vector<const app::KeyCommand*> rows;
    int scroll = 0;
    static constexpr int rowHeight = 22, top = 70;
    void update();
};
}

#pragma once
#include "../../AudioEngine.h"
#include "../../app/AutomationCommands.h"
#include "../theme/SilenteLookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

namespace silente::ui
{
// Panel «Automatización» (Cubase F6, manual p. 913), reduced: one row per mixable track with the
// automation mode (Desactivado / Leer / Escribir / Touch / Latch) and «Mostrar» buttons that open
// the lane of a parameter (volume, pan, sends); a bar with the global «Suspender lectura» and
// «Suspender escritura». It only shows state and sends commands (app::AutomationCommands); the
// owner records the undo step (beforeEdit) and the dirty flag (changed), and shows the lane (onShow).
// Pending: Cross-Over, Trim, Fill, Show/Hide by category, parameters of plugins and instruments.
class AutomationPanel final : public juce::Component, private juce::Timer
{
public:
    explicit AutomationPanel(AudioEngine&);
    ~AutomationPanel() override;
    std::function<void()> beforeEdit, changed;
    std::function<void(int trackIndex, domain::AutomationTarget)> onShow;

    void refresh();                  // rebuild rows when the tracks change, update modes
    int rowCount() const noexcept { return (int) rows.size(); }
    int rowTrack(int row) const noexcept;
    void setMode(int row, domain::AutomationMode); // what the row's combo box does
    void show(int row, domain::AutomationTarget);  // what the Mostrar buttons do
    void refreshLanguage();           // translated texts after a language change
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Row;
    theme::SilenteLookAndFeel look;
    AudioEngine& engine;
    juce::TextButton suspendRead, suspendWrite;
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<Row>> rows;
    juce::StringArray ids;
    void rebuild();
    juce::StringArray signature() const;
    void timerCallback() override { refresh(); }
};
}

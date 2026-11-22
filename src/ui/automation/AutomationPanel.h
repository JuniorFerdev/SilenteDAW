#pragma once
#include "../../AudioEngine.h"
#include "../../app/AutomationCommands.h"
#include "../theme/SilenteLookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <memory>

namespace silente::ui
{
// Panel «Automatización» (Cubase F6, manual pp. 913–924), reduced: one row per mixable track with
// the automation mode (Desactivado / Leer / Escribir / Touch / Latch / Cross-Over), «Mostrar»
// buttons that open the lane of a parameter (volume, pan, sends) and make it the row's parameter,
// and the row's Trim (dB) for that parameter; a bar with the global «Suspender lectura» and
// «Suspender escritura»; a «Rellenar» bar (Hasta punch, Hasta inicio, Hasta final, Bucle, Huecos)
// that acts on the selected row's parameter. It only shows state and sends commands
// (app::AutomationCommands); the owner records the undo step (beforeEdit) and the dirty flag
// (changed), and shows the lane (onShow).
// Pending: Fill on punch out (one-shot / continuous), Show/Hide and Suspend by category, Trim
// curves and Freeze Trim, parameters of plugins and instruments.
class AutomationPanel final : public juce::Component, private juce::Timer
{
public:
    explicit AutomationPanel(AudioEngine&);
    ~AutomationPanel() override;
    std::function<void()> beforeEdit, changed;
    std::function<void(int trackIndex, domain::AutomationTarget)> onShow;

    void refresh();                  // rebuild rows when the tracks change, update modes and trims
    int rowCount() const noexcept { return (int) rows.size(); }
    int rowTrack(int row) const noexcept;
    void setMode(int row, domain::AutomationMode); // what the row's combo box does
    void show(int row, domain::AutomationTarget);  // what the Mostrar buttons do (also selects the row)
    void selectRow(int row);                       // clicking the track name
    int selectedRow() const noexcept { return selected; }
    domain::AutomationTarget rowTarget(int row) const noexcept; // parameter of Trim and Fill
    bool setTrim(int row, double factor);          // trim of the row's parameter, one undo step
    bool fill(domain::AutomationFill);             // Fill on the selected row's parameter, one undo step
    void refreshLanguage();           // translated texts after a language change
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Row;
    theme::SilenteLookAndFeel look;
    AudioEngine& engine;
    juce::TextButton suspendRead, suspendWrite;
    std::array<juce::TextButton, 5> fillButtons;
    juce::Label fillTarget;
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<Row>> rows;
    juce::StringArray ids;
    int selected = 0;
    void rebuild();
    void setFillTexts();
    Track* rowTrackPointer(int row) const noexcept;
    juce::StringArray signature() const;
    void timerCallback() override { refresh(); }
};
}

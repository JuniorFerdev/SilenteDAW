#pragma once
#include "../../domain/TempoMap.h"
#include "../theme/SilenteLookAndFeel.h"
#include <functional>

namespace silente::ui
{
// Tempo and time signature map editor (reduced form of Cubase's «Editor de pista de tempo» and
// signature events). It edits a copy of the map and hands the whole new map to `onChange`; the
// owner applies it as one undoable command. The tempo at the project start stays in the
// transport bar. Positions: tempo points in Compases+Tiempos, signature changes in bars.
class TempoMapPanel final : public juce::Component, private juce::ListBoxModel
{
public:
    TempoMapPanel();
    ~TempoMapPanel() override;
    std::function<void(const domain::TempoMap&, const juce::String& action)> onChange;
    // Shows the project's map; does nothing when it did not change (keeps the selection).
    void setMap(const domain::TempoMap&, double startBpm);
    const domain::TempoMap& map() const noexcept { return current; }
    // Editing (also used by the interface tests). Each returns an error text or empty on success.
    juce::String addFromFields();
    juce::String updateSelected();
    juce::String removeSelected();
    void selectRow(int row);
    int rowCount() const noexcept { return (int) rows.size(); }
    juce::ComboBox kind;                 // 1 = tempo, 2 = time signature
    juce::TextEditor position, bpm, numerator;
    juce::ComboBox denominator;
    juce::ToggleButton ramp;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Row { bool tempo = true; int index = 0; double beat = 0.0; };
    theme::SilenteLookAndFeel look;
    domain::TempoMap current;
    double baseBpm = 120.0;
    std::vector<Row> rows;
    juce::ListBox list;
    juce::TextButton add, update, remove;
    juce::Label message;
    domain::TempoView view() const { return { baseBpm, &current }; }
    void rebuildRows();
    void showKind();
    void fillFields(int row);
    // Reads the fields into an event; returns an error text when they are not valid.
    juce::String readTempo(domain::TempoEvent&) const;
    juce::String readSignature(domain::SignatureEvent&) const;
    void commit(domain::TempoMap next, const juce::String& action);
    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem(int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged(int row) override;
};
}

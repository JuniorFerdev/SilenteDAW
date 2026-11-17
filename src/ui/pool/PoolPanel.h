#pragma once
#include "../../domain/MediaPool.h"
#include "../theme/SilenteLookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace silente::ui
{
// Ventana Pool (reducida): lista los medios del proyecto con nombre, ruta, duración, usos y estado.
// Solo muestra y manda: el dueño entrega las referencias a clips (`provider`) y ejecuta la
// reasignación (`onRelink`, un paso de deshacer) y la selección de clips (`onSelectClips`).
class PoolPanel final : public juce::Component, private juce::TableListBoxModel, private juce::Timer
{
public:
    PoolPanel();
    ~PoolPanel() override;
    std::function<std::vector<domain::MediaClipRef>()> provider;
    std::function<void(const domain::MediaEntry&)> onSelectClips;            // doble clic
    std::function<juce::String(const juce::String& from, const juce::File& to)> onRelink; // error o vacío
    void refresh();
    int rowCount() const noexcept { return (int) entries.size(); }
    const domain::MediaEntry* entry(int row) const noexcept { return juce::isPositiveAndBelow(row, (int) entries.size()) ? &entries[(size_t) row] : nullptr; }
    // Orden por columna 1..5 (nombre, ruta, duración, usos, estado); repetir invierte el sentido.
    void sortByColumn(int columnId, bool ascending);
    void selectRow(int row);
    void activateRow(int row); // lo que hace el doble clic
    // Reasigna el medio de la fila seleccionada al archivo dado (la ruta del selector nativo o de una prueba).
    juce::String relinkSelected(const juce::File& newFile);
    void locateMissing();      // abre el selector nativo
    juce::TextButton locate;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    theme::SilenteLookAndFeel look;
    juce::TableListBox table;
    juce::Label message;
    std::vector<domain::MediaEntry> entries;
    std::set<juce::String> replaced; // rutas reasignadas en esta sesión (no se guarda)
    int sortColumn = 1; bool sortAscending = true;
    std::unique_ptr<juce::FileChooser> chooser;
    int getNumRows() override { return (int) entries.size(); }
    void paintRowBackground(juce::Graphics&, int row, int width, int height, bool selected) override;
    void paintCell(juce::Graphics&, int row, int column, int width, int height, bool selected) override;
    void sortOrderChanged(int newSortColumnId, bool isForwards) override;
    void cellDoubleClicked(int row, int column, const juce::MouseEvent&) override;
    void selectedRowsChanged(int) override { updateButton(); }
    void timerCallback() override { if (isShowing()) refresh(); }
    void updateButton();
    static juce::String stateText(domain::MediaState);
};
}

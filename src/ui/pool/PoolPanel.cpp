#include "PoolPanel.h"
#include "../../i18n/Translations.h"
#include "../theme/Drawing.h"

namespace silente::ui
{
using namespace theme;

PoolPanel::PoolPanel()
{
    setComponentID("pool-panel");
    setLookAndFeel(&look);
    table.setComponentID("pool-table");
    table.setModel(this);
    table.setRowHeight(24);
    table.setColour(juce::ListBox::backgroundColourId, bg);
    table.setColour(juce::ListBox::outlineColourId, line);
    table.setOutlineThickness(1);
    auto& header = table.getHeader();
    header.addColumn(i18n::tr("Nombre"), 1, 170, 60, 600, juce::TableHeaderComponent::defaultFlags);
    header.addColumn(i18n::tr("Ruta"), 2, 300, 80, 1200, juce::TableHeaderComponent::defaultFlags);
    header.addColumn(i18n::tr("Duración"), 3, 90, 50, 200, juce::TableHeaderComponent::defaultFlags);
    header.addColumn(i18n::tr("Usos"), 4, 50, 40, 120, juce::TableHeaderComponent::defaultFlags);
    header.addColumn(i18n::tr("Estado"), 5, 90, 60, 200, juce::TableHeaderComponent::defaultFlags);
    header.setSortColumnId(1, true);
    locate.setButtonText(i18n::tr("Localizar archivo ausente…"));
    locate.setComponentID("pool-locate");
    locate.setEnabled(false);
    locate.onClick = [this] { locateMissing(); };
    message.setColour(juce::Label::textColourId, coral);
    for (auto* c : std::initializer_list<juce::Component*> { &table, &locate, &message }) addAndMakeVisible(c);
    setSize(720, 380);
    startTimer(1000);
}

PoolPanel::~PoolPanel() { stopTimer(); table.setModel(nullptr); setLookAndFeel(nullptr); }

juce::String PoolPanel::stateText(domain::MediaState state)
{
    switch (state)
    {
        case domain::MediaState::present: return i18n::tr("Presente");
        case domain::MediaState::missing: return i18n::tr("Ausente");
        case domain::MediaState::replaced: return i18n::tr("Reemplazado");
    }
    return {};
}

void PoolPanel::refresh()
{
    if (! provider) return;
    const auto* old = entry(table.getSelectedRow());
    const auto keep = old ? old->path : juce::String();
    entries = domain::buildPool(provider(), [](const juce::String& path) { return juce::File(path).existsAsFile(); }, replaced);
    domain::sortPool(entries, (domain::MediaColumn) (sortColumn - 1), sortAscending);
    table.updateContent();
    table.deselectAllRows();
    if (keep.isNotEmpty())
        for (int i = 0; i < (int) entries.size(); ++i)
            if (entries[(size_t) i].path == keep) { table.selectRow(i); break; }
    table.repaint();
    updateButton();
}

void PoolPanel::updateButton()
{
    const auto* e = entry(table.getSelectedRow());
    locate.setEnabled(e != nullptr && e->state == domain::MediaState::missing && e->path.isNotEmpty());
}

void PoolPanel::sortByColumn(int columnId, bool ascending)
{
    sortColumn = juce::jlimit(1, 5, columnId); sortAscending = ascending;
    table.getHeader().setSortColumnId(sortColumn, ascending);
    refresh();
}

void PoolPanel::sortOrderChanged(int newSortColumnId, bool isForwards)
{
    if (newSortColumnId < 1) return;
    sortColumn = newSortColumnId; sortAscending = isForwards;
    refresh();
}

void PoolPanel::selectRow(int row) { table.selectRow(row); updateButton(); }

void PoolPanel::activateRow(int row)
{
    if (const auto* e = entry(row); e && onSelectClips) onSelectClips(*e);
}

void PoolPanel::cellDoubleClicked(int row, int, const juce::MouseEvent&) { activateRow(row); }

juce::String PoolPanel::relinkSelected(const juce::File& newFile)
{
    const auto* e = entry(table.getSelectedRow());
    if (! e || e->path.isEmpty()) return i18n::tr("Selecciona un medio con ruta.");
    if (! onRelink) return {};
    const auto from = e->path;
    auto error = onRelink(from, newFile);
    if (error.isEmpty()) replaced.insert(newFile.getFullPathName());
    message.setText(error, juce::dontSendNotification);
    refresh();
    return error;
}

void PoolPanel::locateMissing()
{
    const auto* e = entry(table.getSelectedRow());
    if (! e || e->state != domain::MediaState::missing) return;
    chooser = std::make_unique<juce::FileChooser>(i18n::tr("Elegir el archivo de audio que reemplaza a ") + e->name, juce::File(e->path).getParentDirectory(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.ogg");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file != juce::File()) relinkSelected(file);
    });
}

void PoolPanel::paintRowBackground(juce::Graphics& g, int row, int, int, bool selected)
{
    g.fillAll(selected ? selectedTrack : row % 2 ? panel : bg);
}

void PoolPanel::paintCell(juce::Graphics& g, int row, int column, int width, int height, bool selected)
{
    const auto* e = entry(row);
    if (! e) return;
    const auto colour = selected ? activeInk : e->state == domain::MediaState::missing ? coral : ink;
    juce::String value;
    switch (column)
    {
        case 1: value = e->name; break;
        case 2: value = e->path; break;
        case 3: value = e->lengthSamples > 0 ? juce::String(e->seconds, 3) + " s" : juce::String(juce::CharPointer_UTF8("\xe2\x80\x94")); break;
        case 4: value = juce::String(e->uses); break;
        case 5: value = stateText(e->state); break;
        default: break;
    }
    text(g, value, { 6, 0, width - 8, height }, 11, colour);
}

void PoolPanel::paint(juce::Graphics& g) { g.fillAll(panel); }

void PoolPanel::resized()
{
    auto area = getLocalBounds().reduced(10);
    auto bottom = area.removeFromBottom(30);
    locate.setBounds(bottom.removeFromLeft(220));
    message.setBounds(bottom.withTrimmedLeft(8));
    area.removeFromBottom(6);
    table.setBounds(area);
}
}

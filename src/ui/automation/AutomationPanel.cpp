#include "AutomationPanel.h"
#include "../../i18n/Translations.h"
#include "../theme/Drawing.h"

namespace silente::ui
{
using namespace theme;
using domain::AutomationFill;
using domain::AutomationMode;
using domain::AutomationParameter;
using domain::AutomationTarget;

namespace
{
constexpr int rowHeight = 30, barHeight = 38;
constexpr double trimFloorDb = -24.0, trimCeilingDb = 12.0;
bool listed(const Track& t) { return t.hasAudioChannel(); }
juce::String parameterName(AutomationTarget t)
{
    if (t.parameter == AutomationParameter::pan) return i18n::tr("Panorama");
    if (t.parameter == AutomationParameter::send) return i18n::tr("Envío ") + juce::String(t.send + 1);
    return i18n::tr("Volumen");
}
double trimToDb(double factor) { return juce::jlimit(trimFloorDb, trimCeilingDb, juce::Decibels::gainToDecibels(factor, trimFloorDb)); }
constexpr AutomationFill fills[] { AutomationFill::toPunch, AutomationFill::toStart, AutomationFill::toEnd, AutomationFill::loop, AutomationFill::gaps };
}

struct AutomationPanel::Row
{
    int trackIndex = 0;
    AutomationTarget target; // parameter of Trim and Fill: the last one shown
    bool trimDragging = false;
    juce::TextButton name;
    juce::ComboBox mode;
    juce::TextButton volume, pan, sends;
    juce::Slider trim { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };
};

AutomationPanel::AutomationPanel(AudioEngine& e) : engine(e)
{
    setComponentID("automation-panel");
    setLookAndFeel(&look);
    suspendRead.setComponentID("automation-suspend-read"); suspendWrite.setComponentID("automation-suspend-write");
    for (auto* b : { &suspendRead, &suspendWrite }) { b->setClickingTogglesState(true); addAndMakeVisible(b); }
    suspendRead.setButtonText(i18n::tr("Suspender lectura")); suspendWrite.setButtonText(i18n::tr("Suspender escritura"));
    suspendRead.setTooltip(i18n::tr("Suspender lectura: todas las pistas suenan con sus valores fijos"));
    suspendWrite.setTooltip(i18n::tr("Suspender escritura: ningún control escribe automatización"));
    suspendRead.setColour(juce::TextButton::buttonOnColourId, automationRead.withAlpha(.8f));
    suspendWrite.setColour(juce::TextButton::buttonOnColourId, automationWrite.withAlpha(.8f));
    suspendRead.onClick = [this] { app::setSuspendRead(engine, suspendRead.getToggleState()); };
    suspendWrite.onClick = [this]
    {
        app::setSuspendWrite(engine, suspendWrite.getToggleState());
        if (suspendWrite.getToggleState()) engine.automationGestures.clear();
    };
    for (size_t k = 0; k < fillButtons.size(); ++k)
    {
        fillButtons[k].setComponentID("automation-fill-" + juce::String((int) k));
        fillButtons[k].onClick = [this, k] { fill(fills[k]); };
        addAndMakeVisible(fillButtons[k]);
    }
    fillTarget.setComponentID("automation-fill-target");
    fillTarget.setColour(juce::Label::textColourId, muted);
    addAndMakeVisible(fillTarget);
    setFillTexts();
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
    setSize(700, 360);
    refresh();
    startTimerHz(4);
}

AutomationPanel::~AutomationPanel() { stopTimer(); setLookAndFeel(nullptr); }

void AutomationPanel::setFillTexts()
{
    const juce::String texts[] { i18n::tr("Hasta punch"), i18n::tr("Hasta inicio"), i18n::tr("Hasta final"), i18n::tr("Bucle"), i18n::tr("Huecos") };
    const juce::String tips[] {
        i18n::tr("Rellenar el tramo del último pase de escritura con el valor encontrado"),
        i18n::tr("Rellenar desde el inicio del proyecto hasta el cursor con el valor actual del control"),
        i18n::tr("Rellenar desde el cursor hasta el final del proyecto con el valor actual del control"),
        i18n::tr("Rellenar entre los localizadores izquierdo y derecho con el valor actual del control"),
        i18n::tr("Rellenar antes del primer punto y después del último con el valor actual del control"),
    };
    for (size_t k = 0; k < fillButtons.size(); ++k) { fillButtons[k].setButtonText(texts[k]); fillButtons[k].setTooltip(tips[k]); }
}

juce::StringArray AutomationPanel::signature() const
{
    juce::StringArray now;
    const juce::ScopedLock lock(engine.modelLock);
    for (const auto& t : engine.tracks) now.add(t->id + "|" + t->name + "|" + juce::String((int) listed(*t)));
    return now;
}

void AutomationPanel::rebuild()
{
    std::vector<std::pair<juce::String, AutomationTarget>> previous; // keep each row's parameter
    {
        const juce::ScopedLock lock(engine.modelLock);
        for (const auto& row : rows)
            if (row->trackIndex < (int) engine.tracks.size()) previous.emplace_back(engine.tracks[(size_t) row->trackIndex]->id, row->target);
    }
    rows.clear(); content.removeAllChildren();
    {
        const juce::ScopedLock lock(engine.modelLock);
        for (int i = 0; i < (int) engine.tracks.size(); ++i)
        {
            const auto& track = *engine.tracks[(size_t) i];
            if (! listed(track)) continue;
            auto row = std::make_unique<Row>();
            row->trackIndex = i;
            for (const auto& [id, target] : previous) if (id == track.id) row->target = target;
            const int r = (int) rows.size();
            row->name.setButtonText(track.name);
            row->name.setComponentID("automation-row-" + juce::String(r));
            row->name.setClickingTogglesState(false);
            row->name.setTooltip(i18n::tr("Seleccionar la pista para Rellenar"));
            row->name.onClick = [this, r] { selectRow(r); };
            row->mode.setComponentID("automation-mode-" + juce::String(r));
            row->mode.addItem(i18n::tr("Desactivado"), 1); row->mode.addItem(i18n::tr("Leer"), 2);
            row->mode.addItem(i18n::tr("Escribir"), 3); row->mode.addItem("Touch", 4); row->mode.addItem("Latch", 5); row->mode.addItem("Cross-Over", 6);
            row->mode.setTooltip(i18n::tr("Modo de automatización de la pista"));
            row->mode.onChange = [this, r] { setMode(r, (AutomationMode) (rows[(size_t) r]->mode.getSelectedId() - 1)); };
            row->volume.setButtonText(i18n::tr("Volumen")); row->pan.setButtonText(i18n::tr("Panorama")); row->sends.setButtonText(i18n::tr("Envíos"));
            row->volume.setComponentID("automation-show-volume-" + juce::String(r));
            row->pan.setComponentID("automation-show-pan-" + juce::String(r));
            row->sends.setComponentID("automation-show-sends-" + juce::String(r));
            row->volume.setTooltip(i18n::tr("Mostrar la curva de volumen en la zona inferior"));
            row->pan.setTooltip(i18n::tr("Mostrar la curva de panorama en la zona inferior"));
            row->sends.setTooltip(i18n::tr("Mostrar la curva de un envío en la zona inferior"));
            row->volume.onClick = [this, r] { show(r, { AutomationParameter::volume }); };
            row->pan.onClick = [this, r] { show(r, { AutomationParameter::pan }); };
            row->sends.onClick = [this, r]
            {
                juce::PopupMenu menu;
                {
                    const int index = rows[(size_t) r]->trackIndex;
                    const juce::ScopedLock inner(engine.modelLock);
                    if (index < (int) engine.tracks.size())
                        for (int k = 0; k < domain::automationSendCount; ++k)
                        {
                            const auto& t = *engine.tracks[(size_t) index];
                            if (t.sends[(size_t) k].target.isNotEmpty() || ! t.automation.sends[(size_t) k].empty())
                                menu.addItem(k + 1, i18n::tr("Envío ") + juce::String(k + 1));
                        }
                }
                if (menu.getNumItems() == 0) menu.addItem(-1, i18n::tr("Sin envíos"), false);
                menu.showMenuAsync({}, [this, r](int chosen) { if (chosen > 0) show(r, { AutomationParameter::send, chosen - 1 }); });
            };
            auto& trim = row->trim;
            trim.setComponentID("automation-trim-" + juce::String(r));
            trim.setRange(trimFloorDb, trimCeilingDb, 0.1);
            trim.setTextValueSuffix(" dB");
            trim.setDoubleClickReturnValue(true, 0.0);
            trim.setValue(0.0, juce::dontSendNotification);
            trim.onDragStart = [this, r] { if (beforeEdit) beforeEdit(); rows[(size_t) r]->trimDragging = true; };
            trim.onDragEnd = [this, r] { rows[(size_t) r]->trimDragging = false; };
            trim.onValueChange = [this, r]
            {
                auto& self = *rows[(size_t) r];
                const double factor = juce::Decibels::decibelsToGain(self.trim.getValue(), trimFloorDb - 1.0);
                if (! self.trimDragging) { setTrim(r, factor); return; }
                if (auto* t = rowTrackPointer(r); t && app::setTrim(engine, *t, self.target, factor) && changed) changed();
            };
            for (auto* c : std::initializer_list<juce::Component*> { &row->name, &row->mode, &row->volume, &row->pan, &row->sends, &row->trim }) content.addAndMakeVisible(c);
            rows.push_back(std::move(row));
        }
    }
    selected = juce::jlimit(0, juce::jmax(0, rowCount() - 1), selected);
    ids = signature();
    resized();
}

void AutomationPanel::refresh()
{
    if (signature() != ids) rebuild();
    {
        const juce::ScopedLock lock(engine.modelLock);
        for (int r = 0; r < rowCount(); ++r)
        {
            auto& row = *rows[(size_t) r];
            if (row.trackIndex >= (int) engine.tracks.size()) continue;
            const auto& automation = engine.tracks[(size_t) row.trackIndex]->automation;
            row.mode.setSelectedId(1 + (int) automation.mode(), juce::dontSendNotification);
            const bool trimmable = domain::trimmable(row.target);
            row.trim.setEnabled(trimmable);
            row.trim.setTooltip(trimmable ? i18n::tr("Trim de ") + parameterName(row.target) + i18n::tr(": escala la curva leída sin reescribirla")
                                          : i18n::tr("El panorama no tiene Trim"));
            if (! row.trimDragging) row.trim.setValue(trimToDb(automation.trim(row.target)), juce::dontSendNotification);
            row.name.setToggleState(r == selected, juce::dontSendNotification);
        }
    }
    const auto* track = rowTrackPointer(selected);
    fillTarget.setText(track ? track->name + juce::String::fromUTF8(" · ") + parameterName(rowTarget(selected)) : juce::String(), juce::dontSendNotification);
    for (size_t k = 0; k < fillButtons.size(); ++k) fillButtons[k].setEnabled(track && app::canFill(engine, *track, rowTarget(selected), fills[k]));
    suspendRead.setToggleState(engine.automationSuspendRead.load(), juce::dontSendNotification);
    suspendWrite.setToggleState(engine.automationSuspendWrite.load(), juce::dontSendNotification);
}

Track* AutomationPanel::rowTrackPointer(int row) const noexcept
{
    const int index = rowTrack(row);
    return index >= 0 && index < (int) engine.tracks.size() ? engine.tracks[(size_t) index].get() : nullptr;
}

void AutomationPanel::setMode(int row, AutomationMode mode)
{
    auto* track = rowTrackPointer(row);
    if (! track || track->automation.mode() == mode) return;
    if (beforeEdit) beforeEdit();
    app::setMode(engine, *track, mode);
    if (changed) changed();
    refresh();
}

void AutomationPanel::show(int row, AutomationTarget target)
{
    if (row < 0 || row >= rowCount() || ! domain::validTarget(target)) return;
    rows[(size_t) row]->target = target;
    selected = row;
    refresh();
    if (onShow) onShow(rows[(size_t) row]->trackIndex, target);
}

void AutomationPanel::selectRow(int row)
{
    if (row < 0 || row >= rowCount()) return;
    selected = row;
    refresh();
}

AutomationTarget AutomationPanel::rowTarget(int row) const noexcept { return row >= 0 && row < rowCount() ? rows[(size_t) row]->target : AutomationTarget {}; }

bool AutomationPanel::setTrim(int row, double factor)
{
    auto* track = rowTrackPointer(row);
    const auto target = rowTarget(row);
    if (! track || ! domain::trimmable(target) || track->automation.trim(target) == domain::clampTrim(factor)) return false;
    if (beforeEdit) beforeEdit();
    app::setTrim(engine, *track, target, factor);
    if (changed) changed();
    refresh();
    return true;
}

bool AutomationPanel::fill(AutomationFill mode)
{
    auto* track = rowTrackPointer(selected);
    const auto target = rowTarget(selected);
    if (! track || ! app::canFill(engine, *track, target, mode)) return false;
    if (beforeEdit) beforeEdit();
    app::fill(engine, *track, target, mode);
    if (changed) changed();
    refresh();
    return true;
}

int AutomationPanel::rowTrack(int row) const noexcept { return row >= 0 && row < rowCount() ? rows[(size_t) row]->trackIndex : -1; }

void AutomationPanel::refreshLanguage()
{
    suspendRead.setButtonText(i18n::tr("Suspender lectura")); suspendWrite.setButtonText(i18n::tr("Suspender escritura"));
    setFillTexts();
    ids.clear(); refresh(); repaint();
}

void AutomationPanel::paint(juce::Graphics& g)
{
    g.fillAll(panel);
    text(g, i18n::tr("Rellenar"), { 12, barHeight + 6, 80, 22 }, 11, muted);
    const int y = 2 * barHeight + 2;
    text(g, i18n::tr("Pista"), { 12, y, 150, 18 }, 11, muted);
    text(g, i18n::tr("Modo"), { 166, y, 120, 18 }, 11, muted);
    text(g, i18n::tr("Mostrar"), { 296, y, 200, 18 }, 11, muted);
    text(g, "Trim", { 530, y, 150, 18 }, 11, muted);
}

void AutomationPanel::resized()
{
    auto area = getLocalBounds().reduced(8);
    auto bar = area.removeFromTop(barHeight - 8);
    suspendRead.setBounds(bar.removeFromLeft(150)); bar.removeFromLeft(6); suspendWrite.setBounds(bar.removeFromLeft(150));
    area.removeFromTop(8);
    auto fillBar = area.removeFromTop(barHeight - 8);
    fillBar.removeFromLeft(78);
    for (auto& b : fillButtons) { b.setBounds(fillBar.removeFromLeft(88)); fillBar.removeFromLeft(4); }
    fillTarget.setBounds(fillBar.reduced(2, 0));
    area.removeFromTop(24);
    viewport.setBounds(area);
    const int width = juce::jmax(300, viewport.getMaximumVisibleWidth());
    content.setSize(width, juce::jmax(1, (int) rows.size() * rowHeight));
    int y = 0;
    for (auto& row : rows)
    {
        row->name.setBounds(4, y + 3, 150, rowHeight - 6);
        row->mode.setBounds(158, y + 3, 120, rowHeight - 6);
        row->volume.setBounds(288, y + 3, 70, rowHeight - 6); row->pan.setBounds(362, y + 3, 80, rowHeight - 6); row->sends.setBounds(446, y + 3, 70, rowHeight - 6);
        row->trim.setBounds(522, y + 3, juce::jmax(80, width - 526), rowHeight - 6);
        y += rowHeight;
    }
}
}

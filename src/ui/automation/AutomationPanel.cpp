#include "AutomationPanel.h"
#include "../../i18n/Translations.h"
#include "../theme/Drawing.h"

namespace silente::ui
{
using namespace theme;
using domain::AutomationMode;
using domain::AutomationParameter;
using domain::AutomationTarget;

namespace
{
constexpr int rowHeight = 30, barHeight = 38;
bool listed(const Track& t) { return t.hasAudioChannel(); }
}

struct AutomationPanel::Row
{
    int trackIndex = 0;
    juce::Label name;
    juce::ComboBox mode;
    juce::TextButton volume, pan, sends;
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
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
    setSize(560, 320);
    refresh();
    startTimerHz(4);
}

AutomationPanel::~AutomationPanel() { stopTimer(); setLookAndFeel(nullptr); }

juce::StringArray AutomationPanel::signature() const
{
    juce::StringArray now;
    const juce::ScopedLock lock(engine.modelLock);
    for (const auto& t : engine.tracks) now.add(t->id + "|" + t->name + "|" + juce::String((int) listed(*t)));
    return now;
}

void AutomationPanel::rebuild()
{
    rows.clear(); content.removeAllChildren();
    {
        const juce::ScopedLock lock(engine.modelLock);
        for (int i = 0; i < (int) engine.tracks.size(); ++i)
        {
            const auto& track = *engine.tracks[(size_t) i];
            if (! listed(track)) continue;
            auto row = std::make_unique<Row>();
            row->trackIndex = i;
            const int r = (int) rows.size();
            row->name.setText(track.name, juce::dontSendNotification);
            row->name.setColour(juce::Label::textColourId, ink);
            row->mode.setComponentID("automation-mode-" + juce::String(r));
            row->mode.addItem(i18n::tr("Desactivado"), 1); row->mode.addItem(i18n::tr("Leer"), 2);
            row->mode.addItem(i18n::tr("Escribir"), 3); row->mode.addItem("Touch", 4); row->mode.addItem("Latch", 5);
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
            for (auto* c : std::initializer_list<juce::Component*> { &row->name, &row->mode, &row->volume, &row->pan, &row->sends }) content.addAndMakeVisible(c);
            rows.push_back(std::move(row));
        }
    }
    ids = signature();
    resized();
}

void AutomationPanel::refresh()
{
    if (signature() != ids) rebuild();
    {
        const juce::ScopedLock lock(engine.modelLock);
        for (auto& row : rows)
            if (row->trackIndex < (int) engine.tracks.size())
                row->mode.setSelectedId(1 + (int) engine.tracks[(size_t) row->trackIndex]->automation.mode(), juce::dontSendNotification);
    }
    suspendRead.setToggleState(engine.automationSuspendRead.load(), juce::dontSendNotification);
    suspendWrite.setToggleState(engine.automationSuspendWrite.load(), juce::dontSendNotification);
}

void AutomationPanel::setMode(int row, AutomationMode mode)
{
    if (row < 0 || row >= rowCount()) return;
    const int index = rows[(size_t) row]->trackIndex;
    if (index >= (int) engine.tracks.size()) return;
    auto& track = *engine.tracks[(size_t) index];
    if (track.automation.mode() == mode) return;
    if (beforeEdit) beforeEdit();
    app::setMode(engine, track, mode);
    if (changed) changed();
    refresh();
}

void AutomationPanel::show(int row, AutomationTarget target)
{
    if (row >= 0 && row < rowCount() && onShow) onShow(rows[(size_t) row]->trackIndex, target);
}

int AutomationPanel::rowTrack(int row) const noexcept { return row >= 0 && row < rowCount() ? rows[(size_t) row]->trackIndex : -1; }

void AutomationPanel::refreshLanguage()
{
    suspendRead.setButtonText(i18n::tr("Suspender lectura")); suspendWrite.setButtonText(i18n::tr("Suspender escritura"));
    ids.clear(); refresh(); repaint();
}

void AutomationPanel::paint(juce::Graphics& g)
{
    g.fillAll(panel);
    text(g, i18n::tr("Pista"), { 12, barHeight + 2, 150, 18 }, 11, muted);
    text(g, i18n::tr("Modo"), { 166, barHeight + 2, 110, 18 }, 11, muted);
    text(g, i18n::tr("Mostrar"), { 288, barHeight + 2, 200, 18 }, 11, muted);
}

void AutomationPanel::resized()
{
    auto area = getLocalBounds().reduced(8);
    auto bar = area.removeFromTop(barHeight - 8);
    suspendRead.setBounds(bar.removeFromLeft(150)); bar.removeFromLeft(6); suspendWrite.setBounds(bar.removeFromLeft(150));
    area.removeFromTop(24);
    viewport.setBounds(area);
    const int width = juce::jmax(300, viewport.getMaximumVisibleWidth());
    content.setSize(width, juce::jmax(1, (int) rows.size() * rowHeight));
    int y = 0;
    for (auto& row : rows)
    {
        row->name.setBounds(4, y + 3, 150, rowHeight - 6);
        row->mode.setBounds(158, y + 3, 110, rowHeight - 6);
        row->volume.setBounds(280, y + 3, 70, rowHeight - 6); row->pan.setBounds(354, y + 3, 80, rowHeight - 6); row->sends.setBounds(438, y + 3, 70, rowHeight - 6);
        y += rowHeight;
    }
}
}

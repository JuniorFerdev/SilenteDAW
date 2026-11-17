#include "TempoMapPanel.h"
#include "../../i18n/Translations.h"
#include "../theme/Drawing.h"
#include <cmath>

namespace silente::ui
{
using namespace theme;
namespace
{
constexpr int denominators[] { 1, 2, 4, 8, 16, 32 };
constexpr double minimumEditBpm = 20.0, maximumEditBpm = 300.0; // same range as the transport bar

// Compases+Tiempos of a musical position (beats counted in the signature's unit).
juce::String barsBeats(const domain::TempoView& view, double beats)
{
    const auto at = view.barAt(beats);
    const double unit = at.signature.unitBeats();
    const int beat = juce::jlimit(0, at.signature.numerator - 1, (int) std::floor(at.beatInBar / unit + 1.0e-9));
    const double sixteenths = juce::jmax(0.0, at.beatInBar - beat * unit) * 4.0;
    const int six = (int) std::floor(sixteenths + 1.0e-9);
    const int ticks = juce::jmax(0, (int) std::floor((sixteenths - six) * 120.0 + 1.0e-6));
    return juce::String(at.bar + 1) + ". " + juce::String(beat + 1) + ". " + juce::String(six + 1) + ". " + juce::String(ticks);
}
double number(const juce::String& text) { return text.trim().replace(",", ".").getDoubleValue(); }
bool isNumber(const juce::String& text) { const auto t = text.trim().replace(",", "."); return t.isNotEmpty() && t.containsOnly("0123456789.") && t.indexOfChar('.') == t.lastIndexOfChar('.'); }
}

TempoMapPanel::TempoMapPanel()
{
    setComponentID("tempo-map-panel");
    setLookAndFeel(&look);
    list.setModel(this); list.setRowHeight(24); list.setComponentID("tempo-map-list");
    list.setColour(juce::ListBox::backgroundColourId, bg); list.setColour(juce::ListBox::outlineColourId, line); list.setOutlineThickness(1);
    kind.addItem(i18n::tr("Tempo"), 1); kind.addItem(i18n::tr("Tipo de compás"), 2); kind.setSelectedId(1, juce::dontSendNotification);
    kind.onChange = [this] { showKind(); };
    for (int d : denominators) denominator.addItem(juce::String(d), d);
    denominator.setSelectedId(4, juce::dontSendNotification);
    position.setText("2. 1. 1. 0", false); bpm.setText("120", false); numerator.setText("4", false);
    position.setComponentID("tempo-map-position"); bpm.setComponentID("tempo-map-bpm"); numerator.setComponentID("tempo-map-numerator");
    ramp.setButtonText(i18n::tr("Rampa desde el punto anterior"));
    add.setButtonText(i18n::tr("Añadir")); update.setButtonText(i18n::tr("Actualizar")); remove.setButtonText(i18n::tr("Quitar"));
    add.setComponentID("tempo-map-add"); update.setComponentID("tempo-map-update"); remove.setComponentID("tempo-map-remove");
    add.onClick = [this] { message.setText(addFromFields(), juce::dontSendNotification); };
    update.onClick = [this] { message.setText(updateSelected(), juce::dontSendNotification); };
    remove.onClick = [this] { message.setText(removeSelected(), juce::dontSendNotification); };
    message.setColour(juce::Label::textColourId, coral);
    for (auto* c : std::initializer_list<juce::Component*> { &list, &kind, &position, &bpm, &numerator, &denominator, &ramp, &add, &update, &remove, &message }) addAndMakeVisible(c);
    showKind();
    setSize(560, 460);
}

TempoMapPanel::~TempoMapPanel() { list.setModel(nullptr); setLookAndFeel(nullptr); }

void TempoMapPanel::setMap(const domain::TempoMap& map, double startBpm)
{
    if (map == current && startBpm == baseBpm) return;
    current = map; baseBpm = startBpm;
    rebuildRows();
    repaint();
}

void TempoMapPanel::rebuildRows()
{
    rows.clear();
    for (int i = 0; i < (int) current.tempos.size(); ++i) rows.push_back({ true, i, current.tempos[(size_t) i].beat });
    for (int i = 0; i < (int) current.signatures.size(); ++i) rows.push_back({ false, i, view().barStart(current.signatures[(size_t) i].bar) });
    std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.beat < b.beat || (a.beat == b.beat && ! a.tempo && b.tempo); });
    list.updateContent(); list.repaint();
}

void TempoMapPanel::showKind()
{
    const bool tempo = kind.getSelectedId() == 1;
    bpm.setVisible(tempo); ramp.setVisible(tempo);
    numerator.setVisible(! tempo); denominator.setVisible(! tempo);
    resized(); repaint();
}

void TempoMapPanel::fillFields(int row)
{
    if (! juce::isPositiveAndBelow(row, (int) rows.size())) return;
    const auto& r = rows[(size_t) row];
    kind.setSelectedId(r.tempo ? 1 : 2, juce::dontSendNotification);
    if (r.tempo)
    {
        const auto& e = current.tempos[(size_t) r.index];
        position.setText(barsBeats(view(), e.beat), false); bpm.setText(juce::String(e.bpm, 3), false);
        ramp.setToggleState(e.ramp, juce::dontSendNotification);
    }
    else
    {
        const auto& s = current.signatures[(size_t) r.index];
        position.setText(juce::String(s.bar + 1), false); numerator.setText(juce::String(s.numerator), false);
        denominator.setSelectedId(s.denominator, juce::dontSendNotification);
    }
    showKind();
}

juce::String TempoMapPanel::readTempo(domain::TempoEvent& e) const
{
    auto parts = juce::StringArray::fromTokens(position.getText(), ". :", "");
    parts.removeEmptyStrings();
    if (parts.isEmpty() || parts.size() > 4) return i18n::tr("Posición no válida.");
    double values[] { 1.0, 1.0, 1.0, 0.0 };
    for (int i = 0; i < parts.size(); ++i) { if (! parts[i].containsOnly("0123456789")) return i18n::tr("Posición no válida."); values[i] = parts[i].getDoubleValue(); }
    const auto signature = view().signatureAtBar((int) values[0] - 1);
    if (values[0] < 1 || values[1] < 1 || values[1] > signature.numerator || values[2] < 1 || values[2] > 4 || values[3] >= 120) return i18n::tr("Posición no válida.");
    e.beat = view().beatsFromBarsBeats(values[0], values[1], values[2], values[3]);
    if (e.beat <= 0.0) return i18n::tr("El cambio de tempo debe estar después del inicio del proyecto.");
    if (! isNumber(bpm.getText()) || number(bpm.getText()) < minimumEditBpm || number(bpm.getText()) > maximumEditBpm) return i18n::tr("El tempo debe estar entre 20 y 300 BPM.");
    e.bpm = number(bpm.getText());
    e.ramp = ramp.getToggleState();
    return {};
}

juce::String TempoMapPanel::readSignature(domain::SignatureEvent& s) const
{
    const auto bar = position.getText().trim(), top = numerator.getText().trim();
    if (! bar.containsOnly("0123456789") || bar.isEmpty() || bar.getIntValue() < 1) return i18n::tr("Posición no válida.");
    if (! top.containsOnly("0123456789") || top.isEmpty() || top.getIntValue() < 1 || top.getIntValue() > 64 || ! domain::validDenominator(denominator.getSelectedId()))
        return i18n::tr("Tipo de compás no válido.");
    s = { bar.getIntValue() - 1, top.getIntValue(), denominator.getSelectedId() };
    return {};
}

void TempoMapPanel::commit(domain::TempoMap next, const juce::String& action)
{
    next.normalise();
    if (next == current) return;
    if (onChange) onChange(next, action);
    setMap(next, baseBpm);
}

juce::String TempoMapPanel::addFromFields()
{
    auto next = current;
    if (kind.getSelectedId() == 1) { domain::TempoEvent e; if (auto error = readTempo(e); error.isNotEmpty()) return error; next.tempos.push_back(e); }
    else { domain::SignatureEvent s; if (auto error = readSignature(s); error.isNotEmpty()) return error; next.signatures.push_back(s); }
    commit(std::move(next), i18n::tr("Mapa de tempo"));
    return {};
}

juce::String TempoMapPanel::updateSelected()
{
    const int row = list.getSelectedRow();
    if (! juce::isPositiveAndBelow(row, (int) rows.size())) return i18n::tr("Selecciona un evento de la lista.");
    const auto r = rows[(size_t) row];
    auto next = current;
    if (r.tempo) next.tempos.erase(next.tempos.begin() + r.index); else next.signatures.erase(next.signatures.begin() + r.index);
    if (kind.getSelectedId() == 1) { domain::TempoEvent e; if (auto error = readTempo(e); error.isNotEmpty()) return error; next.tempos.push_back(e); }
    else { domain::SignatureEvent s; if (auto error = readSignature(s); error.isNotEmpty()) return error; next.signatures.push_back(s); }
    commit(std::move(next), i18n::tr("Mapa de tempo"));
    return {};
}

juce::String TempoMapPanel::removeSelected()
{
    const int row = list.getSelectedRow();
    if (! juce::isPositiveAndBelow(row, (int) rows.size())) return i18n::tr("Selecciona un evento de la lista.");
    const auto r = rows[(size_t) row];
    auto next = current;
    if (r.tempo) next.tempos.erase(next.tempos.begin() + r.index); else next.signatures.erase(next.signatures.begin() + r.index);
    commit(std::move(next), i18n::tr("Mapa de tempo"));
    list.deselectAllRows();
    return {};
}

void TempoMapPanel::selectRow(int row) { list.selectRow(row); fillFields(row); }

void TempoMapPanel::selectedRowsChanged(int row) { fillFields(row); message.setText({}, juce::dontSendNotification); }

void TempoMapPanel::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow(row, (int) rows.size())) return;
    g.fillAll(selected ? selectedTrack : row % 2 ? panel : bg);
    const auto& r = rows[(size_t) row];
    const auto colour = selected ? activeInk : ink;
    if (r.tempo)
    {
        const auto& e = current.tempos[(size_t) r.index];
        text(g, i18n::tr("Tempo"), { 8, 0, 120, height }, 11, colour);
        text(g, barsBeats(view(), e.beat), { 130, 0, 130, height }, 11, colour);
        text(g, juce::String(e.bpm, 3) + " BPM", { 262, 0, 120, height }, 11, colour);
        text(g, e.ramp ? i18n::tr("Rampa") : i18n::tr("Salto"), { 384, 0, width - 390, height }, 11, selected ? activeInk : muted);
    }
    else
    {
        const auto& s = current.signatures[(size_t) r.index];
        text(g, i18n::tr("Tipo de compás"), { 8, 0, 120, height }, 11, colour);
        text(g, i18n::tr("Compás ") + juce::String(s.bar + 1), { 130, 0, 130, height }, 11, colour);
        text(g, juce::String(s.numerator) + " / " + juce::String(s.denominator), { 262, 0, 120, height }, 11, colour);
    }
}

void TempoMapPanel::paint(juce::Graphics& g)
{
    g.fillAll(panel);
    text(g, i18n::tr("Tempo inicial: ") + juce::String(baseBpm, 3) + i18n::tr(" BPM (barra de transporte)"), { 14, 8, getWidth() - 28, 22 }, 12, ink);
    text(g, i18n::tr("Rampa: el tempo cambia de forma lineal desde el punto anterior. Un tipo de compás empieza al inicio del compás indicado."),
         { 14, getHeight() - 58, getWidth() - 28, 20 }, 10, muted);
    const bool tempo = kind.getSelectedId() == 1;
    const int y = getHeight() - 124;
    text(g, i18n::tr("Posición"), { 150, y - 18, 120, 16 }, 10, muted);
    if (tempo) text(g, "BPM", { 282, y - 18, 80, 16 }, 10, muted);
    else { text(g, i18n::tr("Numerador"), { 282, y - 18, 80, 16 }, 10, muted); text(g, i18n::tr("Denominador"), { 352, y - 18, 90, 16 }, 10, muted); }
}

void TempoMapPanel::resized()
{
    const int y = getHeight() - 124;
    list.setBounds(14, 34, getWidth() - 28, juce::jmax(60, y - 58));
    kind.setBounds(14, y, 128, 26);
    position.setBounds(150, y, 124, 26);
    bpm.setBounds(282, y, 80, 26); ramp.setBounds(370, y, getWidth() - 384, 26);
    numerator.setBounds(282, y, 60, 26); denominator.setBounds(352, y, 70, 26);
    add.setBounds(14, y + 34, 100, 26); update.setBounds(120, y + 34, 100, 26); remove.setBounds(226, y + 34, 100, 26);
    message.setBounds(334, y + 34, getWidth() - 348, 26);
}
}

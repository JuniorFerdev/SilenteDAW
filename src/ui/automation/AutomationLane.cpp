#include "AutomationLane.h"
#include "../../i18n/Translations.h"
#include "../theme/Drawing.h"
#include <cmath>

namespace silente::ui
{
using namespace theme;
using domain::AutomationParameter;

namespace
{
constexpr double volumeTopDb = 12.0, floorDb = -60.0, sendTopDb = 6.0;
constexpr int sendItem = 10; // ComboBox ids: 1 volume, 2 pan, 10 + k send k
constexpr float hitRadius = 6.0f;
}

AutomationLane::AutomationLane(AudioEngine& e) : engine(e)
{
    setComponentID("automation-lane");
    setWantsKeyboardFocus(true);
    parameter.setComponentID("automation-parameter");
    read.setComponentID("automation-read"); write.setComponentID("automation-write");
    for (auto* c : std::initializer_list<juce::Component*> { &parameter, &read, &write }) addAndMakeVisible(c);
    for (auto* b : { &read, &write }) b->setClickingTogglesState(true);
    read.setColour(juce::TextButton::buttonOnColourId, automationRead.withAlpha(.8f));
    write.setColour(juce::TextButton::buttonOnColourId, automationWrite.withAlpha(.8f));
    read.setTooltip(i18n::tr("Leer automatización: volumen, panorama y envíos siguen sus curvas"));
    write.setTooltip(i18n::tr("Escribir automatización: con el transporte parado, mover un control escribe un punto en el cursor"));
    parameter.setTooltip(i18n::tr("Parámetro automatizado"));
    parameter.onChange = [this]
    {
        if (syncing) return;
        const int id = parameter.getSelectedId();
        setTarget(id >= sendItem ? domain::AutomationTarget { AutomationParameter::send, id - sendItem }
                 : id == 2 ? domain::AutomationTarget { AutomationParameter::pan } : domain::AutomationTarget {});
    };
    read.onClick = [this]
    {
        if (auto* t = current()) { if (beforeEdit) beforeEdit(); app::setReadMode(engine, *t, read.getToggleState()); if (changed) changed(); refresh(); }
    };
    write.onClick = [this]
    {
        if (auto* t = current()) { if (beforeEdit) beforeEdit(); app::setWriteMode(engine, *t, write.getToggleState()); if (changed) changed(); refresh(); }
    };
    refresh();
    startTimerHz(30);
}

AutomationLane::~AutomationLane() = default;

Track* AutomationLane::current() const
{
    if (track < 0 || track >= (int) engine.tracks.size()) return nullptr;
    auto* t = engine.tracks[(size_t) track].get();
    return t->hasAudioChannel() ? t : nullptr;
}

void AutomationLane::setTrack(int index)
{
    if (index != track) selected = -1;
    track = index;
    refresh();
    repaint();
}

void AutomationLane::setTarget(domain::AutomationTarget t)
{
    if (! domain::validTarget(t)) return;
    if (! (t == target)) selected = -1;
    target = t;
    refresh();
    repaint();
}

void AutomationLane::refresh()
{
    const juce::ScopedValueSetter<bool> guard(syncing, true);
    parameter.clear(juce::dontSendNotification);
    parameter.addItem(i18n::tr("Volumen"), 1);
    parameter.addItem(i18n::tr("Panorama"), 2);
    auto* t = current();
    for (int k = 0; k < Track::sendCount; ++k)
    {
        juce::String name = i18n::tr("Envío ") + juce::String(k + 1);
        if (t != nullptr)
            if (auto fx = t->sends[(size_t) k].target.isEmpty() ? nullptr : engine.findTrack(t->sends[(size_t) k].target)) name << juce::String::fromUTF8(" · ") << fx->name;
        parameter.addItem(name, sendItem + k);
    }
    parameter.setSelectedId(target.parameter == AutomationParameter::send ? sendItem + target.send : target.parameter == AutomationParameter::pan ? 2 : 1, juce::dontSendNotification);
    read.setToggleState(t != nullptr && t->automation.read, juce::dontSendNotification);
    write.setToggleState(t != nullptr && t->automation.write, juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> { &parameter, &read, &write }) c->setEnabled(t != nullptr);
}

void AutomationLane::resized()
{
    auto h = juce::Rectangle<int>(0, 0, header, getHeight()).reduced(8, 6);
    h.removeFromTop(22); // track name
    parameter.setBounds(h.removeFromTop(24));
    h.removeFromTop(6);
    auto row = h.removeFromTop(24);
    read.setBounds(row.removeFromLeft(34)); row.removeFromLeft(4); write.setBounds(row.removeFromLeft(34));
}

void AutomationLane::view(double& pixelsPerSecond, double& scrollSeconds) const
{
    pixelsPerSecond = 75.0; scrollSeconds = 0.0;
    if (timeline) timeline(pixelsPerSecond, scrollSeconds);
    pixelsPerSecond = juce::jmax(1.0, pixelsPerSecond);
}

float AutomationLane::beatToX(double beat) const
{
    double pps = 0, scroll = 0; view(pps, scroll);
    return (float) (header + (engine.tempoView().secondsAt(beat) - scroll) * pps);
}

double AutomationLane::xToBeat(float x) const
{
    double pps = 0, scroll = 0; view(pps, scroll);
    return engine.tempoView().beatsAt(juce::jmax(0.0, scroll + ((double) x - header) / pps));
}

double AutomationLane::toDisplay(double value) const
{
    switch (target.parameter)
    {
        case AutomationParameter::pan: return (value + 1.0) * 0.5;
        case AutomationParameter::send: return (juce::jlimit(floorDb, sendTopDb, value) - floorDb) / (sendTopDb - floorDb);
        case AutomationParameter::volume: break;
    }
    const double db = value <= 0.0 ? floorDb : juce::jlimit(floorDb, volumeTopDb, 20.0 * std::log10(value));
    return (db - floorDb) / (volumeTopDb - floorDb);
}

double AutomationLane::fromDisplay(double p) const
{
    p = juce::jlimit(0.0, 1.0, p);
    switch (target.parameter)
    {
        case AutomationParameter::pan: return p * 2.0 - 1.0;
        case AutomationParameter::send: return floorDb + p * (sendTopDb - floorDb);
        case AutomationParameter::volume: break;
    }
    return p <= 0.0 ? 0.0 : std::pow(10.0, (floorDb + p * (volumeTopDb - floorDb)) / 20.0);
}

float AutomationLane::valueToY(double value) const
{
    const float h = (float) juce::jmax(1, getHeight() - top - bottomMargin);
    return (float) top + h * (float) (1.0 - toDisplay(value));
}

double AutomationLane::yToValue(float y) const
{
    const double h = (double) juce::jmax(1, getHeight() - top - bottomMargin);
    return fromDisplay(1.0 - (y - top) / h);
}

int AutomationLane::pointAt(juce::Point<float> p) const
{
    const auto* t = current();
    if (t == nullptr) return -1;
    const auto& points = t->automation.curve(target).points;
    for (int i = (int) points.size() - 1; i >= 0; --i)
        if (juce::Point<float>(beatToX(points[(size_t) i].beat), valueToY(points[(size_t) i].value)).getDistanceFrom(p) <= hitRadius) return i;
    return -1;
}

void AutomationLane::paint(juce::Graphics& g)
{
    // Message-thread read of the model without modelLock (as the other views): only this thread edits it.
    g.fillAll(automationLane);
    g.setColour(panel); g.fillRect(0, 0, header, getHeight());
    g.setColour(line); g.drawVerticalLine(header - 1, 0.0f, (float) getHeight());
    const auto* t = current();
    if (t == nullptr)
    {
        text(g, i18n::tr("Selecciona una pista con canal de mezcla para ver su automatización"), getLocalBounds().withTrimmedLeft(header), 13, muted, juce::Justification::centred);
        return;
    }
    text(g, t->name, { 8, 4, header - 16, 20 }, 12, t->colour.brighter(.2f));
    const auto& curve = t->automation.curve(target);
    const auto area = juce::Rectangle<float>((float) header, 0.0f, (float) (getWidth() - header), (float) getHeight());
    juce::Graphics::ScopedSaveState clip(g);
    g.reduceClipRegion(area.toNearestInt());
    // Reference line: 0 dB for volume and sends, centre for pan.
    const double reference = target.parameter == AutomationParameter::pan ? 0.0 : target.parameter == AutomationParameter::volume ? 1.0 : 0.0;
    g.setColour(gridBar.withAlpha(.6f)); g.drawHorizontalLine((int) valueToY(reference), area.getX(), area.getRight());
    const juce::Colour curveColour = t->automation.read ? automationCurve : automationCurve.withAlpha(.45f);
    if (curve.empty())
    {   // No points: the static value, dashed.
        const float y = valueToY(app::staticValue(*t, target));
        const float dashes[] { 6.0f, 4.0f };
        g.setColour(curveColour.withAlpha(.6f)); g.drawDashedLine({ area.getX(), y, area.getRight(), y }, dashes, 2, 1.2f);
        text(g, i18n::tr("Sin puntos · clic para añadir"), { header + 10, (int) y - 20, 260, 16 }, 11, muted);
    }
    else
    {
        juce::Path path;
        for (int step = 0; (float) step * 2.0f <= area.getWidth() + 2.0f; ++step)
        {
            const float x = area.getX() + (float) step * 2.0f, y = valueToY(domain::evaluate(curve, xToBeat(x), 0.0));
            if (step == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
        }
        g.setColour(curveColour); g.strokePath(path, juce::PathStrokeType(1.6f));
        for (int i = 0; i < (int) curve.points.size(); ++i)
        {
            const auto& p = curve.points[(size_t) i];
            const auto r = juce::Rectangle<float>(8.0f, 8.0f).withCentre({ beatToX(p.beat), valueToY(p.value) });
            g.setColour(i == selected ? ink : automationLane); g.fillEllipse(r);
            g.setColour(curveColour); g.drawEllipse(r, 1.4f);
        }
    }
    const float cursor = beatToX(engine.tempoView().beatsAt(engine.positionSeconds.load()));
    if (cursor >= area.getX()) { g.setColour(ink.withAlpha(.6f)); g.drawVerticalLine((int) cursor, 0.0f, (float) getHeight()); }
}

void AutomationLane::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    dragging = moved = false; gestureRecorded = false;
    auto* t = current();
    if (t == nullptr || e.x < header) return;
    const int hit = pointAt(e.position);
    if (hit >= 0 && e.mods.isAltDown())
    {
        if (beforeEdit) beforeEdit();
        app::removePoint(engine, *t, target, hit);
        selected = -1;
        if (changed) changed();
        repaint();
        return;
    }
    if (hit >= 0) selected = hit;
    else
    {   // New point under the mouse; the same gesture can drag it.
        if (beforeEdit) beforeEdit();
        gestureRecorded = true;
        selected = app::addPoint(engine, *t, target, xToBeat(e.position.x), yToValue(e.position.y));
        if (changed) changed();
    }
    const auto& points = t->automation.curve(target).points;
    if (juce::isPositiveAndBelow(selected, (int) points.size())) { dragging = true; dragStart = e.position; dragOrigin = points[(size_t) selected]; }
    repaint();
}

void AutomationLane::mouseDrag(const juce::MouseEvent& e)
{
    auto* t = current();
    if (! dragging || t == nullptr || e.getDistanceFromDragStart() < 1) return;
    if (! gestureRecorded) { if (beforeEdit) beforeEdit(); gestureRecorded = true; }
    const double beat = xToBeat(beatToX(dragOrigin.beat) + e.position.x - dragStart.x);
    const double value = yToValue(valueToY(dragOrigin.value) + e.position.y - dragStart.y);
    app::movePoint(engine, *t, target, selected, beat, value);
    moved = true;
    repaint();
}

void AutomationLane::mouseUp(const juce::MouseEvent&)
{
    if (dragging && moved && changed) changed();
    dragging = moved = false;
}

void AutomationLane::deleteSelected()
{
    auto* t = current();
    if (t == nullptr || ! juce::isPositiveAndBelow(selected, (int) t->automation.curve(target).points.size())) return;
    if (beforeEdit) beforeEdit();
    app::removePoint(engine, *t, target, selected);
    selected = -1;
    if (changed) changed();
    repaint();
}

bool AutomationLane::keyPressed(const juce::KeyPress& k)
{
    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) { deleteSelected(); return true; }
    return false;
}

void AutomationLane::timerCallback()
{
    if (! isShowing()) return;
    if (auto* t = current())
        if (read.getToggleState() != t->automation.read || write.getToggleState() != t->automation.write) refresh();
    if (engine.playing.load()) repaint();
}
}

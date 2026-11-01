#include "KeyCommandsPanel.h"
#include "../../i18n/Translations.h"
#include "../theme/Drawing.h"

namespace silente::ui
{
using namespace theme;

KeyCommandsPanel::KeyCommandsPanel(std::function<bool(const juce::String&)> a) : available(std::move(a))
{
    setLookAndFeel(&look);
    addAndMakeVisible(search);
    search.setTextToShowWhenEmpty(i18n::tr("Buscar comando o tecla"), muted);
    search.onTextChange = [this] { scroll = 0; update(); };
    setSize(720, 620);
    update();
}

void KeyCommandsPanel::update()
{
    rows.clear();
    const auto query = search.getText().trim();
    for (const auto& c : app::keyCommands())
        if (query.isEmpty() || c.name().containsIgnoreCase(query) || c.category().containsIgnoreCase(query) || app::describeKey(c).containsIgnoreCase(query))
            rows.push_back(&c);
    repaint();
}

void KeyCommandsPanel::resized() { search.setBounds(16, 34, getWidth() - 32, 26); }

void KeyCommandsPanel::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int visible = (getHeight() - top - 30) / rowHeight;
    scroll = juce::jlimit(0, juce::jmax(0, (int) rows.size() + 40 - visible), scroll + (w.deltaY > 0 ? -3 : 3));
    repaint();
}

void KeyCommandsPanel::paint(juce::Graphics& g)
{
    g.fillAll(panel);
    int bound = 0;
    for (const auto* c : rows) bound += c->id.isNotEmpty() && available(c->id) ? 1 : 0;
    text(g, i18n::tr("Comandos de teclado por defecto de Cubase Pro 15") + "  ·  " + juce::String(bound) + "/" + juce::String((int) rows.size()) + i18n::tr(" disponibles en Silente"),
         { 16, 6, getWidth() - 32, 24 }, 12, ink);
    g.saveState();
    g.reduceClipRegion(0, top, getWidth(), getHeight() - top - 26);
    int y = top - scroll * rowHeight;
    juce::String lastCategory;
    for (const auto* c : rows)
    {
        if (c->category() != lastCategory)
        {
            lastCategory = c->category();
            g.setColour(raised); g.fillRect(12, y + 2, getWidth() - 24, rowHeight - 4);
            text(g, lastCategory, { 20, y, getWidth() - 40, rowHeight }, 11, ink);
            y += rowHeight;
        }
        const bool ok = c->id.isNotEmpty() && available(c->id);
        text(g, c->name(), { 28, y, getWidth() - 300, rowHeight }, 11, ok ? ink : muted);
        text(g, app::describeKey(*c), { getWidth() - 270, y, 150, rowHeight }, 11, ok ? ink : muted);
        text(g, ok ? juce::String::fromUTF8("✓") : juce::String::fromUTF8("—"), { getWidth() - 100, y, 80, rowHeight }, 12, ok ? mint : muted, juce::Justification::centred);
        y += rowHeight;
    }
    g.restoreState();
    text(g, i18n::tr("✓ funciona en Silente  ·  — aún no disponible  ·  Num = teclado numérico (en portátil: / ciclo, * grabar, + y − avanzar/rebobinar)"),
         { 16, getHeight() - 24, getWidth() - 32, 20 }, 10, muted);
}
}

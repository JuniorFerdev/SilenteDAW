#include "Drawing.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <unordered_map>

namespace silente::ui::theme
{
namespace
{
// Shaping text (HarfBuzz) on every frame was the main cost of drawing the window, above all under
// Rosetta. A laid-out line is kept per text, size, box and justification and drawn again from
// the cache; it is shaped again only when one of them changes. Message thread only.
// The cache holds fonts, so it must go before JUCE closes its typeface system: a static object
// destroyed at exit crashed every quit (CoreTextTypeface destructor after JUCE shutdown).
// DeletedAtShutdown deletes it during JUCE's orderly shutdown instead.
struct LayoutCache final : juce::DeletedAtShutdown
{
    std::unordered_map<juce::String, juce::GlyphArrangement> lines;
    std::unordered_map<juce::String, float> widths;
    static inline LayoutCache* instance = nullptr;
    ~LayoutCache() override { instance = nullptr; }
    void trim() { if (lines.size() > 6000) lines.clear(); if (widths.size() > 6000) widths.clear(); }
};
LayoutCache& cache() { if (LayoutCache::instance == nullptr) LayoutCache::instance = new LayoutCache(); return *LayoutCache::instance; }
juce::Font plainFont(float size) { return juce::Font(juce::FontOptions(size, juce::Font::plain)); }
}

void font(juce::Graphics& g, float size, bool bold) { g.setFont(juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain))); }

void text(juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, float size, juce::Colour c, juce::Justification j)
{
    font(g, size); g.setColour(c); // as before: later drawing may rely on this font and colour
    if (t.isEmpty() || r.isEmpty() || ! g.getClipBounds().intersects(r)) return; // outside the area being repainted
    auto& lines = cache().lines;
    const auto key = t + juce::String::charToString(0x1f) + juce::String(size) + "/" + juce::String(r.getWidth()) + "x" + juce::String(r.getHeight()) + "/" + juce::String(j.getFlags());
    auto it = lines.find(key);
    if (it == lines.end())
    {
        cache().trim();
        juce::GlyphArrangement line;
        line.addCurtailedLineOfText(plainFont(size), t, 0.0f, 0.0f, (float) r.getWidth(), true);
        line.justifyGlyphs(0, line.getNumGlyphs(), 0.0f, 0.0f, (float) r.getWidth(), (float) r.getHeight(), j);
        it = lines.emplace(key, std::move(line)).first;
    }
    it->second.draw(g, juce::AffineTransform::translation((float) r.getX(), (float) r.getY()));
}

float textWidth(const juce::String& t, float size)
{
    auto& widths = cache().widths;
    const auto key = t + juce::String::charToString(0x1f) + juce::String(size);
    if (auto it = widths.find(key); it != widths.end()) return it->second;
    cache().trim();
    return widths[key] = juce::GlyphArrangement::getStringWidth(plainFont(size), t);
}

void logo(juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour(accent); g.fillRoundedRectangle(r, 10);
    juce::Path p; p.startNewSubPath(26, 9); p.cubicTo(8, 3, 4, 19, 17, 20); p.cubicTo(31, 21, 27, 37, 10, 30);
    g.setColour(bg); g.strokePath(p, juce::PathStrokeType(4, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::scale(r.getWidth()/38, r.getHeight()/38).translated(r.getX(), r.getY()));
}
}

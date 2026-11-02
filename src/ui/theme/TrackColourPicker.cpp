#include "../../i18n/Translations.h"
#include "TrackColourPicker.h"
#include "Colours.h"
#include "SilenteLookAndFeel.h"
#include <array>

namespace silente::ui::theme
{
namespace
{
// The whole range at once, like Cubase's project colours: 16 hues in five strengths (pastel to
// deep) and a row of greys.
constexpr int columns = 16, rows = 6;
const std::array<juce::uint32, columns * rows>& palette()
{
    static const auto colours = []
    {
        std::array<juce::uint32, columns * rows> c {};
        const float strengths[5][2] { { .38f, .96f }, { .55f, .92f }, { .70f, .85f }, { .80f, .70f }, { .85f, .52f } };
        for (int row = 0; row < 5; ++row)
            for (int col = 0; col < columns; ++col)
                c[(size_t) (row * columns + col)] = juce::Colour::fromHSV((float) col / columns, strengths[row][0], strengths[row][1], 1.0f).getARGB();
        for (int col = 0; col < columns; ++col) c[(size_t) (5 * columns + col)] = juce::Colour::greyLevel(.18f + .78f * (float) col / (columns - 1)).getARGB();
        return c;
    }();
    return colours;
}
class Picker final : public juce::Component
{
public:
    Picker(juce::Colour initial, std::function<void(juce::Colour)> callback)
        : selector(juce::ColourSelector::showColourAtTop | juce::ColourSelector::showSliders | juce::ColourSelector::showColourspace), apply(std::move(callback))
    {
        setLookAndFeel(&look);
        setSize(360, 448);
        selector.setName(i18n::tr("Color personalizado de pista")); selector.setCurrentColour(initial, juce::dontSendNotification);
        selector.setColour(juce::ColourSelector::backgroundColourId, panel); addAndMakeVisible(selector);
        addAndMakeVisible(confirm); confirm.setButtonText(i18n::tr("Aplicar color personalizado"));
        confirm.onClick = [this] { commit(selector.getCurrentColour()); };
    }
    ~Picker() override { setLookAndFeel(nullptr); }
    void resized() override { selector.setBounds(8, 166, getWidth() - 16, 233); confirm.setBounds(15, 410, getWidth() - 30, 27); }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(panel); g.setColour(ink); g.setFont(juce::Font(juce::FontOptions(14, juce::Font::bold)));
        g.drawText(i18n::tr("Color de pista"), 15, 8, 280, 26, juce::Justification::centredLeft);
        for (int i = 0; i < static_cast<int>(palette().size()); ++i)
        {
            auto bounds = swatch(i); g.setColour(juce::Colour(palette()[static_cast<size_t>(i)])); g.fillRoundedRectangle(bounds.toFloat(), 2);
            if (selector.getCurrentColour().getARGB() == palette()[static_cast<size_t>(i)]) { g.setColour(ink); g.drawRect(bounds.expanded(2), 1); }
        }
    }
    void mouseDown(const juce::MouseEvent& event) override
    {
        for (int i = 0; i < static_cast<int>(palette().size()); ++i)
            if (swatch(i).contains(event.getPosition())) { commit(juce::Colour(palette()[static_cast<size_t>(i)])); return; }
    }
private:
    SilenteLookAndFeel look;
    juce::ColourSelector selector;
    juce::TextButton confirm;
    std::function<void(juce::Colour)> apply;
    static juce::Rectangle<int> swatch(int i) { return {12 + (i % columns) * 21, 40 + (i / columns) * 20, 18, 16}; }
    void commit(juce::Colour colour)
    {
        auto callback = std::move(apply);
        if (callback) callback(colour.withAlpha(1.0f));
        if (auto* box = findParentComponentOfClass<juce::CallOutBox>()) box->dismiss();
    }
};
}
void showTrackColourPicker(juce::Rectangle<int> anchor, juce::Colour initial, std::function<void(juce::Colour)> apply)
{
    juce::CallOutBox::launchAsynchronously(std::make_unique<Picker>(initial, std::move(apply)), anchor, nullptr);
}
}

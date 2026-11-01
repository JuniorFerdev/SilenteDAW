#include "../../i18n/Translations.h"
#include "TrackColourPicker.h"
#include "Colours.h"
#include "SilenteLookAndFeel.h"
#include <array>

namespace silente::ui::theme
{
namespace
{
constexpr std::array<juce::uint32, 24> palette {
    0xffdf6262, 0xffe58b53, 0xffe3b653, 0xffd2ce50, 0xff9bc64e, 0xff57bd77, 0xff4fbaa1, 0xff51b7c8,
    0xff639cdd, 0xff727be0, 0xffa276da, 0xffc97ac1, 0xffb94f69, 0xffb87f44, 0xffa1a445, 0xff668e58,
    0xff477f79, 0xff4b7d9d, 0xff665f9e, 0xff9e608e, 0xff7b8d9b, 0xffa6afb5, 0xffc1c8ce, 0xffe0e3e6
};
class Picker final : public juce::Component
{
public:
    Picker(juce::Colour initial, std::function<void(juce::Colour)> callback)
        : selector(juce::ColourSelector::showColourAtTop | juce::ColourSelector::showSliders | juce::ColourSelector::showColourspace), apply(std::move(callback))
    {
        setLookAndFeel(&look);
        setSize(320, 405);
        selector.setName(i18n::tr("Color personalizado de pista")); selector.setCurrentColour(initial, juce::dontSendNotification);
        selector.setColour(juce::ColourSelector::backgroundColourId, panel); addAndMakeVisible(selector);
        addAndMakeVisible(confirm); confirm.setButtonText(i18n::tr("Aplicar color personalizado"));
        confirm.onClick = [this] { commit(selector.getCurrentColour()); };
    }
    ~Picker() override { setLookAndFeel(nullptr); }
    void resized() override { selector.setBounds(8, 123, getWidth() - 16, 233); confirm.setBounds(15, 367, getWidth() - 30, 27); }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(panel); g.setColour(ink); g.setFont(juce::Font(juce::FontOptions(14, juce::Font::bold)));
        g.drawText(i18n::tr("Color de pista"), 15, 8, 280, 26, juce::Justification::centredLeft);
        for (int i = 0; i < static_cast<int>(palette.size()); ++i)
        {
            auto bounds = swatch(i); g.setColour(juce::Colour(palette[static_cast<size_t>(i)])); g.fillRoundedRectangle(bounds.toFloat(), 2);
            if (selector.getCurrentColour().getARGB() == palette[static_cast<size_t>(i)]) { g.setColour(ink); g.drawRect(bounds.expanded(2), 1); }
        }
    }
    void mouseDown(const juce::MouseEvent& event) override
    {
        for (int i = 0; i < static_cast<int>(palette.size()); ++i)
            if (swatch(i).contains(event.getPosition())) { commit(juce::Colour(palette[static_cast<size_t>(i)])); return; }
    }
private:
    SilenteLookAndFeel look;
    juce::ColourSelector selector;
    juce::TextButton confirm;
    std::function<void(juce::Colour)> apply;
    static juce::Rectangle<int> swatch(int i) { return {15 + (i % 8) * 37, 42 + (i / 8) * 26, 30, 20}; }
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

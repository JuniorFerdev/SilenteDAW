#pragma once
#include "../theme/SilenteLookAndFeel.h"
#include <array>
#include <functional>

namespace silente::ui
{
// «Editor(es) de fundido» (Cubase manual p. 401): fade-in and fade-out of the selected event side
// by side, each with its curve, the curve-shape buttons and the length; Por defecto stores the
// settings used by «Aplicar fundido de entrada/salida estándar».
class FadeEditor final : public juce::Component
{
public:
    struct Settings { double fadeIn = 0.0, fadeOut = 0.0; int inShape = 0, outShape = 0; };
    FadeEditor(Settings initial, double eventLength, std::function<void(Settings)> apply, std::function<void(Settings)> makeDefault);
    ~FadeEditor() override { setLookAndFeel(nullptr); }
    void paint(juce::Graphics&) override;
    void resized() override;
    Settings current() const { return settings; }
    void setShape(bool fadeIn, int shape);
    void setLength(bool fadeIn, double seconds);
    void pressApply() { if (apply) apply(settings); }
    void pressDefault() { if (makeDefault) makeDefault(settings); }

private:
    theme::SilenteLookAndFeel look;
    Settings settings;
    double length;
    std::function<void(Settings)> apply, makeDefault;
    std::array<std::array<juce::TextButton, 5>, 2> shapes;
    std::array<juce::Slider, 2> lengths;
    juce::TextButton defaults, applyButton;
    juce::Rectangle<int> curveArea(int side) const;
    void sync();
};

void showFadeEditor(juce::Component& parent, FadeEditor::Settings initial, double eventLength,
                    std::function<void(FadeEditor::Settings)> apply, std::function<void(FadeEditor::Settings)> makeDefault);
}

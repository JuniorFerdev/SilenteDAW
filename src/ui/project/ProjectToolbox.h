#pragma once
#include "../theme/ToolIcons.h"
#include "../theme/SilenteLookAndFeel.h"

namespace silente::ui
{
struct ProjectToolDefinition
{
    int id;
    theme::ToolIcon icon;
    juce::String name, shortcut, unavailableReason;
    bool available() const { return unavailableReason.isEmpty(); }
};
// IDs 1–9 preserve the existing arrangement tools; 10–12 are reserved for missing tools.
std::vector<ProjectToolDefinition> projectToolDefinitions();
// Traverse Silente's main toolbar (numeric tool order, then Color), skipping
// unavailable modes. The popup's visual order stays the one in the manual.
int adjacentProjectTool(int activeTool, int direction);
bool opensProjectToolbox(bool showByDefault, juce::ModifierKeys);

class ProjectToolbox final : public juce::Component
{
public:
    ProjectToolbox(int activeTool, std::function<void(int)> choose, bool showOnRightClick = true);
    ~ProjectToolbox() override { setLookAndFeel(nullptr); }
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
private:
    void chooseAndDismiss(int tool);
    theme::SilenteLookAndFeel look;
    juce::TooltipWindow tips { this, 350 };
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    std::function<void(int)> chosen;
    int activeTool;
    juce::String contextHint;
};
}

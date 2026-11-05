#pragma once
#include "../../PluginManager.h"
#include "PluginSlotButton.h"

namespace silente::ui
{
// Ten numbered effect slots, independent of the instrument, shared by every channel view.
class InsertRack final : public juce::Component
{
public:
    InsertRack(AudioEngine&,PluginManager&,const juce::String& componentId,bool bypassButtons=false);
    void setTrack(int);
    void refresh();
    void resized() override;
    void revealSlot(int);
private:
    AudioEngine& engine;PluginManager& plugins;
    juce::String trackId;
    bool withBypass;
    std::array<PluginSlotButton,Track::insertCount> slots;
    std::array<juce::TextButton,Track::insertCount> bypass;
    juce::Component content;
    juce::Viewport viewport;
    int trackIndex() const;
};
}

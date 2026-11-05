#include "InsertRack.h"
#include "../theme/Colours.h"

namespace silente::ui
{
InsertRack::InsertRack(AudioEngine& e,PluginManager& p,const juce::String& id,bool withButtons)
    :engine(e),plugins(p),withBypass(withButtons)
{
    setComponentID(id);viewport.setComponentID(id+"-scroll");addAndMakeVisible(viewport);
    viewport.setViewedComponent(&content,false);viewport.setScrollBarsShown(true,false);viewport.setScrollBarThickness(9);
    viewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
    for(int n=0;n<Track::insertCount;++n)
    {
        auto& slot=slots[(size_t)n];content.addAndMakeVisible(slot);slot.setComponentID(id+"-slot-"+juce::String(n+1));
        slot.onClick=[this,n]{const int index=trackIndex();if(index>=0)plugins.showEffectEditor(index,n,&slots[(size_t)n]);};
        slot.onOptions=[this,n]{const int index=trackIndex();if(index>=0)plugins.showEffectSlot(index,n,&slots[(size_t)n]);};
        auto& button=bypass[(size_t)n];content.addChildComponent(button);button.setComponentID(id+"-bypass-"+juce::String(n+1));
        button.setButtonText(juce::String::fromUTF8("Ø"));button.setTooltip(i18n::tr("Bypass del inserto"));button.setClickingTogglesState(true);
        button.onClick=[this,n]{const int index=trackIndex();if(index>=0){const int slotIndex=effectSlotIndex(*engine.tracks[(size_t)index],n);if(slotIndex>=0)plugins.toggleBypass(index,slotIndex);}refresh();};
    }
}
int InsertRack::trackIndex() const
{
    for(int n=0;n<(int)engine.tracks.size();++n)if(engine.tracks[(size_t)n]->id==trackId&&engine.tracks[(size_t)n]->hasAudioChannel())return n;return -1;
}
void InsertRack::setTrack(int index)
{
    const auto id=juce::isPositiveAndBelow(index,(int)engine.tracks.size())?engine.tracks[(size_t)index]->id:juce::String();
    if(id!=trackId){trackId=id;viewport.setViewPosition(0,0);}refresh();
}
void InsertRack::refresh()
{
    const int index=trackIndex();const auto track=index>=0?engine.tracks[(size_t)index]:nullptr;
    for(int n=0;n<Track::insertCount;++n)
    {
        const int slotIndex=track?effectSlotIndex(*track,n):-1;const auto slot=slotIndex>=0?track->plugins[(size_t)slotIndex]:nullptr;
        auto& button=slots[(size_t)n];button.setEnabled(track!=nullptr);
        button.setButtonText(juce::String(n+1).paddedLeft('0',2)+"  "+(slot?slot->description.name:juce::String()));
        button.setColour(juce::TextButton::buttonColourId,slot?theme::insertLoaded:theme::bg.brighter(.06f));
        button.setAlpha(slot&&slot->bypassed.load()?.45f:1.0f);
        button.setTooltip(juce::String(n+1)+" / "+juce::String(Track::insertCount)+" · "+(slot?(slot->error.isNotEmpty()?slot->error:i18n::tr("Abrir editor del plugin")):i18n::tr("Añadir efecto de inserción")));
        auto& bypassButton=bypass[(size_t)n];bypassButton.setVisible(withBypass);bypassButton.setEnabled(slot!=nullptr);bypassButton.setToggleState(slot&&slot->bypassed.load(),juce::dontSendNotification);
    }
}
void InsertRack::resized()
{
    const int scroll=viewport.getViewPositionY();viewport.setBounds(getLocalBounds());
    const int width=juce::jmax(1,getWidth()-viewport.getScrollBarThickness());content.setSize(width,25*Track::insertCount);
    for(int n=0;n<Track::insertCount;++n)
    {
        auto row=juce::Rectangle<int>(0,n*25,width,22);if(withBypass){bypass[(size_t)n].setBounds(row.removeFromRight(26));row.removeFromRight(3);}slots[(size_t)n].setBounds(row);
    }
    viewport.setViewPosition(0,scroll);
}
void InsertRack::revealSlot(int n){viewport.setViewPosition(0,juce::jlimit(0,Track::insertCount-1,n)*25);}
}

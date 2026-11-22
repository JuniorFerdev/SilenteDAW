#include "MidiTools.h"
#include "../../i18n/Translations.h"
#include "../../app/KeyCommands.h"
#include "../theme/Drawing.h"

namespace silente::ui
{
std::vector<ProjectToolDefinition> midiToolDefinitions()
{
    using I=theme::ToolIcon;
    return {
        {midiToolSelect,I::select,i18n::tr("Seleccionar"),"1",{}},
        {midiToolDraw,I::pencil,i18n::tr("Dibujar"),"8",{}},
        {midiToolErase,I::erase,i18n::tr("Borrar"),"5",{}},
        {midiToolSplit,I::scissors,i18n::tr("Dividir"),"3",{}},
        {midiToolGlue,I::glue,i18n::tr("Pegar"),"4",{}},
        {midiToolMute,I::muteTool,i18n::tr("Enmudecer"),"7",{}}
    };
}
int midiToolForKey(const juce::KeyPress& key,int active)
{
    const auto* command=app::findKeyCommand(key);if(!command)return 0;
    const auto& id=command->id;
    if(id=="toolNext")return adjacentTool(midiToolDefinitions(),active,1);
    if(id=="toolPrevious")return adjacentTool(midiToolDefinitions(),active,-1);
    if(id=="toolSelect")return midiToolSelect;
    if(id=="toolDraw")return midiToolDraw;
    if(id=="toolErase")return midiToolErase;
    if(id=="toolSplit")return midiToolSplit;
    if(id=="toolGlue")return midiToolGlue;
    if(id=="toolMute")return midiToolMute;
    return 0;
}
std::vector<MidiTransposeDefinition> midiTransposeDefinitions()
{
    using I=theme::ToolIcon;
    return {
        {"midi-transpose-up",I::transposeUp,i18n::tr("Hacia arriba"),i18n::tr("Transpone la selección media nota hacia arriba"),1},
        {"midi-transpose-down",I::transposeDown,i18n::tr("Hacia abajo"),i18n::tr("Transpone la selección media nota hacia abajo"),-1},
        {"midi-transpose-octave-up",I::transposeOctaveUp,i18n::tr("Desplazar más hacia arriba"),i18n::tr("Transpone la selección una octava hacia arriba"),12},
        {"midi-transpose-octave-down",I::transposeOctaveDown,i18n::tr("Desplazar más hacia abajo"),i18n::tr("Transpone la selección una octava hacia abajo"),-12}
    };
}
MidiToolBar::MidiToolBar()
{
    setComponentID("midi-toolbar");
    for(const auto& tool:midiToolDefinitions())
    {
        auto button=std::make_unique<juce::TextButton>();
        theme::setButtonIcon(*button,tool.icon);button->setName(tool.name);
        button->setComponentID("midi-tool-"+juce::String(tool.id));
        button->setTooltip(tool.name+" · "+tool.shortcut);
        button->onClick=[this,id=tool.id]{setActiveTool(id);if(chooseTool)chooseTool(id);};
        addAndMakeVisible(*button);buttons.push_back(std::move(button));ids.push_back(tool.id);
    }
    for(const auto& step:midiTransposeDefinitions())
    {
        auto button=std::make_unique<juce::TextButton>();
        theme::setButtonIcon(*button,step.icon);button->setName(step.name);
        button->setComponentID(step.componentId);
        button->setTooltip(step.name+" · "+step.description);
        button->onClick=[this,semitones=step.semitones]{if(transpose)transpose(semitones);};
        addAndMakeVisible(*button);transposeButtons.push_back(std::move(button));
    }
    setActiveTool(midiToolSelect);
}
void MidiToolBar::setActiveTool(int tool)
{
    for(size_t i=0;i<buttons.size();++i)buttons[i]->setToggleState(ids[i]==tool,juce::dontSendNotification);
}
void MidiToolBar::paint(juce::Graphics& g){g.fillAll(theme::panel);g.setColour(theme::line);g.drawHorizontalLine(getHeight()-1,0.f,(float)getWidth());}
void MidiToolBar::resized()
{
    int x=6;for(auto& button:buttons){button->setBounds(x,2,26,getHeight()-5);x+=28;}
    x+=14; // Transpose palette, separated from the tools.
    for(auto& button:transposeButtons){button->setBounds(x,2,26,getHeight()-5);x+=28;}
}
}

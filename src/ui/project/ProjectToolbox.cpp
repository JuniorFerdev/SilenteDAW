#include "ProjectToolbox.h"
#include "../../i18n/Translations.h"
#include "../../app/KeyCommands.h"
#include "../theme/Drawing.h"
#include <algorithm>

namespace silente::ui
{
std::vector<ProjectToolDefinition> projectToolDefinitions()
{
    using I=theme::ToolIcon;
    return {
        {1,I::select,i18n::tr("Seleccionar"),"1",{}},
        {2,I::rangeTool,i18n::tr("Selección de rango"),"2",{}},
        {8,I::pencil,i18n::tr("Dibujar"),"8",{}},
        {5,I::erase,i18n::tr("Borrar"),"5",{}},
        {3,I::scissors,i18n::tr("Dividir"),"3",{}},
        {4,I::glue,i18n::tr("Pegar"),"4",{}},
        {7,I::muteTool,i18n::tr("Enmudecer"),"7",{}},
        {6,I::zoomTool,i18n::tr("Zoom"),"6",{}},
        {10,I::compTool,i18n::tr("Comp"),{},i18n::tr("Pendiente: carriles y ensamblaje de tomas.")},
        {11,I::timeWarpTool,i18n::tr("Time Warp"),{},i18n::tr("Pendiente: mapa de tempo y Time Warp.")},
        {12,I::lineTool,i18n::tr("Línea"),{},i18n::tr("Pendiente: dibujo de series de eventos.")},
        {9,I::playTool,i18n::tr("Iniciar"),"9",{}},
        {13,I::palette,i18n::tr("Color"),{},{}}
    };
}
int adjacentProjectTool(int active,int direction){return adjacentTool(projectToolDefinitions(),active,direction);}
int adjacentTool(const std::vector<ProjectToolDefinition>& definitions,int active,int direction)
{
    if(direction==0)return active;
    std::vector<int> tools;
    for(const auto& tool:definitions)if(tool.available())tools.push_back(tool.id);
    if(tools.empty())return active;
    std::sort(tools.begin(),tools.end());
    const auto found=std::find(tools.begin(),tools.end(),active);
    if(found==tools.end())return direction>0?tools.front():tools.back();
    const int size=(int)tools.size(),index=(int)std::distance(tools.begin(),found);
    return tools[(size_t)((index+(direction>0?1:-1)+size)%size)];
}
bool opensProjectToolbox(bool showByDefault,juce::ModifierKeys mods)
{
    // Ctrl-click on macOS is itself a context-click gesture. Ctrl + an actual right click,
    // like Shift/Option/Command + right click, switches to the alternative menu.
    auto flags=mods.getRawFlags()&juce::ModifierKeys::allKeyboardModifiers;
    if(!mods.isRightButtonDown())flags&=~juce::ModifierKeys::ctrlModifier;
    return showByDefault!=(flags!=0);
}
ProjectToolbox::ProjectToolbox(int active,std::function<void(int)> choose,bool showOnRightClick)
    :ProjectToolbox(projectToolDefinitions(),"project-toolbox",active,std::move(choose),showOnRightClick){}
ProjectToolbox::ProjectToolbox(std::vector<ProjectToolDefinition> definitions,const juce::String& componentId,int active,std::function<void(int)> choose,bool showOnRightClick)
    :tools(std::move(definitions)),prefix(componentId),chosen(std::move(choose)),activeTool(active)
{
    contextHint=showOnRightClick?i18n::tr("Tecla modificadora + clic derecho: menú contextual"):i18n::tr("Clic derecho: menú contextual");
    setLookAndFeel(&look);setComponentID(prefix);
    for(const auto& tool:tools)
    {
        auto button=std::make_unique<juce::TextButton>();
        theme::setButtonIcon(*button,tool.icon);button->setName(tool.name);
        button->setComponentID(prefix+"-tool-"+juce::String(tool.id));
        button->setTooltip(tool.name+(tool.shortcut.isNotEmpty()?" · "+tool.shortcut:juce::String())+(tool.available()?juce::String():"\n"+tool.unavailableReason));
        button->setEnabled(tool.available());button->setToggleState(tool.id==active,juce::dontSendNotification);
        if(tool.available())button->onClick=[this,id=tool.id]{chooseAndDismiss(id);};
        addAndMakeVisible(*button);buttons.push_back(std::move(button));
    }
    // Wide enough for the hint even with few tools (the MIDI editor has six).
    setSize(juce::jmax(16+30*(int)buttons.size(),250),66);
}
void ProjectToolbox::chooseAndDismiss(int id)
{
    activeTool=id;
    for(auto& button:buttons)button->setToggleState(button->getComponentID()==prefix+"-tool-"+juce::String(id),juce::dontSendNotification);
    auto callback=chosen;
    if(auto* popup=findParentComponentOfClass<juce::CallOutBox>())popup->dismiss();
    if(callback)callback(id);
}
bool ProjectToolbox::keyPressed(const juce::KeyPress& key)
{
    const auto* command=app::findKeyCommand(key);if(!command)return false;
    const int direction=command->id=="toolNext"?1:command->id=="toolPrevious"?-1:0;
    if(direction==0)return false;
    chooseAndDismiss(adjacentTool(tools,activeTool,direction));return true;
}
void ProjectToolbox::resized()
{
    int x=8;for(auto& button:buttons){button->setBounds(x,8,28,28);x+=30;}
}
void ProjectToolbox::paint(juce::Graphics& g)
{
    g.fillAll(theme::panel);
    theme::text(g,contextHint,{8,40,getWidth()-16,20},10,theme::muted,juce::Justification::centred);
}
}

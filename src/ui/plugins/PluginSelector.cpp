#include "PluginSelector.h"
#include "../../i18n/Translations.h"
#include "../theme/Colours.h"
#include "../../plugins/PluginImport.h"
#include "../../app/Preferences.h"
#include <map>

namespace silente::ui
{
using namespace juce;
using namespace theme;
PluginSelector::PluginSelector(KnownPluginList& c,PluginKind k,const String& destination,const File& file)
    :catalog(c),kind(k),target(destination),favouriteFile(file),list("PluginSelector",this)
{
    setLookAndFeel(&look);setComponentID("plugin-selector");
    title.setFont(Font(FontOptions(18.0f,Font::bold)));title.setColour(Label::textColourId,ink);
    channel.setColour(Label::textColourId,muted);status.setColour(Label::textColourId,muted);status.setFont(Font(FontOptions(11.0f)));
    search.setComponentID("plugin-search");search.setColour(TextEditor::backgroundColourId,bg);search.setColour(TextEditor::textColourId,ink);
    search.onTextChange=[this]{rebuild();};search.onReturnKey=[this]{accept();};
    format.addItem("VST3",2);format.addItem("AudioUnit",3);format.setSelectedId(1,dontSendNotification);format.onChange=[this]{rebuild();};
    grouping.onChange=[this]{collapsed.clear();rebuild();};
    favourites.setClickingTogglesState(true);favourites.onClick=[this]{rebuild();};
    scan.onClick=[this]{if(scanning&&scanning()){if(onStop)onStop();}else if(onScan)onScan();};
    choose.onClick=[this]{accept();};
    list.setComponentID("plugin-results");list.setRowHeight(36);list.setColour(ListBox::backgroundColourId,bg);
    for(auto* component:std::initializer_list<Component*>{&title,&channel,&search,&format,&grouping,&favourites,&list,&status,&scan,&choose})addAndMakeVisible(component);
    auto saved=JSON::parse(favouriteFile.loadFileAsString());
    if(auto* items=saved.getArray())for(const auto& item:*items)favouriteIds.insert(item.toString());
    setSize(540,540);translate();rebuild();timerCallback();startTimerHz(4);
}
void PluginSelector::translate()
{
    title.setText(kind==PluginKind::instrument?i18n::tr("Seleccionar instrumento"):i18n::tr("Seleccionar efecto"),dontSendNotification);
    channel.setText(target,dontSendNotification);
    search.setTextToShowWhenEmpty(i18n::tr("Buscar por nombre, fabricante o categoría…"),muted);
    format.changeItemText(1,i18n::tr("Todos los formatos"));
    if(format.getNumItems()<3){format.addItem(i18n::tr("Todos los formatos"),1);format.setSelectedId(1,dontSendNotification);}
    const int group=grouping.getSelectedId();grouping.clear(dontSendNotification);
    grouping.addItem(i18n::tr("Categoría"),1);grouping.addItem(i18n::tr("Fabricante"),2);grouping.setSelectedId(group>0?group:1,dontSendNotification);
    favourites.setButtonText(i18n::tr("Favoritos"));choose.setButtonText(i18n::tr("Cargar"));
    favourites.setTooltip(i18n::tr("Pulsa la estrella de un plugin para guardarlo en favoritos."));
    lastLanguage=static_cast<int>(i18n::language());
}
void PluginSelector::paint(Graphics& g){g.fillAll(panel);}
void PluginSelector::resized()
{
    auto r=getLocalBounds().reduced(16);title.setBounds(r.removeFromTop(27));channel.setBounds(r.removeFromTop(22));r.removeFromTop(10);search.setBounds(r.removeFromTop(30));r.removeFromTop(8);
    auto filters=r.removeFromTop(28);format.setBounds(filters.removeFromLeft(155));filters.removeFromLeft(6);grouping.setBounds(filters.removeFromLeft(153));filters.removeFromLeft(6);favourites.setBounds(filters);
    auto bottom=r.removeFromBottom(31);choose.setBounds(bottom.removeFromRight(110));scan.setBounds(bottom.removeFromLeft(145));r.removeFromBottom(7);status.setBounds(r.removeFromBottom(38));r.removeFromBottom(5);r.removeFromTop(8);list.setBounds(r);
}
void PluginSelector::focusSearch(){search.grabKeyboardFocus();}
void PluginSelector::rebuild()
{
    String selected;
    int previous=list.getSelectedRow();if(isPositiveAndBelow(previous,static_cast<int>(rows.size()))&&!rows[(size_t)previous].heading)selected=rows[(size_t)previous].plugin.createIdentifierString();
    importFolder=app::pluginSearchFolder();
    rows.clear();std::map<String,std::vector<PluginDescription>> groups;
    const String selectedFormat=format.getSelectedId()==2?"VST3":format.getSelectedId()==3?"AudioUnit":"";
    auto available=catalog.getTypes();if(kind==PluginKind::instrument)available.add(internalInstrumentDescription());
    for(const auto& item:available)
    {
        if(!matchesPlugin(item,kind,search.getText(),selectedFormat))continue;
        if(favourites.getToggleState()&&!favouriteIds.count(item.createIdentifierString()))continue;
        String group=grouping.getSelectedId()==2?item.manufacturerName:i18n::tr(item.category);
        if(group.isEmpty())group=i18n::tr("Sin categoría");
        groups[group].push_back(item);
    }
    for(auto& [group,items]:groups)
    {
        std::sort(items.begin(),items.end(),[](const auto& a,const auto& b){return a.name.compareNatural(b.name)<0;});
        rows.push_back({group,{},true,static_cast<int>(items.size())});
        if(!collapsed.count(group)||search.getText().isNotEmpty())for(const auto& item:items)rows.push_back({group,item,false,0});
    }
    list.updateContent();list.deselectAllRows();
    int first=-1;for(int i=0;i<static_cast<int>(rows.size());++i)if(!rows[(size_t)i].heading){if(first<0)first=i;if(rows[(size_t)i].plugin.createIdentifierString()==selected)list.selectRow(i);}
    if(list.getSelectedRow()<0&&first>=0)list.selectRow(first);
    lastCount=catalog.getNumTypes();choose.setEnabled(list.getSelectedRow()>=0);repaint();
}
int PluginSelector::getNumRows(){return static_cast<int>(rows.size());}
String PluginSelector::getTooltipForRow(int index)
{
    if(!isPositiveAndBelow(index,static_cast<int>(rows.size()))||rows[(size_t)index].heading)return {};
    return plugins::describeOrigin(rows[(size_t)index].plugin,importFolder);
}
String PluginSelector::originText(int index) const
{
    return isPositiveAndBelow(index,static_cast<int>(rows.size()))&&!rows[(size_t)index].heading?plugins::describeOrigin(rows[(size_t)index].plugin,importFolder):String();
}
void PluginSelector::paintListBoxItem(int index,Graphics& g,int width,int height,bool selected)
{
    if(!isPositiveAndBelow(index,static_cast<int>(rows.size())))return;const auto& row=rows[(size_t)index];
    g.setColour(row.heading?raised:selected?selectedTrack:bg);g.fillRect(0,0,width,height);
    g.setFont(Font(FontOptions(row.heading?12.0f:13.0f,row.heading?Font::bold:Font::plain)));
    g.setColour(row.heading?muted:ink);
    if(row.heading){g.drawText((collapsed.count(row.group)?String::fromUTF8("▸ "):String::fromUTF8("▾ "))+row.group+" ("+String(row.count)+")",10,0,width-20,height,Justification::centredLeft);return;}
    const bool imported=plugins::originOf(row.plugin,importFolder)==plugins::Origin::imported;
    g.drawText(row.plugin.name,27,2,width-135,18,Justification::centredLeft);g.setColour(muted);g.drawText(row.plugin.pluginFormatName,width-100,0,67,height,Justification::centredRight);
    // Origin line: distinguishes plugins imported from the extra folder from system ones.
    g.setFont(Font(FontOptions(10.0f)));g.setColour(imported?accent:selected?ink:muted);
    g.drawText(plugins::describeOrigin(row.plugin,importFolder),27,19,width-135,14,Justification::centredLeft,true);
    g.setColour(favouriteIds.count(row.plugin.createIdentifierString())?cycleColour:muted);g.drawText(String::fromUTF8("★"),width-28,0,24,height,Justification::centred);
}
void PluginSelector::listBoxItemClicked(int index,const MouseEvent& e)
{
    if(!isPositiveAndBelow(index,static_cast<int>(rows.size())))return;auto row=rows[(size_t)index];
    if(row.heading){if(collapsed.count(row.group))collapsed.erase(row.group);else collapsed.insert(row.group);rebuild();return;}
    if(e.x>=list.getWidth()-36)
    {
        const auto id=row.plugin.createIdentifierString();if(favouriteIds.count(id))favouriteIds.erase(id);else favouriteIds.insert(id);
        Array<var> values;for(const auto& item:favouriteIds)values.add(item);TemporaryFile temporary(favouriteFile);if(temporary.getFile().replaceWithText(JSON::toString(var(values))))temporary.overwriteTargetFileWithTemporary();rebuild();
    }
}
void PluginSelector::listBoxItemDoubleClicked(int index,const MouseEvent& e){if(isPositiveAndBelow(index,static_cast<int>(rows.size()))&&!rows[(size_t)index].heading&&e.x<list.getWidth()-36)accept();}
void PluginSelector::returnKeyPressed(int){accept();}
void PluginSelector::selectedRowsChanged(int index){choose.setEnabled(isPositiveAndBelow(index,static_cast<int>(rows.size()))&&!rows[(size_t)index].heading);}
void PluginSelector::accept(){int index=list.getSelectedRow();if(isPositiveAndBelow(index,static_cast<int>(rows.size()))&&!rows[(size_t)index].heading&&onChoose)onChoose(rows[(size_t)index].plugin);}
void PluginSelector::timerCallback()
{
    if(lastLanguage!=static_cast<int>(i18n::language())){translate();rebuild();}
    if(lastCount!=catalog.getNumTypes())rebuild();
    scan.setButtonText(scanning&&scanning()?i18n::tr("Detener búsqueda"):i18n::tr("Buscar plugins"));
    auto message=scanStatus?scanStatus():String();
    if(catalog.getNumTypes()==0)message=i18n::tr("No hay plugins en el catálogo. Pulsa Buscar plugins para explorarlos.");
    status.setText(message,dontSendNotification);
}
}

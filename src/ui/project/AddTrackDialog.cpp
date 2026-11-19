#include "AddTrackDialog.h"
#include "TimelineTracks.h"
#include "../../plugins/PluginSelection.h"
#include "../theme/Drawing.h"
#include <algorithm>

namespace silente::ui
{
using namespace theme;
namespace
{
struct Type { const char* id; const char* caption; std::optional<TrackKind> kind; };
const Type definitions[]{
    {"audio","Audio",TrackKind::audio},{"instrument","Instrumento",TrackKind::instrument},{"sampler","Muestreador",{}},{"midi","MIDI",TrackKind::midi},
    {"group","Grupo",TrackKind::group},{"fx","Efecto",TrackKind::fx},{"vca","VCA",{}},{"marker","Marcador",TrackKind::marker},{"ruler","Regla",TrackKind::ruler},{"folder","Carpeta",TrackKind::folder},
    {"tempo","Tempo",{}},{"signature","Tipo de compás",{}},{"arranger","Arreglos",{}},{"transpose","Transposición",{}},{"chord","Acorde",{}},{"video","Video",TrackKind::video}
};
void icon(juce::Graphics& g,int type,juce::Rectangle<float> r,juce::Colour colour)
{
    g.setColour(colour);const float x=r.getX(),y=r.getY(),w=r.getWidth(),h=r.getHeight();juce::Path p;
    if(type==0){p.startNewSubPath(x,y+h*.5f);for(int n=1;n<10;++n)p.lineTo(x+w*n/10,y+h*(n%2?.2f:.8f));p.lineTo(x+w,y+h*.5f);g.strokePath(p,juce::PathStrokeType(w*.085f));}
    else if(type==1){for(int n=0;n<4;++n)g.fillRect(x+n*w/4,y,w/4-1,h);g.setColour(panel);for(int n=0;n<3;++n)g.fillRect(x+(n+1)*w/4-2,y,4.0f,h*.6f);}
    else if(type==2){for(int n=0;n<5;++n)g.fillRoundedRectangle(x+n*w/5,y+h*(n%2?.1f:.25f),w*.12f,h*(n%2?.8f:.5f),w*.04f);}
    else if(type==3){g.fillEllipse(r);g.setColour(panel);for(int n=0;n<5;++n){const float a=(float)n*.55f+3.6f;g.fillEllipse(x+w*.5f+std::cos(a)*w*.32f-w*.055f,y+h*.5f+std::sin(a)*h*.32f-h*.055f,w*.11f,h*.11f);}}
    else if(type==4){for(int n=0;n<3;++n){g.fillRect(x+w*(.15f+.3f*n),y,w*.12f,h*.65f);g.fillRect(x+w*(.05f+.3f*n),y+h*.25f,w*.3f,h*.12f);}g.drawLine(x,y+h*.9f,x+w,y+h*.9f,2);}
    else if(type==5){font(g,h*.7f,true);g.drawText("FX",r,juce::Justification::centred);}
    else if(type==6){p.addTriangle(x,y+h*.2f,x+w*.4f,y+h*.5f,x,y+h*.8f);g.fillPath(p);g.fillRect(x+w*.6f,y,w*.3f,h);}
    else if(type==7){p.addTriangle(x+w*.2f,y,x+w*.8f,y,x+w*.5f,y+h*.23f);g.fillPath(p);for(int n=0;n<3;++n)g.fillRect(x+w*.42f,y+h*(.3f+n*.22f),w*.16f,h*.13f);}
    else if(type==8){g.drawRect(r.withHeight(h*.65f),2);for(int n=1;n<5;++n)g.fillRect(x+w*n/5,y,w*.07f,h*(n%2?.35f:.5f));}
    else if(type==9){p.addRoundedRectangle(x,y+h*.2f,w,h*.7f,2);p.addRectangle(x,y,w*.45f,h*.3f);g.fillPath(p);}
    else if(type==10){g.fillEllipse(x,y+h*.6f,w*.5f,h*.3f);g.fillRect(x+w*.4f,y,w*.1f,h*.8f);}
    else if(type==11){font(g,h*.5f,true);g.drawText("4",r.withHeight(h*.5f),juce::Justification::centred);g.drawText("4",r.withTrimmedTop(h*.5f),juce::Justification::centred);}
    else if(type==12){g.drawRect(r,2);p.startNewSubPath(x+w*.15f,y+h*.3f);p.lineTo(x+w*.5f,y+h*.8f);p.lineTo(x+w*.8f,y+h*.3f);g.strokePath(p,juce::PathStrokeType(2));}
    else if(type==13){p.startNewSubPath(x,y+h*.75f);p.lineTo(x+w*.3f,y+h*.75f);p.lineTo(x+w*.3f,y+h*.15f);p.lineTo(x+w*.7f,y+h*.15f);p.lineTo(x+w*.7f,y+h*.75f);p.lineTo(x+w,y+h*.75f);g.strokePath(p,juce::PathStrokeType(2));}
    else if(type==14){for(int n=1;n<4;++n)g.drawHorizontalLine((int)(y+h*n/4),x,x+w);g.drawLine(x+w*.3f,y,x+w*.2f,y+h,2);g.drawLine(x+w*.75f,y,x+w*.65f,y+h,2);}
    else{g.drawRect(r,2);g.drawRect(r.reduced(w*.18f,0),1);for(int n=1;n<4;++n){g.drawHorizontalLine((int)(y+h*n/4),x,x+w*.18f);g.drawHorizontalLine((int)(y+h*n/4),x+w*.82f,x+w);}}
}
}
class AddTrackDialog::TypeButton final : public juce::Button
{
public:
    explicit TypeButton(int n):Button(definitions[n].id),index(n){setComponentID("add-type-"+juce::String(definitions[n].id));setClickingTogglesState(false);setEnabled(definitions[n].kind.has_value());}
    int index;
    void paintButton(juce::Graphics& g,bool over,bool down) override
    {
        const bool chosen=getToggleState();const float size=index<4?58.0f:30.0f;
        const auto box=juce::Rectangle<float>(size,size).withCentre({getWidth()*.5f,size*.5f+4});
        const auto fill=chosen?juce::Colour(0xffa8abad):isEnabled()?juce::Colour(0xff929698):juce::Colour(0xff53575a);
        g.setColour(fill.brighter(over||down?.12f:0));g.fillRoundedRectangle(box,3);
        if(chosen){g.setColour(ink);g.drawRoundedRectangle(box.expanded(1),4,2);}
        icon(g,index,box.reduced(size*.17f),chosen?juce::Colours::white:isEnabled()?activeInk:panel.brighter(.1f));
        theme::text(g,getButtonText(),{0,(int)size+7,getWidth(),22},index<4?12:10.5f,isEnabled()?ink:muted.withAlpha(.55f),juce::Justification::centred);
    }
};
void AddTrackDialog::Expander::paintButton(juce::Graphics& g,bool,bool)
{
    theme::text(g,getButtonText(),{0,0,130,getHeight()},12,ink);g.setColour(raised);g.drawHorizontalLine(getHeight()/2,135,(float)getWidth());
}
AddTrackDialog::AddTrackDialog()
{
    setComponentID("add-track-dialog");setWantsKeyboardFocus(true);
    for(int n=0;n<16;++n){auto b=std::make_unique<TypeButton>(n);addAndMakeVisible(*b);b->onClick=[this,n]{if(definitions[n].kind)selectType(*definitions[n].kind);};types.push_back(std::move(b));}
    for(auto* c:std::initializer_list<juce::Component*>{&more,&add,&connections,&keep,&name,&count,&input,&configuration,&output,&plugin,&format,&error})addAndMakeVisible(c);
    more.setComponentID("add-more-types");add.setComponentID("add-confirm");keep.setComponentID("add-keep-open");name.setComponentID("add-name");count.setComponentID("add-count");
    input.setComponentID("add-input");configuration.setComponentID("add-configuration");output.setComponentID("add-output");plugin.setComponentID("add-plugin");format.setComponentID("add-ruler-format");connections.setComponentID("add-audio-settings");
    count.setSliderStyle(juce::Slider::IncDecButtons);count.setTextBoxStyle(juce::Slider::TextBoxLeft,false,75,28);count.setRange(1,100,1);count.setValue(1,juce::dontSendNotification);
    name.setSelectAllWhenFocused(true);name.onReturnKey=[this]{submit();};name.onEscapeKey=[this]{if(close)close();};
    more.onClick=[this]{expanded=!expanded;refreshLanguage();resized();repaint();};add.onClick=[this]{submit();};connections.onClick=[this]{if(audioSettings)audioSettings();};
    configuration.addItem("Mono",1);configuration.addItem(i18n::tr("Estéreo"),2);configuration.setSelectedId(2,juce::dontSendNotification);configuration.onChange=[this]{if(!updating)rebuildInputs();};
    output.onChange=[this]{if(!updating){const int index=output.getSelectedId()-2;outputId=juce::isPositiveAndBelow(index,(int)outputOptions.size())?outputOptions[(size_t)index].first:juce::String();}};
    error.setColour(juce::Label::textColourId,coral);error.setFont(juce::Font(juce::FontOptions(11)));error.setJustificationType(juce::Justification::centredLeft);
    setSize(490,660);refreshLanguage();selectType(TrackKind::audio);
}
AddTrackDialog::~AddTrackDialog()=default;
void AddTrackDialog::refreshLanguage()
{
    for(int n=0;n<16;++n){auto& b=*types[(size_t)n];b.setButtonText(i18n::tr(juce::String::fromUTF8(definitions[n].caption)));b.setTooltip(b.isEnabled()?b.getButtonText():i18n::tr("Este tipo de pista aún no está disponible."));}
    more.setButtonText((expanded?juce::String::fromUTF8("▾  "):juce::String::fromUTF8("▸  "))+i18n::tr("Más pistas"));
    add.setButtonText(i18n::tr("Añadir pista"));keep.setButtonText(i18n::tr("Mantener el diálogo abierto"));name.setTextToShowWhenEmpty(i18n::tr("Introducir nombre"),muted);
    connections.setButtonText(juce::String::fromUTF8("⚙"));connections.setTooltip(i18n::tr("Abrir ajustes de audio y MIDI"));configuration.changeItemText(2,i18n::tr("Estéreo"));rebuildFields();repaint();
}
void AddTrackDialog::selectType(TrackKind value)
{
    if(kind!=value)plugin.setSelectedId(0,juce::dontSendNotification);
    kind=value;for(int n=0;n<16;++n)types[(size_t)n]->setToggleState(definitions[n].kind&&*definitions[n].kind==kind,juce::dontSendNotification);
    error.setText({},juce::dontSendNotification);rebuildFields();resized();repaint();
}
void AddTrackDialog::updateOptions(const juce::StringArray& inputs,const std::vector<std::pair<juce::String,juce::String>>& outputs,const juce::Array<juce::PluginDescription>& catalog,int markerCount)
{
    inputNames=inputs;outputOptions=outputs;plugins=catalog;markers=markerCount;rebuildFields();resized();
}
void AddTrackDialog::rebuildInputs()
{
    const int previous=input.getSelectedId();input.clear(juce::dontSendNotification);const bool mono=configuration.getSelectedId()==1;
    const int size=juce::jmin(inputNames.size(),DeviceSettings::channelLimit);
    for(int n=0;n<size;n+=mono?1:2)if(mono||n+1<size)input.addItem(mono?inputNames[n]:inputNames[n]+" / "+inputNames[n+1],n+1);
    if(input.getNumItems()==0){input.setTextWhenNothingSelected(i18n::tr("Sin entradas disponibles"));input.setEnabled(false);}
    else{input.setEnabled(true);input.setSelectedId(input.indexOfItemId(previous)>=0?previous:1,juce::dontSendNotification);}
}
void AddTrackDialog::rebuildFields()
{
    updating=true;const int selectedPlugin=plugin.getSelectedId(),selectedFormat=format.getSelectedId();
    output.clear(juce::dontSendNotification);output.addItem("Stereo Out",1);for(int n=0;n<(int)outputOptions.size();++n)output.addItem(outputOptions[(size_t)n].second,n+2);
    output.setSelectedId(1,juce::dontSendNotification);
    if(outputId.isNotEmpty())
    {
        bool found=false;for(int n=0;n<(int)outputOptions.size();++n)if(outputOptions[(size_t)n].first==outputId){output.setSelectedId(n+2,juce::dontSendNotification);found=true;break;}
        if(!found)output.setText(i18n::tr("Salida no disponible"),juce::dontSendNotification);
    }
    const bool instrument=kind==TrackKind::instrument||kind==TrackKind::midi;plugin.clear(juce::dontSendNotification);pluginOptions.clear();
    if(instrument)pluginOptions.push_back(internalInstrumentDescription());
    plugin.addItem(instrument?i18n::tr("Sintetizador interno"):i18n::tr("Sin efecto"),1);
    for(const auto& description:plugins)if(description.isInstrument==instrument){pluginOptions.push_back(description);plugin.addItem(description.name+" ("+description.pluginFormatName+")",(int)pluginOptions.size()+(instrument?0:1));}
    plugin.setSelectedId(juce::isPositiveAndBelow(selectedPlugin-1,plugin.getNumItems())?selectedPlugin:1,juce::dontSendNotification);
    format.clear(juce::dontSendNotification);for(int n=0;n<5;++n)format.addItem(rulerFormatName(n),n+1);format.setSelectedId(selectedFormat>0?selectedFormat:1,juce::dontSendNotification);
    const bool audio=kind==TrackKind::audio,bus=kind==TrackKind::fx||kind==TrackKind::group;
    input.setVisible(audio);configuration.setVisible(audio);connections.setVisible(audio);output.setVisible(audio||instrument||bus);plugin.setVisible(instrument||kind==TrackKind::fx);format.setVisible(kind==TrackKind::ruler);
    const int maximum=kind==TrackKind::marker?juce::jmax(1,10-markers):100;count.setRange(1,maximum,1);add.setEnabled(kind!=TrackKind::marker||markers<10);add.setTooltip(kind==TrackKind::marker&&markers>=10?i18n::tr("El proyecto admite hasta diez pistas de marcador."):juce::String());
    rebuildInputs();updating=false;
}
void AddTrackDialog::submit()
{
    count.hideTextBox(false);app::TrackCreation request;request.kind=kind;request.name=name.getText();request.count=(int)count.getValue();
    request.inputLeft=juce::jmax(0,input.getSelectedId()-1);request.inputRight=configuration.getSelectedId()==1?request.inputLeft:request.inputLeft+1;
    if(output.isVisible())request.output=outputId;
    request.rulerFormat=juce::jmax(0,format.getSelectedId()-1);
    const bool instrument=kind==TrackKind::instrument||kind==TrackKind::midi;
    if(plugin.isVisible()){const int index=plugin.getSelectedId()-(instrument?1:2);if(juce::isPositiveAndBelow(index,(int)pluginOptions.size()))request.plugin=pluginOptions[(size_t)index];}
    if(!create)return;const auto result=create(request);
    if(result.failed()){error.setText(result.getErrorMessage(),juce::dontSendNotification);return;}
    error.setText({},juce::dontSendNotification);
    if(keep.getToggleState()){if(kind==TrackKind::marker)markers+=request.count;name.clear();rebuildFields();}
    else if(close)close();
}
void AddTrackDialog::resized()
{
    const int margin=24,width=getWidth()-2*margin;
    for(int n=0;n<4;++n)types[(size_t)n]->setBounds(margin+n*width/4,30,width/4,92);
    for(int n=0;n<6;++n)types[(size_t)n+4]->setBounds(margin+n*width/6,148,width/6,61);
    more.setBounds(margin,221,width,25);
    for(int n=0;n<6;++n){types[(size_t)n+10]->setVisible(expanded);types[(size_t)n+10]->setBounds(margin+n*width/6,265,width/6,62);}
    fieldsTop=expanded?355:270;const int controlX=200,controlW=getWidth()-controlX-margin;
    input.setBounds(controlX,fieldsTop,controlW-30,27);connections.setBounds(getWidth()-margin-28,fieldsTop,28,27);configuration.setBounds(controlX,fieldsTop+35,controlW,27);
    plugin.setBounds(controlX,fieldsTop,controlW,27);format.setBounds(controlX,fieldsTop,controlW,27);
    output.setBounds(controlX,fieldsTop+(kind==TrackKind::audio?70:kind==TrackKind::group?0:35),controlW,27);
    nameTop=fieldsTop+131;name.setBounds(controlX,nameTop,controlW,28);count.setBounds(getWidth()-margin-95,nameTop+37,95,28);
    footerTop=nameTop+105;keep.setBounds(margin,footerTop+13,265,30);add.setBounds(getWidth()-margin-155,footerTop+13,155,30);error.setBounds(margin,nameTop+70,width,28);
}
void AddTrackDialog::paint(juce::Graphics& g)
{
    g.fillAll(panel);g.setColour(juce::Colour(0xff8c9092));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(3),5,1);
    g.setColour(raised);for(int y:{fieldsTop-20,footerTop})g.drawHorizontalLine(y,24,(float)getWidth()-24);
    const auto label=[&](const juce::String& value,int y){text(g,value,{29,y,165,27},12,ink);};
    if(kind==TrackKind::audio){label(i18n::tr("Entradas audio"),fieldsTop);label(i18n::tr("Configuración"),fieldsTop+35);label(i18n::tr("Salidas audio"),fieldsTop+70);}
    else if(kind==TrackKind::instrument||kind==TrackKind::midi||kind==TrackKind::fx){label(kind==TrackKind::fx?i18n::tr("Efecto"):i18n::tr("Instrumento"),fieldsTop);label(i18n::tr("Salidas audio"),fieldsTop+35);}
    else if(kind==TrackKind::group)label(i18n::tr("Salidas audio"),fieldsTop);
    else if(kind==TrackKind::ruler)label(i18n::tr("Formato de regla"),fieldsTop);
    label(i18n::tr("Nombre"),nameTop);label(i18n::tr("Número"),nameTop+37);
}
bool AddTrackDialog::keyPressed(const juce::KeyPress& key)
{
    if(key.isKeyCode(juce::KeyPress::escapeKey)){if(close)close();return true;}
    if(key.isKeyCode(juce::KeyPress::returnKey)){submit();return true;}return false;
}
AddTrackWindow::AddTrackWindow():DocumentWindow(i18n::tr("Añadir pista"),panel,juce::DocumentWindow::closeButton)
{
    setComponentID("add-track-window");setLookAndFeel(&look);setUsingNativeTitleBar(true);auto* viewport=new juce::Viewport();body=new AddTrackDialog();
    viewport->setViewedComponent(body,true);viewport->setScrollBarsShown(true,false);viewport->setScrollBarThickness(9);setContentOwned(viewport,false);setResizable(false,false);
    body->close=[this]{setVisible(false);};
}
void AddTrackWindow::present(juce::Component& parent)
{
    const auto area=juce::Desktop::getInstance().getDisplays().getDisplayForRect(parent.getScreenBounds())->userArea.reduced(12);
    centreAroundComponent(&parent,510,juce::jmin(700,area.getHeight()));setBounds(getBounds().constrainedWithin(area));setVisible(true);toFront(true);body->grabKeyboardFocus();
}
}

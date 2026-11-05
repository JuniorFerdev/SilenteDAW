#include "TrackCreation.h"
#include <algorithm>
namespace silente::app
{
juce::Result validateTrackCreation(const AudioEngine& engine,const TrackCreation& request)
{
    const juce::ScopedLock lock(engine.modelLock);
    if(engine.recording.load())return juce::Result::fail(i18n::tr("Detén la grabación antes de añadir pistas."));
    if(request.count<1||request.count>100)return juce::Result::fail(i18n::tr("El número de pistas debe estar entre 1 y 100."));
    if(request.kind==TrackKind::marker&&request.count+std::count_if(engine.tracks.begin(),engine.tracks.end(),[](const auto& t){return t->kind==TrackKind::marker;})>10)
        return juce::Result::fail(i18n::tr("El proyecto admite hasta diez pistas de marcador."));
    if(request.inputLeft<0||request.inputRight<0||request.inputLeft>=DeviceSettings::channelLimit||request.inputRight>=DeviceSettings::channelLimit)
        return juce::Result::fail(i18n::tr("Selecciona una entrada de audio válida."));
    if(request.output.isNotEmpty()){const auto output=engine.findTrack(request.output);if(!output||output->kind!=TrackKind::group)return juce::Result::fail(i18n::tr("La salida seleccionada ya no existe. Elige otra salida."));}
    if(request.plugin&&request.kind!=TrackKind::instrument&&request.kind!=TrackKind::midi&&request.kind!=TrackKind::fx)return juce::Result::fail(i18n::tr("El plugin seleccionado no corresponde al tipo de pista."));
    if(request.plugin&&request.plugin->isInstrument!=(request.kind==TrackKind::instrument||request.kind==TrackKind::midi))
        return juce::Result::fail(i18n::tr("El plugin seleccionado no corresponde al tipo de pista."));
    return juce::Result::ok();
}
juce::Result createTracks(AudioEngine& engine,const TrackCreation& request,std::vector<std::shared_ptr<Track>>& created)
{
    const juce::ScopedLock lock(engine.modelLock);if(auto result=validateTrackCreation(engine,request);result.failed())return result;
    created.clear();created.reserve((size_t)request.count);
    const auto base=request.name.trim();
    for(int n=0;n<request.count;++n)
    {
        auto name=base;if(name.isNotEmpty()&&request.count>1)name+=" "+juce::String(n+1).paddedLeft('0',2);
        auto track=engine.addTrack(request.kind,name);track->inputLeft=request.inputLeft;track->inputRight=request.inputRight;
        if(track->hasAudioChannel())track->output=request.output;
        track->data.rulerFormat=juce::jlimit(0,4,request.rulerFormat);created.push_back(std::move(track));
    }
    return juce::Result::ok();
}
}

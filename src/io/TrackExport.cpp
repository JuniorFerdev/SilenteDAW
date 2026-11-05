#include "TrackExport.h"
#include "../i18n/Translations.h"

namespace silente::io
{
juce::File exportTarget(const ExportJob& job, const juce::String& channelName)
{
    const auto name = channelName.isEmpty() ? job.baseName : job.baseName + " - " + channelName;
    return job.folder.getChildFile(juce::File::createLegalFileName(name) + extensionFor(job.settings.format));
}

namespace
{
juce::Result renderTo(AudioEngine& engine, const juce::File& target, const EncodeSettings& settings, const std::function<bool(double)>& progress)
{
    {
        const juce::ScopedLock lock(engine.modelLock);
        for (const auto& t : engine.tracks)
            for (const auto& clip : t->clips)
                if (clip.file == target) return juce::Result::fail(i18n::tr("Elige otro nombre: el destino es un archivo de audio utilizado por este proyecto."));
    }
    EncodedFileWriter writer;
    if (auto r = writer.open(target, settings, 2); r.failed()) return r;
    auto r = engine.renderMix(settings.sampleRate, [&writer](const juce::AudioBuffer<float>& b, int start, int count) { return writer.write(b, start, count); }, progress);
    if (r.failed()) return r;
    return writer.finish();
}
}

juce::Result exportChannels(AudioEngine& engine, const ExportJob& job, const std::function<bool(double)>& progress, juce::StringArray& written)
{
    if (auto r = validate(job.settings); r.failed()) return r;
    if (! job.mix && job.trackIds.isEmpty()) return juce::Result::fail(i18n::tr("Selecciona al menos un canal para exportar."));
    for(const auto& id:job.trackIds)if(auto track=engine.findTrack(id);track&&!track->hasAudioChannel())
        return juce::Result::fail(i18n::tr("Selecciona un canal de audio para exportar."));
    const int jobs = (job.mix ? 1 : 0) + job.trackIds.size();
    int index = 0;
    const auto step = [&](double p) { return ! progress || progress(((double) index + p) / (double) jobs); };
    if (job.mix)
    {
        const auto target = exportTarget(job, {});
        if (auto r = renderTo(engine, target, job.settings, step); r.failed()) return r;
        written.add(target.getFullPathName()); ++index;
    }
    for (const auto& id : job.trackIds)
    {
        auto track = engine.findTrack(id);
        if (! track) continue;
        // Solo this channel for the render, then restore every mute/solo state exactly.
        std::vector<std::pair<bool, bool>> saved;
        {
            const juce::ScopedLock lock(engine.modelLock);
            for (const auto& t : engine.tracks) { saved.push_back({ t->mute.load(), t->solo.load() }); t->solo = t == track; }
            track->mute = false;
        }
        const auto target = exportTarget(job, track->name);
        auto r = renderTo(engine, target, job.settings, step);
        {
            const juce::ScopedLock lock(engine.modelLock);
            for (size_t n = 0; n < engine.tracks.size() && n < saved.size(); ++n) { engine.tracks[n]->mute = saved[n].first; engine.tracks[n]->solo = saved[n].second; }
        }
        if (r.failed()) return r;
        written.add(target.getFullPathName()); ++index;
    }
    return juce::Result::ok();
}
}

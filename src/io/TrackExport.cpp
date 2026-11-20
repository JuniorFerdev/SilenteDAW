#include "TrackExport.h"
#include "../i18n/Translations.h"
#include <algorithm>

namespace silente::io
{
juce::File exportTarget(const ExportJob& job, const juce::String& channelName, const juce::String& rangeName)
{
    auto name = channelName.isEmpty() ? job.baseName : job.baseName + " - " + channelName;
    if (rangeName.isNotEmpty()) name += " - " + rangeName;
    return job.folder.getChildFile(juce::File::createLegalFileName(name) + extensionFor(job.settings.format));
}

juce::File uniqueFile(const juce::File& file)
{
    if (! file.exists()) return file;
    for (int n = 2; n < 100000; ++n)
    {
        const auto candidate = file.getParentDirectory().getChildFile(file.getFileNameWithoutExtension() + " " + juce::String(n) + file.getFileExtension());
        if (! candidate.exists()) return candidate;
    }
    return file;
}

std::vector<ExportRange> cycleMarkerRanges(AudioEngine& engine)
{
    std::vector<ExportRange> out;
    const juce::ScopedLock lock(engine.modelLock);
    std::shared_ptr<Track> track = engine.findTrack(engine.activeMarkerTrack);
    if (! track || track->kind != TrackKind::marker)
    {
        track = nullptr;
        for (const auto& t : engine.tracks) if (t->kind == TrackKind::marker) { track = t; break; }
    }
    if (! track) return out;
    auto markers = track->data.markers;
    std::stable_sort(markers.begin(), markers.end(), [](const auto& a, const auto& b) { return a.position < b.position; });
    juce::StringArray used;
    for (const auto& m : markers)
    {
        if (! m.cycle || ! (m.end > m.position)) continue;
        auto name = m.description.trim().isEmpty() ? i18n::tr("Ciclo") + " " + juce::String(m.number) : m.description.trim();
        name = juce::File::createLegalFileName(name);
        auto unique = name;
        for (int n = 2; used.contains(unique, true); ++n) unique = name + " " + juce::String(n);
        used.add(unique);
        out.push_back({ unique, m.position, m.end });
    }
    return out;
}

namespace
{
juce::Result renderTo(AudioEngine& engine, const juce::File& target, const EncodeSettings& settings, const std::function<bool(double)>& progress,
                      double rangeStart, double rangeEnd)
{
    {
        const juce::ScopedLock lock(engine.modelLock);
        for (const auto& t : engine.tracks)
            for (const auto& clip : t->clips)
                if (clip.file == target) return juce::Result::fail(i18n::tr("Elige otro nombre: el destino es un archivo de audio utilizado por este proyecto."));
    }
    EncodedFileWriter writer;
    if (auto r = writer.open(target, settings, 2); r.failed()) return r;
    auto r = engine.renderMix(settings.sampleRate, [&writer](const juce::AudioBuffer<float>& b, int start, int count) { return writer.write(b, start, count); }, progress,
                               rangeStart, rangeEnd);
    if (r.failed()) return r;
    return writer.finish();
}

// Ranges the job renders: explicit ranges, the locators, or the whole project (end < 0).
std::vector<ExportRange> rangesOf(const ExportJob& job)
{
    if (! job.ranges.empty()) return job.ranges;
    if (job.useRange) return { { {}, job.rangeStart, job.rangeEnd } };
    return { { {}, 0.0, -1.0 } };
}
}

std::vector<juce::File> plannedTargets(AudioEngine& engine, const ExportJob& job)
{
    std::vector<juce::File> out;
    std::vector<juce::String> names;
    if (job.mix) names.push_back({});
    for (const auto& id : job.trackIds) if (auto t = engine.findTrack(id)) names.push_back(t->name);
    for (const auto& n : names)
        for (const auto& r : rangesOf(job)) out.push_back(exportTarget(job, n, r.name));
    return out;
}

juce::Result exportChannels(AudioEngine& engine, const ExportJob& job, const std::function<bool(double)>& progress, juce::StringArray& written)
{
    if (auto r = validate(job.settings); r.failed()) return r;
    if (job.useRange && job.ranges.empty() && ! (job.rangeEnd > job.rangeStart)) return juce::Result::fail(i18n::tr("El rango de localizadores no es válido."));
    for (const auto& r : job.ranges) if (! (r.end > r.start) || r.start < 0.0) return juce::Result::fail(i18n::tr("El rango de localizadores no es válido."));
    if (! job.mix && job.trackIds.isEmpty()) return juce::Result::fail(i18n::tr("Selecciona al menos un canal para exportar."));
    for(const auto& id:job.trackIds)if(auto track=engine.findTrack(id);track&&!track->hasAudioChannel())
        return juce::Result::fail(i18n::tr("Selecciona un canal de audio para exportar."));
    const auto ranges = rangesOf(job);
    const int jobs = ((job.mix ? 1 : 0) + job.trackIds.size()) * (int) ranges.size();
    int index = 0;
    const auto step = [&](double p) { return ! progress || progress(((double) index + p) / (double) jobs); };
    const auto renderRanges = [&](const juce::String& channelName) -> juce::Result
    {
        for (const auto& range : ranges)
        {
            auto target = exportTarget(job, channelName, range.name);
            if (job.conflict == NameConflict::uniqueName) target = uniqueFile(target);
            if (auto r = renderTo(engine, target, job.settings, step, range.start, range.end); r.failed()) return r;
            written.add(target.getFullPathName()); ++index;
        }
        return juce::Result::ok();
    };
    if (job.mix)
        if (auto r = renderRanges({}); r.failed()) return r;
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
        auto r = renderRanges(track->name);
        {
            const juce::ScopedLock lock(engine.modelLock);
            for (size_t n = 0; n < engine.tracks.size() && n < saved.size(); ++n) { engine.tracks[n]->mute = saved[n].first; engine.tracks[n]->solo = saved[n].second; }
        }
        if (r.failed()) return r;
    }
    return juce::Result::ok();
}
}

#include "CprImport.h"
#include "../AudioEngine.h"
#include "../i18n/Translations.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <unordered_map>

namespace silente::io::cpr
{
namespace
{
// ------------------------------------------------------------------ lectura big-endian
struct Bytes
{
    const juce::uint8* d = nullptr;
    size_t size = 0;
    juce::uint64 be(size_t o, int n) const noexcept { juce::uint64 v = 0; for (int i = 0; i < n; ++i) v = (v << 8) | d[o + (size_t) i]; return v; }
    juce::uint32 u32(size_t o) const noexcept { return (juce::uint32) be(o, 4); }
    juce::uint16 u16(size_t o) const noexcept { return (juce::uint16) be(o, 2); }
    juce::int64 i64(size_t o) const noexcept { return (juce::int64) be(o, 8); }
    double f64(size_t o) const noexcept { const auto v = be(o, 8); double x; std::memcpy(&x, &v, 8); return x; }
    float f32(size_t o) const noexcept { const auto v = u32(o); float x; std::memcpy(&x, &v, 4); return x; }
};

const juce::uint8 utf8Flag[] { 0xEF, 0xBB, 0xBF };

// Cadena: u32 longitud (incluye NUL y, si hay, EF BB BF tras el NUL = UTF-8).
bool isString(const Bytes& b, size_t o, size_t end, juce::uint32 maxLength = 4096)
{
    if (o + 4 > end) return false;
    const auto n = b.u32(o);
    if (n < 1 || n > maxLength || o + 4 + n > end) return false;
    const auto* raw = b.d + o + 4;
    if (raw[n - 1] == 0) return true;
    return n >= 4 && raw[n - 4] == 0 && std::memcmp(raw + n - 3, utf8Flag, 3) == 0;
}

juce::String readString(const Bytes& b, size_t o, size_t* end = nullptr)
{
    const auto n = b.u32(o);
    const auto* raw = reinterpret_cast<const char*>(b.d + o + 4);
    const bool utf8 = n >= 4 && raw[n - 4] == 0 && std::memcmp(raw + n - 3, utf8Flag, 3) == 0;
    size_t length = 0;
    while (length < n && raw[length] != 0) ++length;
    if (end) *end = o + 4 + n;
    if (utf8) return juce::String::fromUTF8(raw, (int) length);
    juce::String text; text.preallocateBytes(length * 2);
    for (size_t i = 0; i < length; ++i) text += (juce::juce_wchar) (juce::uint8) raw[i]; // latin1
    return text;
}

bool validClassName(const juce::uint8* s, size_t n)
{
    if (n < 2 || n > 81) return false;
    const auto alpha = [](juce::uint8 c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; };
    if (! alpha(s[0])) return false;
    for (size_t i = 1; i < n; ++i) if (! (alpha(s[i]) || (s[i] >= '0' && s[i] <= '9') || s[i] == ':')) return false;
    return true;
}

// ------------------------------------------------------------------ flujo de objetos (bloque ARCH)
struct Obj
{
    juce::String cls;
    int version = 0;
    size_t at = 0, pa = 0, pb = 0; // etiqueta, inicio y fin del payload (absolutos)
    int parent = -1;
    std::vector<int> kids;
};

// Objeto = etiqueta(W) + [nombre + u16 versión si la clase es nueva] + tamaño(W) + payload.
// Etiqueta todo unos: clase nueva + instancia; todo unos − 1: clase base declarada; bit alto: clase
// ya vista (resto = desplazamiento de su marcador). Id de objeto = desplazamiento de su campo tamaño.
class Arch
{
public:
    Arch(const Bytes& data, size_t start, size_t length, bool wide)
        : b(data), base(start), W(wide ? 8 : 4),
          all(wide ? ~juce::uint64 {} : 0xFFFFFFFFull), ref(wide ? (1ull << 63) : (1ull << 31))
    {
        scan(base, base + length, -1);
    }
    const Bytes& b;
    size_t base;
    int W;
    juce::uint64 all, ref;
    std::vector<Obj> objs;
    std::unordered_map<juce::uint64, int> byId;
    std::unordered_map<size_t, int> byAt;

    juce::uint64 word(size_t o) const noexcept { return b.be(o, W); }

    // Ranura de objeto: objeto en línea o referencia a uno anterior. -1 si no se reconoce.
    int slot(size_t o) const
    {
        if (auto it = byAt.find(o); it != byAt.end()) return it->second;
        const auto t = word(o);
        if (t < ref) if (auto it = byId.find(t); it != byId.end()) return it->second;
        return -1;
    }
    int firstKid(int index, const char* cls) const
    {
        for (int k : objs[(size_t) index].kids) if (objs[(size_t) k].cls == cls) return k;
        return -1;
    }
    int firstDescendant(int index, const char* cls) const // preorden, como desc() de cpr_read.py
    {
        for (int k : objs[(size_t) index].kids)
        {
            if (objs[(size_t) k].cls == cls) return k;
            if (int found = firstDescendant(k, cls); found >= 0) return found;
        }
        return -1;
    }
    int ancestor(int index, const char* cls) const
    {
        for (int p = objs[(size_t) index].parent; p >= 0; p = objs[(size_t) p].parent) if (objs[(size_t) p].cls == cls) return p;
        return -1;
    }

private:
    std::unordered_map<juce::uint64, std::pair<juce::String, int>> classes; // marcador relativo -> clase

    bool tryObject(size_t o, size_t limit, Obj& out, size_t& sizeField)
    {
        std::vector<std::pair<std::pair<juce::String, int>, size_t>> chain;
        while (true)
        {
            if (o + (size_t) W > limit) return false;
            const auto t = word(o);
            if (t == all || t == all - 1)
            {
                if (o + (size_t) W + 4 > limit) return false;
                const auto n = b.u32(o + (size_t) W);
                if (n < 2 || n > 90 || o + (size_t) W + 4 + n + 2 > limit) return false;
                const auto* name = b.d + o + (size_t) W + 4;
                if (name[n - 1] != 0 || ! validClassName(name, n - 1)) return false;
                const int version = b.u16(o + (size_t) W + 4 + n);
                chain.push_back({ { juce::String::fromUTF8(reinterpret_cast<const char*>(name), (int) n - 1), version }, o });
                o += (size_t) W + 4 + n + 2;
                if (t == all - 1) continue;
                out.cls = chain.back().first.first; out.version = version;
                break;
            }
            if ((t & ref) != 0 && chain.empty())
                if (auto it = classes.find(t & ~ref); it != classes.end())
                { out.cls = it->second.first; out.version = it->second.second; o += (size_t) W; break; }
            return false;
        }
        if (o + (size_t) W > limit) return false;
        const auto size = word(o);
        if (size > limit - (o + (size_t) W)) return false;
        for (const auto& [cls, marker] : chain) classes[marker - base] = cls;
        sizeField = o; out.pa = o + (size_t) W; out.pb = out.pa + (size_t) size;
        return true;
    }

    void scan(size_t a, size_t end, int parent)
    {
        size_t o = a;
        while (o + 2 * (size_t) W <= end)
        {
            if (b.d[o] == 0xFF || b.d[o] == 0x80)
            {
                Obj object; size_t sizeField = 0;
                if (tryObject(o, end, object, sizeField))
                {
                    object.at = o; object.parent = parent;
                    const int index = (int) objs.size();
                    const auto pa = object.pa, pb = object.pb;
                    objs.push_back(std::move(object));
                    byId[(juce::uint64) (sizeField - base)] = index;
                    byAt[o] = index;
                    if (parent >= 0) objs[(size_t) parent].kids.push_back(index);
                    scan(pa, pb, index);
                    o = pb;
                    continue;
                }
            }
            ++o;
        }
    }
};

// ------------------------------------------------------------------ extractores (ver cpr_read.py)
std::vector<juce::String> ownStrings(const Arch& a, int index)
{
    const auto& ob = a.objs[(size_t) index];
    std::vector<std::pair<size_t, size_t>> ranges;
    size_t p = ob.pa;
    for (int k : ob.kids) { const auto& kid = a.objs[(size_t) k]; if (kid.at > p) ranges.push_back({ p, kid.at }); p = kid.pb; }
    if (ob.pb > p) ranges.push_back({ p, ob.pb });
    std::vector<juce::String> out;
    for (auto [from, to] : ranges)
        for (size_t o = from; o + 4 <= to;)
        {
            if (isString(a.b, o, to, 1024))
            {
                size_t end = 0;
                const auto s = readString(a.b, o, &end);
                bool printable = s.isNotEmpty();
                for (auto c : s) if (c < 32 || c == 127) { printable = false; break; }
                if (printable) { out.push_back(s); o = end; continue; }
            }
            ++o;
        }
    return out;
}

// «clave\0» + u16 tipo 1 (i64), precedida de su longitud u32; primera coincidencia dentro de [a, b).
bool findIntAttribute(const Bytes& b, size_t from, size_t to, const char* key, juce::int64& value)
{
    const auto keyLength = std::strlen(key) + 1;
    for (size_t i = from + 4; i + keyLength + 2 + 8 <= to; ++i)
        if (std::memcmp(b.d + i, key, keyLength) == 0 && b.u32(i - 4) == keyLength && b.u16(i + keyLength) == 1)
        { value = b.i64(i + keyLength + 2); return true; }
    return false;
}

void readFnPath(const Arch& a, int index, ClipInfo& clip)
{
    const auto& ob = a.objs[(size_t) index];
    size_t o = ob.pa;
    if (! isString(a.b, o, ob.pb)) return;
    clip.fileName = readString(a.b, o, &o);
    if (o + 4 > ob.pb) return;
    clip.fourcc = juce::String(reinterpret_cast<const char*>(a.b.d + o), 4);
    o += 4;
    for (int i = 0; i < 3; ++i) { if (! isString(a.b, o, ob.pb)) return; readString(a.b, o, &o); }
    o += 6; // u32 + u16 sin verificar
    if (isString(a.b, o, ob.pb)) clip.folder = readString(a.b, o);
}

// AudioFile v1: u64 frames, u16 bits, u16 bytes por frame, u16 canales, f32 Hz (verificado 99/99 en tom-01).
void readAudioFile(const Arch& a, int index, ClipInfo& clip)
{
    const auto& ob = a.objs[(size_t) index];
    if (ob.pb - ob.pa < 18) return;
    clip.frames = (juce::int64) a.b.be(ob.pa, 8);
    clip.bits = a.b.u16(ob.pa + 8); clip.bytesPerFrame = a.b.u16(ob.pa + 10); clip.channels = a.b.u16(ob.pa + 12);
    clip.sampleRate = a.b.f32(ob.pa + 14);
    clip.hasFormat = true;
}

juce::String count(int n) { return juce::String(n); }
}

double ProjectInfo::bpm() const noexcept
{
    if (secondsPerQuarter <= 0.0) return 0.0;
    const double raw = 60.0 / secondsPerQuarter, rounded = std::round(raw * 1000.0) / 1000.0;
    // El .cpr guarda segundos por negra en f32: 67 BPM se lee 66,999998. Se redondea a 0,001 BPM solo si
    // la diferencia está dentro de 2 ulp de f32 (2^-23 relativo); si no, se conserva el valor leído.
    return std::abs(raw - rounded) <= raw * 1.2e-7 ? rounded : raw;
}

juce::Result parse(const void* data, size_t size, ProjectInfo& out)
{
    out = {};
    Bytes b { static_cast<const juce::uint8*>(data), size };
    size_t header = 0;
    if (size >= 12 && std::memcmp(b.d, "RIFF", 4) == 0 && std::memcmp(b.d + 8, "NUND", 4) == 0) { out.wide = false; header = 12; }
    else if (size >= 16 && std::memcmp(b.d, "RIF2", 4) == 0 && std::memcmp(b.d + 12, "NUND", 4) == 0) { out.wide = true; header = 16; }
    else return juce::Result::fail(i18n::tr("No es un proyecto de Cubase (.cpr) reconocible."));
    const size_t ext = out.wide ? 8 : 4;
    // Bloques ROOT (nombre de sección) y ARCH (datos), tamaños big-endian.
    std::map<juce::String, std::pair<size_t, size_t>> sections;
    juce::String name;
    for (size_t o = header; o + 4 + ext <= size;)
    {
        const auto blockSize = b.be(o + 4, (int) ext);
        const size_t payload = o + 4 + ext;
        if (blockSize > size - payload) return juce::Result::fail(i18n::tr("El archivo .cpr está incompleto o dañado."));
        if (std::memcmp(b.d + o, "ROOT", 4) == 0 && blockSize >= 4)
        {
            // Nombre de sección: u32 longitud + texto, sin NUL obligatorio (p. ej. 00000007 "Version").
            const auto n = juce::jmin<juce::uint64>(b.u32(payload), blockSize - 4);
            size_t length = (size_t) n;
            while (length > 0 && b.d[payload + 4 + length - 1] == 0) --length;
            name = juce::String(reinterpret_cast<const char*>(b.d + payload + 4), length);
        }
        else if (std::memcmp(b.d + o, "ARCH", 4) == 0 && ! sections.count(name))
            sections[name] = { payload, (size_t) blockSize };
        o = payload + (size_t) blockSize;
    }
    if (auto it = sections.find("Version"); it != sections.end())
    {
        Arch version(b, it->second.first, it->second.second, out.wide);
        for (int i = 0; i < (int) version.objs.size(); ++i)
            if (version.objs[(size_t) i].cls == "PAppVersion") { for (auto& s : ownStrings(version, i)) out.version.add(s); break; }
    }
    const auto arrangement = sections.find("Arrangement1");
    if (arrangement == sections.end()) return juce::Result::fail(i18n::tr("El .cpr no contiene la sección del arreglo (Arrangement1)."));
    Arch a(b, arrangement->second.first, arrangement->second.second, out.wide);

    std::map<juce::String, int> classCount;
    std::unordered_map<int, int> trackOf, clipOf;
    for (int i = 0; i < (int) a.objs.size(); ++i)
    {
        const auto& ob = a.objs[(size_t) i];
        ++classCount[ob.cls];
        if (ob.cls == "MTempoTrackEvent" && out.secondsPerQuarter <= 0.0 && ob.pb - ob.pa >= 8)
        {
            // MTempoTrackEvent v3 (v2 en Cubase 12): u32 puntos + f32 segundos por negra del primero.
            const auto points = b.u32(ob.pa);
            const auto spq = b.f32(ob.pa + 4);
            if ((ob.version == 2 || ob.version == 3) && points >= 1 && points <= 100000 && spq > 0.01f && spq < 10.0f)
            { out.tempoPoints = (int) points; out.secondsPerQuarter = spq; }
            else out.warnings.add(i18n::tr("Tempo en un formato no reconocido: se usa 120 BPM."));
        }
        else if (ob.cls == "MSignatureTrackEvent" && out.numerator == 0)
        {
            juce::int64 numerator = 0, denominator = 0;
            if (findIntAttribute(b, ob.pa, ob.pb, "Numerator", numerator) && findIntAttribute(b, ob.pa, ob.pb, "Denominator", denominator)
                && numerator >= 1 && numerator <= 64 && denominator >= 1 && denominator <= 64)
            { out.numerator = (int) numerator; out.denominator = (int) denominator; }
            else out.warnings.add(i18n::tr("Compás en un formato no reconocido: se usa 4/4."));
        }
        else if (ob.cls == "MAudioTrackEvent")
        {
            TrackInfo track;
            if (int node = a.firstKid(i, "MListNode"); node >= 0 && isString(b, a.objs[(size_t) node].pa, a.objs[(size_t) node].pb))
                track.name = readString(b, a.objs[(size_t) node].pa);
            trackOf[i] = (int) out.tracks.size();
            out.tracks.push_back(track);
        }
        else if (ob.cls == "PAudioClip")
        {
            ClipInfo clip;
            if (isString(b, ob.pa, ob.pb)) clip.name = readString(b, ob.pa);
            if (int path = a.firstKid(i, "FNPath"); path >= 0) readFnPath(a, path, clip);
            if (int file = a.firstDescendant(i, "AudioFile"); file >= 0) readAudioFile(a, file, clip);
            clipOf[i] = (int) out.clips.size();
            out.clips.push_back(clip);
        }
    }
    if (out.tempoPoints > 1)
        out.warnings.add(i18n::tr("La pista de tempo tiene varios puntos; solo se importa el primero (formato no verificado)."));
    // Eventos: MAudioEvent v6 = u16, f64 inicio (ticks), f64 longitud y f64 offset (muestras), ranura PAudioClip.
    int otherVersions = 0, unresolved = 0;
    for (int i = 0; i < (int) a.objs.size(); ++i)
    {
        const auto& ob = a.objs[(size_t) i];
        if (ob.cls != "MAudioEvent") continue;
        if (ob.version != 6 || ! out.wide || ob.pa + 26 + (size_t) a.W > ob.pb) { ++otherVersions; continue; }
        EventInfo event;
        event.startTicks = b.f64(ob.pa + 2); event.lengthSamples = b.f64(ob.pa + 10); event.offsetSamples = b.f64(ob.pa + 18);
        if (int clip = a.slot(ob.pa + 26); clip >= 0 && clipOf.count(clip)) event.clip = clipOf[clip];
        if (int track = a.ancestor(i, "MAudioTrackEvent"); track >= 0) event.track = trackOf[track];
        if (event.clip < 0 || event.track < 0) ++unresolved;
        out.events.push_back(event);
    }
    if (otherVersions > 0)
        out.warnings.add(i18n::tr("Eventos de audio en un formato no verificado (versión o Cubase 12), omitidos: ") + count(otherVersions));
    if (unresolved > 0)
        out.warnings.add(i18n::tr("Eventos de audio sin clip o sin pista reconocibles: ") + count(unresolved));
    // Contenido presente que este tramo no importa.
    const auto omitted = [&](const char* cls, const char* what)
    { if (auto it = classCount.find(cls); it != classCount.end() && it->second > 0) out.warnings.add(i18n::tr(what) + count(it->second)); };
    omitted("MFolderTrack", "Pistas de carpeta no importadas (se importa su contenido sin jerarquía): ");
    omitted("MFadeIn", "Fundidos de entrada no importados: ");
    omitted("MFadeOut", "Fundidos de salida no importados: ");
    omitted("MAudioPartEvent", "Partes de audio no importadas: ");
    omitted("MAutomationTrackEvent", "Pistas de automatización no importadas: ");
    omitted("MMarkerTrackEvent", "Pistas de marcadores no importadas: ");
    int midi = 0, otherTracks = 0;
    for (const auto& [cls, n] : classCount)
    {
        if (cls.startsWith("MMidi") || cls.startsWith("MInstrument")) midi += n;
        else if (cls.endsWith("TrackEvent") && cls != "MAudioTrackEvent" && cls != "MTempoTrackEvent" && cls != "MSignatureTrackEvent"
                 && cls != "MAutomationTrackEvent" && cls != "MMarkerTrackEvent" && cls != "MDeviceTrackEvent")
            otherTracks += n;
    }
    if (midi > 0) out.warnings.add(i18n::tr("Objetos MIDI o de instrumento no importados: ") + count(midi));
    if (otherTracks > 0) out.warnings.add(i18n::tr("Otras pistas no importadas (grupo, FX, vídeo…): ") + count(otherTracks));
    out.warnings.add(i18n::tr("No se importan (sin verificar): volumen, pan, mute/solo, envíos, inserciones, color ni volumen de evento."));
    return juce::Result::ok();
}

juce::Result readProject(const juce::File& cpr, ProjectInfo& out)
{
    juce::MemoryBlock data;
    if (! cpr.existsAsFile() || ! cpr.loadFileAsData(data)) return juce::Result::fail(i18n::tr("No se puede leer el proyecto Cubase: ") + cpr.getFileName());
    return parse(data.getData(), data.getSize(), out);
}

bool readWavHeader(const juce::File& file, WavHeader& out)
{
    juce::FileInputStream in(file);
    if (! in.openedOk()) return false;
    char id[4], form[4];
    if (in.read(id, 4) != 4) return false;
    in.readInt();
    if (in.read(form, 4) != 4 || (std::memcmp(id, "RIFF", 4) != 0 && std::memcmp(id, "RF64", 4) != 0) || std::memcmp(form, "WAVE", 4) != 0) return false;
    bool haveFormat = false;
    juce::int64 ds64Data = -1;
    while (! in.isExhausted())
    {
        char tag[4];
        if (in.read(tag, 4) != 4) return false;
        const auto chunk = (juce::uint32) in.readInt();
        const auto next = in.getPosition() + (juce::int64) chunk + (chunk & 1u);
        if (std::memcmp(tag, "fmt ", 4) == 0 && chunk >= 16)
        {
            in.readShort(); out.channels = (juce::uint16) in.readShort(); out.sampleRate = (juce::uint32) in.readInt();
            in.readInt(); out.blockAlign = (juce::uint16) in.readShort(); out.bits = (juce::uint16) in.readShort();
            haveFormat = true;
        }
        else if (std::memcmp(tag, "ds64", 4) == 0 && chunk >= 24) { in.readInt64(); ds64Data = in.readInt64(); }
        else if (std::memcmp(tag, "data", 4) == 0)
        {
            if (! haveFormat || out.blockAlign <= 0) return false;
            const juce::int64 bytes = (chunk == 0xFFFFFFFFu && ds64Data >= 0) ? ds64Data : (juce::int64) chunk;
            out.frames = bytes / out.blockAlign;
            return true;
        }
        if (! in.setPosition(next)) return false;
    }
    return false;
}

std::vector<ClipResolution> resolveClips(const ProjectInfo& project, const juce::File& cpr)
{
    std::vector<ClipResolution> out;
    const auto audioFolder = cpr.getParentDirectory().getChildFile("Audio");
    for (const auto& clip : project.clips)
    {
        ClipResolution r;
        r.nameDiffers = clip.name.isNotEmpty() && clip.fileName.isNotEmpty() && ! clip.fileName.startsWith(clip.name);
        if (clip.fileName.isEmpty() || clip.fileName.containsAnyOf("/\\"))
        { r.reason = i18n::tr("el clip no tiene un nombre de archivo legible"); out.push_back(r); continue; }
        juce::Array<juce::File> candidates { audioFolder.getChildFile(clip.fileName) };
        if (juce::File::isAbsolutePath(clip.folder)) candidates.add(juce::File(clip.folder).getChildFile(clip.fileName));
        for (const auto& candidate : candidates) if (candidate.existsAsFile()) { r.file = candidate; break; }
        if (r.file == juce::File()) { r.reason = i18n::tr("archivo no encontrado en Audio/ ni en la carpeta guardada"); out.push_back(r); continue; }
        WavHeader wav;
        // La FourCC del FNPath no sirve de prueba: en tom-01 vale "WAVE" o 00000000 para WAV reales.
        if (! clip.hasFormat || ! readWavHeader(r.file, wav))
        { r.status = ClipStatus::unsupported; r.reason = i18n::tr("formato no verificable (solo WAV con datos de formato en el .cpr)"); out.push_back(r); continue; }
        juce::StringArray differences;
        if (wav.frames != clip.frames) differences.add(i18n::tr("frames ") + juce::String(clip.frames) + " / " + juce::String(wav.frames));
        if (wav.channels != clip.channels) differences.add(i18n::tr("canales ") + juce::String(clip.channels) + " / " + juce::String(wav.channels));
        if (std::abs(wav.sampleRate - clip.sampleRate) > 0.5) differences.add(i18n::tr("frecuencia ") + juce::String(clip.sampleRate, 0) + " / " + juce::String(wav.sampleRate, 0));
        if (wav.bits != clip.bits) differences.add(i18n::tr("bits ") + juce::String(clip.bits) + " / " + juce::String(wav.bits));
        if (! differences.isEmpty())
        { r.status = ClipStatus::mismatch; r.reason = i18n::tr("no coincide con el WAV (.cpr / archivo): ") + differences.joinIntoString(", "); out.push_back(r); continue; }
        r.status = ClipStatus::verified;
        out.push_back(r);
    }
    return out;
}

juce::Result prepare(AudioEngine& engine, const juce::File& cpr, PreparedImport& out, const std::function<bool(double)>& progress)
{
    out = {};
    out.source = cpr;
    if (auto result = readProject(cpr, out.project); result.failed()) return result;
    const auto& project = out.project;
    out.resolution = resolveClips(project, cpr);
    out.audio.assign(project.clips.size(), nullptr);
    out.eventSkip.assign(project.events.size(), {});
    std::vector<bool> used(project.clips.size(), false);
    for (size_t e = 0; e < project.events.size(); ++e)
    {
        const auto& event = project.events[e];
        auto& skip = out.eventSkip[e];
        if (event.track < 0) { skip = i18n::tr("sin pista de audio reconocible"); continue; }
        if (event.clip < 0) { skip = i18n::tr("sin clip reconocible"); continue; }
        const auto& clip = project.clips[(size_t) event.clip];
        const auto& resolution = out.resolution[(size_t) event.clip];
        if (resolution.status != ClipStatus::verified) { skip = i18n::tr("clip no verificado: ") + resolution.reason; continue; }
        // Tolerancia de media muestra: longitud y offset son f64 fraccionarios (exceso máximo observado 0,049).
        if (! (event.lengthSamples > 0.0) || ! (event.offsetSamples >= 0.0) || event.offsetSamples + event.lengthSamples > (double) clip.frames + 0.5)
        { skip = i18n::tr("el evento sale de los límites del archivo"); continue; }
        used[(size_t) event.clip] = true;
    }
    const int toLoad = (int) std::count(used.begin(), used.end(), true);
    int loaded = 0;
    for (size_t c = 0; c < project.clips.size(); ++c)
    {
        if (! used[c]) continue;
        auto clip = std::make_shared<AudioClip>();
        if (auto result = engine.readAudio(out.resolution[c].file, *clip); result.failed())
        {
            out.loadErrors.add(out.resolution[c].file.getFileName() + ": " + result.getErrorMessage());
            for (size_t e = 0; e < project.events.size(); ++e)
                if (project.events[e].clip == (int) c && out.eventSkip[e].isEmpty()) out.eventSkip[e] = i18n::tr("no se pudo leer el audio");
        }
        else out.audio[c] = std::move(clip);
        if (progress && ! progress(++loaded / (double) juce::jmax(1, toLoad))) return juce::Result::fail("cancel");
    }
    return juce::Result::ok();
}

ImportSummary apply(AudioEngine& engine, const PreparedImport& in)
{
    const auto& project = in.project;
    ImportSummary summary;
    juce::StringArray warnings(project.warnings);
    engine.clear();
    double bpm = project.bpm();
    if (bpm <= 0.0) bpm = 120.0;
    if (bpm < domain::minimumBpm || bpm > domain::maximumBpm)
        warnings.add(i18n::tr("El tempo queda fuera del rango de Silente y se limita: ") + juce::String(bpm, 3));
    int numerator = 4, denominator = 4;
    if (project.numerator > 0 && domain::validDenominator(project.denominator)) { numerator = project.numerator; denominator = project.denominator; }
    else if (project.numerator > 0) warnings.add(i18n::tr("Denominador de compás no admitido; se usa 4/4."));
    {
        const juce::ScopedLock lock(engine.modelLock);
        engine.tempo = juce::jlimit(domain::minimumBpm, domain::maximumBpm, bpm);
        engine.tempoMap = {};
        if (numerator != 4 || denominator != 4) engine.tempoMap.signatures.push_back({ 0, numerator, denominator });
        engine.tempoMap.normalise();
        engine.projectName = in.source.getFileNameWithoutExtension();
    }
    std::vector<std::shared_ptr<Track>> tracks;
    for (const auto& track : project.tracks) tracks.push_back(engine.addAudioTrack(track.name));
    const auto timeline = engine.tempoView();
    std::vector<int> eventsPerTrack(project.tracks.size(), 0);
    std::vector<bool> clipImported(project.clips.size(), false), clipUsed(project.clips.size(), false);
    int negativeStarts = 0;
    juce::StringArray skipped;
    for (size_t e = 0; e < project.events.size(); ++e)
    {
        const auto& event = project.events[e];
        if (event.clip >= 0) clipUsed[(size_t) event.clip] = true;
        const auto audio = event.clip >= 0 ? in.audio[(size_t) event.clip] : nullptr;
        if (in.eventSkip[e].isNotEmpty() || audio == nullptr || event.track < 0)
        {
            const auto clipName = event.clip >= 0 ? project.clips[(size_t) event.clip].name : juce::String("?");
            const auto trackName = event.track >= 0 ? project.tracks[(size_t) event.track].name : juce::String("?");
            skipped.add("  " + trackName + " / " + clipName + " @ " + juce::String(event.startTicks, 3) + " ticks: "
                        + (in.eventSkip[e].isNotEmpty() ? in.eventSkip[e] : i18n::tr("no se pudo leer el audio")));
            continue;
        }
        AudioClip clip = *audio;
        clip.id = juce::Uuid().toString();
        if (project.clips[(size_t) event.clip].name.isNotEmpty()) clip.name = project.clips[(size_t) event.clip].name;
        const double rate = clip.sourceRate > 0.0 ? clip.sourceRate : 48000.0;
        if (event.startTicks < 0.0) ++negativeStarts;
        clip.startSeconds = timeline.secondsAt(juce::jmax(0.0, event.startTicks) / ticksPerQuarter);
        clip.offsetSeconds = event.offsetSamples / rate;
        const double available = clip.audio ? clip.audio->getNumSamples() / rate - clip.offsetSeconds : 0.0;
        clip.durationSeconds = juce::jlimit(0.0, juce::jmax(0.0, available), event.lengthSamples / rate);
        {
            const juce::ScopedLock lock(engine.modelLock);
            tracks[(size_t) event.track]->clips.push_back(std::move(clip));
        }
        ++eventsPerTrack[(size_t) event.track];
        clipImported[(size_t) event.clip] = true;
        ++summary.eventsImported;
    }
    if (negativeStarts > 0) warnings.add(i18n::tr("Eventos antes del inicio del proyecto, colocados en 0: ") + juce::String(negativeStarts));
    warnings.add(i18n::tr("Fundidos: se aplica el fundido mínimo de Silente (5 ms); los del .cpr no se leen."));
    for (const auto& r : in.resolution) if (r.status == ClipStatus::verified) ++summary.clipsVerified;
    for (size_t c = 0; c < project.clips.size(); ++c) { if (clipImported[c]) ++summary.clipsImported; if (! clipUsed[c]) ++summary.clipsUnused; }
    for (const auto& clip : project.clips) if (clip.channels > 2 && clip.hasFormat)
    { warnings.add(i18n::tr("Clips de más de 2 canales: Silente reproduce los 2 primeros.")); break; }
    summary.tracks = (int) project.tracks.size(); summary.clips = (int) project.clips.size(); summary.events = (int) project.events.size();
    summary.bpm = engine.tempo.load(); summary.numerator = numerator; summary.denominator = denominator;

    // Informe legible.
    juce::StringArray r;
    r.add(i18n::tr("Importación de proyecto Cubase (.cpr)"));
    r.add(i18n::tr("Origen: ") + in.source.getFullPathName());
    r.add(i18n::tr("Versión: ") + project.version.joinIntoString(juce::String::fromUTF8(" · ")) + (project.wide ? " (RIF2)" : " (RIFF)"));
    r.add(i18n::tr("Tempo: ") + juce::String(summary.bpm, 3) + " BPM" + (project.secondsPerQuarter > 0.0 ? juce::String() : i18n::tr(" (no leído; por defecto)")));
    r.add(i18n::tr("Compás: ") + juce::String(numerator) + "/" + juce::String(denominator) + (project.numerator > 0 ? juce::String() : i18n::tr(" (no leído; por defecto)")));
    r.add(i18n::tr("Pistas de audio: ") + juce::String(summary.tracks));
    r.add(i18n::tr("Clips: ") + juce::String(summary.clips) + i18n::tr(" · verificados con el WAV: ") + juce::String(summary.clipsVerified)
          + i18n::tr(" · importados: ") + juce::String(summary.clipsImported) + i18n::tr(" · sin eventos: ") + juce::String(summary.clipsUnused));
    r.add(i18n::tr("Eventos de audio: ") + juce::String(summary.events) + i18n::tr(" · importados: ") + juce::String(summary.eventsImported)
          + i18n::tr(" · omitidos: ") + juce::String(summary.events - summary.eventsImported));
    r.add({});
    r.add(i18n::tr("Pistas (eventos importados):"));
    for (size_t t = 0; t < project.tracks.size(); ++t) r.add("  " + juce::String((int) t + 1) + ". " + project.tracks[t].name + " (" + juce::String(eventsPerTrack[t]) + ")");
    juce::StringArray unverified, renamed;
    for (size_t c = 0; c < project.clips.size(); ++c)
    {
        const auto& clip = project.clips[c];
        const auto& res = in.resolution[c];
        if (res.status != ClipStatus::verified) unverified.add("  " + clip.name + " (" + clip.fileName + "): " + res.reason);
        if (res.nameDiffers) renamed.add("  " + clip.name + " -> " + clip.fileName);
    }
    const auto section = [&r](const juce::String& title, const juce::StringArray& lines)
    { if (lines.isEmpty()) return; r.add({}); r.add(title + " " + juce::String(lines.size())); r.addArray(lines); };
    section(i18n::tr("Clips no verificados (no importados):"), unverified);
    section(i18n::tr("Clips cuyo nombre no coincide con el archivo (importados; el archivo se verificó por su cabecera):"), renamed);
    section(i18n::tr("Eventos omitidos:"), skipped);
    juce::StringArray loadErrors; for (const auto& e : in.loadErrors) loadErrors.add("  " + e);
    section(i18n::tr("Errores al leer audio:"), loadErrors);
    juce::StringArray warningLines; for (const auto& w : warnings) warningLines.add("  - " + w);
    section(i18n::tr("Avisos:"), warningLines);
    summary.report = r.joinIntoString("\n") + "\n";
    return summary;
}

juce::File reportFileFor(const juce::File& project)
{
    return project.getSiblingFile(project.getFileNameWithoutExtension() + i18n::tr(" - importación Cubase") + ".txt");
}

bool isInsideCubaseFolder(const juce::File& destination, const juce::File& cpr)
{
    const auto folder = cpr.getParentDirectory();
    return destination == cpr || destination.getParentDirectory() == folder || destination.isAChildOf(folder);
}
}

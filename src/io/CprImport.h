#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#include <memory>
#include <vector>

namespace silente
{
class AudioEngine;
struct AudioClip;
}

namespace silente::io::cpr
{
// Lectura de proyectos Cubase (.cpr) en modo solo lectura (docs/CPR_INVESTIGACION.md, tramos 1–2;
// misma gramática que tools/cpr/cpr_read.py). Solo se extraen los campos verificados con evidencia
// independiente: tempo (1 punto), compás, nombres de pistas de audio, clips (archivo y formato) y
// eventos de audio MAudioEvent v6. Todo lo demás se cuenta y se informa como omitido.
constexpr double ticksPerQuarter = 480.0;

struct TrackInfo { juce::String name; };

struct ClipInfo
{
    juce::String name, fileName, fourcc, folder; // nombre del clip; archivo y carpeta guardados (FNPath)
    bool hasFormat = false;                       // AudioFile leído
    juce::int64 frames = 0;
    int bits = 0, bytesPerFrame = 0, channels = 0;
    double sampleRate = 0.0;
};

struct EventInfo
{
    int track = -1, clip = -1;   // índices en ProjectInfo::tracks / clips (-1 = sin resolver)
    double startTicks = 0.0;     // 480 por negra desde el inicio del proyecto
    double lengthSamples = 0.0, offsetSamples = 0.0; // muestras del archivo del clip
};

struct ProjectInfo
{
    bool wide = true;            // RIF2 (Cubase 13.0.30+: 8 bytes) o RIFF (4 bytes)
    juce::StringArray version;   // cadenas de PAppVersion
    int tempoPoints = 0;
    double secondsPerQuarter = 0.0; // 0 = tempo no leído
    int numerator = 0, denominator = 0; // 0 = compás no leído
    std::vector<TrackInfo> tracks;
    std::vector<ClipInfo> clips;
    std::vector<EventInfo> events;
    juce::StringArray warnings;  // contenido presente que no se importa, en el idioma activo
    double bpm() const noexcept; // 60/s por negra, redondeado a 0,001 si la diferencia cabe en la precisión f32
};

// Analiza el contenido del .cpr ya cargado en memoria. No escribe nada.
juce::Result parse(const void* data, size_t size, ProjectInfo&);
juce::Result readProject(const juce::File& cpr, ProjectInfo&);

struct WavHeader
{
    juce::int64 frames = 0;
    int channels = 0, bits = 0, blockAlign = 0;
    double sampleRate = 0.0;
};
// Cabecera de un WAV/RF64 (fmt y data); solo lectura.
bool readWavHeader(const juce::File&, WavHeader&);

enum class ClipStatus { verified, missing, mismatch, unsupported };
struct ClipResolution
{
    juce::File file;
    ClipStatus status = ClipStatus::missing;
    juce::String reason;     // vacío si está verificado
    bool nameDiffers = false; // el nombre del clip no es el del archivo (se informa; no impide importar)
};
// Busca cada clip primero en «Audio/» junto al .cpr y luego en la carpeta guardada; lo acepta solo
// si frames, canales, frecuencia y bits coinciden con la cabecera del WAV.
std::vector<ClipResolution> resolveClips(const ProjectInfo&, const juce::File& cpr);

// Resultado de preparar la importación (hilo de trabajo): datos leídos, resolución y audio cargado.
struct PreparedImport
{
    juce::File source;
    ProjectInfo project;
    std::vector<ClipResolution> resolution;
    std::vector<std::shared_ptr<AudioClip>> audio; // por clip; nullptr si no verificado o ilegible
    std::vector<juce::String> eventSkip;           // por evento: motivo de omisión (vacío = se importa)
    juce::StringArray loadErrors;
};

// Lee el .cpr, resuelve y verifica los archivos y decodifica el audio verificado. No toca el modelo
// del proyecto: puede ejecutarse fuera del hilo de mensajes. `progress` (0–1) devuelve false para cancelar.
juce::Result prepare(AudioEngine&, const juce::File& cpr, PreparedImport&, const std::function<bool(double)>& progress = {});

struct ImportSummary
{
    int tracks = 0, clips = 0, clipsVerified = 0, clipsImported = 0, clipsUnused = 0;
    int events = 0, eventsImported = 0;
    double bpm = 0.0;
    int numerator = 0, denominator = 0;
    juce::String report; // informe legible (idioma activo)
};

// Sustituye el proyecto del motor por el importado (hilo de mensajes; el transporte se detiene).
ImportSummary apply(AudioEngine&, const PreparedImport&);

// Nombre del informe junto al proyecto guardado: «<proyecto> - importación Cubase.txt».
juce::File reportFileFor(const juce::File& silenteProject);
// Silente nunca escribe en la carpeta del .cpr (ni en sus subcarpetas).
bool isInsideCubaseFolder(const juce::File& destination, const juce::File& cpr);
}

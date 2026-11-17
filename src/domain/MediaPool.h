#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#include <set>
#include <vector>

namespace silente::domain
{
// Pool de medios: un medio es un archivo de origen usado por uno o más clips de audio. Funciones
// puras sobre una lista plana de referencias a clips; no leen disco (la presencia del archivo la
// responde el llamador) ni tocan el motor, así que valen para el hilo de mensajes y las pruebas.
struct MediaClipRef
{
    juce::String trackId, clipId;
    juce::String path;            // ruta guardada del archivo de origen; vacía = audio solo en memoria
    juce::String name;            // nombre de archivo mostrado
    long long lengthSamples = 0;  // longitud del archivo en muestras (0 = desconocida, medio no cargado)
    int channels = 0;
    double sampleRate = 0.0;
};

enum class MediaState { present, missing, replaced };

struct MediaEntry
{
    juce::String path, name;
    long long lengthSamples = 0;
    double seconds = 0.0;
    int channels = 0;
    double sampleRate = 0.0;
    int uses = 0;
    MediaState state = MediaState::present;
    std::vector<MediaClipRef> clips; // quién lo usa, en orden de pista y de clip
};

enum class MediaColumn { name, path, duration, uses, state };

// Agrupa por ruta (los clips sin ruta forman un medio propio cada uno). `exists` responde si la
// ruta está en disco; `replaced` contiene las rutas puestas por una reasignación de esta sesión.
std::vector<MediaEntry> buildPool(const std::vector<MediaClipRef>& clips,
                                  const std::function<bool(const juce::String& path)>& exists,
                                  const std::set<juce::String>& replaced = {});
// Orden estable por columna; los empates conservan el orden previo.
void sortPool(std::vector<MediaEntry>&, MediaColumn, bool ascending);
// Pone `toPath` en todas las referencias cuya ruta sea `fromPath`; devuelve cuántas cambió.
int reassignPath(std::vector<MediaClipRef>&, const juce::String& fromPath, const juce::String& toPath);
}

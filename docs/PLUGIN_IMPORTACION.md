# Importación de plugins: formatos y carpeta adicional

Etapa 1 (19-11-2026). Referencia UX: en Cubase, el Plug-in Manager permite añadir rutas de VST 3 además de las del sistema. Silente reutiliza su escáner fuera de proceso (`PluginManager::Scanner` lanza `--enumerate-plugins` y `--scan-plugin` en procesos hijos); el hilo de audio no participa.

## Matriz de formatos (macOS, JUCE 8.0.12 en `third_party/JUCE`)

| Formato | Estado en Silente | Base verificada |
|---|---|---|
| VST3 | Se importa hoy. Rutas estándar + carpeta adicional. | `JUCE_PLUGINHOST_VST3=1` en `CMakeLists.txt`; SDK VST3 incluido en `juce_audio_processors_headless/format_types/VST3_SDK`. `VST3PluginFormatHeadless::searchPathsForPlugins` recorre cualquier carpeta y solo comprueba la extensión `.vst3`. |
| Audio Unit (v2 y v3) | Se importa hoy, solo los registrados por macOS. | `JUCE_PLUGINHOST_AU=1`. `AudioUnitPluginFormatHeadless::searchPathsForPlugins` ignora las rutas y enumera `AudioComponentFindNext`; `getDefaultLocationsToSearch()` devuelve vacío. Un `.component` fuera de `/Library/Audio/Plug-Ins/Components` o `~/Library/Audio/Plug-Ins/Components` no se puede cargar: Silente lo cuenta como «AU sin registrar» y no lo mueve. |
| LV2 | Posible sin SDK externo, no activado. | `JUCE_PLUGINHOST_LV2=0` por defecto; JUCE incluye `LV2_SDK`. No hay LV2 instalados en este Mac. |
| VST2 (`.vst`) | Fuera. Requiere SDK de terceros. | `juce_VSTPluginFormatImpl.h` incluye `pluginterfaces/vst2.x/aeffect.h`, que no está en JUCE; Steinberg ya no licencia el SDK VST2. `JUCE_PLUGINHOST_VST=0`. |
| AAX (Pro Tools, `.aaxplugin`) | Fuera. **Bloqueo**. | JUCE solo trae el SDK AAX para *crear* plugins (`juce_audio_plugin_client/AAX/SDK`, licencia de Avid). JUCE no tiene clase de alojamiento AAX y Avid no ofrece alojamiento AAX a terceros con ese SDK. No se intentó obtener ningún SDK. |
| CLAP (`.clap`) | Fuera en JUCE 8. | JUCE 8.0.12 no contiene un formato de alojamiento CLAP. Requeriría una biblioteca aparte (no evaluada). |
| ARA | Fuera. Requiere SDK de Celemony. | `JUCE_PLUGINHOST_ARA` pide `juce_set_ara_sdk_path()`. |

## Inventario de este Mac (19-11-2026, solo lectura)

- `/Library/Audio/Plug-Ins/VST3`: 13 (Archetype Plini X, Mesa Boogie Mark IIC+ Suite, TONE3000, 10 WaveShell1-VST3 14.12–14.31).
- `/Library/Audio/Plug-Ins/Components`: 13 (los mismos en AU).
- `/Library/Audio/Plug-Ins/VST`: 13 VST2 (no compatibles).
- `/Library/Application Support/Avid/Audio/Plug-Ins`: 13 AAX (no compatibles).
- `/Library/Audio/Plug-Ins/CLAP`: 1 (TONE3000.clap).
- Carpetas de usuario `~/Library/Audio/Plug-Ins/{VST3,Components,VST,LV2,CLAP}`: vacías.

Cada AAX instalado tiene su gemelo VST3 y AU, así que el usuario no pierde ningún plugin por la falta de AAX.

## Implementado en esta etapa

- Preferencia `plugins.xml` (`app::pluginSearchFolder` / `savePluginSearchFolder`) y página **Ajustes › Plugins**: elegir o quitar la carpeta, lanzar la búsqueda e inventario de la carpeta.
- El escáner pasa la carpeta a `--enumerate-plugins`; VST3 la recorre junto a las rutas estándar. El resumen final informa VST3, AU sin registrar y AAX/VST2/CLAP no compatibles.
- El selector de plugins muestra bajo cada nombre su origen: «Importado · ruta», «Sistema · ruta», «Usuario · ruta» o «Audio Units registrados en macOS» (`plugins::describeOrigin`), con la ruta completa como tooltip.

## Diseño propuesto, no implementado: plugins de sesiones de otros DAW

Una sesión `.ptx` (Pro Tools) es binaria y no documentada; Silente no la lee. Si más adelante se importa una sesión de otro DAW, sus plugins solo pueden mapearse **por nombre y fabricante** a un plugin del catálogo (por ejemplo, AAX «Mesa Boogie Mark IIC+ Suite» → VST3 o AU del mismo nombre). El estado interno del plugin no es transferible entre formatos sin el formato del fabricante, por lo que el mapeo cargaría el plugin con valores por defecto y lo indicaría al usuario. La importación de Cubase `.cpr` existente (`src/io/CprImport.cpp`) sería el primer candidato para probar este mapeo.

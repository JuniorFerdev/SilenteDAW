# Silente — separación de responsabilidades

Estado a 29-10-2026: tema visual extraído a src/ui/theme/ (Colours, Drawing, SilenteLookAndFeel, ToolIcons, TrackColourPicker); traducciones y preferencia de idioma en src/i18n/. La extracción del resto de subsistemas sigue pendiente. La etapa activa sigue registrada en ETAPAS.md. Este mapa no autoriza iniciar varias etapas en paralelo.

## Situación actual

- `src/ui/settings/SettingsWorkspace.*`: ventana de preferencias con índice de búsqueda y páginas de UI reutilizadas (audio, metrónomo, comandos) y páginas de apariencia/transporte. Recibe callbacks del coordinador para aplicar transporte y escala; no modifica muestras ni procesa audio.
- `src/app/Preferences.*`: ubicación de preferencias globales y lectura/escritura de escala de interfaz. El modo de pruebas suministra un directorio aislado; `MainComponent::settingsFolder` conserva su API y delega aquí. No es el serializador del proyecto.

- src/i18n/: Translations carga catálogos empaquetados, selecciona idioma y conserva la preferencia; UiTranslation actualiza botones y ayudas sin tocar nombres editables. resources/locales/es.json y en.json contienen los textos; resources/macos/ contiene los mensajes de permiso de micrófono. tests/LocalisationUiTests.cpp comprueba el cambio de idioma y conservación de datos.
- src/ui/plugins/: PluginSelector contiene búsqueda, grupos y favoritos; PluginSlotButton separa clic de editor y menú de opciones. src/plugins/PluginSelection.h contiene filtros y correspondencia entre instrumentos e insertos. La carga y ventanas todavía se coordinan en PluginManager.
- src/Main.cpp: entrada, menús, ventana y modos auxiliares.
- src/MainComponent.cpp: interfaz, tema, pistas, piano roll, mezclador visual, diálogos, historial y coordinación. Aproximadamente72KB: separación insuficiente.
- src/AudioEngine.cpp: dispositivo, reproducción, mezcla, síntesis, MIDI, grabación, plugins, importación y persistencia. Aproximadamente53KB: separación insuficiente.
- src/PluginManager.cpp: catálogo, procesos de exploración, destinos de selección, carga y ventanas de plugins. Selector visual extraído; conviene continuar separando explorador y ventanas del servicio por tramos.
- tests/EngineTests.cpp: pruebas del motor; consultar evidencia de ejecución antes de declararlas aprobadas.

## Módulos objetivo

| Carpeta | Responsabilidad | Ejemplos |
|---|---|---|
| src/app/ | Entrada y coordinación de ventanas | SilenteApplication, WindowManager |
| src/domain/ | Datos y reglas del proyecto sin ventanas ni dispositivos | Project, Track, AudioClip, MidiNote |
| src/application/ | Comandos de edición, coordinación e historial | ProjectController, EditCommands, UndoHistory |
| src/audio/ | Dispositivo, transporte, reproducción, mezcla y grabación | AudioDeviceService, Transport, AudioMixer, Recorder |
| src/audio/dsp/ | Procesamiento de muestras | PanLaw, Equalizer, Fade, DelayCompensation |
| src/midi/ | Entrada, importación, planificación y síntesis interna | MidiInputService, MidiImporter, MidiScheduler, InternalSynth |
| src/plugins/ | Descubrimiento aislado, catálogo, instancias, estado y cadena | PluginScanner, PluginCatalog, PluginHost, PluginChain |
| src/persistence/ | Proyectos, medios y preferencias | ProjectSerializer, MediaStore, PreferencesStore |
| src/ui/theme/ | Colores, tamaños, tipografía e iconos | Colours, Metrics, SilenteLookAndFeel |
| src/ui/project/ | Pistas, eventos, ondas y regla | ProjectView, TrackList, TimelineView |
| src/ui/mixer/ | Canales y faders visuales | MixerView, ChannelStrip, ChannelSettings |
| src/ui/midi/ | Piano roll y teclado visual | PianoRollView, KeyboardView |
| src/ui/plugins/ | Selector y ventanas de editores | PluginBrowser, PluginEditorWindow |
| src/ui/transport/ y src/ui/dialogs/ | Transporte visual y diálogos | TransportBar, AudioSettingsDialog |
| tests/ | Pruebas de cada módulo e integración | audio/, midi/, plugins/, persistence/ |

## Límites

La UI usa comandos de application y lee estado; no implementa DSP. domain no depende de UI. audio y midi no dependen de ventanas. plugins administra procesadores y expone una cadena al motor; ui/plugins administra sus ventanas. persistence serializa datos y estado sin dibujar ni mezclar. app conecta los servicios. Colores visuales centralizados; la identidad de color de cada pista se conserva como dato de proyecto.

Separar por responsabilidades y contratos, no crear un archivo por cada función. Cabeceras públicas pequeñas y parejas .h/.cpp cuando corresponda. JUCE y demás dependencias conservan su código/licencias en third_party; CMake declara las fuentes y dependencias.

## Primer tramo de reorganización implementado; revisión pendiente

Se extrajo el tema visual hacia ui/theme/ y se compiló. El usuario amplió este tramo con colores de pista y una apariencia más cercana a Cubase; cambios y verificación en TRAMO_VISUAL_01.md. Entrega: diff pequeño, app que abre y mapa actualizado. Después, un tramo independiente para cada vista o servicio. No mover todo el motor y la interfaz en una sola operación.

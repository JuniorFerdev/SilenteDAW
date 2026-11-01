# Silente · plugins, cursor y arranque vacío · 29-10-2026

## Comportamiento entregado

La apertura normal crea una sesión vacía, con transporte detenido y sin pistas. Abrir explícitamente un archivo .silente sigue cargando ese archivo. La sesión de ejemplo queda como acción voluntaria en Medios; --demo la conserva para demostraciones y pruebas.

Un clic en la regla, encima de las pistas, coloca el cursor exactamente bajo el ratón. Mientras se mantiene el botón y se arrastra, el cursor sigue la coordenada horizontal, incluso si se baja hacia las pistas. Al soltar termina el gesto. No cuantiza el cursor a la rejilla de edición. Los tiradores de ciclo de la franja superior conservan su gesto independiente.

Los insertos del Inspector, el mezclador y la ventana de canal abren el selector de efectos al estar vacíos. Un clic sobre un efecto cargado abre su editor. La flecha o el clic derecho permiten abrir, sustituir, activar/desactivar bypass y quitar el efecto. La sustitución conserva las otras instancias y prepara el reemplazo antes de retirar la instancia anterior.

El selector ofrece búsqueda por palabras, grupos de categoría o fabricante, filtros VST3/AudioUnit y favoritos persistentes. Doble clic o Intro cargan la selección. Buscar plugins explora las ubicaciones estándar mediante procesos separados; Apple AU y plugins individuales se priorizan antes de grandes shells. No se inicia una exploración completa automáticamente.

El botón «+ Instrumento» y el botón del rack abren la selección de instrumento. La pista solo se crea después de elegir y cargar con éxito. El instrumento tiene un selector separado en el Inspector; su flecha lo sustituye. El sintetizador interno aparece siempre como alternativa. Los instrumentos externos se colocan antes de la cadena de efectos. Los destinos de cargas asíncronas se conservan por identidad de pista y ranura; eliminar una pista no redirige su carga a otra.

## Referencia y alcance

El flujo toma como referencia los [insertos de Cubase Pro 15](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/audio_effects/audio_effects_insert_effects_adding_t.html) y su [selector de efectos](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/audio_effects/audio_effects_vst_effect_selector_r.html). Tres intentos de acceso por la herramienta de control a Cubase terminaron en timeout; no se observaron sus ventanas ni se modificó su activación.

Es una primera implementación del flujo, no una reproducción completa de Cubase. Inspector muestra cuatro efectos, ventana de canal seis y cada tira del mezclador dos accesos. Las ranuras se compactan al quitar efectos. Arrastrar/reordenar insertos, colecciones avanzadas, sidechain y ruteo multisalida quedan pendientes. Los editores, categorías y nombres proporcionados por terceros pueden conservar textos del fabricante.

## Código

- src/ui/plugins/PluginSelector.h/.cpp: selector visual, agrupación, búsqueda y favoritos.
- src/ui/plugins/PluginSlotButton.h: acceso al editor y a las opciones de una ranura.
- src/plugins/PluginSelection.h: filtros y correspondencia entre instrumentos y números de inserto.
- src/PluginManager.h/.cpp: selección, carga asíncrona, destino estable y ventanas nativas.
- src/AudioEngine.h/.cpp: preparación y sustitución de procesadores; instrumentos antes de efectos.
- src/MainComponent.cpp: gesto de regla y conexiones con Inspector, rack, canal y mezclador. La extracción de la vista de arreglo sigue pendiente.
- resources/locales/: textos propios en español e inglés; 336 entradas por idioma.
- tests/WorkflowUiTests.cpp y scripts/check_workflow.sh: comprobaciones reproducibles; tarea disponible en Visual Studio Code.

## Verificación

Compilación Release y prueba de flujo aprobadas. Se verifican arranque sin pistas; clic, arrastre, liberación y límite inicial del cursor; independencia de localizadores; filtros, selección con Intro y guardado de favoritos; carga AU asíncrona, bypass, inserción en serie, sustitución y retirada; creación de pista con instrumento; regreso al sintetizador interno; y callback tardío tras eliminar su pista.

Plugins concretos probados: Apple AULowpass (AudioUnit), Apple DLSMusicDevice (instrumento AudioUnit) y TONE3000 0.0.9 (VST3 instalado). AU y VST3 procesaron un bloque offline sin muestras no finitas. Esto no equivale a una prueba audible ni a validación de todos los plugins. La captura interna de TONE3000 resulta negra porque su superficie nativa no se incluye en el snapshot JUCE; la revisión visual interactiva del editor sigue pendiente. macOS emitió avisos de clases duplicadas de CoreAudio al crear su editor AU; la ejecución terminó correctamente.

Ejecutar `./scripts/check_workflow.sh /Library/Audio/Plug-Ins/VST3/TONE3000.vst3` para repetir la prueba con ese efecto instalado. Sin argumento, la comprobación omite la parte VST3. Solo modifica datos de prueba bajo build/workflow-check; no añade las instancias de prueba al catálogo personal. Resultado en workflow.txt. El cambio ES → EN → ES y los catálogos se comprueban con scripts/check_locales.sh.

## Entrega

releases/macOS/Silente-plugins-cursor-01.app. Cerrar versiones anteriores antes de abrirla. Capturas y registros junto a la entrega en Silente-plugins-cursor-01-pruebas/. La revisión manual de los gestos y los menús por el usuario queda pendiente.

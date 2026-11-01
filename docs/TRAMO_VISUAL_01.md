# Tramo visual 01 · 29-10-2026

Objetivo: separar el tema visual y acercar colores, herramientas y regla temporal a la organización de Cubase, conservando el procesamiento existente.

## Entrega

- Tema extraído a src/ui/theme/: Colours, Drawing, SilenteLookAndFeel, ToolIcons y TrackColourPicker.
- Paleta de 24 colores y selector personalizado. Se abre desde el icono de paleta, la franja coloreada de una pista o su menú contextual «Color de pista».
- El comando cambia Track.colour, compartido por eventos, inspector, rack y mezclador. Usa el historial de deshacer existente y la serialización de color existente.
- Iconos vectoriales de selección, tijeras, borrador y lápiz; atajos1/3/5/8 conectados a las herramientas existentes. Transporte con iconos vectoriales en la zona inferior.
- Regla con compases, tiempos y subdivisiones; localizadores de ciclo arrastrables, barra horizontal de navegación. El historial captura también los localizadores.
- Inspector/Visibilidad y rack VSTi/Media/Meter conectados a las vistas existentes.
- Corregida la conversión de literales UTF-8 a juce::String para mostrar correctamente tildes y símbolos. En AudioEngine solo se cambió la construcción de texto; no sus algoritmos.

## Verificación observada

Compilación Release final completada. La aplicación generó una captura de la pantalla nueva a1440×900. Inspección visual: títulos, herramientas, eventos y transporte legibles; tildes corregidas y sin solapamientos visibles en esa resolución. La sesión de revisión nativa abrió y expuso los controles nuevos en su árbol de accesibilidad.

La herramienta de interacción rechazó el control de la sesión de prueba aislada. No se cuenta como aprobada la prueba manual del clic en la paleta, deshacer o persistencia de un color modificado. Tampoco se declara validación de audio, MIDI, grabación o plugins en este tramo.

## Revisión del usuario

1. Cerrar las ventanas anteriores de Silente y abrir la entrega Silente-visual-01.app.
2. Seleccionar una pista; abrir la paleta y elegir un color. Comprobar pista, evento, inspector y canal.
3. Deshacer y rehacer el cambio. Probar un color personalizado.
4. Guardar en una carpeta de prueba, cerrar y reabrir; comprobar el color.
5. Revisar herramientas1/3/5/8, desplazamiento y arrastre de localizadores con la sesión de ejemplo.

Este tramo requiere revisión del usuario antes de iniciar otra extracción. La similitud visual no equivale a toda la UX o las funciones de Cubase; la separación completa del resto de subsistemas sigue pendiente.

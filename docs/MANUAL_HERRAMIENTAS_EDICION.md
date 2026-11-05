# Herramientas de edición — inventario y primer tramo

Consulta: 05-11-2026. Fernando prioriza las herramientas de edición y la barra que aparece al hacer clic derecho. La investigación de colores de apariencia queda pendiente; no se editaron sus fuentes.

## Referencias verificadas

- Manual local Cubase Pro 15.0.30: pp. 62–64 (herramientas y empujar), p. 67 (Caja de herramientas, figura y modificadores) y p. 1658 (Opciones de edición—Herramientas).
- [Barra de herramientas de la ventana de proyecto](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/project_window/project_window_toolbar_r.html).
- [Caja de herramientas](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/project_window/project_window_toolbox_c.html?contentId=mWQbgRlwmFSQwEUM8LD5Ow).
- [Opciones de edición—Herramientas](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/preferences/preferences_editing_tools_r.html).

Figura revisada: `build/edit-tools-reference/pagina-67.png`. La barra es una fila compacta de iconos en el puntero. Clic derecho abre la caja o el menú contextual según una preferencia; una tecla modificadora invierte esa elección. La cabecera de pista y la regla tienen contextos distintos. Los recursos gráficos de Silente se dibujan con su propio sistema de iconos.

## Inventario contra las fuentes de Silente

| Herramienta del manual | Estado antes de este tramo | Pendiente real |
|---|---|---|
| Seleccionar | Mover y redimensionar eventos; selección simple | Selección múltiple y variantes de tamaño/contenido/estiramiento completas. |
| Selección de rango | Selección, mover/copiar/pegar, empujar y operaciones básicas | Cruce cero, segmentos de audio, pistas no contiguas y combinado con Seleccionar. |
| Dibujar | Añade notas MIDI básicas | Partes MIDI completas, automatización y otros contextos. |
| Borrar | Borra eventos | Variantes y contextos de editores avanzados. |
| Dividir | Divide audio en la posición ajustada | Variantes sobre partes/MIDI y modificadores avanzados. |
| Pegar | Reúne o consolida audio | Variantes de partes y selección múltiple. |
| Enmudecer | Mute de evento | Selección múltiple y editores. |
| Zoom | Acercar y Opción alejar | Rectángulo de zoom, historial y preferencias avanzadas. |
| Comp | No implementada | Carriles y ensamblaje de tomas. |
| Time Warp | No implementada | Mapa de tempo y anclajes musicales. |
| Línea | No implementada | Series de eventos y controladores en sus contextos. |
| Iniciar / reproducir | Escucha mientras se mantiene pulsado | Otros contextos y variantes del editor. |
| Color | Selector de color de pista y menú de evento; sin herramienta de clic | Añadir el modo Color al visor, con paleta sobre el evento y Deshacer. |

La caja al clic derecho no estaba implementada. El piano roll es básico y el clic derecho actualmente borra una nota: necesita su propio tramo, con herramientas específicas y revisión del comportamiento. No se considera resuelto por añadir una caja en la ventana de proyecto.

## Tramo herramientas01

Objetivo: caja de herramientas en el visor de eventos de la ventana de proyecto, preferencia persistente y herramienta Color operativa.

Entrega visible: fila de los 13 iconos del manual, conectada a los modos disponibles; Comp, Time Warp y Línea desactivados con explicación de lo pendiente. Caja accesible también con el proyecto vacío; cabeceras y regla conservan su contexto. Preferencia en Ajustes › Opciones de edición.

Aceptación: clic derecho/modificadores según la preferencia; seleccionar herramientas mediante la caja cambia el modo real, no modifica audio ni crea un paso de Deshacer al abrirla; Dividir ejecuta la edición y permite Deshacer. Color actúa sobre el evento pulsado, conserva el color de pista y permite Deshacer. Comprobar guardado de preferencia, idiomas, cancelación y capturas.

Punch, localizadores, cortar/copiar/pegar y fundidos son comandos/acciones, no los 13 modos de esta caja. Se mantendrán accesibles desde el menú contextual y el transporte; su existencia se documenta por separado para no confundir símbolos con funciones implementadas.

## Siguientes bloques pendientes

1. MIDI: investigación del editor, caja y herramientas específicas; corregir el borrado directo al clic derecho dentro de ese tramo.
2. Selección múltiple y variantes de Seleccionar; combinación con selección de rango.
3. Comp con carriles de tomas; Time Warp con mapa de tempo; Línea con controladores. Cada uno requiere motor y pruebas propios.
4. Cursores en cruz, información adicional del puntero, notificaciones y otras preferencias de herramientas.

Estado final y evidencia del tramo: `ETAPAS.md`.

## Auditoría de comandos · 05-11-2026 · herramientas02

Fernando pidió consultar el comando de cada herramienta y comprobar omisiones contra Cubase real. Se contrastaron tres fuentes:

1. Manual local, pp. **1622–1623**, categoría Herramientas; capturas revisadas `build/edit-tools-reference/pagina-1622.png` y `pagina-1623.png`.
2. [Categoría Herramientas oficial de Cubase Pro 15](https://www.steinberg.help/r/cubase-pro/15.0/es/cubase_nuendo/topics/key_commands/key_commands_tool_category_c.html): 13 asignaciones predeterminadas.
3. Archivo de la instalación real `~/Library/Preferences/Cubase 15/Key Commands.xml`, **solo lectura**, categoría `Tool`: **43 comandos**, 13 asignados y 30 sin asignación. Versión del bundle instalado: **15.0.30.287**. No se abrió ni modificó ningún proyecto ni se cambiaron asignaciones de Cubase. No se afirma una prueba interactiva de cada comando en Cubase ni equivalencia con Cubase 16.

El extracto reproducible, sin otras categorías ni macros del usuario, está en `referencias/cubase15/herramientas-comandos-15.0.30.json`. Conserva el nombre interno real, ID de Silente, tecla y hash del archivo fuente. `python3 tests/check_tool_commands.py` verifica correspondencia completa de nombres/IDs, teclas y ausencia de duplicados. Los nombres internos Select Tool, Range Tool y Delete Tool se muestran como Object Selection Tool, Range Selection Tool y Erase Tool, según los nombres de la ayuda actual.

### Cada herramienta de la caja del proyecto

| Herramienta | Comando en el catálogo real | Tecla de Cubase consultada | Estado de Silente |
|---|---|---|---|
| Seleccionar | Select Tool | **1** | Modo básico; faltan variantes y selección múltiple completa. |
| Selección de rango | Range Tool | **2** | Modo básico; combinación y variantes pendientes. |
| Dibujar | Draw Tool | **8** | Notas MIDI básicas, sin partes/automatización completas. |
| Borrar | Delete Tool | **5** | Borra eventos; variantes pendientes. |
| Dividir | Split Tool | **3** | Audio; repetición y otros contextos pendientes. |
| Pegar | Glue Tool | **4** | Une/consolida audio; no es el comando Pegar del portapapeles (⌘V). |
| Enmudecer | Mute Tool | **7** | Mute de evento. El icono X no cambia el atajo X de Fundido cruzado. |
| Zoom | Zoom Tool | **6** | Acercar y ⌥clic alejar; otras variantes pendientes. |
| Comp | Comp Tool | **Sin asignación** | Pendiente; desactivada en la caja. |
| Time Warp | TimeWarp Tool | **Sin asignación** | Pendiente; desactivada en la caja. |
| Línea | Curve Tool / Curve Tool: Line | **Sin asignación** | Pendiente; desactivada en la caja. |
| Iniciar / reproducir | Play Tool | **9** | Parcial: mantiene reproducción al pulsar; no equivale aún a la escucha aislada de Cubase. |
| Color | Color Tool | **Sin asignación** | Parcial: paleta y Deshacer; falta el flujo de pintar con color activo y modificadores. |

La ausencia de tecla no significa ausencia de comando: Comp, Time Warp, Curva/Línea y Color existen en el catálogo de Cubase, pero no tienen una asignación en la configuración consultada y no figuran con tecla en su tabla oficial de valores predeterminados. No se les inventa una tecla.

**⌥⇧C sí existe**, en la categoría Proyecto: «Ajustar color de pista/evento» (manual p. 1622). Abre la selección de color y ya figura en Silente; es un comando diferente de activar el modo **Color Tool**. Del mismo modo, Pegar/Glue (4) y Pegar del portapapeles (⌘V) son acciones diferentes.

### Comandos que faltaban en nuestra referencia

- **F9: herramienta anterior** y **F10: herramienta siguiente**. Añadidos al catálogo de consulta; su navegación por herramientas aún no se ejecuta en Silente. No se supone un orden de recorrido sin comprobarlo.
- **0 (fila principal): Baqueta**, para el editor de percusión, pendiente. **Num 0 sigue siendo Detener**. El 0 no es Time Warp.
- **⌥⇧1: Combinar herramientas de selección act./desact.**, pendiente. [Cubase divide el carril: Rango arriba y Seleccionar abajo](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/parts_events/parts_and_events_combine_selection_tools_mode_c.html); no es alternar simplemente entre las teclas 1 y 2.
- Sin asignación: los tres modos explícitos de Seleccionar (**Normal Sizing**, **Sizing Moves Content**, **Sizing Applies Time Stretch**); **Scrub**, **Trim**, **Color**, **Hitpoint**; **Curve** y sus seis variantes **Line, Parabola, Sine, Triangle, Square, Paint**; **Tool 1–10**; **Popup Tools**; **Free Warp**, **Audio Tempo Definition**, **Comp**, **TimeWarp** y **Edit Pitch/Warp**.

Antes se listaban nueve comandos de Herramientas; ahora están los **43**, con **10 acciones básicas disponibles y 33 sin implementación**. La referencia de las demás categorías sigue siendo parcial; esta auditoría no acredita todos los comandos de Cubase. El nuevo texto «Sin asignación» se distingue tanto de la tecla 0 como de una función no disponible.

### Variantes e interacciones que también faltan

- [Modificadores oficiales](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/preferences/preferences_editing_tool_modifiers_r.html): ⌥ con Borrar elimina eventos posteriores; ⌥ con Pegar une los siguientes; ⌥ con Dividir repite divisiones. Seleccionar incluye dividir con ⌥, cursor con ⌥⇧ y deslizar contenido con ⌘⌥. Registrar y verificar por contexto antes de implementarlos.
- [Color sobre la marcha](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/project_window/project_window_colorizing_events_with_the_color_tool_t.html): Cubase pinta con un color activo, ⌥clic toma otro color y ⌘clic abre el selector. Silente actualmente abre la paleta al clic ordinario. Son flujos distintos y queda pendiente igualarlos.
- [Escucha de partes/eventos](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/parts_events/parts_and_events_audio_parts_and_events_auditioning_t.html): Cubase escucha la pista pulsada y evita su cadena de canal; Silente inicia el transporte general con su mezcla/procesamiento. Falta escucha aislada. [Scrub](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/parts_events/parts_and_events_scrubbing_using_the_scrub_tool_t.html) es una variante distinta de Play que depende de dirección y velocidad del arrastre; pendiente.
- Herramientas específicas de MIDI, percusión, editor de muestras y VariAudio no deben insertarse como si fueran herramientas del visor del proyecto. El catálogo las registra; cada contexto requiere su propia etapa.

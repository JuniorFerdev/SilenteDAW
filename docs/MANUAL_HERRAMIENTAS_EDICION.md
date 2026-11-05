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

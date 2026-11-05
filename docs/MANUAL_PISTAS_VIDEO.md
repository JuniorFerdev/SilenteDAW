# Pistas: referencia visual, alcance y aceptación

Fernando pidió estudiar [el tutorial de MUNERASONG](https://www.youtube.com/watch?v=ZqR_-KOAYYE), leer la transcripción, observar capturas cada diez segundos y desarrollar el manejo y la apariencia de las pistas. Prioridad confirmada: **Audio, Instrumento, MIDI, Carpeta, Regla y Marcador**; después añadió **Video para musicalización**. Muestreador y los demás tipos nuevos quedan aplazados. También pidió reordenamiento por arrastre y actualización de la app al entregar.

## Evidencia revisada

Se leyó toda la transcripción automática en español (20:35) y se observaron **125 capturas del reproductor**: 00:00–20:30 cada 10 s y 20:35. Se comprobaron los tiempos y la ausencia de huecos en `referencias/tipos-pista/capturas.json`. [Galería completa](referencias/tipos-pista/index.html), originales en `referencias/tipos-pista/frames/` y 21 hojas de contacto en `referencias/tipos-pista/contactos/`. La transcripción sin editar permanece como material de trabajo en `build/referencia-pistas-video/transcripcion-auto.txt`.

Las capturas se tomaron navegando por los controles públicos de YouTube, en pausa, con subtítulos y revisión por intervalos. No se afirma un visionado audiovisual continuo. No se descargó el archivo del video: la descarga no quedó autorizada y se usaron capturas directas del reproductor. Se revisó el encuadre completo para evitar recortes/superposiciones del navegador. Los anuncios intermedios no forman parte del muestreo.

## Mapa del video

| Tiempo | Tema observado | Alcance solicitado |
|---|---|---|
| 00:30–01:00 | Añadir pista: T, +, menú contextual | Diálogo con iconos, ajustes, nombre y cantidad. |
| 01:06–02:03 | Audio | Entrada, mono/estéreo, salida; grabar e importar. |
| 02:03–03:13 | Instrumento y rack | Instrumento existente en un canal de proyecto. |
| 03:13–03:40 | MIDI | Partes y editor inferior; editor avanzado sigue aparte. |
| 03:40–05:25 | Muestreador | Aplazado expresamente. |
| 05:25–06:04 | FX | Ya hay base; no ampliar en este tramo. |
| 06:04–07:57 | Grupo | Ya hay base; no ampliar en este tramo. |
| 07:57–08:50 | VCA | Aplazado. |
| 08:50–10:50 | Marcador | Puntos, descripción y navegación; añadir ciclos coloreados por pedido de Fer. |
| 10:50–11:37 | Regla | Formatos independientes y pista movible. |
| 11:37–12:13 | Carpeta | Color, arrastrar contenido, plegar/desplegar. |
| 12:13–13:41 | Tempo | Aplazado. |
| 13:41–15:12 | Tipo de compás | Aplazado. |
| 15:12–16:22 | Arreglos | Aplazado; su lista de reproducción no equivale a los marcadores de ciclo. |
| 16:22–16:47 | Transposición | Aplazado. |
| 16:47–18:22 | Acordes | Aplazado. |
| 18:22–20:35 | Video y cierre | Imagen/miniaturas y visor sincronizado; la extracción de audio sola no resuelve esta función. |

## Contraste con el manual actual

- [Reordenar pistas](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/track_handling/track_handling_moving_tracks_in_the_track_list_t.html): arrastrar por nombre/área libre, línea verde de destino, consola en el mismo orden y Deshacer.
- [Organización en carpetas](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/track_handling/track_handling_organizing_tracks_in_folder_tracks_c.html): cualquier tipo y carpetas anidadas, meter/sacar por arrastre, plegado y reproducción de pistas ocultas. Evitar ciclos de anidamiento. Una carpeta organizativa no cambia por sí sola el ruteo del audio.
- [Regla](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/tracks_about/tracks_about_ruler_tracks_c.html?contentId=tPErEToIno0NJZue30o5xg): varias reglas independientes de la principal. [Controles](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/tracks_about/tracks_about_ruler_track_controls_r.html): formato y desplazamiento temporal.
- [Marcadores](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/markers/markers_c.html) y [marcadores de ciclo](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/markers/markers_cycle_markers_c.html): puntos y secciones con inicio/fin; doble clic recupera localizadores. El uso solicitado de secciones pintadas se implementó con ciclos nombrados y coloreados en marcadores01.
- [Importar video](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/video/video_video_files_importing_t.html): evento en pista de video y extracción opcional de audio a una pista separada. [Inspector de video](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/tracks_about/tracks_about_video_track_inspector_r.html): visor y miniaturas.

## Desarrollo y pruebas de aceptación

1. **Organización:** selección de pistas, reordenar sin cambiar sus IDs, plugins o ruteo; meter/sacar cualquier tipo en carpetas y subcarpetas, plegar, mover una carpeta con sus descendientes, impedir anidamiento circular, guardar/cargar y Deshacer. Verificar señal antes/después y selección estable; no perder contenido al eliminar solo el contenedor.
2. **Creación y front:** módulo propio para el diálogo Añadir pista, iconos dibujados por Silente, distribución del video, nombre/cantidad, configuración pertinente y cancelar sin editar el proyecto. T, + y menú contextual reutilizan el mismo flujo. ES/EN, tamaño de interfaz y capturas comparadas.
3. **Audio/Instrumento/MIDI:** conservar importación, grabación y host existentes; completar las opciones de creación y el flujo básico de partes/editor mostrado. No presentar esto como editor MIDI avanzado completo.
4. **Regla/Marcador:** tipos sin canal de audio; regla independiente con formatos, marcadores de posición y ciclos con nombre/color, mover/redimensionar, localizar, localizadores y persistencia/deshacer. Las secciones deben permitir navegar de verdad.
5. **Video:** selector nativo y arrastre, evento con miniaturas, duración/posición, visor que siga Play/pausa/cursor/ciclo, audio extraído opcional y sin reproducción duplicada, guardar/cargar y archivo ausente manejado. Ensayar con video sintético de imagen real, no solo un contenedor de audio.
6. **Entrega:** pruebas pertinentes de cada módulo, capturas de Silente, compilación y actualización de Universal con respaldo; comprobar arm64 y Rosetta. Mantener fuentes, UI, persistencia, pruebas y referencias organizadas en el workspace de VS Code.

Estos puntos representan la aceptación global; su avance se detalla abajo, sin dar por entregados los pendientes. Estado y resultados observados se registran en `ETAPAS.md`.


## Marcadores: comprobación específica 09:00–11:00 · 05-11-2026

Fernando dio prioridad explícita a este intervalo. Se volvieron a inspeccionar las hojas 0540/0600 y la captura 0640. Para las acciones adicionales se consultaron [Inspector de marcador](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/tracks_about/tracks_about_marker_track_inspector_r.html), [edición en la pista](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/markers/markers_editing_markers_on_the_marker_track_c.html) y [ciclos](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/markers/markers_cycle_markers_c.html). Cubase local no tiene licencia y no se abrió.

| Referencia | Silente y prueba observada |
|---|---|
| 09:00 crear pista | Tipo Marcador en + y menú Pista, controles propios, sin canal de mezcla. |
| 09:20 botón añadir / 09:28 Insert | Punto en cursor exacto, ID consecutivo libre, pista activa independiente de la seleccionada. |
| 09:38 Descripción | Campo editable en la línea de información, nombre actualizado en evento/lista/menú. |
| 10:10 insertar más secciones | INTRO y ESTROFA añadidos con posiciones distintas. |
| 10:17 lista Inspector | ID, posición y descripción; columna izquierda navega sin editar. |
| 10:29 mover Descripción | Cabeceras reordenables y redimensionables con desplazamiento horizontal. |
| 10:39 Localizar | Menú real muestra nombres; selección con teclado salta a INTRO. Captura independiente del menú. |
| 10:50–11:00 Regla | Pista independiente; formato propio y representación de tiempo sin canal de audio. |

Además del video, se comprobaron ciclos CORO con color propio, recuperación exacta de localizadores sin modificar Ciclo del transporte, ⌥doble clic, arrastre y extremos, edición numérica con rechazo de límites invertidos, ⌘arrastre, ⌥clic, Mayús + añadir, cancelar ciclos de longitud cero, Suprimir, deshacer, guardar/reabrir y rechazo de referencias obsoletas. Límite de diez pistas sin crear pasos falsos de historial. Los marcadores y reglas no afectan las muestras renderizadas ni se ofrecen como canales de exportación o inserciones.

Verificación del candidato Universal firmado en **arm64 y x86_64: 176/176 UI, 41/41 motor, 6/6 formatos por arquitectura**, 844 traducciones por idioma y cambio ES→EN→ES. Capturas de Silente conservadas: [secciones](referencias/tipos-pista/silente-marker-sections.png), [Localizar](referencias/tipos-pista/silente-marker-locate-menu.png), [regla](referencias/tipos-pista/silente-marker-ruler.png). No equivale a una comparación de cada píxel ni a ejecutar Cubase sin licencia.

### Checklist que permanece abierta

- [ ] Carpetas con contenido real, anidamiento, plegado y reordenamiento por arrastre (prioridad siguiente).
- [ ] Diálogo completo de creación Audio/Instrumento/MIDI/Carpeta/Regla/Marcador/Video y front correspondiente.
- [ ] Partes MIDI básicas del video; editor MIDI avanzado conserva su investigación y etapa propias.
- [ ] Video con imagen, miniaturas y visor sincronizado; extraer audio no cubre esta función.
- [ ] Regla: editar desplazamiento temporal y moverla en la lista cuando exista reordenamiento.
- [ ] Marcadores: ventana completa ⌘M, selección/edición múltiple, cortar/copiar/pegar/mover entre pistas, más comandos e importación/exportación de listas.
- [ ] Base musical de marcadores con cambios de tempo; por ahora las posiciones son segundos absolutos.
- [x] Diez inserciones de efectos por pista, independientes del instrumento: entregadas el 05-11 en las cuatro vistas, con desplazamiento, posiciones persistentes y pruebas en ambas arquitecturas.
- [x] Entrega Universal marcadores01: app sustituida, firma/arquitecturas/SHA-256 comprobados y respaldo anterior conservado. Detalles en ETAPAS.

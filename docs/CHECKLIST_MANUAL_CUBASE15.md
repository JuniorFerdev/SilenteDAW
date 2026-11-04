# Checklist del manual de Cubase Pro 15 en Silente

Estado de cada función del manual (págs. del PDF en español). ✅ hecho y probado · 🟡 parcial · ⬜ falta.
«Probado» = prueba automática (motor, io o flujo de interfaz); nada de esto sustituye una prueba con
hardware real. Actualizar al cerrar cada tramo.

## Ventana de proyecto (55–123)
- ✅ Zonas: columna Canal, Inspector, zona inferior, zona derecha; mostrar/ocultar y diseño guardado.
- ✅ Bordes de zonas arrastrables (Canal, Inspector, zona derecha, zona inferior).
- ✅ Barra de herramientas: Hub, deshacer/rehacer, M/S globales, MixConsole, herramientas, color, ajustar, rejilla, zoom, exportar, zonas.
- ✅ Herramientas 1 Selección · 3 Dividir · 4 Pegar · 5 Borrar · 6 Zoom · 7 Enmudecer · 8 Dibujar · 9 Reproducir.
- ✅ Herramienta 2 Selección de rango: arrastrar (varias pistas), Mayús amplía, doble clic cubre el evento o el hueco; inicio/fin/duración en la línea de información.
- ✅ Edición › Rango: Dividir (⇧X), Truncar, Suprimir intervalo de tiempo (⇧Retroceso), Insertar silencio (⇧⌘E), Ajustar fundidos al rango (A), Suprimir, Duplicar; menú contextual del rango. Audio y MIDI.
- ✅ Rangos: arrastrar mueve el contenido (también a otras pistas), ⌥ arrastrar copia; ⌥←/⌥→ empuja el inicio, ⌥⇧←/⌥⇧→ el final; ⌘←/⌘→ desplaza el rango sin contenido; cortar/copiar/pegar rango (⌥V en su origen).
- ⬜ Rangos: fijar a cruce por cero, ampliar a segmentos de audio, incluir/excluir pistas con ⌘clic.
- ✅ Línea de estado y línea de información.
- ✅ Lista de pistas estilo Cubase (M S nombre volumen / ● ◉ R W e / medidor), filtro y búsqueda.
- ✅ Altura de pistas (deslizador y ⇧H/⇧G) y zoom de forma de onda.
- ✅ Botón de color: paleta de pista directa; color de evento en el menú del evento.
- ✅ Regla con formatos, localizadores, ajustar a rejilla/eventos/cursor, historial de edición.
- 🟡 Inspector: Enrutado, Inserciones, Ecualizadores; faltan Envíos, Controles rápidos, Notas.
- ⬜ Diálogo de colores del proyecto, cruceta, línea de vista general, pestaña Control Room.

## Reproducción y transporte (306–343)
- ✅ Barra de transporte: localizadores, posición, ciclo/parar/reproducir/grabar, tiempo, tempo, Tap, metrónomo.
- ✅ Menú Transporte: modos de inicio, volver al inicio, pre/post-roll, rangos de reproducción, desplazamiento auto., cursor fijo.
- ✅ Metrónomo: sonidos, subdivisión, precuenta, tap tempo, tecla C.
- ⬜ Marcadores (438–450), Teclado en pantalla como ventana flotante (344–346).

## Grabación (347–369)
- ✅ Habilitar grabación, grabar con precuenta, en pausa o en marcha (Num *).
- ✅ Punch in / Punch out (I / O), parar tras punch out con post-roll.
- ✅ Grabación en ciclo con una toma por vuelta; modos Mantener historial / Ciclo historial + reemplazar / Reemplazar.
- ✅ Modos de grabación: Punch in/out o Regrabar; inicio en cursor, localizador izquierdo o selección.
- ✅ Formato 16/24/32 bit flotante; carpeta Audio del proyecto; deshacer grabación.
- ✅ Evento rojo «Grabando…» que crece con la forma de onda en vivo (por vuelta de ciclo, desde el punch in).
- ⬜ Pregrabación (60 s), carriles de tomas, modos MIDI, compensación de latencia de entrada.

## Partes, eventos y fundidos (253–294, 396–418)
- ✅ Mover, redimensionar, dividir, duplicar, borrar, color de evento.
- ✅ Pegar (rejunta o consolida), enmudecer evento.
- ✅ Fundido cruzado (X) entre eventos solapados o que se tocan; solo suena el evento superior fuera del cruce.
- ✅ Manipuladores de fundido (triángulos arriba) y de volumen (abajo a la izquierda, −∞…+6 dB); la forma de onda refleja fundidos y volumen.
- ✅ Audio › Fundidos: editor(es) de fundido (5 curvas, duración, Por defecto, Aplicar), aplicar fundido estándar, eliminar fundidos, fundido cruzado.
- ⬜ Editor de fundido cruzado completo (solapado, empujar, igual energía en cruce), fundidos automáticos, curvas de volumen de evento con puntos, fundidos por rango.

## MixConsole (451–533)
- ✅ 04-11-2026 · 16 envíos FX por canal (Cubase 16): rack desplazable y columna Canal hasta 13–16; señal por cada ranura, pre/post, persistencia y proyectos de cuatro ranuras probados. Entrega universal actualizada y probada en arm64 y x86_64.
- ⬜ Pedido de Fer · 04-11-2026: hasta **10 plugins de inserción por pista**. Ampliar las ranuras en Canal, Inspector, configuración de canal y ambas MixConsole; conservar instrumento separado, orden de procesamiento, bypass, guardado/carga y Deshacer. Verificar señal a través de las diez inserciones y apertura de sus editores. Pendiente para un tramo posterior; no implementado aún.
- ✅ Ventana F3 y MixConsole inferior, racks Enrutado/Inserciones/EQ/Envíos, grupos y FX, solo/mute, medidores con pico.
- ✅ Ancho de canal arrastrando su borde (⌥: todos), guardado en el proyecto.
- ⬜ Contraste completo con el capítulo (instantáneas, enlaces de canales, rack de cadena de canal, etc.).

## Importar / exportar (370–380, 1527–1553)
- ✅ Importar audio/MIDI/video; exportar mezcla y pistas separadas; WAV/AIFF/FLAC/MP3/M4A; conversión de formatos.
- ⬜ Exportar por rango de localizadores, cola de exportación.

## Comandos de teclado (1607–1628)
- ✅ Tabla de ~130 comandos por defecto con ventana de consulta; los marcados ✓ funcionan.
- ⬜ Editar y guardar asignaciones propias.

## Ajustes (referencia Cubase 16)
- ✅ Ventana no modal reutilizable: Audio/MIDI, escala de interfaz, Transporte, Metrónomo y consulta de comandos de teclado; navegación y buscador transversal con resultados por control/comando. Preferencias y cambio de idioma comprobados; entrega universal actualizada y flujo probado en arm64 y x86_64.
- ⬜ Preferencias de color de pistas, carpetas y consola del video; otras páginas avanzadas de Ajustes y asignaciones de teclas editables.

## Pedidos de Fer · 04-11-2026 · investigación y desarrollo pendientes
- ⬜ **Editor MIDI completo:** el piano roll actual es básico y no equivale a la ventana avanzada de Cubase. Investigar el capítulo Editores MIDI (1170–1304), documentación vigente de Cubase 16 y videos completos; guardar referencias, capturas y una matriz de herramientas, selección, información de notas, velocidades/controladores, zoom, cuantización, navegación y ventanas. Incluir Rasgueo y Estirar selección de Cubase 16. Diseñar y probar por subetapas, sin declarar completo el editor por tener notas dibujadas.
- ⬜ **Barra de herramientas contextual sobre las pistas:** estudiar la paleta de símbolos al hacer clic derecho en el visor de eventos. Inventariar selección, rango, dividir/cortar, pegar, dibujar, borrar, enmudecer, zoom, reproducción, fundido X, localizadores y punch según el contexto real y la configuración de Cubase. Distinguir herramientas de edición, comandos del menú y botones de transporte; confirmar cada icono con captura y referencia antes de asignarle una acción. Silente aún no reproduce este flujo completo.
- ⬜ Antes de cada nuevo tramo, informar a Fer qué función se desarrollará, qué hará y cómo se verificará; mantener avances mientras se ejecuta.
- Seguimiento de novedades sin plugins nuevos: `docs/CUBASE16_NOVEDADES_Y_PLAN.md`.

## Pendientes grandes (sin empezar)
Automatización, editores MIDI completos, Pool, MediaBay, efectos de audio propios, tempo y tipo de compás, marcadores, pista de arreglos, VariAudio, sampler.

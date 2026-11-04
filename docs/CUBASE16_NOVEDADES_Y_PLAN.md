# Cubase 16 — referencias y plan para Silente

Fecha de consulta: 04-11-2026. Pedido de Fernando: incorporar progresivamente las mejoras del programa, sin desarrollar plugins nuevos. Antes de cada tramo se comunica la función, el resultado visible y su prueba; se informa mientras se ejecuta. Esta matriz es un plan, no una afirmación de que Silente iguale a Cubase.

## Fuentes y método observado

- Estudio Durango, [CUBASE 16 ya está AQUÍ y va por BUEN camino](https://www.youtube.com/watch?v=mczbPhUpGss), 15:15, publicado el 04-11-2026. Se leyó la transcripción automática completa, de 0:00 al cierre en 15:06; se inspeccionaron demostraciones por secciones y se guardaron capturas. No se ha hecho un visionado continuo íntegro ni una comparación auditiva de sus ejemplos. La transcripción puede contener errores; los nombres se contrastan con Steinberg. El comentario del autor no sustituye la documentación técnica.
- Steinberg: [novedades](https://www.steinberg.net/cubase/new-features/) y [notas de Cubase 16.0.0](https://www.steinberg.net/cubase/release-notes/16/), leídas en el navegador (la página requiere JavaScript).
- Manual local Cubase Pro 15, capítulo MixConsole: páginas 508–510, lectura de texto y figuras. Sección Envíos: destino, ganancia, activación y pre/post; los envíos cue son otra función. Las figuras renderizadas de 508–509 quedan en `build/manual-cubase16/`. El PDF sigue siendo referencia de flujos existentes; no documenta las novedades de la versión 16.

## Recorrido del video

Los tiempos son referencias para volver a la demostración; las imágenes conservan la página de YouTube y subtítulos cuando están visibles. Resolución limitada por el panel del navegador: no usar estos fotogramas para medir colores o letras diminutas. Antes de reproducir un diseño en detalle habrá que obtener referencias legibles de sus controles.

| Tiempo | Tema | Referencia visual / decisión |
|---|---|---|
| 00:25–00:58 | Colores de pistas, carpetas y consola | [Captura](referencias/cubase16/00-42-apariencia.jpg). Preferencias de apariencia pendientes. |
| 00:58–02:01 | DrumChain y VocalTouch | Excluidos: plugins nuevos. |
| 02:05–02:24 | Arrastrar inserción para crear canal FX | [Captura](referencias/cubase16/02-18-arrastrar-insercion-fx.jpg). Pendiente del host, aunque no se desarrolle ningún efecto. |
| 02:24–03:14 | Spectral AudioWarp | [Captura](referencias/cubase16/02-33-audiowarp.jpg). Pendiente de motor y evaluación de calidad. |
| 03:14–03:27 | Separación de stems | Pendiente; requiere motor/modelos y evaluación, no es exportar pistas separadas. |
| 03:27–05:06 | Instrument Stacks | [Captura](referencias/cubase16/04-34-instrument-stacks.jpg). Pendiente de ruteo MIDI/capas; usar instrumentos existentes. |
| 05:06–06:30 | Rasgueo MIDI | [Captura](referencias/cubase16/06-06-rasgueo-midi.jpg). Pendiente de la etapa del editor MIDI. |
| 06:30–07:10 | Estirar selección MIDI | [Captura](referencias/cubase16/07-02-estirar-midi.jpg). Pendiente; selección múltiple, tiempos y duraciones. |
| 07:10–07:59 | Detección de tempo | [Captura](referencias/cubase16/07-37-tempo.jpg). Pendiente; distinguir tempo fijo, mapa de tempo y ajuste de audio. |
| 07:59–10:34 | Module, Omnivocal, Illugen, paquetes de sonidos | Plugins/bibliotecas comerciales excluidos. Explorar medios propios sí sigue pendiente. |
| 10:34–11:26 | Ajustes con buscador | [Captura](referencias/cubase16/10-41-ajustes.jpg). Pendiente de espacio no modal e índice real de ajustes. |
| 11:26–11:58 | Ayuda rápida contextual | [Captura](referencias/cubase16/11-41-ayuda-rapida.jpg). Pendiente; contenido propio y cobertura explícita. |
| 11:58–12:25 | 16 envíos; 8 cue | [Captura](referencias/cubase16/12-15-envios.jpg). Envíos FX ampliados en este tramo; cue pendiente de Control Room. |
| 12:25–12:34 | Mayor búfer anticipado | Pendiente de arquitectura de procesamiento; no equivale a aumentar el búfer del dispositivo. |
| 12:34–13:13 | Configuración de hardware externo | [Captura](referencias/cubase16/12-57-conexiones-externas.jpg). Pendiente; referencia de mayor detalle y prueba física necesarias. |
| 13:13–13:35 | Pool renovada | [Captura](referencias/cubase16/13-24-pool.jpg). Pool aún no implementada. |
| 13:35–14:00 | Importar M4A/AAC | [Captura](referencias/cubase16/13-44-importacion-m4a.jpg). Hay infraestructura de formatos; verificar importación directa y variantes antes de declararlo completo. |
| 14:00–15:15 | Valoración y cierre | Leídos en la transcripción. No añaden una función del programa. |

## Matriz completa de novedades oficiales

Pendiente = no desarrollada aquí. Parcial = hay una base, falta cubrir el comportamiento. La aceptación se concreta antes de programar cada tramo.

| Función | Estado de Silente / trabajo necesario |
|---|---|
| Instrument Stacks | Pendiente: carpeta/canal tocable, capas con rangos de teclas y velocidad, transposición, selección y presets. |
| Ajustes unificados y búsqueda | Pendiente: reunir opciones existentes, búsqueda transversal, cambios no modales y persistencia. |
| Separación de seis stems | Pendiente: motor independiente, licencia de dependencias, CPU/memoria y calidad. No confundir con exportación por canal. |
| Rasgueo | Pendiente de editor MIDI avanzado. |
| Estirar selección MIDI | Pendiente de editor MIDI avanzado. |
| Detección de tempo | Pendiente: análisis, edición del resultado y aplicación al proyecto/audio. |
| AudioWarp espectral | Pendiente: diseño propio o dependencia autorizada; medir artefactos y alineación. No copiar algoritmos de Steinberg. |
| DrumChain / VocalTouch | Excluidos por el pedido de no desarrollar plugins nuevos. |
| Omnivocal Basic / Module | Excluidos; alojar un instrumento comprado por Fer sigue siendo función del host. |
| Nuevos paquetes de sonidos | Excluida su reproducción/distribución; explorar archivos propios queda en MediaBay. |
| Waves Illugen | Sin implementar; integración comercial externa, fuera del primer ciclo de funciones locales. |
| Ableton Link | Pendiente: sincronización de tempo, pulso y transporte, desconexión y convivencia con el reloj propio. |
| MIDI 2.0 | Pendiente: modelo, entrada/salida, expresiones y dispositivos reales. |
| Quick Help | Pendiente: ayuda contextual en zona izquierda/ventana flotante. |
| Procesamiento anticipado ampliado | Pendiente: trabajos fuera del callback, búferes y compensación, sin bloquear monitorización/grabación. |
| Nombres de CC por pista/globales | Pendiente: modelo y carriles de controladores. |
| Letras asociadas a notas | Pendiente: carril y persistencia de asociación durante edición. No desarrollar síntesis vocal. |
| Follow Tempo | Pendiente: modo musical real de clips, sin confundirlo con cambiar BPM del metrónomo. |
| Definir tempo del audio | Pendiente: metadatos musicales y editor de muestras. |
| Importación M4A/M4B/M4R, AAC-LC/HE-AAC | Parcial: lectores/conversión presentes; faltan pruebas específicas de importación directa y variantes. |
| Pitch detection e inversión de fase de samples | Pendiente de pistas de muestreo/batería; no comenzar un plugin nuevo para cubrirlo. |
| 16 envíos de audio | Implementado en este tramo para los destinos FX que ya admite Silente. Destinos grupo/salida/sidechain adicionales siguen pendientes. |
| 8 cue / 8 envíos MIDI | Pendiente; cue requiere Control Room y mezclas de monitorización, MIDI requiere destinos externos. |
| Arrastrar/copiar inserts entre canales | Pendiente: identidad, estado, orden, deshacer y seguridad del hilo de audio. |
| Arrastrar inserción al vacío crea FX | Pendiente; referencia 02:18 del video. |
| Comando añadir inserción | Pendiente: primera ranura libre y selector del host. |
| Retirar plugins desactivados/bypass | Pendiente: proteger automatización de bypass cuando exista. |
| Randomización y nuevas interfaces de plugins Steinberg | Excluidas: pertenecen a sus plugins. |
| Compensación de moduladores | Pendiente de automatización/modulación y latencia completa del ruteo. |
| Drum Maps desde pads | Pendiente de editor de batería/modelo de mapas; no desarrollar Drum Machine ahora. |
| Presets de mastering | Excluidos los recursos comerciales; presets propios del host podrían estudiarse después. |
| Pool | Pendiente: inventario, referencias, medios ausentes, reemplazo seguro y nueva disposición. |
| Asignar teclas desde menús contextuales | Pendiente de asignaciones editables y persistentes; tabla actual es de consulta. |
| Visibilidad de carpetas sincronizada | Pendiente de jerarquía de carpetas real y su representación en MixConsole. |
| Instrumentos/efectos externos | Pendiente: buses, puertos, búsqueda/orden y calibración de latencia real. |
| Visita guiada | Pendiente; explicar exclusivamente controles operativos. |
| Actualizaciones MIDI Remote separadas | Pendiente de MIDI Remote y un mecanismo seguro de versiones. |
| Video: salidas simultáneas y decodificación | Parcial: extracción de audio presente; reproductor/salidas/VFR pendientes. |
| Windows MIDI Services | No aplica al tramo actual en macOS. Registrar para una futura plataforma Windows. |
| Legibilidad de fuentes macOS | Parcial: escalado y caché existen; revisar texto fino con capturas del Mac. |

## Orden de ejecución y aceptación

1. **Envíos 16 (actual):** señal por cada ranura, pre/post, límites, rechazo de bucles, guardado y compatibilidad con cuatro ranuras. Rack desplazable sin invadir el fader. Es una mejora de ruteo del host, no un plugin.
2. **Ajustes y apariencia:** anunciar antes su alcance; analizar más de cerca el buscador y los tres controles de color antes de construirlos. Mantener el proyecto editable con la ventana abierta.
3. **Ayuda rápida y herramientas contextuales:** inventario visual por contexto; distinguir paleta de herramientas, menú de acciones y transporte. No asignar significados a iconos sin comprobarlos.
4. **Editor MIDI:** investigación específica amplia pedida por Fer; piano roll actual reconocido como básico. Subetapas de selección/navegación, notas, controladores, cuantización y herramientas de versión 16.
5. **Medios y tempo:** importación móvil, Pool/MediaBay, análisis de tempo y Follow Tempo con ensayos de audio.
6. **Ruteo y estabilidad:** completar compensación por buses y de grabación, inserciones por arrastre, Control Room, dispositivos externos y procesamiento anticipado.
7. **Integraciones restantes:** Stacks, Link, MIDI 2.0 y video con sus propias pruebas. Separación/AudioWarp necesitan evaluación técnica antes de prometer calidad.

Pedido separado, diferido por Fer: **10 plugins de inserción por pista**, registrado también en la checklist del manual. No se cambió el límite visual de inserciones en este tramo.

## Evidencia del tramo envios16

Código: `Track::sendCount`, resolución de destinos y lista de envíos activos en el motor; rack con desplazamiento en MixConsole y columna Canal. La rueda en el rack no cambia el nivel de sus perillas. El callback recorre solo envíos activos por muestra, sin añadir asignaciones ni E/S.

Verificado en Apple Silicon: motor 38/38, formatos 6/6, interfaz 87/87, idiomas 769 y cambio ES→EN→ES. Compilación de desarrollo; distribución universal sin reconstruir. Detalle en `ETAPAS.md`. Esta investigación no significa que las funciones pendientes estén entregadas.

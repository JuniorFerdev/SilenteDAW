# Silente — investigación y criterios de primera versión

Fecha de consulta: 26 de octubre de 2026. Fuentes primarias de Steinberg, Apple y Avid. Este documento distingue lo observado en la documentación de Cubase de las propuestas de implementación para Silente. No es una afirmación de funciones ya terminadas ni una validación de equivalencia con Cubase.

## 1. Qué aprender de Cubase Pro 15

| Área | Comportamiento documentado | Aplicación propuesta a Silente |
|---|---|---|
| Proyecto | La ventana organiza pistas y eventos en la zona central, ajustes de pista a la izquierda, editores o mezcla abajo y herramientas a la derecha. | Transporte persistente, inspector compacto, arreglo central y panel inferior con mezcla/piano roll. Diseño e iconografía propios. |
| Audio | Un evento refiere a un clip; el desplazamiento y la duración seleccionan una región sin modificar el clip original. | Guardar archivo origen, posición, offset y duración; mover, cortar y recortar sin reescribir el audio. |
| Mezcla | MixConsole puede abrirse aparte. Presenta canales, faders y secciones de ruteo, insertos y envíos. | Ventana de canales con volumen, panorama, mute, solo, medidor e insertos, sincronizada con las pistas. |
| MIDI | Key Editor presenta notas en una rejilla de piano y carriles de controladores; puede ocupar la zona inferior o una ventana. | Piano roll con creación, movimiento, duración y velocidad de notas; teclado de preescucha y mensajes de apagado al detener. |
| Instrumentos | La ventana VST Instruments reúne los instrumentos del proyecto y sus controles rápidos. | Un selector de instrumento por pista MIDI, editor nativo del plugin y alternativa de sintetizador interno. |
| Exportación | La mezcla exportada considera los ajustes de consola y efectos; el rango se puede definir con localizadores. | Exportar un rango de la mezcla estéreo a WAV con los mismos niveles e insertos que se oyen. |

Fuentes específicas: [ventana de proyecto](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/project_window/project_window_overview_r.html), [eventos de audio](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/parts_events/parts_and_events_audio_events_c.html), [MixConsole](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/mixconsole/mixconsole_mixconsole_window_r.html), [Key Editor](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/midi_editors/midi_editors_key_editor_r.html), [instrumentos VST](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/vst_instruments/vst_instruments_window_r.html), [exportación de mezcla](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/export_audio_mixdown/export_audio_mixdown_options_r.html).

Las páginas de proyecto, MixConsole y Key Editor incluyen referencias visuales oficiales. Se usan como referencia de organización funcional; los recursos gráficos, marcas, contenido de instrumentos y código de Cubase no forman parte de Silente. El [índice oficial del manual 15](https://www.steinberg.help/r/cubase-pro/15.0/en) permite ampliar cualquier flujo. Se consultaron las áreas relevantes, no la totalidad del manual.

## 2. Compatibilidad real de plugins

“Todos los plugins del mercado” no es una especificación verificable: hay formatos, arquitecturas, versiones del sistema, dependencias, licencias de terceros y extensiones opcionales diferentes. La compatibilidad debe expresarse por formato y plugin probado.

| Formato/caso | Objetivo razonable | Límite explícito |
|---|---|---|
| VST3 macOS arm64/universal | Escanear, crear instancia, procesar efectos e instrumentos, abrir editor y conservar estado. | Detectar un archivo no demuestra que procese ni que restaure correctamente. Probar cada plugin. |
| Audio Units macOS | Explorar componentes registrados y alojar efectos/instrumentos compatibles. | AUv2, AUv3 y ejecución fuera de proceso requieren rutas de integración y pruebas propias. |
| VST3 Intel dentro de host arm64 | No prometerlo como parte de la carga nativa. | Necesita una estrategia de proceso/puente o ejecución compatible diferente; no se resuelve cambiando la extensión. |
| AU Intel desde host Apple Silicon | Posible ampliación mediante alojamiento fuera de proceso de macOS. | Apple documenta soporte de arquitectura cruzada en ese mecanismo; debe probarse en la implementación concreta. |
| VST2 | Fuera del objetivo inicial de distribución. | Steinberg dejó de admitir nuevos acuerdos de host/plugin VST2 en octubre de 2018. La documentación actual exige un acuerdo previo para distribuirlo. |
| AAX | Fuera del objetivo inicial. | El SDK de Avid está orientado a plugins de su ecosistema; no proporciona por sí mismo compatibilidad universal de alojamiento para Silente. |
| Plugins de Windows/Linux | Fuera de la app macOS nativa. | Son binarios y dependencias distintos. |
| Plugins que requieren licencia | El host puede intentar cargar la instalación legítima. | Silente no concede licencias de productos de terceros ni reemplaza sus sistemas de activación. |
| ARA, MPE, multisalida, sidechain | Capacidades separadas por implementar y verificar. | “Soporta VST3/AU” no demuestra soporte de cada extensión o configuración. |

Steinberg publica el SDK VST3 actual bajo MIT y conserva sus reglas separadas sobre marcas. Hay que incluir los avisos de las dependencias efectivamente distribuidas. Véanse [SDK y licencia actual](https://www.steinberg.net/developers/vstsdk/) y [distribución de archivos/licencia VST2](https://steinbergmedia.github.io/vst3_dev_portal/pages/VST%2B3%2BLicensing/Which%2Bfiles%2Bfall%2Bunder%2Bwhich%2Blicense.html). La propia [información de VST2 en Cubase 14/15](https://helpcenter.steinberg.de/hc/en-us/articles/22554401894162-Using-VST-2-Plug-ins-in-Cubase-Nuendo-14-and-15) ya diferencia este formato legado.

Apple documenta el alojamiento AU en [Incorporating Audio Effects and Instruments](https://developer.apple.com/documentation/audiotoolbox/incorporating-audio-effects-and-instruments), la adaptación de código en [Porting your audio code to Apple silicon](https://developer.apple.com/documentation/apple-silicon/porting-your-audio-code-to-apple-silicon), y la capacidad de cargar AU x86 fuera de proceso en [Debugging Out-of-Process Audio Units on Apple Silicon](https://developer.apple.com/documentation/audiotoolbox/debugging-out-of-process-audio-units-on-apple-silicon). No hace falta desactivar protecciones del sistema para la utilización normal; las instrucciones de depuración avanzada de ese artículo no son pasos de instalación de Silente. Avid describe su alcance en [AAX SDK](https://developer.avid.com/aax/).

## 3. Condiciones técnicas del motor

El host VST3 debe respetar la secuencia de creación/configuración, comunicar buses, entregar audio y eventos con offsets dentro del bloque, y facilitar contexto temporal y cambios de parámetros. Las llamadas de procesamiento no son trabajo de la interfaz. La documentación distingue el hilo de UI de las llamadas de procesamiento y pide evitar asignaciones de memoria en estas últimas. Fuentes: [API y modelo de hilos](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/API%2BDocumentation/Index.html), [secuencia de cambio de buses](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Workflow%2BDiagrams/Bus%2BArrangement%2BSetting%2BSequence.html), [ProcessContext](https://steinbergmedia.github.io/vst3_doc/vstinterfaces/structSteinberg_1_1Vst_1_1ProcessContext.html).

Rutas VST3 macOS previstas por Steinberg: carpeta del usuario, `/Library/Audio/Plug-ins/VST3`, ubicación de red y plugins locales de la aplicación; la prioridad y la identidad del procesador importan al resolver duplicados. Fuente: [ubicaciones VST3](https://github.com/steinbergmedia/vst3_dev_portal/blob/main/src/pages/Technical%2BDocumentation/Locations%2BFormat/Plugin%2BLocations.md). El paquete macOS `.vst3` contiene un binario Mach-O y metadatos; un paquete de Windows con el mismo sufijo no es intercambiable. Fuente: [estructura de paquetes](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Locations%2BFormat/Plugin%2BFormat.html).

Propuestas de ingeniería para Silente:

- Separar el documento del proyecto, el estado visual y el estado procesable del motor. El callback de audio no debe leer archivos de proyecto, escanear plugins, esperar diálogos ni dibujar.
- Usar posiciones enteras en muestras para audio y un dominio musical definido para MIDI. Especificar dónde se convierte entre segundos, muestras y pulsos; centralizar la conversión.
- Preasignar buffers. Llevar decodificación/lectura de archivos y cálculo de forma de onda a tareas de fondo. Evitar precargar cada archivo largo íntegramente como estrategia definitiva.
- Gestionar cambio de dispositivo y frecuencia fuera del callback. Publicar de forma segura cambios del grafo; no destruir una instancia que todavía procese audio.
- Suavizar variaciones de ganancia y panorama. Diferenciar balance estéreo de panorama mono y documentar la ley usada.
- Definir una única ruta de procesamiento para reproducción y exportación. Conservar cola de reverberación/delay según una política explícita.
- Mantener una caché de plugins con identidad, formato, arquitectura detectada, versión y resultado. Un escaneo que carga código requiere poder aislar fallos y recordar plugins problemáticos.
- Guardar el estado binario de los plugins y su identificación; conservar placeholders si falta alguno al reabrir. No borrar ese estado por no poder instanciarlo.
- Separar contador de carga de audio, uso de CPU total, memoria y latencia estimada. “Liviano” debe respaldarse con mediciones del equipo y sesión concreta.

La latencia de plugins necesita atención especial: Cubase compensa retardos a lo largo del recorrido de audio para mantener canales sincronizados. Silente debe probar compensación con impulsos antes de declararla disponible. Si no está implementada, debe figurar expresamente como limitación. Fuente: [Plug-In Delay Compensation](https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/audio_effects/audio_effects_plugin_delay_compensation_c.html).

## 4. Matriz propuesta de entrega inicial

Estado de esta tabla: propuesta previa a la implementación. El acta de pruebas debe indicar después qué quedó comprobado, pendiente o ausente.

| Prioridad | Función | Criterio para considerarla entregada |
|---|---|---|
| P0 | App nativa independiente y marca Silente | Arranca desde su `.app`, presenta interfaz propia y tiene icono/logotipo. |
| P0 | Importación de audio | Abrir WAV y AIFF mono/estéreo; indicar errores. Añadir otros formatos solo tras probar los decodificadores disponibles. |
| P0 | Arreglo multipista | Mostrar formas de onda; seleccionar, mover y cortar clips sin alterar originales. |
| P0 | Transporte | Play, stop, seek, retorno al inicio, cursor coherente y espacio. |
| P0 | Mezcla | Volumen, panorama/balance definido, mute, solo y maestro; medidores derivados de audio real. |
| P0 | Guardar/abrir | Recuperar posiciones, clips, parámetros y referencias a medios. Rechazar documento malformado sin perder el proyecto activo. |
| P0 | Exportar | WAV estéreo reproducible, duración y niveles coherentes con el rango; informar errores de escritura. |
| P0 | Pruebas de humo | Importar → reproducir → editar → mezclar → guardar → reabrir → exportar, con evidencia. |
| P1 | MIDI/piano roll | Crear/editar notas, elegir instrumento y obtener audio; no dejar notas sostenidas al parar o cambiar de posición. |
| P1 | VST3 y AU | Lista de detección, efecto e instrumento realmente procesando, editor accesible y estado conservado. Registrar exactamente los probados. |
| P1 | Grabación | Dispositivo/entrada seleccionable, permiso de micrófono, armado y archivo resultante íntegro. |
| P1 | Loop/metrónomo | Continuidad y ubicación correctas; no acumulación de notas o deriva al repetir. |
| P1 | Undo/redo | Operaciones de edición recuperables sin duplicar referencias incorrectamente. |
| P2 | Ruteo ampliado | Grupos, envíos, sidechain, salidas múltiples y compensación verificados como grafo. |
| P2 | Producción avanzada | Automatización, tempo variable, comping, stretch, afinación, ARA, MPE, superficies de control. |

Las prioridades son decisiones de ingeniería para una primera entrega en una sesión de trabajo, no una promesa de que toda la matriz quepa en ocho horas. Si una función carece de motor o verificación, la interfaz debe indicarlo o no exponerla como disponible.

## 5. Pruebas sugeridas con datos sintéticos

Todos los archivos de prueba se deben generar dentro del proyecto Silente. No usar sesiones académicas ni el proyecto de TopoVía. El material comercial del usuario queda fuera de los fixtures.

| Prueba | Método | Resultado exigido |
|---|---|---|
| Suma y ganancia | Seno de amplitud conocida y copias con fases iguales/opuestas; comprobar exportación numéricamente. | Escala lineal esperada; cancelación dentro de tolerancia; ausencia de NaN/Inf. |
| Mute/solo | Tres pistas distinguibles; combinaciones mute/solo y maestro. | Solo los canales previstos aportan señal. Definir precedencia mute/solo. |
| Panorama | Señal mono al centro y extremos, estéreo con canales diferentes. | Ley documentada y separación L/R correctas; no confundir balance con panner. |
| Offsets/cortes | Impulsos en muestras conocidas; mover, cortar y exportar. | Impulsos en posiciones esperadas, sin repetir/perder muestras por redondeo. |
| Sample rate | Clips de 44,1 y 48 kHz con tono y duración conocidos. | Pitch/duración conservados en el proyecto según política de conversión. |
| Límites de buffer | Clip y nota que empiezan/terminan dentro de un bloque y exactamente en el borde. | Sin bloque extra, nota perdida ni offset fuera del buffer. |
| MIDI | Note-on/off repetidos, seek a mitad de nota, loop, stop y eliminación de pista sonando. | No quedan notas colgadas. |
| Persistencia | Guardar, cerrar y reabrir con audio, MIDI y plugin editado. | Misma estructura/estado; audio equivalente para fuentes deterministas. |
| Medio ausente | Mover un fixture después de guardar. | Mostrar medio ausente y permitir recuperar; no eliminar clips silenciosamente. |
| Plugin ausente/fallido | Sesión con ID no disponible o fallido. | Conservar estado y proyecto; error identificable. |
| Plugin real | Efecto conocido y un instrumento en VST3/AU; bypass/editor/reabrir/exportar. | Procesamiento audible y medible, GUI estable y parámetros retenidos. |
| PDC | Dos impulsos iguales, uno con retardo declarado por plugin. | Solo declarar compensación si vuelven alineados. |
| Exportación | Exportar rango corto, sesión vacía, silencio y pico alto. | Longitud exacta, formato correcto y política de clipping explícita. |
| Recuperación | Documento JSON truncado, rutas Unicode y archivos solo lectura. | Error comprensible, proyecto anterior intacto, sin archivo parcialmente válido presentado como guardado. |
| Rendimiento | 8/16/32 pistas sintéticas; medir memoria/carga a 48 kHz, buffers 128/256/512 y UI activa. | Registrar equipo, versión, número de plugins y duración; no extrapolar a todos los equipos. |

Una comprobación de interfaz no prueba calidad de audio. Un render numérico no prueba que cada plugin sea estable en tiempo real. La entrega debe separar ambos tipos de evidencia y enumerar claramente lo no probado.

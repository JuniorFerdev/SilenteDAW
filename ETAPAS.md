# Silente — trabajo por etapas

> Desde el 31-10-2026 la referencia de diseño es el Manual de operaciones de Cubase Pro 15 (`/Applications/ARCHIVO FER/Cubase_Pro_15_0_Operation_Manual_es.pdf`). Plan y método: `docs/PLAN_MANUAL_CUBASE15.md`.

## Acuerdo de trabajo · 28 de octubre de 2026

1. Un solo objetivo por etapa. Antes de editar: objetivo, entrega visible y prueba de aceptación en tres líneas.
2. Trabajar sobre lo existente. No repetir investigación ni reconstruir módulos sin una necesidad concreta.
3. Sin subagentes por defecto. Después de definir y revisar el tramo, el usuario decide la delegación. Cada agente tiene un alcance y archivos concretos; no abrir frentes nuevos.
4. Cerrar cada etapa con una app o cambio comprobable, una prueba pertinente y los pendientes reales. No presentar código escrito como función validada.
5. El usuario revisa el resultado antes de iniciar la etapa siguiente.
6. Comunicaciones breves. Sin búsquedas amplias, lecturas completas, capturas repetidas ni pruebas generales si no aportan a la etapa activa.


## Fase cerrada · 05-11-2026 · creacion01 · Cuadro Añadir pista y lotes con nombre

- Pedido actual: incorporar a la app el cuadro de la captura de Fernando, con tipos por iconos, nombre y cantidad para crear varias pistas. Referencia observada: captura adjunta del tutorial, filas 4 + 6, sección Más pistas plegable, campos de audio y pie Mantener el diálogo abierto / Añadir pista. Contraste: ayuda oficial Cubase Pro 15, Add Track / Audio (hasta 100 por operación).
- Implementado en `src/ui/project/AddTrackDialog.*` y `src/app/TrackCreation.*`. Botón +, T y menú Pista usan el mismo diálogo; contexto de lista permite abrirlo. Ocho tipos actuales: Audio, Instrumento, MIDI, Carpeta, FX, Grupo, Regla y Marcador. Los tipos aún aplazados conservan su posición visual y están desactivados con explicación; no se crean sustitutos falsos. Iconos vectoriales propios y marco nativo de macOS.
- Nombre base y cantidad 1–100 (Marcador respeta el máximo de diez por proyecto); lotes numerados 01/02/03, valores por defecto si no se escribe nombre, Mantener el diálogo abierto, cancelar con Escape, crear con Intro y un solo paso de deshacer/rehacer por lote. Mono/estéreo y entrada física se aplican a cada pista; salida a Stereo Out o grupo existente; selección de instrumento/efecto del catálogo y formato propio de Regla. Validación previa: rechazar lotes durante grabación, cantidad inválida y destinos eliminados sin crear pistas parciales ni historial falso.
- Entregado y verificado en **arm64 y Rosetta: motor 43/43, formatos 6/6, interfaz 198/198 por arquitectura**. Dos instrumentos AU reales independientes creados con nombre desde el diálogo, grabación mono sintética en la entrada elegida y guardar/reabrir lotes de los ocho tipos. ES/EN conserva nombre/cantidad; 873 textos. Se amplió el espacio del nombre en las cabeceras, desplazando el minifader para que no lo tape. Las comprobaciones de foco fallaron mientras el Mac estaba bloqueado; Fernando lo desbloqueó y el recorrido completo pasó en ambas arquitecturas sobre la app instalada, sin cambiar código para eludir las pruebas.
- Sustituida la única `releases/macOS/Silente-Universal.app`, fuentes `967848e`, firma local válida `audio.silente.studio`, SHA-256 `64acc3a1df3334bccb3eeee78c40e8f6f7344810b340821eb7fae661d2b9fc9f`. Evidencia compacta en `docs/referencias/creacion01/` (capturas, informes y manifiesto). Conservadas las diez inserciones anteriores; retiradas apps y fixtures temporales, sin crear respaldos. No se repitieron pruebas físicas de dispositivos ni Waves. Carpetas con contenido real, reordenamiento, MIDI avanzado y video sincronizado mantienen sus etapas propias.
- Abierta la app entregada **bajo Rosetta**, proyecto vacío; clic real en + mostró el cuadro con iconos, nombre, número y ajustes reales. Se deja abierto para revisión. Retirados 45,8 MiB en dos apps temporales y 52,5 MiB en fixtures/logs de este tramo; fuentes y proyectos originales conservados.

## Fase cerrada · 05-11-2026 · inserciones10 y limpieza de disco

- Orden vigente: una sola app `releases/macOS/Silente-Universal.app`; **no crear respaldos**. Fernando autorizó borrar variantes, copias anteriores y resultados regenerables para ahorrar disco. Retiradas SilenteReview, dos paquetes anteriores, el Intel independiente y todos los archivos de backups. Las fuentes y proyectos musicales originales se conservan. Depurados WAV, cachés y pruebas antiguas; los informes antiguos regenerables también se depuran; se conserva la evidencia del desarrollo vigente en ETAPAS y sus capturas.
- **Diez inserciones implementadas y entregadas:** módulo común `src/ui/plugins/InsertRack.*` para Inspector, Canal, MixConsole y Configuración de canal; desplazamiento dentro de cada rack sin reducir el fader. Posiciones 1–10 independientes del instrumento, huecos conservados, orden de procesamiento/persistencia, reemplazo y bypass por ranura. Las cargas directas independientes ya no cancelan los callbacks de las anteriores.
- Universal firmada verificada en **arm64 y x86_64**: motor **42/42**, formatos **6/6**, interfaz **183/183**, 846 traducciones y ES→EN→ES. Incluye diez instancias reales de Apple AULowpass, acceso a la décima ranura en las cuatro vistas y bypass del menú real. Posiciones/huecos y orden de proceso probados con señal sintética; instrumento separado de diez efectos. Capturas y manifiesto del ejecutable entregado en `docs/referencias/inserciones10/`. Se conservaron los objetos de compilación útiles y se eliminaron apps/fixtures/respaldos regenerables: **más de 1 GB retirado**. No hubo nuevas pruebas físicas de Waves. La sesión abierta del usuario no se cerró; debe reabrir Silente para cargar la actualización.
- El diálogo **Añadir pista** que quedó pendiente en este tramo se entregó después en creacion01 (ver arriba). Carpetas reales/reordenamiento, partes MIDI y video sincronizado siguen en la checklist.

## Etapa activa · 05-11-2026 · pistas03 · Organización, creación y musicalización

- Alcance acumulado confirmado por Fernando: **Audio, Instrumento, MIDI, Carpeta, Regla** (aclaró que «Guía» significa regla de compases/tiempo), **Marcador** (secciones coloreadas para navegar) y **Video para musicalización**. Muestreador/VCA/Tempo/Compás/Arreglos/Transposición/Acordes nuevos quedan para después. Conservar FX/grupo existentes sin ampliar sus funciones. También exige trabajar apariencia/front, reordenar pistas y actualizar la app al finalizar los cambios verificados.
- Investigación terminada: transcripción automática completa y **125 capturas revisadas**, cada diez segundos desde 00:00 hasta 20:30 más 20:35. Originales, manifest y 21 hojas de contacto en `docs/referencias/tipos-pista/`; galería `index.html`. Se usaron controles públicos del reproductor, sin descargar el video ni afirmar un visionado audiovisual continuo. El tamaño temporal del navegador se restauró. Alcance, referencia por tiempo y aceptación: `docs/MANUAL_PISTAS_VIDEO.md`.
- Faltantes reconocidos: Carpeta aún es un contenedor vacío sin jerarquía; importar video actualmente solo extrae audio. Deben convertirse en organización real y reproducción de imagen sincronizada respectivamente. Creación Audio/Instrumento/MIDI y los demás tipos existentes ya usa el diálogo global con nombre/cantidad (creacion01); falta completar funciones propias de los tipos según la checklist. No presentar estos puntos como resueltos por tener sus nombres/iconos. Marcador y Regla ya tienen base funcional verificada en desarrollo; ver tramo marcadores01.
- Última prioridad de Fernando: reproducir y verificar el flujo de **Marcador de 09:00–11:00**. Implementado en marcadores01, con 176/176 comprobaciones de interfaz, 41/41 motor y 6/6 formatos por arquitectura (Apple Silicon y Rosetta). Universal actualizada y abierta bajo Rosetta, con firma e identidad comprobadas; ver entrega marcadores01. Después permanecen autorizados carpeta real/reordenamiento, diálogo de creación, partes MIDI y video sincronizado. Se conserva esa checklist sin afirmar que esté terminada.


## Tramo · 05-11-2026 · marcadores01 · Secciones y navegación (video 09:00–11:00)

- Referencias observadas: capturas originales 0540–0660, contactos 0540/0600 y transcripción de MUNERASONG. Contraste con ayuda oficial de Cubase Pro 15: Inspector, edición de marcadores y marcadores de ciclo. **No se abrió Cubase ni se afirma validación interactiva allí.** Matriz de aceptación y diferencias en `docs/MANUAL_PISTAS_VIDEO.md`.
- Pista Marcador en + y menú Pista: Insert y botón añaden punto en el cursor de la pista activa; Descripción editable en la línea de información; lista del Inspector con ID, posición y nombre, columnas desplazables/redimensionables. Clic en la columna izquierda o menú Localizar navega a la sección por ID estable. Hasta diez pistas, con una activa; no aparecen como canales de audio ni candidatos de exportación/inserción.
- Ciclos nombrados y coloreados guardan inicio/final. Doble clic recupera localizadores sin activar el bucle; ⌥doble clic ajusta zoom. Arrastre mueve el rango; extremos se redimensionan con Ajustar; posiciones también editables en la línea de información. ⌥clic añade punto, ⌘arrastre crea ciclo, Mayús + botón abre inmediatamente Descripción. Suprimir/Borrador y ⌥Borrador para puntos posteriores; Deshacer/rehacer y guardar/cargar conservan identidades, nombres, límites y colores. Editores obsoletos no afectan pistas nuevas.
- Regla independiente: formatos compases/tiempos, segundos, código de tiempo a 30 fps, muestras y fotogramas a 60 fps desde la cabecera; persistencia propia sin cambiar la regla principal. El desplazamiento temporal se conserva en el modelo, pero su control de edición y el reordenamiento de pistas siguen pendientes. Los cambios de tempo no retienen una base musical de marcadores: se guardan en segundos.
- Módulos nuevos: `src/domain/TrackData.*`, `src/persistence/TrackDataJson.*`, `src/ui/project/TimelineTracks.*`, `src/ui/project/MarkerInspector.*`; pruebas de flujo en `tests/MarkerUiTests.*`. El coordinador mantiene selección, comandos e historial; no hay DSP en estos componentes. Catálogos 844 por idioma y ES→EN→ES aprobados.
- Evidencia de desarrollo: `build/workflow-check/marker-sections.png`, `marker-locate-menu.png`, `marker-ruler.png`, informe `build/workflow-check/workflow.txt` **176/176 PASS**; motor **41/41**, formatos **6/6** en `build/markers-test-results/`. La comprobación de foco F9/F10 ahora espera la activación asíncrona real de macOS hasta 1,5 s, sin omitir la aserción. Pruebas con preferencias aisladas y material sintético.
- Pendientes explícitos del capítulo de marcadores: ventana completa ⌘M, selección/edición múltiple, copiar/cortar/pegar y mover entre pistas, atajos numéricos/localizadores adicionales, base musical/mapa de tempo e importación/exportación de listas. No se presenta este tramo como reproducción íntegra de todo Cubase.
- **Entregada** `releases/macOS/Silente-Universal.app`, fuentes `5148d84`, arm64 + x86_64, firma local válida e identidad estable `audio.silente.studio`. Ambas arquitecturas aprobaron **41/41 motor, 6/6 formatos y 176/176 interfaz**, 844 textos y ES→EN→ES. SHA-256 del ejecutable `12ab6fdeb965f6d0d0f9b5be8bb3d5a0184f6c93d5580a6568dc6af2b8301ff7`, idéntico al candidato probado. Informes `build/markers-universal-arm64/` y `build/markers-universal-x86_64/`; manifiesto `build/markers-delivery.json`.
- Respaldo anterior con CRC comprobado: `backups/Silente-Universal-antes-marcadores01-2026-11-05.zip`; paquete anterior conservado en `build/Silente-Universal-anterior-marcadores01.app`. No había Silente abierto al sustituir; se abrió la versión nueva con `open --arch x86_64`, sin cargar proyectos del usuario. No se realizaron nuevas pruebas físicas de audio ni de Waves. Capturas duraderas de la versión verificada en `docs/referencias/tipos-pista/silente-marker-*.png`.

## Tramo · 05-11-2026 · herramientas03 · Anterior/Siguiente herramienta

- F9 y F10 ejecutan Anterior/Siguiente en la ventana de proyecto y en la caja emergente. Recorrido circular de los diez modos disponibles; se omiten Comp, Time Warp y Línea. La caja cierra al confirmar con esas teclas y los botones de la barra reflejan el modo elegido. Fuente oficial: categoría Herramientas de Cubase Pro 15; F9/F10 confirmadas en internet.
- Decisión explícita de Silente: recorrido de la barra principal actual (Selección → Rango → Dividir → Pegar → Borrar → Zoom → Enmudecer → Dibujar → Reproducir → Color). La ayuda oficial consultada no especifica el orden interno exacto de Cubase 15; no se afirma haberlo reproducido/verificado interactivamente. Fernando aclaró que Cubase no tiene licencia: **usar manual/internet y no volver a abrirlo para investigar**.
- Cambiar herramienta conserva dibujo de eventos/selección, posición, transporte e historial. F9/F10 se respetan al editar texto y no se redirigen al proyecto cuando el foco está en el piano roll o el teclado MIDI (su sistema de herramientas sigue pendiente). La navegación y los clics comparten selección/disponibilidad en `src/ui/project/ProjectToolbox.*`; coordinación en MainComponent, sin DSP.
- **Verificado:** 145/145 comprobaciones UI; catálogo de 43 comandos y 13 asignaciones sin cambios, ahora **12 acciones disponibles / 31 pendientes**. Idiomas 816 y ES→EN→ES aprobados. Las pruebas de foco activan y esperan el foco real de su ventana; la primera ejecución no lo recibía y no se contó como aprobada. Capturas revisadas `build/workflow-check/tool-navigation-colour.png` y `tool-navigation-popup.png`; informes `build/tool-navigation-workflow.log`, `build/workflow-check/workflow.txt`, `build/i18n-check/`.
- Fernando añadió el siguiente tramo autorizado: estudiar **todo el video de MUNERASONG** `https://www.youtube.com/watch?v=ZqR_-KOAYYE` (20:35), leer transcripción y observar capturas **cada diez segundos**, contrastar tipos de pista/diseño, registrar faltantes y **desarrollar reordenamiento de pistas por arrastre**. Organizar por módulos para VS Code. **Actualizar la app al cerrar los cambios verificados**; Universal aún sin sustituir durante este tramo. Próximo trabajo inmediato: análisis visual/transcripción, luego reordenamiento y entrega actualizada.

## Tramo · 05-11-2026 · herramientas02 · Auditoría de comandos contra Cubase instalado

- Pedido: consultar el comando de cada herramienta y comprobar que no falte ninguna respecto de Cubase real. Contrastadas páginas 1622–1623 del manual (incluidas capturas), categoría Herramientas oficial de Steinberg y categoría `Tool` del archivo de comandos de **Cubase 15.0.30.287 instalado**, en modo lectura. Extracto limitado a nombres/teclas en `docs/referencias/cubase15/herramientas-comandos-15.0.30.json`; el SHA-256 del original sigue intacto. No se modificaron preferencias ni proyectos de Cubase ni se afirma una prueba interactiva de cada comando.
- La referencia de Silente pasa de nueve a **43 comandos de Herramientas**, sin omisiones respecto del catálogo leído. **13 teclas asignadas, 30 sin asignación; diez acciones básicas disponibles y 33 comandos sin implementar.** Las demás categorías siguen siendo parciales. `Sin asignación` se muestra explícitamente en ES/EN y no se confunde con la tecla 0 ni puede capturar un evento de tecla vacío.
- Omisiones encontradas/registradas: **F9 Anterior, F10 Siguiente, 0 Baqueta y ⌥⇧1 Combinar herramientas de selección**. Figuran como pendientes, sin acciones ficticias. Los números 1–9 ya coincidían. Num 0 conserva Detener; no se asignó 0 a Time Warp. Comp, Time Warp, Curva/Línea y Color no tienen tecla en la configuración consultada; ⌥⇧C corresponde a otro comando, Ajustar color de pista/evento.
- Inventario y diferencias de comportamiento ampliados en `docs/MANUAL_HERRAMIENTAS_EDICION.md`: modos explícitos de Seleccionar, Scrub, Trim, Hitpoint, variantes de curvas, herramientas de muestra/VariAudio y modificadores. Reproducir sigue usando mezcla/transporte general (falta escucha aislada); Color abre paleta al clic ordinario (falta pintar con color activo, tomar color con ⌥ y abrir selector con ⌘). No se consideran iguales al flujo completo de Cubase.
- Verificación en Apple Silicon: `python3 tests/check_tool_commands.py` coteja los **43 nombres/IDs y las 13 asignaciones**, sin teclas duplicadas en la categoría. Recorrido UI **134/134**, incluida búsqueda de los 43, distinción 0/Num 0, comandos vacíos y no ejecución de pendientes. Catálogos **816** y cambio ES→EN→ES aprobados. Captura revisada: `build/workflow-check/tool-command-reference.png`; informes `build/tool-command-audit.txt`, `build/tool-commands-workflow.log` y `build/tool-commands-locales.log`.
- Cambio de **referencia y verificación**, sin nuevos algoritmos ni herramientas avanzadas. Compilación de desarrollo actualizada; **Universal sin sustituir**, sin nueva prueba Rosetta/audio físico. Siguiente pendiente: implementar los comandos e interacciones auditados por tramos, empezando por la navegación y selección que Fernando priorice.

## Tramo · 05-11-2026 · herramientas01 · Caja de herramientas y Color

- Pedido de Fernando: buscar y desarrollar las herramientas de edición. Contrastadas pp. 62–64, 67 y 1658 del manual, incluida la figura de la caja de herramientas, y las páginas oficiales de Steinberg. Inventario de los 13 modos, variantes que faltan y dependencias en `docs/MANUAL_HERRAMIENTAS_EDICION.md`. No se inició el tramo de colores de apariencia ni se considera completo el editor MIDI.
- En el visor de eventos, **clic derecho abre una fila de herramientas junto al puntero**, incluso sin pistas. Diez modos conectados a las funciones existentes: Seleccionar, Rango, Dibujar, Borrar, Dividir, Pegar, Enmudecer, Zoom, Iniciar y Color. Comp, Time Warp y Línea aparecen desactivados con explicación de sus dependencias. Iconos propios, orden del manual. Cabeceras y regla conservan sus contextos.
- **Ajustes › Opciones de edición** añade la preferencia persistente y buscable «Mostrar caja de herramientas con clic derecho». Una tecla modificadora invierte caja/menú; Ctrl-clic de macOS se trata como gesto contextual. El menú de eventos conserva edición y añade acceso a cortar/copiar/pegar y localizadores. La X de la caja significa Enmudecer; el atajo X sigue siendo Fundido cruzado, y Punch pertenece al transporte.
- **Color** actúa sobre el evento pulsado, abre su paleta y permite Deshacer. Reencuentra pista/evento por sus IDs al aplicar: reordenar o borrar el destino con la paleta abierta no colorea otro evento ni crea una edición falsa. Abrir/cancelar la caja o la paleta no añade pasos de Deshacer.
- Durante la verificación se detectó y corrigió un acceso anterior al primer pico de la onda de grabación cuando el evento empieza en un píxel fraccionario. Añadida regresión con inicio a 1,013 s. Corregido también el lanzador de pruebas: las ventanas emergentes necesitan procesar mensajes sin reentrar en el temporizador que inicia el recorrido; un informe en curso ya no puede confundirse con el resultado anterior.
- **Verificación observada en Apple Silicon:** 127/127 comprobaciones del recorrido de interfaz, sin cierres; catálogos 815 por idioma y ES→EN→ES aprobados. Preferencias de prueba aisladas. Capturas inspeccionadas: `build/workflow-check/project-toolbox.png`, `event-colour-tool.png` y figura de referencia `build/edit-tools-reference/pagina-67.png`. Informes: `build/tools-workflow.log`, `build/workflow-check/workflow.txt`, `build/tools-locales.log`. No se modificó DSP ni se repitieron pruebas físicas de audio/plugins.
- **Fuentes y compilación de desarrollo actualizadas; Universal instalada sin sustituir.** Este tramo no se ha probado bajo Rosetta. Siguen pendientes selección múltiple/variantes completas, MIDI avanzado, Comp con carriles, Time Warp con mapa de tempo y Línea con controladores. La checklist conserva también el pedido de diez inserciones por pista.

## Entrega anterior · 04-11-2026 · Universal con envios16 y ajustes01

- Por pedido de Fernando «actualiza la app», sustituida `releases/macOS/Silente-Universal.app` con las fuentes del commit `2bc21b3` (incluye envios16). Universal **arm64 + x86_64**, firma local válida con identidad estable `audio.silente.studio`. El ejecutable entregado coincide por SHA-256 con el candidato probado (`1621bfd14d38bb64a16c8db8ef1795a15f79b41f471627e35e5b22b00e9fa2b1`).
- Candidato firmado probado en ambas arquitecturas: **38/38 motor, 6/6 formatos, 106/106 interfaz**. Informes `build/update-arm64/` y `build/update-x86_64/`. Preferencias de layout, interfaz, metrónomo, transporte y audio intactas durante las pruebas aisladas. Sin ensayo físico de audio/grabación ni nueva validación de plugins Waves.
- Se cerró la sesión vacía que estaba abierta, se actualizó y se volvió a abrir **bajo Rosetta**, conservando el modo que Fernando usaba. Comprobado desde la app entregada: Edición › Ajustes abre las cinco páginas nuevas. Captura nativa: `build/update-ajustes-rosetta.png`. Se deja abierta la ventana de Ajustes.
- Respaldo anterior verificado: `backups/Silente-Universal-antes-envios16-ajustes01-2026-11-04.zip`; copia adicional anterior en `build/update-app-anterior.app`. Las demás novedades de Cubase 16 siguen en la checklist; no forman parte de esta entrega.

## Tramo · 04-11-2026 · ajustes01 · Ajustes con buscador

- Ventana **no modal**, redimensionable y reutilizable, con cinco páginas operativas: Audio/MIDI, Apariencia (escala de interfaz), Transporte, Metrónomo y consulta de Comandos de teclado. Acceso desde **Edición › Ajustes…**; Audio/F4, ajustes del metrónomo y comandos de teclado abren su página en esta misma ventana. Se puede editar el proyecto sin cerrarla.
- Buscador transversal por categorías, nombres de controles y comandos; admite varias palabras, mayúsculas y palabras sin tilde. Cada resultado abre su página, desplaza solo si hace falta y resalta el control. Intro elige el resultado; Escape limpia la búsqueda. Las páginas se conservan al navegar; no se inventaron páginas de Control Room, plugins o configuración avanzada que aún no existen.
- Módulos: `src/ui/settings/SettingsWorkspace.*` (índice, navegación y páginas; sin DSP), `src/app/Preferences.*` (directorio compartido y escala, reutilizada por el menú existente). Transporte se aplica/guarda a través del coordinador existente; el metrónomo usa el mismo directorio y ahora también guarda el volumen escrito con teclado. Cambiar ES/EN reconstruye textos/índice conservando valores y datos del proyecto. Ventana limitada al área de pantalla al abrir y al cambiar escala.
- Verificación observada en Apple Silicon: **106/106 comprobaciones de interfaz**, catálogos **793** y cambio ES→EN→ES aprobados. Pruebas nuevas en `tests/SettingsUiTests.*`: búsqueda real, precuenta/volumen persistentes, escala 140 % y rechazo de un valor inválido sin sobrescribirla, transporte con cursor fijo/auto-scroll, búsqueda de comandos, idiomas, tamaño mínimo, edición de pista con Ajustes abierta, persistencia por el coordinador y cierre/reapertura de la misma ventana. Preferencias aisladas; sin prueba física de dispositivos. No se repitieron pruebas de DSP porque no se modificó el procesamiento.
- Capturas revisadas: `build/workflow-check/settings-search.png`, `settings-appearance.png`, `settings-transport.png`, `settings-audio.png`, `settings-keys.png`. Referencia visual: video de Estudio Durango 10:34–11:26 y `docs/referencias/cubase16/10-41-ajustes.jpg`; funciones contrastadas con las novedades oficiales de Steinberg 16.
- Compilación de desarrollo actualizada en `build/Silente_artefacts/Release/Silente.app`; **entrega universal sin reconstruir**, Rosetta sin nueva prueba. Alcance parcial respecto de los Ajustes completos de Cubase: faltan preferencias de color de pistas/carpetas/consola, configuración avanzada de proyecto/plugins, editar asignaciones de teclas y otras páginas dependientes de funciones pendientes. Próximo tramo propuesto: preferencias de apariencia de los tres colores del video, anunciando su alcance antes de editar.

## Tramo anterior · 04-11-2026 · envios16 · Novedades de Cubase 16

- Fernando pidió estudiar el video completo de Estudio Durango y las novedades oficiales, con capturas, e incorporar gradualmente las mejoras del host sin desarrollar plugins nuevos. Investigación y matriz: `docs/CUBASE16_NOVEDADES_Y_PLAN.md`; capturas en `docs/referencias/cubase16/`. Se leyó toda la transcripción automática y se inspeccionaron escenas; no se afirma un visionado continuo íntegro. Manual leído en este tramo: MixConsole, págs. 508–510, incluidas las figuras 508–509.
- Primer cambio: **16 envíos FX por canal** (antes cuatro). Se guardan todos y se cargan proyectos antiguos de cuatro ranuras sin crear rutas nuevas. MixConsole muestra el rack con desplazamiento; Canal muestra cuatro filas desplazables hasta 13–16, sin invadir el fader. Rueda sobre las perillas de envío no altera el nivel mientras se navega. El motor procesa por muestra solo los envíos activos, con arreglos reservados y sin E/S nueva.
- Pruebas observadas en Apple Silicon: motor **38/38**, formatos **6/6**, flujo **87/87**, idiomas **769** y cambio ES→EN→ES. Nuevas pruebas: señal acumulada por cada una de las 16 ranuras con pre/post y niveles independientes; índices inválidos y autobucle; persistencia de la ranura 16; compatibilidad de proyecto con cuatro envíos; desplazamiento hasta la última ranura, destino visible y fader intacto; columna Canal hasta 13–16. Capturas: `build/cubase16-workflow/mixconsole-sixteen-sends.png` y `channel-sixteen-sends.png`.
- Compilación de desarrollo: `build/Silente_artefacts/Release/Silente.app`. **No se reconstruyó ni sustituyó la entrega universal** en este tramo; Rosetta no se volvió a probar. No se hizo prueba física ni se repitió el render de la sesión del usuario. Los destinos siguen limitados a canales FX; cue, MIDI y sidechain son pendientes separados.
- Pedidos diferidos registrados: **10 plugins de inserción por pista**, **editor MIDI avanzado** (actual básico; requiere videos/manual/diseño/funcionamiento) y **paleta contextual de herramientas al clic derecho** (inventariar símbolos y acciones por contexto). Antes del siguiente tramo: informar a Fernando la funcionalidad concreta; continuar con los pendientes de la matriz, una entrega verificable por vez.

## Manual etapa 1 · Ventana de proyecto (págs. 55–123; historial)

Análisis: `docs/MANUAL_01_VENTANA_PROYECTO.md`. Lectura del PDF sin instalar nada: `swift` + PDFKit (extraer texto por páginas y renderizar páginas a PNG con `PDFPage.thumbnail`); no hay poppler ni Homebrew.

**1A hecha · 01-11-2026 · estructura y barra de herramientas.**
- Zonas Canal, izquierda, inferior y derecha que se muestran u ocultan desde la barra; «Configurar disposición de ventanas» (zonas + línea de estado, línea de información, barra de transporte). Se guarda en ~/Library/Silente/layout.xml.
- Barra en el orden del manual: deshacer/rehacer · M/S «desactivar todos» (se encienden si hay alguno) · abrir MixConsole · Seleccionar, Dibujar, Borrar, Dividir · color · ajustar · zoom · Exportar · zonas y disposición a la derecha. Idioma, Audio e Importar quedan en los menús.
- Línea de estado: Entradas/Salidas audio (conectadas o no), Tiempo de grabación máx. (disco libre a 24 bit estéreo), Formato de grabación, Pan law −3 dB, Tamaño de búfer; clic en entradas, salidas o búfer abre Audio.
- Línea de información: «Sin objeto seleccionado» o nombre, inicio, duración y ganancia del evento.
- Zona izquierda con pestañas inferiores Pista/Editor (Editor muestra el editor de teclas abierto o el aviso del manual). Pestaña inferior «Editor» y «MixConsole» con los nombres del manual.
- Pestaña Canal nueva (`src/ui/project/ChannelTab.*`): nombre y color, 4 inserciones, M/S, fader en dB y medidor.
- Nombre del proyecto en el título de la ventana (con «*» si hay cambios); «Guardar como» toma el nombre del archivo.
- Pruebas: motor 24/24; flujo 30 PASS (2 nuevas: ocultar/mostrar zona inferior y derecha cambia el área de pistas y vuelve); idiomas 494; dispositivos 22; reproducción con interfaz sin pérdidas. Captura: releases/macOS/Silente-ventana-proyecto-1A.png.
- Pendiente: zona derecha sin pestaña CR (no hay Control Room); vista global → 1C.

**1B hecha · 01-11-2026 · lista de pistas y controles globales** (págs. 67–71, 83–85): encima de la lista «visibles / total», filtro de tipo de pista (embudo; menú con tipos; se resalta si filtra; ⌘-clic lo restablece) y Buscar pistas (lupa; selector con campo que filtra al escribir, incluye pistas ocultas, al elegir la muestra, selecciona y desplaza). Visibilidad: Mayús-clic muestra solo esa pista. Lógica en `src/ui/project/TrackFilter.h`, selector en `TrackFinder.h`. Pruebas: 6 nuevas en el flujo.

**1C hecha · 01-11-2026 · regla, zoom, ajustar y rejilla** (págs. 71–72, 104–112): `src/ui/project/SnapGrid.h`. Barra: Ajustar act./desact. (imán), Tipo de ajuste (Rejilla, Eventos, Cursor, Rejilla + Cursor, Eventos + Cursor, Eventos + Rejilla + Cursor; eventos y cursor magnéticos a 10 px) y Tipo de rejilla (Compás, Tiempo, 1/8, 1/16, 1/32, Adaptar a zoom; con formato de tiempo: 10/100/1000 ms). Clic derecho en la regla: Compases+Tiempos, Segundos, Código de tiempo (30 fps), Muestras. H acerca y G aleja (además de ⌘+rueda). Pruebas: 7 nuevas (rejilla, adaptar a zoom, imanes, ajuste apagado, formatos). Pendiente: zoom vertical (Mayús-H/G), Relativo a rejilla y Shuffle, presets e historial de zoom, vista global.

**Comandos de teclado hechos · 01-11-2026** (pedido de Fer; manual «Comandos de teclado por defecto», págs. 1609–1628): `src/app/KeyCommands.*` con la tabla de Cubase (≈130 comandos, mismas categorías, nombres ES/EN y teclas), despacho único en `MainComponent::keyPressed`, menú Edición (deshacer, rehacer, cortar, copiar, pegar, pegar al origen, duplicar, suprimir, dividir en el cursor) y ventana Edición › Comandos de teclado (`src/ui/keys/KeyCommandsPanel.*`: búsqueda, categorías, tecla y ✓ si Silente ya lo ejecuta). Funcionan ≈70: transporte (Espacio, Intro, Num 0, Num * y *, Num / y /, C, P, ⌥P, L, N, B, Num 1/2, ⌘Num 1/2, Num . , Num +/−, ⇧L/⇧R/⇧P, ⇧T, ⇧Espacio), edición (⌘Z, ⇧⌘Z, ⌘C/⌘X/⌘V, ⌥V, ⌘D, ⌘K, Supr/Retroceso, ⌥X, ⌘L, M, S, R, J, ⌘F, F2, ⌥F2, ⌘E, Intro, ⌥⇧C, ⇧Supr), navegación (flechas, Inicio, Fin, ⌘←/→ empujar), zonas (⌥⌘L/C/R/E/T, ⌘I), zoom (H, G, ⇧F, ⌥S), herramientas 1/3/5/8, F3, ⌥F3, F4, F11, T, ⌘N/O/S, ⇧⌘S. Portátil: / ciclo, * grabar, + y − avanzar/rebobinar. Pruebas: 6 nuevas (tabla y acciones reales en la ventana). Pendiente: comandos de funciones que Silente aún no tiene (automatización, punch, marcadores, cuantizar, herramientas 2/4/6/7/9, zoom vertical) y personalizar teclas.

**1D hecha · 01-11-2026 · historial de ediciones y colores** (págs. 113–118): Edición › Historial (`src/ui/project/EditHistoryPanel.h`): acciones con nombre, hora y estado Hecho/Deshecho, línea del punto actual, clic en una fila deshace/rehace hasta allí (100 pasos). Botón de color de la barra = «Seleccionar color para pistas o eventos seleccionados»: con evento seleccionado colorea el evento (color propio, guardado en el proyecto; «Ajustar color de evento a pista»), si no la pista («Ajustar color de pista a por defecto»). Pruebas: motor 25/25 (color de evento guardado/cargado), flujo 54 (historial nombrado, deshacer, rehacer). Pendiente de la etapa 1: diálogo Configuración de colores de proyecto, herramienta Color, cursor en cruz, foco por zonas con Tab, vista global, pestaña CR.

**Etapa 1 (Ventana de proyecto) cerrada en lo esencial.** Siguiente: Reproducción y transporte (págs. 306–343), luego Grabación (347–369), según docs/PLAN_MANUAL_CUBASE15.md.

## Tramo · 01-11-2026 · transporte01 · Reproducción y transporte (págs. 306–343)

- Menú Transporte (ids 100–163): modos de inicio, volver a la posición de inicio al parar, pre/post-roll, reproducir rangos (selección, desde/hasta), desplazamiento automático y cursor fijo; opciones guardadas en ~/Library/Silente/transport.xml.
- Motor: `stopAt` y `playRange` (la reproducción se detiene sola al final del rango).
- Pruebas: motor 26/26 (nueva: rango de reproducción se detiene), flujo 54.

## Tramo · 02-11-2026 · rendimiento01 · Interfaz más ligera (sobre todo en Rosetta) y cierre al salir

- Medido con sulphuric (20 s de reproducción con la ventana abierta): el motor de audio usa 1,5–2,3 % (nativo/Rosetta); más del 90 % de la CPU era dibujar la interfaz. Nuevo medidor de carga de audio en el motor (`dspLoadAverage/Peak`) y `--ui-playback-check` informa CPU total y estable.
- Perfil (`sample`): el texto se componía con HarfBuzz en cada fotograma; la ventana se repintaba entera 24 veces por segundo; columna Canal y zona derecha también; las tiras repintaban todo para cada medidor.
- Cambios: caché de composición de texto y de anchos (`theme::text`, `textWidth`); el temporizador repinta todo solo si cambió algo visible (firma del estado), si no, solo cursor, medidores y posición del transporte; descarte por región en ventana principal, visor y tiras; repintado por regiones con Metal (`JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1`; sin él, repintar por partes salía peor).
- Resultado (CPU del proceso reproduciendo): nativo ~34 % → ~24 %; Rosetta 38–98 % (muy irregular) → 32–36 % estable. Hilo principal dibujando: ~25 % → ~14 %. Audio idéntico (sulphuric bit a bit).
- Cierre al salir: la caché de texto, al ser estática, liberaba fuentes después de que JUCE cerrara su tipografía y abortaba en cada salida (16 informes, todos de mis pruebas con la compilación de desarrollo; ninguno de la app instalada). Ahora se libera en el cierre ordenado de JUCE (DeletedAtShutdown): 0 cierres en las corridas siguientes.
- Pruebas: universal arm64 y x86_64 motor 37/37, io 6/6, flujo 83/83; sulphuric idéntico.

## Corrección · 02-11-2026 · ventana de plugin colapsada, parpadea y no se mueve

- Síntoma (Fer): al abrir un Waves en Silente la ventana queda estrecha, parpadea, se queda pegada y no deja moverla.
- Causa: la escala de interfaz (115 %, visual01) también escalaba la ventana del plugin. La vista nativa del plugin no se escala y se redimensiona; la ventana la reajustaba y así en bucle. Reproducido con `--probe-editor … legacy`: C4 pasó de 574×415 a 3162×2484, 22366×17584… hasta millones de píxeles en 4 s.
- Corrección: `src/ui/plugins/PluginEditorWindow.*` (como el AudioPluginHost de JUCE): escala 1 para la ventana del plugin, la ventana sigue el tamaño del editor y respeta sus límites más el marco nativo. PluginManager la usa.
- Verificado con la app universal: C4 574×415 y Abbey Road Chambers 732×625 estables en arm64 y x86_64 (Rosetta); motor 37/37, io 6/6, flujo 83/83. Falta confirmación de Fer moviendo y cerrando ventanas.
- Licencia Waves (diagnóstico cerrado): con el mismo binario de Silente, en Rosetta Abbey Road Chambers y C4 abren sin demo; en nativo ambos abren «DEMO MODE» (el probador `--probe-editor` lista ahora las ventanas nativas del proceso, `src/diagnostics/NativeWindows.mm`). Fer confirma lo mismo en Cubase: Rosetta bien, nativo en demo. Causa: el motor de licencias instalado `/Library/Application Support/Waves/Modules/WavesLicenseEngine.bundle` es 1.1.2 (2016) solo i386/x86_64; en arm64 dlopen lo rechaza. Waves Central 16.7.2 trae dentro el motor 3.4.0.0 universal (x86_64 + arm64), pero no reemplazó el instalado. No es un fallo de Silente; la solución es actualizar ese motor (decisión de Fer, software ajeno). Con su permiso se instaló el motor 3.4.0.0 (respaldo del viejo en «/Library/Application Support/Waves/Respaldo WavesLicenseEngine 1.1.2 (2026-11-02)»): carga en arm64, pero C4, CLA-76, H-Reverb y Abbey Road Chambers quedaron en demo en nativo **y también en Rosetta** (las licencias activadas no las ve el motor nuevo); reiniciar WavesLocalServer no cambió nada. Se restauró el motor 1.1.2 en el mismo momento: Rosetta vuelve a abrir C4 y Chambers sin demo. El 3.4.0.0 queda guardado en esa carpeta de respaldo. Siguiente paso posible (cuenta de Fer): activar o rescanear las licencias con Waves Central usando el motor nuevo.

## Análisis · 02-11-2026 · Waves en nativo (pregunta de Fer)

- Instalado: Waves V14 (versiones de shell 14.12 a 14.31). Los WaveShell AU, VST3 y VST son universales (x86_64 + arm64) y los 239 plugins de «Plug-Ins V14» también (DSP en GenericMacArm.dylib / GenericMacIntel.dylib). Ninguno es solo Intel.
- El catálogo nativo de Silente (~/Library/Silente/Plugins.xml) ya tiene 584 entradas de Waves; el de Rosetta (Plugins-Intel.xml) está vacío.
- `--probe-plugin` ahora también procesa 2 s de un tono de 440 Hz a −12 dB y mide la salida. En arm64 nativo: C4 (s), CLA-76 (s), H-Reverb (s) y Abbey Road Chambers (s) se crean y procesan audio de verdad (salida distinta de la entrada: compresión, color, reverberación).
- Conclusión: no hace falta Rosetta ni un puente Intel para estos Waves. Si algún día hubiera un plugin solo Intel, la vía sería alojarlo en un proceso aparte bajo Rosetta (como Logic con AUHostingService o los puentes de Reaper/Bitwig); no está implementado.
- Pendiente de Fer: abrir el editor de un Waves en Silente nativo y confirmar que no aparece en modo demo.

## Corrección · 02-11-2026 · permiso del micrófono repetido

- Síntoma (Fer): macOS pedía acceso al micrófono unas 8 veces al abrir Silente y otra vez al usar WhatsApp con Silente abierto.
- Causa: el paquete Silente-Universal.app no estaba firmado (solo la firma automática del enlazador en el ejecutable, identificador «Silente», Info.plist sin vincular). Sin identidad estable macOS no guarda la respuesta y pregunta en cada apertura de la entrada de audio (al arrancar y cada vez que otro programa toma el micrófono y Silente reabre el dispositivo).
- Corrección: `scripts/sign_app.sh` firma el paquete (ad hoc, local) con identificador `audio.silente.studio` y requisito designado `identifier "audio.silente.studio"`, así el permiso queda guardado también tras recompilar. build_universal.sh y build_intel.sh lo usan. No hay certificado de Apple en este Mac: no está notarizada.
- Verificado: firma válida y requisito estable; universal arm64/x86_64 motor 37/37, io 6/6, flujo 83/83 con la app firmada. Falta la confirmación de Fer en uso real (un solo «Permitir»).

## Tramo · 02-11-2026 · grabacion02 · Evento rojo al grabar (manual pág. 356)

- Mientras se graba, cada pista armada muestra un evento rojo «Grabando…» desde el punto donde empezó a escribirse la entrada (o el punch in, o el inicio de la vuelta en ciclo) hasta el cursor, con la forma de onda en vivo.
- Motor: picos mín/máx cada 10 ms escritos por el hilo de audio en memoria reservada antes de grabar (2 h por pista, sin asignaciones en el hilo de audio) y publicados con un contador atómico; posición y longitud de la vuelta en `recordLapStart/recordLapOffset/recordCaptured`.
- Pruebas: motor 37/37 (nada antes del punch in, inicio exacto, 1 s = 100 picos que siguen la entrada, limpieza al terminar), flujo 83 (evento creciendo 3 s). Captura: build/workflow-check/recording-live.png.
- App actualizada: releases/macOS/Silente-Universal.app (02-11-2026), probada en arm64 y x86_64: motor 37/37, io 6/6, flujo 83/83; sulphuric idéntico; layout.xml intacto. Pendiente: verificación manual de Fer.

## Tramo · 02-11-2026 · rango02 · Mover, copiar y empujar rangos (manual págs. 300–303)

- Arrastrar dentro del rango (herramienta 2) mueve su contenido en el tiempo y a otras pistas, dejando hueco en el origen; con ⌥ lo copia. Vista previa del destino mientras se arrastra; el rango sigue al contenido. Todas las capas bajo el rango se mueven y conservan su orden.
- Empujar (por el valor de rejilla): ⌥← / ⌥→ inicio del rango, ⌥⇧← / ⌥⇧→ final; ⌘← / ⌘→ desplaza el rango sin mover el contenido.
- Cortar / copiar / pegar con rango: portapapeles por pistas; pega en el cursor sobre la pista seleccionada y las siguientes (⌥V en su posición original). Motor: `copyRange` y `pasteContent` en `src/audio/RangeEdit.*`.
- Pruebas: motor 36/36 (mover = copiar, suprimir, pegar; MIDI en su sitio), flujo 82 (arrastrar, ⌥ copiar con capas, ⌥→, copiar/pegar), idiomas.
- Universal probada: motor 36/36, io 6/6, flujo 82/82 en arm64 y x86_64; sulphuric idéntico; layout.xml del usuario intacto.

## Tramo · 02-11-2026 · rango01 · Edición de rangos (manual págs. 295–305)

- Herramienta 2 «Selección de rango» en la barra (entre Selección y Dividir): arrastrar sobre una o varias pistas, Mayús amplía, doble clic cubre un evento o el hueco entre eventos; resaltado sobre las pistas y «Inicio/Fin/Duración del rango» en la línea de información.
- Operaciones (`src/audio/RangeEdit.*`, audio y MIDI): Dividir ⇧X, Truncar, Suprimir intervalo de tiempo ⇧Retroceso, Insertar silencio ⇧⌘E, Ajustar fundidos al rango A, Suprimir (Retroceso) y Duplicar (⌘D) con rango activo. Edición › Rango y clic derecho sobre el rango; cada operación es un paso de Deshacer.
- Pruebas: motor 36/36 (cada operación con posiciones y desplazamientos exactos, MIDI a 120 BPM), flujo 78 (arrastrar rango, suprimir intervalo, deshacer, insertar silencio), idiomas 760. Captura: build/workflow-check/range.png.
- Universal probada (incluye vista02 y fundidos01): motor 36/36, io 6/6, flujo 78/78 en arm64 y x86_64; sulphuric idéntico; layout.xml del usuario intacto.
- Pendiente: mover/copiar el contenido arrastrando el rango, empujar bordes, cortar/copiar/pegar rango.

## Tramo · 01-11-2026 · fundidos01 · Fundidos y volumen de eventos (manual págs. 396–418)

- Curvas de fundido en el motor (`src/audio/FadeCurves.h`): lineal, igual energía, curva S, rápida y lenta, por fundido de entrada y de salida; guardadas en el proyecto. Lineal da el mismo resultado de antes.
- Manipuladores en el evento: triángulos de fundido en las esquinas superiores y control de volumen abajo a la izquierda (−∞…+6 dB, como dice el manual); curvas y sombreado dibujados, la forma de onda sigue al volumen y a los fundidos.
- Menú nuevo **Audio** (entre Pista y Transporte, como Cubase): Fundidos › Abrir editor(es) de fundido, Aplicar fundido de entrada/salida estándar, Eliminar fundido de entrada/salida/fundidos, Fundido cruzado; Pegar con el evento siguiente; Enmudecer evento.
- Editor de fundidos (`src/ui/project/FadeEditor.*`): entrada y salida lado a lado, 5 curvas, duración limitada al evento, Por defecto (fundido estándar en transport.xml) y Aplicar.
- Pruebas: motor 35/35 (curvas a mitad de fundido, volumen, guardado/carga), flujo 75 (menú Audio, arrastre de manipuladores, editor), idiomas 741. Capturas: build/workflow-check/fades.png y fade-editor.png.
- Pendiente: editor de fundido cruzado completo, fundidos automáticos, curvas de volumen con puntos, herramienta de rango.

## Tramo · 01-11-2026 · vista02 · Tamaños, herramientas de Cubase y color (pedido de Fer)

- Visor de eventos: deslizador de zoom de forma de onda (arriba a la derecha) y de altura de pistas (abajo a la derecha, ⇧H/⇧G); la segunda fila de la cabecera se oculta en pistas bajas.
- Bordes arrastrables de la columna Canal, el Inspector, la zona derecha y la zona inferior (MixConsole/editor); medidas en layout.xml.
- MixConsole: el borde derecho de cada canal se arrastra para cambiar su ancho (⌥ todos); se guarda en el proyecto.
- Herramientas en el orden de Cubase: 1 selección, 3 dividir, 4 pegar, 5 borrar, 6 zoom, 7 enmudecer, 8 dibujar, 9 reproducir. Fundido cruzado con X (y menú del evento). Módulo `src/audio/EventEdit.*`.
- Botón de color: abre directamente la paleta de pista (16 tonos × 5 + grises, más color personalizado).
- Las pruebas de flujo y las capturas usan una carpeta temporal de ajustes; ya no tocan ~/Library/Silente.
- Pruebas: motor 34/34, io 6/6, flujo 70/70, idiomas 725; universal arm64/x86_64 igual; sulphuric idéntico bit a bit. Checklist del manual: `docs/CHECKLIST_MANUAL_CUBASE15.md`.
- Pendiente: herramienta 2 (selección de rango), manipuladores de fundido/volumen en el evento (siguiente tramo: fundidos01).

## Tramo · 01-11-2026 · grabacion01 · Grabación (manual págs. 347–359)

- Punch in / Punch out anclados a los localizadores: botones junto a L y R en el transporte, teclas I y O, menú Transporte. Con Punch in, Reproducir pasa a grabar al llegar al localizador izquierdo; con Punch out la toma se cierra en el derecho y la reproducción sigue («Parar después de pinchado de salida automático» la detiene tras el post-roll).
- Grabación en ciclo: una toma por vuelta, todas del mismo archivo; se oye la última. Regla de Cubase aplicada a la reproducción: donde se solapan eventos de audio solo suena el de arriba.
- Transporte › Modos de grabación (Punch in/out o Regrabar; iniciar en el cursor, en el localizador izquierdo/punch in o en la selección), Modo grabación de audio (Mantener historial / Ciclo historial + reemplazar / Reemplazar, `src/audio/RecordingModes.*`) y Formato de grabación 16/24/32 bit flotante (también clic en la línea de estado). Pre-roll: el cursor arranca antes y se graba desde el punto pedido.
- La grabación es un paso de Deshacer («Grabar»). Carpeta de grabación: «Audio» dentro de la carpeta del proyecto, como Cubase. MIDI grabado en posiciones de proyecto (sirve con ciclo y pinchado).
- Pruebas: motor 31/31 (nuevas: recortar/partir eventos y modos, evento superior audible, punch in/out exacto, tomas en ciclo, 32 bit flotante y MIDI en posición), io 6/6, flujo 62 (I/O, menús, toma pinchada desde la interfaz, deshacer), idiomas 703. Exportación de sulphuric idéntica bit a bit. Universal probada: motor 31/31, io 6/6, flujo 62/62 en arm64 y x86_64.
- Incidente: un script vació `src/AudioEngine.cpp` (no tiene git propio). Se reconstruyó desde el respaldo del 29-10 reproduciendo cada edición registrada; 84 archivos reconstruidos coinciden exactos con los actuales y 8 fragmentos del motor coinciden línea por línea. Respaldo nuevo: `backups/fuentes-2026-11-01-motor-recuperado.tar.gz`.
- No probado con micrófono o interfaz real (las pruebas usan entrada sintética offline). Pendiente: evento rojo creciendo mientras graba, pregrabación de audio (hasta 60 s), carriles de tomas, modos de grabación MIDI (Mezcla/Reemplazar), compensación de latencia de entrada.

## Tramo · 01-11-2026 · diseno02 · Rigor de diseño Cubase (pedido de Fer)

- Faders y perillas en `SilenteLookAndFeel`: tapa gris rectangular con raya central sobre ranura oscura; deslizadores horizontales finos con punto blanco; perillas oscuras con anillo gris y aguja blanca. Se quitaron los colores azul/menta forzados.
- Inspector como Cubase: barra de nombre en color, fila M S ● ◉, volumen y panorama, secciones plegables Enrutado / Inserciones / Ecualizadores (clic en la cabecera).
- Columna «Canal» como Cubase: nombre, curva de EQ, «▼ Inserciones» (ranuras ocupadas en verde azulado), «▼ Envíos», M S e, fader con escala y medidor estéreo, ● ◉ y nombre en color al pie.
- Barra de herramientas: icono Hub (casa: Nuevo/Abrir/Guardar) en lugar del texto «SILENTE», separadores finos entre grupos, icono de exportar en lugar del botón de texto. Iconos dibujados por Silente, sin recursos de Steinberg.
- Pruebas: motor 26/26, io 6/6, flujo 54, idiomas 681. Captura: build/visual/d3.png.
- Universal (releases/macOS/Silente-Universal.app) probada en arm64 y x86_64: motor 26/26, io 6/6, flujo 54/54 en ambas.
- Pendiente: comparar contra más capturas de Cubase (zona derecha, regla, colores de eventos) cuando Fer las envíe.

## Tramo · 01-11-2026 · io01 · Importar, exportar y conversión de formatos

Pedido de Fer. Cada función en su propio archivo (VS Code), con pruebas propias:
- `src/io/Mp3Encoder.*`: MP3 con LAME 3.100 (LGPL), descargado de SourceForge con permiso de Fer, SHA-256 verificado (ddfe36ca…da1e), compilado como biblioteca estática universal con `third_party/lame-config/config.h` (sin configure). build.sh lo re-extrae del tarball verificado si falta.
- `src/io/AudioFileFormats.*`: escritor común WAV/AIFF (16/24/32 float), FLAC (16/24), MP3 (128–320 kbit/s, ≤48 kHz) y M4A/AAC (afconvert de macOS); conversión de cualquier audio o video con remuestreo sinc.
- `src/io/VideoAudio.mm`: audio de MP4/MOV/M4V con AVFoundation (sin código de terceros).
- `src/io/TrackExport.*`: «Exportar mezcla de audio»: Stereo Out y/o cualquier número de canales, un archivo por canal («Nombre - Pista»), cada canal en solo (con sus buses), mute/solo restaurados.
- Motor: `AudioEngine::renderMix` (render offline con cancelación y progreso); `exportMix` lo usa.
- Interfaz `src/ui/io/`: ventana Exportar mezcla de audio (selección de canales, nombre, ruta, formato, frecuencia, bits/kbit/s), ventana Conversión de formatos (lote, video incluido, junto al original o en otra carpeta), trabajos en segundo plano con progreso y Cancelar. Archivo: Importar audio/MIDI/video, Exportar mezcla de audio, Conversión de formatos, Extraer audio de video. Importar un video crea la pista con su audio (~/Music/Silente/Audio de video).
- Pruebas `tests/IoTests.cpp` (en --self-test): 6/6 — WAV/AIFF/FLAC exactos a 24 bit; MP3 y M4A con nivel ±0,5 dB, duración y canales correctos; MP3 a 96 kHz rechazado; 44,1→48 kHz conserva tono y duración; audio extraído de un .mov; exportación de mezcla + 2 pistas separadas (pista silenciada exportada con señal, mute/solo restaurados, nombres) y mezcla en MP3. Flujo 31 PASS (capturas releases/macOS/Silente-exportar.png y Silente-conversion.png), motor 24/24, idiomas 552, dispositivos 22; exportación de sulphuric idéntica bit a bit.
- Universal arm64/x86_64 probada: motor 24/24, io 6/6, flujo 31 en ambas.
- Limitación: no se probó con un video real con imagen (la prueba usa un .mov solo de audio; AVFoundation trata ambos igual). Sin rango de exportación por localizadores todavía.

## Tramo · 01-11-2026 · visual01 · Acabado visual estilo Cubase 15 y tamaño de letra

Referencias: figura pág. 55 del manual (colores muestreados píxel a píxel con un script Swift) y capturas de Cubase 15 que envió Fer (proyecto completo, cabeceras de pista, MixConsole inferior, transporte).
- Paleta en `src/ui/theme/Colours.h`: paneles #222528, fondos #1c1e1f, separadores oscuros, visor de eventos #2b2c2f con rejilla tenue, pista seleccionada gris claro con texto oscuro, estados activos gris claro (#8e8f90) con texto oscuro, mute amarillo, solo y grabar rojos, monitor naranja, ciclo morado, reproducir verde, medidores cian.
- Cabecera de pista como Cubase (`Arrangement`): franja de color con icono del tipo y número; fila 1 M · S · nombre · mini-fader de volumen con dB (arrastrable, doble clic 0 dB); fila 2 ● · ◉ · R · W · e (e abre la configuración del canal; R/W solo visual, sin automatización); medidor fino. Cabecera 250 px, carril 60 px.
- MixConsole inferior = MixConsole compacto (mismas tiras que la ventana F3, secciones plegadas, botones en una fila). Corregido fallo de mixconsole01: una sección plegada seguía ocupando altura. Tiras con nombre en color arriba y abajo.
- Inspector con barra de nombre en el color de la pista y cabeceras de sección «▼ Ecualizadores / Inserciones / Enrutado».
- Transporte centrado: localizadores a la izquierda, posición musical, ciclo/parar/reproducir/grabar, tiempo, tempo con Tap, compás y luces.
- Botones estrechos dibujan su letra completa.
- **Tamaño de la interfaz y de la letra:** menú Ventana › 100/115/125/140/150 %, guardado en ~/Library/Silente/interface.xml; arranca en 115 %. La ventana se ajusta a la pantalla.
- Pruebas: motor 24/24, io 6/6, flujo 32 (nueva: MixConsole compacto con secciones plegadas mantiene el fader dentro de la tira; coordenadas de regla adaptadas a la cabecera de 250 px), idiomas, dispositivos 22, reproducción sin pérdidas a 115 %. Captura: releases/macOS/Silente-visual-cubase.png.
- Pendiente visual: iconos de barra de herramientas aún propios; zona derecha sin pestañas CR/Automatización; lista de pistas sin filtro/búsqueda (1B).

## Tramo · 31-10-2026 · mixconsole02

Enrutado real en el motor: cada pista tiene salida (Stereo Out o canal de grupo) y 4 envíos a canales FX (nivel −∞…+6 dB, pre/post fader, activar), guardados en el proyecto (`output`, `sends`) y en deshacer. El motor resuelve destinos por id y ordena por rangos (fuentes antes que buses) sin reservar memoria en el hilo de audio; bucles, destino propio o de tipo equivocado se rechazan (`canRoute/setOutput/setSend`). Envío pre = tras inserciones y EQ, antes del fader (sigue al mute); post = tras fader y panorama. Solo: un bus sigue abierto si le llega una pista en solo, y una pista suena si su grupo/FX está en solo. MixConsole: botón de salida con grupos (los que harían bucle, desactivados) y rack Envíos (destino, nivel, PRE, activar, quitar). Pruebas: motor 24/24 (nueva de señal: suma en grupo con su fader/pan, solo a través de buses, post y pre fader, mute en pre, nivel −6 dB, envío desactivado, bucles rechazados, guardado/carga), flujo 28 PASS, idiomas 459, dispositivos 22; reproducción con interfaz sin pérdidas; exportación del proyecto sulphuric idéntica bit a bit a la anterior. Universal arm64/x86_64 probada. Limitaciones: la compensación de latencia no suma la de los buses en cadena; sin panoramizador de envío propio; clics reales en la ventana pendientes de Fer.

## Tramo · 31-10-2026 · mixconsole01

Ventana MixConsole propia (F3; ⌥F3 sigue siendo el mezclador de la zona inferior; menú Estudio) en `src/ui/mixconsole/MixConsole.*`, según `docs/REFERENCIA_CUBASE15_MIXCONSOLE.md`. Barra: Visibilidad, Buscar canal, Tipos de canal, plegar racks Enrutado/Inserciones/EQ, Restablecer clips. Zona izquierda: visibilidad por canal y anclaje a zona izquierda/derecha. Canales: entrada (audio, menú mono/estéreo 1–18) → Stereo Out, instrumento + 4 inserciones (editor/opciones/bypass existentes), EQ 3 bandas con rótulos, panorama centrado, M/S/«e», R/monitor solo en pistas con eventos, fader en dB con escala, volumen editable (acepta coma), medidor estéreo con retención de pico, indicador de clip y pico máximo (clic restablece), nombre/color/tipo, selección sincronizada; Stereo Out fijo a la derecha; carpeta sin canal. Todas las ediciones con deshacer y marca de cambios. Pruebas: flujo 26 PASS (5 nuevas de MixConsole: canales visibles, mute con deshacer, fader en dB, volumen con coma, búsqueda), motor 23/23, idiomas 445, dispositivos 22, reproducción con interfaz sin bloques perdidos. Captura: releases/macOS/Silente-mixconsole-01.png y revisión visual con la sesión sulphuric. Pendiente: clics reales de Fer (ventana en otro escritorio); visibilidad/anclajes no se guardan aún en el proyecto; salidas a grupo y envíos a FX = mixconsole02.

## Tramo · 31-10-2026 · metronomo01

Metrónomo propio en `src/audio/Metronome.*` (cinco sonidos sintetizados: clásico, bloque de madera, cencerro, digital, clave; acento en el 1, subdivisión negras/corcheas/tresillos/semicorcheas, volumen, solo al grabar, precuenta 0–2 compases que retiene transporte y grabación, vista previa con transporte parado; ajustes en ~/Library/Silente/metronome.xml). Calculadora de tempo `src/audio/TapTempo.h` + `src/ui/metronome/TapTempoPanel` (Espacio o clic; mediana de intervalos, descarta toques atípicos, corrige golpes saltados, reinicia tras 2 s; ÷2, ×2, aplicar con deshacer). Tecla C, menú Estudio (metrónomo, ajustes, calculadora), icono de metrónomo redibujado, flecha de ajustes, luces de pulso y botón TAP en la barra inferior, BPM arrastrable. Pruebas: 23/23 motor (5 nuevas: posición y acento de clics, subdivisión en los 5 sonidos, precuenta, vista previa, tap tempo), 22 dispositivos, flujo, idiomas 411/411, reproducción con interfaz sin bloques perdidos. Ventana de calculadora revisada visualmente; toques reales con Espacio pendientes de Fer (la ventana estaba en otro escritorio). Pendiente: compases distintos de 4/4 y patrones de clic por compás como Cubase.

## Corrección · 31-10-2026 · reproducción con ruido

Causa medida: `Arrangement::paint` (y otros repintados) retenían `modelLock` mientras dibujaban; el callback de audio usa try-lock y entregaba silencio → 8,1 % de bloques perdidos con la ventana abierta (chasquidos, reloj atrasado). El modelo solo se modifica en el hilo de mensajes, así que las lecturas de pintura ya no toman el candado (8 sitios en MainComponent). Además, clips con frecuencia distinta al dispositivo usan `src/audio/BandlimitedInterpolator.h` (sinc con ventana Blackman-Harris, 16 cruces) en vez de interpolación lineal; misma frecuencia sigue directa. Evidencia: motor offline = suma ideal de los WAV a −145 dB; captura en tiempo real = exportación muestra a muestra; con interfaz y proyecto real a 96 kHz 0 bloques perdidos en 20 s (antes 121/1492); prueba 48→96 kHz: 18 kHz a −0,002 dB e imágenes −129 dB (lineal: −4,2 dB e imágenes ~−13 dB). 18/18 motor, 22 dispositivos, flujo e idiomas. Diagnósticos nuevos: `--capture-playback`, `--render-project`, `--ui-playback-check`. Pendiente: escucha de Fer; opción de frecuencia de proyecto que ajuste el dispositivo como Cubase.

## Corrección · 31-10-2026 · reproducción muda

Causa: al arrancar, la entrada por defecto (micrófono de la Mac) queda sin canales activos pero con nombre distinto a la salida; JUCE 8.0.12 crea el combinador de Core Audio y este nunca llama al motor (0 callbacks: cursor quieto, sin sonido). Pasaba con bocinas de la Mac y con audífonos USB. Arreglo en `src/audio/DeviceSettings.cpp`: sin entradas activas se abre solo el dispositivo de salida (también al cambiar ajustes). Prueba nueva `--check-playback WAV SALIDA` con dispositivo real y master en silencio: antes 0 callbacks; después la posición avanza al ritmo real (3,04 s en 3 s), señal presente, 0 bloques perdidos. Con entrada activa (micrófono + audífonos) el combinador sí funciona. 17/17 motor, 22 dispositivos, flujo e idiomas aprobados. Pendiente: prueba audible de Fer.

## Corrección · 31-10-2026 · cierre al cambiar a 96 kHz

Causa (informes .ips): carrera en JUCE 8.0.12 `AudioIODeviceCombiner` (entrada y salida en dispositivos distintos, p. ej. micrófono + bocinas de la Mac): el aviso de Core Audio cerraba el dispositivo desde su hilo mientras el hilo principal lo abría/destruía. Parche propio `scripts/patches/juce-8.0.12-coreaudio-combiner-restart.patch` (aplicado por build.sh): `restartAsync` solo programa y el cierre/reapertura ocurre en el hilo principal. Prueba `--check-rate-switch SALIDA ENTRADA SALIDA_DISPOSITIVO` (12 cambios 48↔96 kHz): antes 2/3 cierres (exit 139), después 0/8 y la frecuencia cambia de verdad. 17/17 motor, 22 dispositivos. Los informes «Python» de esa noche son de apps Qt (QtWebEngine), no de Silente. Pendiente: probar con la SSL 2+ (un solo dispositivo, no usa combinador).

## Ajuste · 31-10-2026 · selector nativo

Abrir, guardar, importar y exportar usan el panel nativo de macOS (FileChooser con diálogo del sistema). Comprobado: «Importar» abre el panel de Finder con favoritos e iCloud; 17/17 pruebas.

## Tramo · 31-10-2026 · pistas01

Inicio con sesión vacía (sin demo) y botón «+» junto a PISTAS con seis tipos: audio, instrumento (selector de instrumento), MIDI, carpeta, FX y grupo; también en menú Pista. `TrackKind` en el motor, guardado como `kind` (proyectos antiguos se leen por `isMidi`). Carpeta sin señal ni canal de mezcla; FX/grupo/carpeta no graban ni reciben audio importado. Comprobado: 17/17 pruebas de motor (incluye prueba nueva de tipos y corrección UTF-8 de la prueba de ida y vuelta), flujo de plugins aprobado, idiomas 371/371, y app abierta vacía con los tipos añadidos vía menú. Apps anteriores de releases/macOS borradas por pedido de Fer; única entrega: Silente-Universal.app (fuentes y backups intactos; se regeneran con los scripts). Pendiente: ruteo real a FX (envíos) y grupo (salida de pista), anidar pistas dentro de carpetas, salida MIDI externa (la pista MIDI suena por ahora con el sintetizador interno); el menú emergente del «+» no se pudo capturar en segundo plano, revisión manual de Fer.

## Tramo · 31-10-2026 · universal-rosetta01

Por petición del usuario (Waves solo valida bajo Rosetta): `scripts/build_universal.sh` genera `releases/macOS/Silente-Universal.app` (arm64+x86_64). Abre nativo; Finder → Obtener información → «Abrir con Rosetta» usa la mitad x86_64, con catálogo propio `Plugins-Intel.xml` y título «· Intel (Rosetta)». Comprobado: `lipo` con ambas arquitecturas y `--probe-plugin` de CLA-76 (s) AU 14.12.90 creado en arm64 y x86_64 (`build/universal-check/`). Pendiente: revisión manual de Fer con la casilla de Rosetta, licencia/editor/sonido de Waves y escaneo WaveShell VST3. Bundle sin firma completa, igual que las entregas anteriores.

## PAUSA solicitada · 29-10-2026 · audio18 y Waves en desarrollo

El usuario pidió detenerse para trabajar en otros proyectos. No continuar automáticamente. Punto de reanudación: docs/RETOMAR_AUDIO18_Y_WAVES.md. Fuentes y compilaciones conservadas. Audio de hasta 18 entradas/salidas configurables, asignación mono/estéreo por pista y persistencia implementados; 22 comprobaciones simuladas aprobadas. SSL 2+ no conectada por decisión del usuario: prueba física pendiente. Compilaciones arm64 e Intel x86_64 completadas. CLA-76 (s) AU 14.12.90 se instanció en ambas arquitecturas, sin verificar licencia, editor ni sonido; escaneo completo WaveShell VST3 14.12 interrumpido tras no terminar en aproximadamente un minuto. No afirmar que Waves esté corregido. Última entrega cerrada: plugins-cursor01. El tramo actual aún requiere revisión y cierre.

## Último tramo cerrado · 29-10-2026 · plugins-cursor01

Por petición del usuario: inicio vacío, arrastre continuo del cursor por la regla y selector de plugins con flujo de insertos/instrumentos. Selector extraído a src/ui/plugins/ y reglas de selección a src/plugins/. Búsqueda, categorías/fabricantes, formatos, favoritos, editor, sustitución, bypass y retirada. Compilación Release y prueba de flujo aprobadas con Apple AULowpass, DLSMusicDevice y TONE3000 VST3; bloque offline sin muestras no finitas para los efectos. Clic/arrastre/liberación/localizadores y destino de carga tras borrar pista comprobados. Bilingüe conservado (336 entradas por idioma). Entrega releases/macOS/Silente-plugins-cursor-01.app. Detalle y límites: docs/TRAMO_PLUGINS_CURSOR_01.md. Pendiente revisión manual, audio audible, otras instancias/plugin formats y funciones avanzadas de Cubase; no iniciar otro tramo automáticamente.

## Tramo anterior · 29-10-2026 · idiomas01

Una aplicación con español por defecto e inglés seleccionable desde la barra superior o el menú Idioma. Preferencia persistente; traducciones separadas en resources/locales/ y servicio en src/i18n/. Compilación Release, 315 entradas por idioma y cambio ES → EN → ES comprobados conservando nombres, colores, notas, tempo y número de pistas. Entrega: releases/macOS/Silente-es-en-01.app y capturas Silente-es.png / Silente-en.png. Detalle y límites: docs/TRAMO_IDIOMAS_01.md. Pendiente revisión del usuario; no iniciar otro tramo automáticamente.

## Tramo anterior · 29-10-2026 · visual01

Tema extraído a `src/ui/theme/`; color de pistas con paleta/personalizado, iconos vectoriales, regla y localizadores, transporte inferior y corrección de textos UTF-8. Compilación Release y captura visual verificadas. Entrega en `releases/macOS/Silente-visual-01.app`; captura `Silente-visual-01.png` en la misma carpeta. Control automatizado del selector de color no autorizado por la herramienta; clic, deshacer y persistencia de un color modificado pendientes de revisión manual. Detalle: `docs/TRAMO_VISUAL_01.md`. No empezar otro tramo sin revisión del usuario.

## Estado comprobado

- Proyecto C++20/JUCE/CMake en Silente.code-workspace, configurado para Visual Studio Code.
- Primera compilación .app generada; ventana principal abierta y comprobada mediante la interfaz nativa de macOS.
- Logotipo original SVG/PNG/ICNS en assets/.
- Código de audio, plugins y pruebas existe; NO implica validación funcional completa. La ejecución previa --self-test no produjo evidencia de resultados y no se cuenta como aprobada.
- Referencias oficiales de Cubase y capturas ya guardadas en docs/INVESTIGACION.md y build/references/. No volver a investigar desde cero.

## Orden propuesto

1. Interfaz: cerrar la pantalla principal con distribución y hábitos de Cubase. Entrega: app abierta y revisión visual del usuario.
2. Audio: importar un WAV, reproducir/parar y ajustar volumen/pan/mute/solo. Entrega: prueba audible reproducible.
3. Proyecto: guardar, cerrar y reabrir sin perder audio ni edición.
4. MIDI: notas, editor e instrumento interno.
5. Plugins: comprobar un VST3 y un AU concretos antes de ampliar compatibilidad.
6. Grabación, efectos y optimización: dividir en etapas independientes según las prioridades del usuario.

El alcance pendiente es audio18 y diagnóstico Waves, actualmente en pausa por solicitud expresa. No reanudar hasta que el usuario lo indique.

## refactor01 — Piano roll y modelo MIDI separados (16-11-2026)

El piano roll pasó de MainComponent.cpp a `src/ui/midi/PianoRoll.{h,cpp}` (`silente::ui::PianoRoll`) y la nota a `src/domain/MidiNote.h` (`silente::domain::MidiNote`, con alias `silente::MidiNote` en AudioEngine.h). Extracción mecánica, sin cambio de comportamiento ni textos. Verificado: compila, motor 43/43, io 6/6, flujo 198 PASS / 0 FAIL / 1 SKIP (VST3 opcional), idiomas 873 PASS. Pendiente: no hay prueba automática de interacción del piano roll; la edición de notas aún no pasa por comandos de aplicación.

## cuantizar01 — Cuantización MIDI y prueba del piano roll (16-11-2026)

Prueba de interacción del piano roll en WorkflowUiTests (crear, mover, longitud, borrar, deshacer). `src/domain/Quantize.{h,cpp}`: función pura (rejilla 1/4–1/32 con tresillos, fuerza 0–100 %, longitud opcional). Comando Q (`quantize`) sobre la nota seleccionada o todas, con un paso de deshacer. Verificado: motor 44/44, io 6/6, flujo 203 PASS sin FAIL (conteo de SKIP no confirmado), idiomas 873. Pendiente: diálogo de ajustes (rejilla/fuerza/longitud; hoy fijo en 1/16, 100 %), selección múltiple de notas y prueba del atajo Q con teclado real.

## exportar01 — Exportar MIDI y mezcla por rango (16-11-2026)

`AudioEngine::exportMidi` (SMF formato 1, 960 PPQ, pista de tempo y una pista por pista MIDI) con comandos `exportMidi` y `exportMidiSelected` e ítems en Archivo. `renderMix`, `exportMix` y `ExportJob` aceptan un rango en segundos; el diálogo de exportación tiene la casilla «Solo entre localizadores izquierdo y derecho» (desactivada por defecto). Verificado: motor 46/46 (MIDI ida y vuelta sin pérdida; WAV por rango de 36000 muestras exactas e idéntico al tramo de la mezcla completa), io 6/6, flujo 407 PASS / 0 FAIL / 1 SKIP (VST3 opcional), idiomas 882. Pendiente: submenú Archivo > Exportar, revisión manual del diálogo y selector, CC/marcadores/cambios de tempo en el MIDI, rango con plugins con latencia y con MP3/M4A/FLAC.

## midi-edit01 — Selección múltiple en el piano roll (16-11-2026)

Funciones puras en `src/domain/NoteEdit.{h,cpp}`; selección por id de nota. Clic, ⇧clic, marquesina, ⌘A y ⌘⇧A; mover, redimensionar, borrar y cuantizar (Q) actúan sobre el grupo con un paso de deshacer por gesto (un clic sin arrastre ya no crea paso). Diseño y subetapas 2–9 en `docs/EDITOR_MIDI.md`. Verificado: motor 47/47, io 6/6, flujo 212 PASS / 0 FAIL / 1 SKIP (VST3 opcional), idiomas 882. Nota de proceso: el código de esta etapa quedó dentro del commit d8fbbb7 (rotulado como investigación .cpr) por un `git add -A` de la sesión principal. Pendiente: contrastar con el manual (págs. 1170–1304, sin lector de PDF), Duplicar del grupo, capa de comandos de aplicación, prueba manual con ratón y teclado.

## tempo01 — Mapa de tempo y de compás (16-11-2026)

`src/domain/TempoMap.{h,cpp}` (eventos de tempo salto/rampa y de compás; `TempoView` sin asignaciones) con persistencia `"tempoMap"` retrocompatible. Reproducción, metrónomo, cabezal de plugins, grabación MIDI, rango, regla, rejilla, piano roll y MIDI SMF (importa y exporta tempo y compás) usan el mapa. Ventana Estudio > «Mapa de tempo y de compás…» (añadir, actualizar, quitar; deshacer). Sin eventos, el render es idéntico bit a bit al anterior (SHA1 comprobado). Verificado: motor 49/49, io 6/6, flujo 225 PASS / 0 FAIL / 1 SKIP (VST3 opcional), idiomas 903. Pendiente: contrastar con el manual (págs. 1494–1519), pista gráfica de tempo, tempo/compás en el cursor en el transporte, atajo ⌘T, precuenta con compás real, caché O(log) en TempoView, base de tiempo musical del audio, revisión visual manual.

## auto01 — Automatización de volumen, pan y envíos (16-11-2026)

`src/domain/Automation.{h,cpp}`: curvas en beats (siguen el mapa de tempo), interpolación lineal; R/W por pista (W enciende R). El motor evalúa la curva en los bordes de cada segmento dentro del try-lock existente, en rampa sin asignaciones (`audio/AutomationRamp.h`); suaviza saltos y conserva la ruta constante (SHA1 idéntico en dos proyectos de referencia). Persistencia `"automation"` retrocompatible; deshacer por instantánea; comandos en `app/AutomationCommands`; pestaña inferior «Automatización» (`ui/automation/AutomationLane`) y R/W en la MixConsole, cuyo fader sigue la curva. W escribe en el cursor solo con el transporte parado. Verificado: motor 51/51, io 6/6, flujo 236 PASS / 0 FAIL / 1 SKIP (VST3 opcional), idiomas 913. Pendiente: escritura en vivo (Touch/Latch/Overwrite; decide Fernando; requiere transferencia sin bloqueo), carril bajo cada pista, ajuste a rejilla, master/plugins, prueba audible, contraste con manual (págs. 900–926).

## carpetas01 — Carpetas reales y reordenamiento de pistas (17-11-2026)

Jerarquía en preorden en `src/domain/TrackTree.{h,cpp}` (mover, anidar, sacar de carpeta, rechazo de ciclos, reparación al cargar); comandos en `src/app/TrackOrderCommands`; `AudioEngine::applyTrackOrder` por id, así envíos, salidas a grupo y automatización se conservan. Mute/solo de carpeta pasan a su contenido (comportamiento conocido de Cubase, sin verificar en el manual). Lista de pistas con sangría y guías, plegado, arrastre con indicador, menú y menú Pista «Mover arriba/abajo», «Sacar de la carpeta», «Mover a la carpeta», un paso de deshacer; vista de partes de solo lectura. Persistencia retrocompatible: campos solo si hay carpetas. Verificado: motor 53/53, io 6/6, flujo 254 PASS / 0 FAIL / 1 SKIP (VST3 opcional), idiomas 921, SHA1 idéntico en ref.silente y ref2.silente. Decisiones propias a confirmar por Fernando: eliminar una carpeta conserva su contenido (Cubase lo borra); marcador y regla no entran en carpetas. Pendiente: pistas nuevas dentro de la carpeta seleccionada, selección múltiple al arrastrar, jerarquía en Visibilidad y MixConsole, ventanas de canal/plugin por id, atajos, prueba con ratón real, contraste con manual (págs. 143–252).

## pool01 — Pool de medios con reasignación de rutas (17-11-2026)

`src/domain/MediaPool.{h,cpp}` (agrupación por ruta, usos, estado presente/ausente/reemplazado, orden por columna, reasignación). Ventana Pool (`src/ui/pool/PoolPanel`, Cmd+P, comando `openPool`) con doble clic para seleccionar un clip del medio y «Localizar archivo ausente…» con un paso de deshacer; `AudioEngine::mediaClips` y `relinkMedia`. Verificado: motor 54/54, io 6/6, flujo 0 FAIL / 1 SKIP (VST3 opcional; el recuento exacto de PASS no se confirmó), idiomas 937, SHA1 de render idéntico. Pendiente: selección múltiple de clips por medio (hoy solo el primero), estado «reemplazado» persistente, prueba del selector nativo y revisión visual, importar/borrar/convertir desde el Pool, contraste con manual.

## cpr-import01 — Importar proyecto Cubase (.cpr) (17-11-2026)

`src/io/CprImport.{h,cpp}`: lector de solo lectura RIF2/RIFF (gramática de tools/cpr/cpr_read.py) y verificación de cada clip contra la cabecera del WAV (frames, canales, Hz y bits; `Audio/` primero, luego la carpeta guardada). `prepare` decodifica en hilo de trabajo y `apply` sustituye el proyecto: pistas de audio, eventos (ticks/480 → TempoView), tempo inicial (f32 redondeado a 0,001) y compás. Archivo › «Importar proyecto Cubase (.cpr)…» guarda la sesión fuera de la carpeta del .cpr con el informe `<sesión> - importación Cubase.txt`; modo `--import-cpr`. tom-01: 43 pistas, 99/99 clips verificados (1 de nombre distinto, informado), 187/187 eventos, 67 BPM, 2/4; inicio idéntico al lector Python y longitud ≤ 1,1 µs. Verificado: motor 56/56, io 6/6, flujo 266 PASS / 0 FAIL / 1 SKIP (VST3 opcional), idiomas 1002. No importa: fundidos (mínimo de 5 ms), volumen/pan, mute/solo, envíos, inserciones, color, carpetas, automatización, marcadores, partes, MIDI. Pendiente: una copia por medio al guardar (hoy 187 copias), prueba manual del menú y de la ventana del informe, prueba audible, varios puntos de tempo, eventos de Cubase 12 (RIFF), canales.

## cpr-open01 — «Abrir proyecto» acepta .cpr (17-11-2026)

«Abrir proyecto de Silente o Cubase» acepta .silente, .json y .cpr. Un .cpr entra en el mismo flujo que «Importar proyecto Cubase»: `routeCpr` → `chooseCubaseDestination` → `runCubaseImport`. Prueba en WorkflowUiTests mediante `openProjectFile` y `cubaseImportOverride`. Verificado: motor 56/56, io 6/6, flujo 0 FAIL / 1 SKIP, idiomas 1003. El selector nativo queda sin probar a mano y el flujo de punta a punta con un .cpr real no se ha ejercitado todavía.

## wave-height01 — Ondas visibles y altura de pistas (17-11-2026)

Las ondas no se veían por dos causas medidas: los eventos del .cpr empiezan hacia 25 s, fuera de la ventana inicial, y el título de 22 px dejaba unos 10 px de onda a 60 px de pista. Ahora el título mide 15 px (22 px desde 90 px de pista) y `Arrangement::revealFirstEvent` coloca la vista en el primer evento al abrir o importar. Altura por pista: arrastrar el borde inferior en la cabecera (⌘/Ctrl = todas), menú de zoom ▾ con los presets del manual (1–4 filas, N, completo, 4/8 pistas, mínimo, ajustar) y Edición › Expandir pista seleccionada. Alturas guardadas en `layout.xml` (`trackHeights`). Verificado: motor 56/56, io 6/6, flujo 284 PASS / 0 FAIL / 1 SKIP (VST3 opcional), idiomas 1020. Sin probar a mano: cursor, arrastre con ratón, menú ▾ y revealFirstEvent por la interfaz. Pendiente: 6 avisos -Wshadow por el `lane` local.

## zoom-bars01 — Barras de zoom horizontal y vertical (19-11-2026)

El zoom horizontal funcionaba, pero no tenía control visible; la rueda requería ⌘/Ctrl y se anclaba al borde izquierdo. Ahora hay barra −/deslizador/+ abajo a la derecha (ids `h-zoom*`), ⌘+rueda y pellizco anclados al puntero. La barra vertical (altura y onda) ya existía y tiene prueba de arrastre y límites. Verificado: motor 56/56, io 6/6, flujo 0 FAIL / 1 SKIP (VST3 opcional), idiomas 1023. Sin probar con ratón real. La fila inferior debe medir 12 px: con 14 fallan dos pruebas de coordenadas.

## ciclo-ruler01 — Clic en el rango de localizadores activa el ciclo (19-11-2026)

Manual p. 325: un clic sin arrastre en el rango de localizadores de la regla activa o desactiva el modo ciclo; el arrastre sigue moviendo el cursor. Implementado en `Arrangement` (`src/MainComponent.cpp`, marca `locatorRangeClick` en `mouseDown` y toggle en `mouseUp` vía `changed`). Se mantienen los tiradores de los localizadores y la botonera de ciclo. Verificado: 304 PASS / 0 FAIL en el flujo de interfaz (4 comprobaciones nuevas en `tests/MarkerUiTests.cpp`). Sin probar con ratón real. Pendiente: no hay paso de Deshacer para el ciclo (igual que el botón); contrastar con el manual la lectura completa de ciclo (págs. 350–351 y 448) y el modo punch/loop de la investigación de punch.

Nota de entorno: el `.venv` y la caché de CMake de `build/` apuntaban a `UNIVERSIDAD/FUNDACIONES/Silente`. Se reconfiguraron dentro de este proyecto (venv reinstalado y `CMakeCache.txt` regenerable retirados); los binarios se reconstruyeron.

## punch-pt01 — Punch in/out al estilo Pro Tools: escucha de la entrada dentro del punch (19-11-2026)

Pedido de Fer: como Pro Tools, la pista armada sigue sonando con su material mientras graba y oye su entrada solo entre punch in y punch out; la toma se graba encima sin que el monitor se grabe. Lo que ya existía: I/O anclados a los localizadores, arranque de la grabación al llegar al punch in, grabación solo dentro de la ventana, reproducción tras el punch out, y el material propio sonando durante la toma. Lo que faltaba: escuchar la entrada sin activar ◉ a mano. Implementado en `src/AudioEngine.cpp` (`inputMonitored`: pista armada que graba en el bloque y dentro de la ventana; la ventana se calcula igual que la captura). Sin cambios de interfaz.
Verificado: motor 57/57 (nueva prueba «punch over an armed track…»: material audible antes, durante y después; entrada monitorizada solo dentro; la toma no contiene el monitor), io 6/6, flujo 304/304. Modo por defecto «Ciclo historial + reemplazar»: el evento existente se corta en el punch; «Mantener historial» en Transporte › Modo grabación de audio lo deja debajo de la toma.
Sin probar con interfaz ni micrófono real. Pendiente: confirmar la latencia de monitorización con hardware, y decidir si el ◉ manual debe seguir teniendo prioridad sobre la escucha automática.

## entrega19-11 — Universal con ciclo en la regla y punch-pt01 (19-11-2026)

Sustituida `releases/macOS/Silente-Universal.app` por un build universal arm64 + x86_64 de las fuentes `7ac8a27`, firmado (`audio.silente.studio`, verificación estricta correcta). SHA-256 del ejecutable `98026bf97042d6e99f97b9e147f3f7bbbc498c2ee161fcce25ad422ef0f5486c`. Sin copias de respaldo. Retiradas las apps temporales de `build/` y `build-universal/` con `clean_generated_apps.py --apply`; solo queda la app entregada.
Verificado sobre la app entregada: motor 57/57 y formatos 6/6 en arm64 y en x86_64 (Rosetta); interfaz 304/304 en ambas. No se probó con interfaz de audio ni micrófono real.
Incidente de entorno: la caché de CMake de `build-universal` apuntaba a `UNIVERSIDAD/FUNDACIONES/Silente`; se borró esa carpeta regenerable y se volvió a compilar.

## pintar-pistas01 — Color de pista también en el panel de pistas (19-11-2026)

Pedido de Fer, según el tutorial de Cubase: al pintar una pista, la fila de su cabecera en el panel izquierdo también se pinta, con un tono algo más oscuro. Antes solo tenía una franja estrecha. Implementado en `src/MainComponent.cpp` (fila de cabecera con `colour.darker(.3f)`, solo si la pista tiene un color distinto del de por defecto, ahora `defaultTrackColour` en `src/AudioEngine.h`; marcadores y reglas no cambian). Los eventos ya tomaban el color de la pista.
Verificado: flujo de interfaz 305/305, con una comprobación nueva en `tests/FolderUiTests.cpp` que lee un píxel de la fila pintada. Sin probar a mano con el ratón ni con el tutorial completo.
Entregada en `releases/macOS/Silente-Universal.app` (arm64 + x86_64, firmada, sin respaldos), SHA-256 `d74e2df9f8e149f514d47c26a717313323bd8fb8725f0f71984920614f4fc775`. Motor 57/57 y formatos 6/6 en ambas arquitecturas; interfaz 305/305 en arm64. En x86_64 (Rosetta) las pruebas de interfaz varían de una ejecución a otra (una vez 303/305, otra 305/305): comprobaciones de menú y foco sensibles al tiempo, no atribuibles a este cambio.
Pendiente: decidir si pintar la fila también al seleccionar la pista, y si el color por defecto debe poder elegirse expresamente.

## entrega-midi01 — Universal con subetapa 2 MIDI (19-11-2026)

Reemplazada `releases/macOS/Silente-Universal.app` desde `36481a1` (comandos MIDI con nombre en el historial, EDITOR_MIDI subetapa 2). Universal arm64 + x86_64, firmada, sin respaldos; SHA-256 `900ec80f229e4d8a44e64bb9809ec14352d2d5aa90774997ace8f9854568537b`. Motor 57/57 en ambas arquitecturas; interfaz 313/313 en arm64.
En x86_64 (Rosetta) la comprobación «Slot ten's real options menu bypasses only its own processor» falla de forma intermitente (2 de 4 ejecuciones). Ocurre también antes de este cambio; sin resolver. Pendiente: investigar la causa temporal antes de tomarla como aprobada.

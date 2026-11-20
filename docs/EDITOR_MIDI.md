# Editor MIDI (Editor de teclas): diseño por subetapas

Referencia UX: Cubase Pro 15, capítulo Editores MIDI (manual, págs. 1170–1304). El texto del PDF se lee con PDFKit (`swift`, sin instalar nada: las páginas PDF coinciden con la numeración del manual). Antes de cerrar cada subetapa hay que contrastar su comportamiento con esas páginas y registrarlo aquí. Sin esa comprobación, el editor no se considera equivalente al de Cubase.

## Contraste con el manual (leído 19-11-2026)

Páginas leídas: 1170–1171 (funciones comunes) y pasajes del Editor de teclas (1229–1262) sobre selección, transposición, empuje, cuantización y escalas. **Pendiente:** leer la sección «Operaciones con el editor de teclas» completa y las páginas 1263–1304 (percusión, lista, in-place).

- **Selección (subetapa 1):** las páginas leídas no describen ⇧-clic, marquesina ni ⌘A con el detalle de la tabla de arriba. Queda sin contrastar.
- **Transposición:** la paleta de transposición mueve ½ tono y una octava (p. 1236). Silente no lo tiene: pendiente en la subetapa 2.
- **Teclas de cursor:** mueven el evento por semitonos y ⇧+flecha por octavas (p. 1247). Silente no las tiene: pendiente en la subetapa 4.
- **Empujar:** inicio y final a izquierda y derecha, y mover por el valor de la rejilla de empuje (p. 1235). Ajustes de empujar son una rejilla propia y separada de la de cuantización. Subetapa 4; la rejilla de empuje no está en el plan.
- **Cuantizar:** presets de cuantización y de groove, cuantización suave, panel y «Cuantizar duración» (pp. 1235–1236). Subetapa 8, ampliada con cuantización suave y de duración y presets de groove.
- **Escalas y tonos (no están en el plan):** Cuantizar tonos, Ajustar edición de tonos y visibilidad de tonos por escala. Son funciones avanzadas; registrarlas como pendientes nuevas, sin prometerlas en la subetapa 9.
- **Extender a siguiente seleccionada y Escalar duración/legato (no están en el plan):** añadir como pendientes nuevos.
- **Junta eventos y Suprime eventos (en la barra del editor, no en el plan):** pendientes nuevos.
- **Controladores en el rango de notas (p. 1229):** Seleccionar controladores en el rango de notas y copiar/pegar entre carriles. Subetapa 7.
- **Zoom (p. 1171):** herramienta Zoom, deslizadores y ⌥-clic. Silente usa H/G y la barra; falta la herramienta de zoom del editor.
- **Partes MIDI (p. 1236):** «Parte activa», «Todas las partes» y «Todas las partes en pista activa» restringen la selección. Silente no tiene partes: ya figura en las limitaciones.

## Capas

- **Dominio** (`src/domain/MidiNote.h`, `NoteEdit.*`, `Quantize.*`): datos y reglas puras sin JUCE GUI ni motor. Son funciones sobre `std::vector<MidiNote>` que se prueban en EngineTests.
- **Comandos de aplicación** (objetivo): `app/MidiEditCommands` recibirá «pista + selección + operación», tomará `modelLock`, hará un único `pushUndo(nombre)` y marcará el proyecto como modificado. Hoy PianoRoll hace ese papel a través de `beforeEdit`/`changed`. La subetapa 2 lo extrae para que el menú, los atajos y el piano roll compartan la misma ruta.
- **Interfaz** (`src/ui/midi/PianoRoll.*`): traduce los gestos a coordenadas musicales, llama al dominio y pinta. No contiene reglas de edición.
- **Audio**: lee `notes` bajo `ScopedTryLock`. Las ediciones mantienen el bloqueo poco tiempo. Durante el arrastre, se copia elemento por elemento, sin reasignar el vector.

## Modelo de selección (subetapa 1, implementada)

`domain::NoteSelection` guarda las notas seleccionadas por **id**, así que sobrevive a borrados, reordenamientos e instantáneas de deshacer. `PianoRoll::selected` queda como nota ancla (la última pulsada o creada). Los gestos se comportan así:

| Gesto | Efecto |
|---|---|
| Clic | Selecciona solo esa nota; si ya estaba seleccionada, conserva el grupo. |
| ⇧ + clic | Añade o quita la nota de la selección. |
| Arrastre en vacío | Marquesina; con ⇧, añade a la selección. |
| ⌘A / ⌘⇧A | Seleccionar todo / anular la selección. |
| Arrastre de nota | Mueve el grupo; el ancla cae en la rejilla y el grupo se limita en bloque (inicio ≥ 0, tono 0–127). |
| Borde derecho | Aplica el mismo cambio de longitud a todo el grupo (mínimo 1/64). |
| Supr / Retroceso | Borra la selección. |
| Q | Cuantiza la selección, o todas las notas si no hay selección. |
| Clic derecho | Caja de herramientas del editor (o menú contextual con la preferencia desactivada o con una tecla modificadora). Ya no borra la nota. |

El primer cambio real de cada gesto genera un solo paso de deshacer. Un clic sin arrastre no crea ninguno.

## Herramientas (subetapa 3, implementada)

Contrastado con el manual, pp. 1233 (botones de herramientas), 1253–1254 (Dibujar, Borrar, doble clic, ⌥ con Seleccionar) y 1257 (Dividir, Pegar). La herramienta es estado de `PianoRoll::tool` (ids y teclas iguales a las del proyecto); `ui/midi/MidiTools` define la lista, la barra `midi-toolbar` y las teclas; las ediciones pasan por `app/MidiEditCommands` con un paso de historial con nombre.

| Herramienta | Clic sobre nota | Clic en vacío |
|---|---|---|
| Seleccionar (1) | Selección y arrastre de la subetapa 1. ⌥ + clic dibuja. Doble clic en vacío inserta una nota de 1 tiempo. | Marquesina |
| Dibujar (8) | Edita como Seleccionar | Nota de 1/16 en la rejilla; arrastrar a la derecha alarga en múltiplos de 1/16 (un solo paso de deshacer) |
| Borrar (5) | Borra la nota pulsada | Nada |
| Dividir (3) | Corta en la posición de la rejilla; si la nota está seleccionada, corta toda la selección en el mismo punto | Nada |
| Pegar (4) | Une con la siguiente nota de la misma altura; conserva las propiedades de la primera | Nada |
| Enmudecer (7) | Alterna el enmudecimiento; sobre una nota seleccionada actúa en toda la selección | Nada |

Teclas con el foco en el editor: 1, 8, 5, 3, 4, 7 y F9/F10 (recorren solo estas seis). Clic derecho: caja `midi-toolbox` (reutiliza `ProjectToolbox`) según la preferencia «Mostrar caja de herramientas con clic derecho»; con modificador o con la preferencia desactivada, menú con «Herramientas», Borrar, Duplicar y Cuantizar.

Las notas enmudecidas (`MidiNote::muted`) se dibujan solo con contorno, se guardan en el proyecto (`"muted": true`), no envían note-on al motor ni se exportan a SMF; conservan su note-off para que enmudecer una nota que suena no la deje colgada.

**Diferencias pendientes con Cubase:** Selección de rango, Trim, Línea, Zoom y Baqueta no están en el editor; Dibujar no edita velocidad/tono/posición al arrastrar (p. 1254); Borrar y Enmudecer no actúan por arrastre; doble clic sobre una nota con Seleccionar no la borra (p. 1253); la duración de dibujo es fija en 1/16 (falta «Cuantizar duración»); no hay cursor propio por herramienta.

## Plan de subetapas (cada una con su prueba de aceptación)

1. **Selección múltiple** (hecha): dominio `NoteEdit`, cuantización de la selección y pruebas de motor e interacción.
2. **Comandos de edición** (hecha; transponer aún sin atajo ni menú, llegan con la subetapa 4): crear `app/MidiEditCommands.{h,cpp}` (mover, redimensionar, borrar, cuantizar, duplicar y transponer, todo sobre la selección) con nombre en el historial (`i18n::tr`). Duplicar (⌘D) debe actuar sobre el grupo. Aceptación: `editHistory()` muestra el nombre y deshacer/rehacer funcionan para cada comando en WorkflowUiTests.
3. **Herramientas del editor** (hecha, ver abajo): Seleccionar, Dibujar, Borrar, Dividir, Pegar y Enmudecer, en una barra del piano roll y en la caja al clic derecho. Aceptación: cada herramienta cambia el resultado del clic en una prueba de interacción.
4. **Portapapeles y empuje**: copiar, cortar y pegar en el cursor; flechas para empujar por la rejilla (tono con ↑/↓, ⇧ = octava). Aceptación: valores exactos tras pegar o empujar, más deshacer.
5. **Línea de información**: inicio, longitud, tono y velocidad de la selección, editables (con valores relativos si hay varias notas). Aceptación: editar un campo cambia todas las notas seleccionadas.
6. **Carril de velocidad**: barras por nota y edición con arrastre o línea; el modelo ya guarda `velocity`. Aceptación: el motor genera eventos noteOn con la velocidad editada (prueba sin dispositivo).
7. **Controladores (CC, inflexión de tono)**: crear `domain::MidiController` y persistirlo, con carriles seleccionables y exportación SMF. Aceptación: ida y vuelta guardar/abrir y exportar/importar sin pérdida.
8. **Diálogo de cuantización**: rejilla, tresillos, fuerza y longitud; hoy es fijo en 1/16 y 100 %. Aceptación: cada ajuste se refleja en el resultado.
9. **Funciones de Cubase 16** (Rasgueo, Estirar selección): solo después de revisar la documentación vigente.

## Limitaciones conocidas

- El modelo es una lista de notas por pista; aún no hay partes MIDI.
- `paint` lee las notas sin bloqueo (comportamiento heredado).
- La copia de la lista al empezar un gesto se hace dentro de `modelLock`, en el hilo de la interfaz. Si el bloque de audio no consigue el `TryLock`, ese bloque se omite.

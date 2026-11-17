# Editor MIDI (Editor de teclas): diseño por subetapas

Referencia UX: Cubase Pro 15, capítulo Editores MIDI (manual, págs. 1170–1304). En esta sesión no se pudieron revisar las páginas del PDF (no hay pdftoppm ni pypdf). Antes de cerrar cada subetapa, hay que contrastar su comportamiento con esas páginas y registrarlo aquí. Sin esa comprobación, el editor no se considera equivalente al de Cubase.

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
| Clic derecho | Sigue borrando la nota pulsada; en la subetapa 3 se sustituirá por el menú o la caja de herramientas. |

El primer cambio real de cada gesto genera un solo paso de deshacer. Un clic sin arrastre no crea ninguno.

## Plan de subetapas (cada una con su prueba de aceptación)

1. **Selección múltiple** (hecha): dominio `NoteEdit`, cuantización de la selección y pruebas de motor e interacción.
2. **Comandos de edición**: crear `app/MidiEditCommands.{h,cpp}` (mover, redimensionar, borrar, cuantizar, duplicar y transponer, todo sobre la selección) con nombre en el historial (`i18n::tr`). Duplicar (⌘D) debe actuar sobre el grupo. Aceptación: `editHistory()` muestra el nombre y deshacer/rehacer funcionan para cada comando en WorkflowUiTests.
3. **Herramientas del editor**: Seleccionar, Dibujar, Borrar, Dividir, Pegar y Enmudecer, en una barra del piano roll y en la caja al clic derecho. Aceptación: cada herramienta cambia el resultado del clic en una prueba de interacción.
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

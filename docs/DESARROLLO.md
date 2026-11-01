# Silente · desarrollo

Aplicación de escritorio para macOS Apple Silicon, C++20 / JUCE 8.0.12, CMake y Ninja. Proyecto nativo abierto en Visual Studio Code. No depende de Cubase ni utiliza su código o recursos.

## Estructura

- `src/AudioEngine.*`: transporte, mezcla, audio, MIDI, síntesis, grabación y persistencia.
- `src/MainComponent.*`: interfaz y edición de la sesión.
- `src/PluginManager.*`: catálogo, exploración en procesos separados y ventanas de plugins.
- `src/Main.cpp`: aplicación, menús, procesos auxiliares y pruebas.
- `tests/`: verificaciones con audio generado y archivos temporales propios.
- `assets/`: identidad original de Silente, SVG editable, PNG e icono de macOS.
- `third_party/JUCE/`: código fuente de la dependencia fijada a la etiqueta 8.0.12 (commit 29396c22c93392d6738e021b83196283d6e4d850).
- `.vscode/`: tareas y configuración del editor.
- `scripts/build.sh`: compilación reproducible.

## Compilar

Abrir `Silente.code-workspace`. Ejecutar la tarea **Silente: compilar** (Cmd+Shift+B). La tarea **Silente: ejecutar** abre la aplicación. En terminal:

```sh
./scripts/build.sh
./scripts/build.sh test
open build/Silente_artefacts/Release/Silente.app
```

Requiere las Command Line Tools de Xcode y Python 3. El script instala CMake/Ninja en el entorno `.venv` del proyecto si no están presentes. No modifica el Python del sistema. El código de JUCE se conserva localmente o se descarga de su repositorio oficial si falta.

## Licencias

Se utiliza JUCE bajo su alternativa AGPLv3. Las licencias de dependencias se conservan en `third_party/JUCE/LICENSE.md` y dentro de sus módulos. Antes de distribuir un derivado cerrado hay que escoger una licencia JUCE adecuada. VST3 incluido conserva su licencia MIT. Los plugins de terceros son software independiente y mantienen sus requisitos de activación.

## Estado

Primera versión en desarrollo. Las funciones verificadas y las limitaciones finales se registrarán en `PRUEBAS.md` y en el README, sin deducir compatibilidad universal a partir de una compilación correcta.

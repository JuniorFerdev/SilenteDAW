#!/bin/bash
# Binario universal arm64+x86_64: abre nativo por defecto y permite «Abrir con Rosetta»
# (Finder → Obtener información). Cada arquitectura usa su propio catálogo de plugins.
set -euo pipefail
cd "$(dirname "$0")/.."
.venv/bin/cmake -S . -B build-universal -G Ninja -DCMAKE_MAKE_PROGRAM="$PWD/.venv/bin/ninja" -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
.venv/bin/cmake --build build-universal --parallel 4
ditto build-universal/Silente_artefacts/Release/Silente.app releases/macOS/Silente-Universal.app
lipo -info releases/macOS/Silente-Universal.app/Contents/MacOS/Silente

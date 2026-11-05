#!/bin/bash
# Binario universal arm64+x86_64: abre nativo por defecto y permite «Abrir con Rosetta»
# (Finder → Obtener información). Cada arquitectura usa su propio catálogo de plugins.
set -euo pipefail
cd "$(dirname "$0")/.."
.venv/bin/cmake -S . -B build-universal -G Ninja -DCMAKE_MAKE_PROGRAM="$PWD/.venv/bin/ninja" -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
.venv/bin/cmake --build build-universal --parallel 4
./scripts/sign_app.sh build-universal/Silente_artefacts/Release/Silente.app
rm -rf releases/macOS/Silente-Universal.app
ditto build-universal/Silente_artefacts/Release/Silente.app releases/macOS/Silente-Universal.app
codesign --verify --deep --strict releases/macOS/Silente-Universal.app
python3 scripts/clean_generated_apps.py --apply
lipo -info releases/macOS/Silente-Universal.app/Contents/MacOS/Silente

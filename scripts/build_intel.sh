#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
.venv/bin/cmake -S . -B build-intel -G Ninja -DCMAKE_MAKE_PROGRAM="$PWD/.venv/bin/ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64
.venv/bin/cmake --build build-intel --parallel 2
rm -rf releases/macOS/Silente-Intel.app
ditto build-intel/Silente_artefacts/Release/Silente.app releases/macOS/Silente-Intel.app
./scripts/sign_app.sh releases/macOS/Silente-Intel.app

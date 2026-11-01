#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
CMAKE="$PWD/.venv/bin/cmake"
NINJA="$PWD/.venv/bin/ninja"
if [ ! -x "$CMAKE" ]; then
  python3 -m venv .venv
  .venv/bin/python -m pip install cmake==4.4.4 ninja==1.13.2
fi
if [ ! -f third_party/JUCE/CMakeLists.txt ]; then
  git clone --depth 1 --branch 8.0.12 https://github.com/juce-framework/JUCE.git third_party/JUCE
fi
# LAME 3.100 (codificador MP3, LGPL): se extrae del archivo descargado y verificado si falta.
if [ ! -f third_party/lame-3.100/include/lame.h ]; then
  echo "ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e  third_party/lame-download/lame-3.100.tar.gz" | shasum -a 256 -c -
  tar -xzf third_party/lame-download/lame-3.100.tar.gz -C third_party
fi
# Parches propios sobre JUCE 8.0.12 (idempotente: se omite si ya está aplicado).
for patch in "$PWD"/scripts/patches/*.patch; do
  [ -f "$patch" ] || continue
  if ! git -C third_party/JUCE apply --reverse --check "$patch" 2>/dev/null; then
    git -C third_party/JUCE apply "$patch"
  fi
done
"$CMAKE" -S . -B build -G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA" -DCMAKE_BUILD_TYPE=Release
if [ "${1:-}" = configure ]; then exit 0; fi
"$CMAKE" --build build --parallel 4
if [ "${1:-}" = test ]; then
  "$CMAKE" -E make_directory build/test-results
  build/Silente_artefacts/Release/Silente.app/Contents/MacOS/Silente --self-test "$PWD/build/test-results"
fi

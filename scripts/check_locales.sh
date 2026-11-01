#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/check_locales.py
./scripts/build.sh
SILENTE_TEST_APP="$PWD/build/Silente_artefacts/Release/Silente.app/Contents/MacOS/Silente"
SILENTE_TEST_OUTPUT="$PWD/build/i18n-check"
mkdir -p "$SILENTE_TEST_OUTPUT"
"$SILENTE_TEST_APP" --check-locales "$SILENTE_TEST_OUTPUT"
"$SILENTE_TEST_APP" --screenshot "$SILENTE_TEST_OUTPUT/switch.png" --check-language-switch
cat "$SILENTE_TEST_OUTPUT/localisation.txt" "$SILENTE_TEST_OUTPUT/language-switch.txt"

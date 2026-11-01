#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
./scripts/build.sh
SILENTE_TEST_APP="$PWD/build/Silente_artefacts/Release/Silente.app/Contents/MacOS/Silente"
SILENTE_TEST_OUTPUT="$PWD/build/workflow-check"
mkdir -p "$SILENTE_TEST_OUTPUT"
SILENTE_TEST_ARGS=(--check-workflow "$SILENTE_TEST_OUTPUT")
if [ "${1:-}" != "" ]; then
    "$SILENTE_TEST_APP" --scan-plugin VST3 "$1" "$SILENTE_TEST_OUTPUT/vst3-fixture.xml"
    SILENTE_TEST_ARGS+=(--plugin-fixture "$SILENTE_TEST_OUTPUT/vst3-fixture.xml")
fi
SILENTE_TEST_STATUS=0
"$SILENTE_TEST_APP" "${SILENTE_TEST_ARGS[@]}" || SILENTE_TEST_STATUS=$?
cat "$SILENTE_TEST_OUTPUT/workflow.txt"
exit "$SILENTE_TEST_STATUS"

#!/bin/bash
# Compatibility entry point: the single Universal app includes its Intel/Rosetta slice.
set -euo pipefail
cd "$(dirname "$0")/.."
exec ./scripts/build_universal.sh

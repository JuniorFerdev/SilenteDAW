#!/bin/bash
# Firma local (ad hoc) del paquete .app con un requisito designado estable.
# Sin firma de paquete macOS no puede identificar la app y no guarda el permiso del micrófono:
# pregunta en cada apertura y cada vez que otra app (WhatsApp, etc.) toma la entrada de audio.
# Con el requisito «identifier audio.silente.studio» el permiso queda guardado, también al
# recompilar. No hay certificado de Apple en este Mac: es una firma local, no notarizada.
set -euo pipefail
app="$1"
codesign --force --deep --sign - --identifier audio.silente.studio \
    -r='designated => identifier "audio.silente.studio"' "$app"
codesign --verify --deep --strict "$app"
codesign -d -r- "$app" 2>&1 | grep -q 'designated => identifier "audio.silente.studio"'
echo "firmado: $app"

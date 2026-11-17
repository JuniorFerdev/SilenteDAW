# Abrir proyectos Cubase (.cpr) — investigación inicial (16-11-2026)

Objetivo (pedido de Fernando): que Silente abra un .cpr real y lea audios, edición y canales. Se hará después de las etapas 4–10.

## Hallazgos verificados en un archivo real
`/Users/fernandom/Music/Cubase Projects/piloto-01/Sin título.cpr` (Cubase 12.0.20, MAC64 LE, 1,4 MB):
- Cabecera `RIFF` + tamaño + `NUND` + chunk `ROOT`; dentro hay cadenas «Version», «PAppVersion», «Arrangement1», «PArrangementARCH».
- Es un archivo de objetos serializados (clases MFC-like de Steinberg): `PArrangement`, `MTrackList`, `MAudioTrackEvent`, `MTempoTrackEvent`, `MSignatureTrackEvent`, `MFolderTrack`, `MGroupEvent`, `MPartEvent`, `CmObject`, `CmIDLink`…
- Los nombres de clase aparecen como cadenas con prefijo de longitud (big-endian); `ARCH` marca un bloque de archivo con tamaño.
- Los ejemplos locales de prueba son SOLO LECTURA (no modificar): ver rutas en la sección «Muestras».

## Referencias externas (formato, no código para copiar)
- cpr2rpp (Python): convierte .cpr moderno a REAPER; trae `docs/FORMAT.md` con el formato derivado empíricamente de Cubase 12. https://github.com/daniel-lynch/cpr2rpp (revisar la licencia antes de reutilizar nada; lo previsto es implementar un lector propio y usarlo solo como documentación).
- cbconv (Rust): parser RIFF con la etiqueta extra `NUND`. https://code.volse.net/audio/cbconv.git/tree/src/main.rs
- omeriko9/Cubase-Project-File-Reverse-Engineering (SX2/SX3, antiguo): https://github.com/omeriko9/Cubase-Project-File-Reverse-Engineering
- fgimian/cubase-project-plugins (Go): solo versión y lista de plugins. https://pkg.go.dev/github.com/fgimian/cubase-project-plugins/parser
- Cubase 13.0.30 añadió un segundo formato de proyecto de 64 bits; el lector debe detectar 32/64 bits.
- Alternativa oficial sin ingeniería inversa: Cubase exporta «Track Archive» XML (lo lee DAWFileKit, Swift: https://www.github.com/orchetect/DAWFileKit).

## Muestras en el equipo (solo lectura)
- `/Applications/ARCHIVO FER/PRODUCCION MUSICAL/almita/que sera del rio/yo.cpr`
- `/Applications/ARCHIVO FER/PRODUCCION MUSICAL/seguricopia tom pere mix fer/sesion con el tom-01.cpr`
- `/Applications/ARCHIVO FER/PRODUCCION MUSICAL/sulph/sesion sulphuric.cpr`
- `/Users/fernandom/Documents/Documentos - MacBook Air de Fernando/Steinberg/` (oiloto, poiloto, sesion con el tom, …)
- `/Users/fernandom/Music/Cubase Projects/piloto-01/Sin título.cpr` (proyecto vacío: línea base para diferenciar)

## Plan propuesto (por tramos, con evidencia en cada uno)
1. Lector de contenedor: recorrer RIFF/NUND/ROOT/ARCH y volcar el árbol de objetos (herramienta `scripts/cpr_dump.py`, solo lectura). Entrega: volcado del proyecto vacío y de uno real.
2. Metadatos: versión, tempo, compás, pistas (nombre, tipo), pool de audio con rutas de archivos. Entrega: lista contrastada con los archivos reales de Audio/ (nombres y longitudes).
3. Eventos: partes y eventos de audio (posición, longitud, offset, fundidos, volumen) y notas MIDI. Entrega: posiciones y longitudes coherentes con el audio real, diferencias entre versiones de Auto Saves y duración de Mixdown/.
4. Canales: volumen, pan, mute/solo, envíos, inserciones (nombres de plugin). Entrega: valores estables entre versiones y consistentes con el Mixdown; lo no confirmable queda «no verificado».
5. Importación en Silente (módulo `src/io/CprImport`), con archivos de audio resueltos por ruta o búsqueda; advertencias para lo no soportado. Los .cpr originales nunca se modifican; Silente no guarda en .cpr.
Riesgo: formato no documentado y variable entre versiones; cada campo se acepta solo con la evidencia independiente descrita abajo (sin licencia de Cubase).

## Sin licencia de Cubase (aviso de Fernando, 16-11-2026): validar sin abrir Cubase
Cada campo leído se acepta por evidencia independiente, no por comparación con la interfaz de Cubase:
- **Audio del pool:** los `.cpr` con carpeta `Audio/` (sesion con el tom: ~100 archivos; sulphuric: ~120; yo.cpr: 1) permiten comprobar que cada ruta/nombre y cada longitud (muestras) leídos del .cpr coinciden con el archivo real.
- **Mezcla exportada:** las carpetas `Mixdown/` traen el resultado renderizado; el proyecto importado y mezclado por Silente debe parecerse en duración y estructura (no bit a bit: plugins distintos).
- **Pares de proyectos:** `sesion con el tom.cpr` vs `...pistas en limpio sin mix...` vs `-01`; `Auto Saves/` con versiones sucesivas: las diferencias entre versiones aíslan qué bytes codifican cada cambio (diferencial, como hicieron cpr2rpp y omeriko9).
- **Proyecto vacío** `piloto-01/Sin título.cpr` como línea base.
- Aceptación mínima por tramo: coincidencia con archivos y longitudes reales; lo que no se pueda confirmar se marca «no verificado» y no se importa en silencio.

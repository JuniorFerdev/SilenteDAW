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

## Tramo 1: contenedor (herramienta `tools/cpr/cpr_dump.py`, solo lectura, 17-11-2026)
- Verificado con bytes reales: cabecera `RIFF`+u32 BE+`NUND` (Cubase 12, piloto vacio) o `RIF2`+u64 BE+`NUND` (yo.cpr, sulphuric). Bloques = etiqueta(4)+tamano BE (u32 en RIFF, u64 en RIF2)+payload; alternan `ROOT` (nombre de seccion: Version/PAppVersion, Arrangement1/PArrangement, ComputerGuid, Devices, WindowLayouts, ProjectLayouts, Metadata) y `ARCH` (datos).
- El tamano declarado = archivo - longitud de cabecera (12 en RIFF, 16 en RIF2), no archivo-8; en los tres archivos los 14 bloques consecutivos llegan exactamente al final.
- Dentro de ARCH: cadenas con longitud u32 BE que incluye un NUL final; marcadores FFFFFFFE/FFFFFFFF antes de nombres de clase. Cada clase se nombra completa solo la primera vez; despues no se repite el nombre (referencias por indice, NO verificado).
- Ejemplo: `MAudioEvent` @0x9d5d y `PAudioClip` @0x9db8 en sulphuric; el nombre "sulph-07" (con BOM UTF-8) @0x9dcd coincide con `Audio/sulph-07.wav` real.
- Pendiente: semantica de campos (tramo 2).

## Tramo 2: flujo de objetos, clips, eventos, tempo y compás (`tools/cpr/cpr_read.py`, solo lectura, 17-11-2026)
Herramienta: `python3 tools/cpr/cpr_read.py PROYECTO.cpr [--classes] [--tree] [--validate DIR_AUDIO] [--mixdown WAV] [--diff OTRO.cpr] [--out TXT]`. Abre todo en `rb`, sin dependencias; `--out` rechaza rutas .cpr/.bak/.wav. Corrige el tramo 1: las referencias NO son por índice sino por desplazamiento en el flujo.

### Gramática del payload ARCH (verificada; W = 8 bytes en RIF2/Cubase 15, 4 bytes en RIFF/Cubase 12)
- Objeto = etiqueta(W) + [nombre + u16 versión si la clase es nueva] + tamaño(W) + payload. Los hijos van anidados dentro del payload; el tamaño cierra exacto: `PArrangement` (sulph @0xfc): `FF..FE 0000000A "GDocument\0" 0000 · FF..FE 00000009 "CmObject\0" 0000 · FF..FF 0000000D "PArrangement\0" 0004 · 0000000000160D7F` → el payload termina justo en el fin del bloque ARCH (0x160ed9).
- Etiquetas: todo unos = clase nueva + instancia; todo unos−1 = clase nueva declarada (cadena de bases, de la más cercana a la más lejana; hipótesis coherente, p. ej. `PMediaDescriptor` antes de `PAudioClip`); bit alto = clase ya vista, resto = desplazamiento (relativo al payload ARCH) del marcador donde se definió. Ej.: 2.º `MHitPointEvent` = `8000000000009CE5 000000000000001A`, y 0x9ce5 = 0x9ded − 0x108.
- Referencia a objeto ya escrito: valor W sin bit alto = desplazamiento relativo de su campo tamaño (id). Ej.: el 2.º `MAudioEvent` (@0x1e205) lleva `0000000000009CBD` = id del `PAudioClip` @0x9dac (tamaño en 0x9dc5 = 0x9cbd + 0x108). Confirmado en 31 referencias a PAudioClip, 120 AudioFile, 119 a un único ElastiquePreset y 240 FNPath.
- Cadena: u32 longitud (incluye NUL) + bytes + NUL, y si sigue `EF BB BF` (contado en la longitud) el texto es UTF-8.
- FAttributes / listas de atributos: u32 n + n × (cadena clave, u16 tipo, valor). Tipo 1 = i64, 4 = f64, 8 = cadena, 2 = grupo anidado, 0x22 = objeto. Ej. sulph @0x5c5: `"Numerator\0" 0001 0000000000000001`.
- Cubase 12 (piloto vacío) se recorre con W = 4 (`FFFFFFFE 0000000A "GDocument\0"`, tamaño `0014FB02`); disposición de eventos/clips en W = 4 no verificada (no hay audio en esa muestra).

### Instancias por clase en sulph (Arrangement1: 25 257 objetos, 145 clases)
MHitPointEvent 23 763 · FNPath 129 · FAttributes 128 · PDrumMapEntry 128 · GTreeEntry 124 · PAudioClip/AudioFile/AudioCluster/AClusterSegment/PGridDefinition 120 · MAutomationTrackEvent/MAutoListNode 57 · NodePath 56 · MAudioEvent 31 · VolumeCurveDataNode 31 · MAutomationTrack 17 · MAudioTrackEvent/MAudioTrack 7 · MTempoTrackEvent/MSignatureTrackEvent/MFolderTrack/MAudioPartEvent(vacía)/PPool/MRoot/PArrangement 1. Solo como base (0 instancias): GDocument, CmObject, MEvent, MPartEvent, MTrackEvent, PMediaDescriptor, AudioStream, FPath, CmContainer… `MTimeSignatureEvent` no es objeto: va dentro de FAttributes. Tabla completa: `--classes`.

### Campos y evidencia
| Campo | Dónde / formato | Evidencia | Estado |
|---|---|---|---|
| Versión de Cubase | PAppVersion: cadenas | "Version 15.0.30" / "12.0.20" | verificado |
| Archivo de audio | PAudioClip → 1.er hijo FNPath: cadena nombre, FourCC (no fiable: `00000000` en 30 de 99 clips de tom-01 que son WAV reales), 3 cadenas, u32+u16 (?), cadena carpeta | sulph 120/120 y tom-01 99/99 existen | verificado (carpeta = ubicación original; ver riesgos) |
| Frames, bits, bytes/frame, canales, Hz | AudioFile v1: u64, u16, u16, u16, f32, id FNPath, 8 bytes (?) | 120/120 sulph, 136/136 tom, 99/99 tom-01 iguales a la cabecera WAV (p. ej. sulph-07: 25 461 504, 32, 8, 2, 96000) | verificado |
| Nombre del clip | PAudioClip: 1.ª cadena | coincide con el archivo en 120/120 sulph; puede diferir si se renombró (tom: 1, «limpio»: 45) | verificado como nombre; no es el nombre de archivo |
| Evento → clip | MAudioEvent v6: ranura tras 3 f64 (objeto en línea o id) | 31/31 sulph, 190/190 tom resueltos | verificado |
| Longitud del evento | MAudioEvent f64 #2, muestras del archivo | offset+long ≤ frames en 31/31, 190/190, 187/187 (exceso máx. 0,049 muestras); 11 de 31 terminan exacto en el último frame | verificado |
| Offset en el archivo | MAudioEvent f64 #3, muestras | evento partido en sulph: fin del 1.º en el archivo = offset del 2.º = 992 631,5765; recorte izq. (tom 17→18): Δoffset = −Δlong exacto | verificado |
| Inicio del evento | MAudioEvent f64 #1, ticks a 480 por negra | sulph: Δinicio/Δs = 1679,9993 ticks/s = 480·210/60; tom recorte: 536,0000 ticks/s = 480·67/60 en 3 eventos; diff 07→08 «Desplazar eventos»: solo cambia este campo | verificado relativo; absoluto (tick 0 = inicio del proyecto) coherente con localizadores, no probado con audio |
| Tempo | MTempoTrackEvent v3 (v2 en C12): u32 n + f32 segundos por negra | sulph 2/7 s = 210 BPM: ciclo 212,857152 s = mezcla (10 217 143/48 000) = 745,000 negras; tom 0,8955 s = 67 BPM: aritmética de recorte y mezcla 297,313 s = 332,000 negras; yo.cpr/piloto 0,5 = 120 (por defecto) | verificado para n = 1; mapas con varios puntos no vistos |
| Compás | MSignatureTrackEvent → FAttributes Numerator/Denominator (i64); también u16 en payload+8 | piloto vacío 4/4 (por defecto), yo 4/4, tom 2/4, sulph 1/4; ambos sitios coinciden | lectura verificada; 1/4 de sulph es el valor guardado (sin otra fuente) |
| Localizadores | Devices: "Cycle Left/Right" → grupo → "Time" f64 (s) | ciclo = duración exacta de la mezcla en sulph y tom-01 | verificado |
| Nombre de pista | MAudioTrackEvent → MListNode: 1.ª cadena | sulph: guitar, guit 2, guit 2 (D)×3 = nombres de los stems de Mixdown | verificado (renombrado: sin par para diferencial) |
| Volumen de canal | MAudioTrack → atributos "Volume" (tipo 2) → "Value" f64 y "AnchorValue" f64 | diff sulph 05→06 «Ajustar volumen Pista Audio 02»: solo cambian 21 399,656→22 513,793 y −3,2337→−2,3056 | ubicación verificada; unidad NO verificada |
| Fundidos, color (`braF`), silencio, VolumeCurveDataNode, nombre propio del evento, partes (inicio 98 400 / long. 42 240), automatización | — | sin prueba independiente | no verificado |

### Diferencial (Auto Saves)
- Sin título1 07→08 «Desplazar eventos»: «Bombo atras» solo cambia inicio (26 801,137→17 688,894); cambia también el orden de pistas.
- tom 17→18 «Redimensionar eventos Pista v»: 3 eventos de «v 2 D» con Δinicio = Δoffset/44 100·536 y Δlong = −Δoffset (recorte por la izquierda).
- sulph 03→04 y 06→07 «Suprimir»: el clip suprimido queda en el pool sin eventos.
- sulph 05→06 «Ajustar volumen»: Volume/Value y AnchorValue (arriba); en Devices solo cambian foco y Solo.
- Los Auto Saves acumulan varias acciones entre instantáneas: el nombre del archivo indica solo la última.

### Riesgos y límites
- Las carpetas absolutas guardadas apuntan a la ubicación original (tom: `/Users/fernandom/Documents/Steinberg/Audio/`); hay que buscar en `Audio/` del proyecto y comprobar frames/formato: en «limpio» 3 clips apuntan a una subcarpeta borrada y el WAV homónimo de `Audio/` es otro (24 bits, más frames). Importar por nombre sin verificar sería silenciosamente incorrecto.
- El recorrido busca etiquetas byte a byte dentro de payloads no decodificados; se valida con anidamiento exacto de tamaños y desplazamientos de clase exactos, pero no excluye por construcción falsos positivos.
- Correlación de envolventes stem↔fuente (posición absoluta): no concluyente (efectos de inserción; desfases dispersos). No se usa como prueba.
- Sin muestras con MIDI, mapas de tempo con varios puntos, ni audio en formato Cubase 12.

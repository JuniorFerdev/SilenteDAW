#!/usr/bin/env python3
"""Lector de solo lectura de proyectos Cubase .cpr (tramo 2 de investigacion).

Fuera de la app: no se enlaza a CMake ni a src/. Abre los .cpr y los WAV solo con 'rb'
y nunca escribe en ellos. Sin dependencias externas. Formato documentado en
docs/CPR_INVESTIGACION.md, seccion "Tramo 2"; cada campo indica si esta verificado.

Uso:
  cpr_read.py PROYECTO.cpr                     resumen: version, tempo, compas, pistas, eventos
  cpr_read.py PROYECTO.cpr --classes           tabla de clases: primera aparicion, version, instancias
  cpr_read.py PROYECTO.cpr --tree              arbol de objetos (sin MHitPointEvent)
  cpr_read.py PROYECTO.cpr --validate DIR      contrasta clips y eventos con los WAV reales de DIR
  cpr_read.py PROYECTO.cpr --mixdown ARCHIVO.wav   duracion de la mezcla en negras segun el tempo leido
  cpr_read.py A.cpr --diff B.cpr               diferencias de eventos/pistas y bytes por objeto
  --out RUTA escribe la salida en un archivo de texto (nunca junto a los .cpr).
"""
import argparse
import collections
import os
import re
import struct
import sys

# Etiquetas del flujo de objetos; W = 8 bytes en RIF2 (Cubase 15), 4 bytes en RIFF (Cubase 12). Verificado.
#   todo unos        -> clase nueva + instancia
#   todo unos - 1    -> clase nueva declarada (base); sigue otra etiqueta
#   bit alto         -> clase ya vista; resto = desplazamiento de su marcador en el payload ARCH
NAME_RE = re.compile(rb"^[A-Za-z_][A-Za-z0-9_:]{1,80}$")
UTF8_FLAG = b"\xef\xbb\xbf"


# ---------------------------------------------------------------- contenedor
def read_blocks(data):
    """Bloques ROOT/ARCH. RIFF: tamanos u32 BE, cabecera 12; RIF2: u64 BE, cabecera 16 (verificado)."""
    if data[:4] == b"RIFF" and data[8:12] == b"NUND":
        wide, hl = False, 12
    elif data[:4] == b"RIF2" and data[12:16] == b"NUND":
        wide, hl = True, 16
    else:
        raise SystemExit("No es un .cpr (RIFF/RIF2 + NUND esperado)")
    out, o, ext = [], hl, 8 if wide else 4
    while o + 4 + ext <= len(data):
        tag = data[o:o + 4].decode("latin1")
        size = struct.unpack_from(">Q" if wide else ">I", data, o + 4)[0]
        out.append((o, tag, o + 4 + ext, size))
        o += 4 + ext + size
    sections, name = [], None
    for off, tag, p, size in out:
        if tag == "ROOT":
            n = struct.unpack_from(">I", data, p)[0]
            name = data[p + 4:p + 4 + n].rstrip(b"\0").decode("latin1")
        elif tag == "ARCH":
            sections.append((name, off, p, size))
    return wide, sections


def rstr(data, o):
    """Cadena: u32 BE longitud (incluye NUL y, si hay, EF BB BF tras el NUL = UTF-8). Devuelve (texto, fin)."""
    n = struct.unpack_from(">I", data, o)[0]
    raw = data[o + 4:o + 4 + n]
    utf8 = raw.endswith(b"\0" + UTF8_FLAG)
    body = raw.split(b"\0", 1)[0]
    return body.decode("utf-8" if utf8 else "latin1", "replace"), o + 4 + n


def is_str(data, o, end, maxlen=4096):
    if o + 4 > end:
        return False
    n = struct.unpack_from(">I", data, o)[0]
    if n < 1 or n > maxlen or o + 4 + n > end:
        return False
    raw = data[o + 4:o + 4 + n]
    return raw.endswith(b"\0") or raw.endswith(b"\0" + UTF8_FLAG)


# ---------------------------------------------------------------- flujo de objetos
class Obj:
    __slots__ = ("cls", "ver", "at", "pa", "pb", "parent", "kids", "decl", "oid", "is_new")

    def __repr__(self):
        return f"{self.cls} v{self.ver} @0x{self.at:x} [{self.pb - self.pa}]"


class Arch:
    """Payload ARCH. Objeto = etiqueta u64 BE + (nombre+u16 version si clase nueva) + u64 tamano + payload.
    Id de objeto = desplazamiento (relativo al payload ARCH) de su campo tamano; las referencias
    posteriores a ese objeto escriben ese id como u64 sin bit alto (verificado en PAudioClip/AudioFile/FNPath)."""

    def __init__(self, data, base, size, wide=True):
        self.d, self.base, self.end = data, base, base + size
        self.W = 8 if wide else 4
        self.fmt = ">Q" if wide else ">I"
        self.ALL = (1 << (8 * self.W)) - 1
        self.REF = 1 << (8 * self.W - 1)
        self.classes = {}        # desplazamiento relativo del marcador -> (nombre, version)
        self.class_first = {}    # nombre -> desplazamiento absoluto de la definicion
        self.objs, self.by_id = [], {}
        self._scan(base, self.end, None)

    def _try(self, o, lim):
        d, chain, W = self.d, [], self.W
        while True:
            if o + W > lim:
                return None
            t = struct.unpack_from(self.fmt, d, o)[0]
            if t in (self.ALL, self.ALL - 1):
                if o + W + 4 > lim:
                    return None
                n = struct.unpack_from(">I", d, o + W)[0]
                if n < 2 or n > 90 or o + W + 4 + n + 2 > lim:
                    return None
                nm = d[o + W + 4:o + W + 4 + n]
                if nm[-1] != 0 or not NAME_RE.match(nm[:-1]):
                    return None
                ver = struct.unpack_from(">H", d, o + W + 4 + n)[0]
                chain.append(((nm[:-1].decode(), ver), o))
                o += W + 4 + n + 2
                if t == self.ALL - 1:
                    continue
                cls, new = chain[-1][0], True
                break
            if t & self.REF and (t & ~self.REF) in self.classes and not chain:
                cls, new = self.classes[t & ~self.REF], False
                o += W
                break
            return None
        if o + W > lim:
            return None
        sz = struct.unpack_from(self.fmt, d, o)[0]
        if o + W + sz > lim:
            return None
        for c, co in chain:
            self.classes[co - self.base] = c
            self.class_first.setdefault(c[0], co)
        return cls, new, [c[0][0] for c in chain[:-1]], o, o + W, o + W + sz

    def _scan(self, a, b, parent):
        d, o = self.d, a
        while o + 2 * self.W <= b:
            if d[o] in (0xFF, 0x80):
                r = self._try(o, b)
                if r:
                    (name, ver), new, decl, szo, pa, pb = r
                    ob = Obj()
                    ob.cls, ob.ver, ob.at, ob.pa, ob.pb, ob.parent = name, ver, o, pa, pb, parent
                    ob.kids, ob.decl, ob.oid, ob.is_new = [], decl, szo - self.base, new
                    self.objs.append(ob)
                    self.by_id[ob.oid] = ob
                    if parent is not None:
                        parent.kids.append(ob)
                    self._scan(pa, pb, ob)
                    o = pb
                    continue
            o += 1

    def slot(self, o):
        """Lee una ranura de objeto en o: objeto en linea, referencia a objeto previo, o None."""
        t = struct.unpack_from(self.fmt, self.d, o)[0]
        for ob in self.objs:  # pocos accesos; busqueda lineal aceptable
            if ob.at == o:
                return ob, "en linea"
        if t < self.REF and t in self.by_id:
            return self.by_id[t], "referencia"
        return None, f"desconocido 0x{t:x}"

    def own_ranges(self, ob):
        """Rangos del payload que no pertenecen a hijos."""
        out, p = [], ob.pa
        for k in ob.kids:
            if k.at > p:
                out.append((p, k.at))
            p = k.pb
        if ob.pb > p:
            out.append((p, ob.pb))
        return out

    def strings(self, ob, own=True):
        res = []
        for a, b in (self.own_ranges(ob) if own else [(ob.pa, ob.pb)]):
            o = a
            while o + 4 <= b:
                if is_str(self.d, o, b, 1024):
                    s, e = rstr(self.d, o)
                    if s and all(c.isprintable() for c in s):
                        res.append((o, s))
                        o = e
                        continue
                o += 1
        return res


def kids(ob, cls):
    return [k for k in ob.kids if k.cls == cls]


def desc(ob, cls):
    st, out = list(ob.kids), []
    while st:
        k = st.pop(0)
        if k.cls == cls:
            out.append(k)
        st[0:0] = k.kids
    return out


# ---------------------------------------------------------------- atributos (FAttributes)
ATTR_TYPES = {1: "i64", 4: "f64", 8: "str"}  # verificados en Numerator/Denominator/Start/Length; 2 = lista anidada


def read_attrs(d, o, end, depth=0):
    """u32 n + n x (cadena clave, u16 tipo, valor). Tipos 1/4/8 decodificados; otros se detienen."""
    out = []
    if o + 4 > end:
        return out, o
    n = struct.unpack_from(">I", d, o)[0]
    o += 4
    if n > 4096:
        return out, o
    for _ in range(n):
        if not is_str(d, o, end):
            break
        k, o = rstr(d, o)
        t = struct.unpack_from(">H", d, o)[0]
        o += 2
        if t == 1:
            out.append((k, struct.unpack_from(">q", d, o)[0])); o += 8
        elif t == 4:
            out.append((k, struct.unpack_from(">d", d, o)[0])); o += 8
        elif t == 8:
            v, o = rstr(d, o); out.append((k, v))
        else:
            out.append((k, f"<tipo {t} no decodificado>"))
            break
    return out, o


def find_attr(d, a, b, key):
    """Busca 'clave' + tipo 1/4 dentro de [a,b) (lectura puntual)."""
    kb = key.encode() + b"\0"
    i = d.find(kb, a, b)
    res = []
    while i >= 0:
        if struct.unpack_from(">I", d, i - 4)[0] == len(kb):
            t = struct.unpack_from(">H", d, i + len(kb))[0]
            if t == 1:
                res.append(struct.unpack_from(">q", d, i + len(kb) + 2)[0])
            elif t == 4:
                res.append(struct.unpack_from(">d", d, i + len(kb) + 2)[0])
        i = d.find(kb, i + 1, b)
    return res


# ---------------------------------------------------------------- extractores
def read_version(d, A):
    ob = next((o for o in A.objs if o.cls == "PAppVersion"), None)
    return [s for _, s in A.strings(ob)] if ob else []


def read_tempo(d, A):
    """MTempoTrackEvent v3: u32 n puntos + f32 segundos por negra (verificado n=1). BPM = 60/f."""
    ob = next((o for o in A.objs if o.cls == "MTempoTrackEvent"), None)
    if not ob:
        return None
    n = struct.unpack_from(">I", d, ob.pa)[0]
    spb = struct.unpack_from(">f", d, ob.pa + 4)[0]
    if ob.ver not in (2, 3) or not (1 <= n <= 100000) or not (0.01 < spb < 10):
        return {"estado": f"formato no reconocido (v{ob.ver})", "hex": d[ob.pa:ob.pa + 32].hex(" ")}
    return {"puntos": n, "seg_por_negra": spb, "bpm": 60.0 / spb,
            "estado": "verificado n=1" if n == 1 else "n>1: solo se lee el primer punto (no verificado)"}


def read_signature(d, A):
    ob = next((o for o in A.objs if o.cls == "MSignatureTrackEvent"), None)
    if not ob:
        return None
    num = find_attr(d, ob.pa, ob.pb, "Numerator")
    den = find_attr(d, ob.pa, ob.pb, "Denominator")
    n_ev = struct.unpack_from(">I", d, ob.pa)[0]
    pair = struct.unpack_from(">HH", d, ob.pa + 8)
    return {"numerador": num, "denominador": den, "eventos": n_ev, "par_u16_cabecera": pair}


def read_locators(d, D):
    """Devices/Transport: 'Cycle Left'/'Cycle Right' -> grupo con 'Time' f64 en segundos
    (verificado: Right-Left = duracion de la mezcla exportada en sulph y en tom-01)."""
    out = {}
    if D is None:
        return out
    for k in ("Cycle Left", "Cycle Right", "Punch Left", "Punch Right"):
        i = d.find(k.encode() + b"\0", D.base, D.end)
        j = d.find(b"\0\0\0\x05Time\0", i, i + 64) if i >= 0 else -1
        if j >= 0 and struct.unpack_from(">H", d, j + 9)[0] == 4:
            out[k] = struct.unpack_from(">d", d, j + 11)[0]
    return out


def read_fnpath(d, A, ob):
    """FNPath: cadena archivo, FourCC (4 bytes, p. ej. WAVE), 3 cadenas (ext, ext, descripcion),
    u32 + u16 (no verificados), cadena carpeta absoluta."""
    o, s = ob.pa, []
    if not is_str(d, o, ob.pb):
        return {"error": "FNPath no reconocido", "at": ob.at}
    v, o = rstr(d, o)
    s.append(v)
    fourcc = d[o:o + 4].decode("latin1")
    o += 4
    for _ in range(3):
        if not is_str(d, o, ob.pb):
            return {"error": "FNPath no reconocido", "at": ob.at}
        v, o = rstr(d, o)
        s.append(v)
    o += 6
    folder, o = rstr(d, o) if is_str(d, o, ob.pb) else ("", o)
    return {"archivo": s[0], "formato": fourcc, "ext": s[1], "descripcion": s[3], "carpeta": folder}


def read_audiofile(d, A, ob):
    """AudioFile v1 (34 bytes): u64 frames, u16 bits, u16 bytes/frame, u16 canales, f32 Hz, u64 id FNPath, 8 bytes."""
    p = ob.pa
    frames, bits, bpf, ch = struct.unpack_from(">QHHH", d, p)
    sr = struct.unpack_from(">f", d, p + 14)[0]
    ref = struct.unpack_from(A.fmt, d, p + 18)[0]  # en RIFF (Cubase 12) disposicion no verificada
    path = A.by_id.get(ref)
    return {"frames": frames, "bits": bits, "bytes_por_frame": bpf, "canales": ch, "hz": sr,
            "ruta": read_fnpath(d, A, path) if path is not None and path.cls == "FNPath" else None}


def read_clip(d, A, ob):
    name = rstr(d, ob.pa)[0] if is_str(d, ob.pa, ob.pb) else None
    fn = kids(ob, "FNPath")
    af = desc(ob, "AudioFile")
    return {"id": ob.oid, "nombre": name,
            "ruta": read_fnpath(d, A, fn[0]) if fn else None,
            "audio": read_audiofile(d, A, af[0]) if af else None}


def track_name(d, A, tr):
    ln = kids(tr, "MListNode")
    if ln and is_str(d, ln[0].pa, ln[0].pb):
        return rstr(d, ln[0].pa)[0]
    return None


def read_events(d, A):
    """MAudioEvent v6: u16, f64 inicio (ticks, 480 por negra), f64 longitud (muestras), f64 offset (muestras), ranura PAudioClip."""
    out = []
    for ev in A.objs:
        if ev.cls != "MAudioEvent":
            continue
        u16 = struct.unpack_from(">H", d, ev.pa)[0]
        start, length, offset = struct.unpack_from(">ddd", d, ev.pa + 2)
        clip, how = A.slot(ev.pa + 26)
        tr = ev.parent
        while tr is not None and tr.cls != "MAudioTrackEvent":
            tr = tr.parent
        out.append({"at": ev.at, "u16": u16, "inicio_ticks": start, "longitud_muestras": length,
                    "offset_muestras": offset, "clip": read_clip(d, A, clip) if clip is not None and clip.cls == "PAudioClip" else None,
                    "clip_por": how, "pista": track_name(d, A, tr) if tr else None})
    return out


def load(path):
    with open(path, "rb") as f:
        data = f.read()
    wide, sections = read_blocks(data)
    archs = {name: Arch(data, p, size, wide) for name, _, p, size in sections}
    return data, wide, archs


# ---------------------------------------------------------------- WAV (solo lectura)
def wav_info(path):
    with open(path, "rb") as f:
        h = f.read(12)
        if h[:4] not in (b"RIFF", b"RF64") or h[8:12] != b"WAVE":
            return None
        fmt = ds64 = None
        while True:
            c = f.read(8)
            if len(c) < 8:
                return None
            tag, sz = c[:4], struct.unpack("<I", c[4:])[0]
            if tag == b"fmt ":
                fmt = struct.unpack("<HHIIHH", f.read(sz)[:16])
                if sz % 2:
                    f.read(1)
            elif tag == b"ds64":
                ds64 = struct.unpack("<QQQ", f.read(sz)[:24])
            elif tag == b"data":
                n = ds64[1] if (sz == 0xFFFFFFFF and ds64) else sz
                return {"canales": fmt[1], "hz": fmt[2], "block": fmt[4], "bits": fmt[5], "frames": n // fmt[4]}
            else:
                f.seek(sz + (sz % 2), 1)


# ---------------------------------------------------------------- informes
def fmt_events(evs, bpm=None):
    lines = []
    for e in evs:
        c = e["clip"] or {}
        a = c.get("audio") or {}
        hz = a.get("hz") or 0
        sec = f" ({e['longitud_muestras'] / hz:.3f} s)" if hz else ""
        pos = f" = {e['inicio_ticks'] / 480 * 60 / bpm:.3f} s" if bpm else ""
        lines.append(f"  @0x{e['at']:x} pista={e['pista']!r} clip={c.get('nombre')!r} ({e['clip_por']}) "
                     f"inicio={e['inicio_ticks']:.3f} ticks{pos} long={e['longitud_muestras']:.4f} muestras{sec} "
                     f"offset={e['offset_muestras']:.4f}")
    return lines


def summary(path, d, archs):
    A = archs.get("Arrangement1")
    L = [f"archivo: {path}", f"version: {read_version(d, archs['Version']) if 'Version' in archs else '?'}"]
    tp = read_tempo(d, A)
    L.append(f"tempo (MTempoTrackEvent): {tp}")
    L.append(f"compas (MSignatureTrackEvent/FAttributes): {read_signature(d, A)}")
    loc = read_locators(d, archs.get("Devices"))
    if "Cycle Left" in loc and "Cycle Right" in loc:
        L.append(f"localizadores (s): {loc}; ciclo = {loc['Cycle Right'] - loc['Cycle Left']:.6f} s")
    tracks = [o for o in A.objs if o.cls == "MAudioTrackEvent"]
    L.append(f"pistas de audio: {len(tracks)} -> {[track_name(d, A, t) for t in tracks]}")
    clips = [o for o in A.objs if o.cls == "PAudioClip"]
    L.append(f"PAudioClip: {len(clips)}; MAudioEvent: {sum(1 for o in A.objs if o.cls == 'MAudioEvent')}")
    evs = read_events(d, A)
    L.append("eventos de audio:")
    L += fmt_events(evs, tp.get("bpm") if tp and "bpm" in tp else None)
    return L, A, evs, tp


def classes_table(A):
    cnt = collections.Counter(o.cls for o in A.objs)
    ver = {}
    for o in A.objs:
        ver.setdefault(o.cls, o.ver)
    L = ["clase | version | instancias | primera definicion (abs) | declarada como base"]
    declared = collections.Counter()
    for o in A.objs:
        for c in o.decl:
            declared[c] += 1
    names = set(cnt) | set(A.class_first)
    for n in sorted(names, key=lambda x: (-cnt.get(x, 0), x)):
        L.append(f"{n} | v{ver.get(n, '?')} | {cnt.get(n, 0)} | 0x{A.class_first.get(n, 0):x} | {'si' if declared[n] else ''}")
    L.append(f"total objetos: {len(A.objs)}; clases en tabla: {len(A.classes)}")
    return L


def tree_lines(A):
    L = []

    def rec(o, dep):
        if o.cls == "MHitPointEvent":
            return
        L.append("  " * dep + f"{o.cls} v{o.ver} @0x{o.at:x} id=0x{o.oid:x} size={o.pb - o.pa}" + (f" bases={o.decl}" if o.decl else ""))
        for k in o.kids:
            rec(k, dep + 1)
    for o in A.objs:
        if o.parent is None:
            rec(o, 0)
    hp = sum(1 for o in A.objs if o.cls == "MHitPointEvent")
    L.append(f"(omitidos {hp} MHitPointEvent)")
    return L


def validate(evs, A, d, audio_dir):
    L, ok, bad, exc = [], collections.Counter(), collections.Counter(), [0.0]
    clips = [read_clip(d, A, o) for o in A.objs if o.cls == "PAudioClip"]
    for c in clips:
        r, a = c["ruta"] or {}, c["audio"] or {}
        fn = r.get("archivo")
        cand = [os.path.join(r.get("carpeta", ""), fn or ""), os.path.join(audio_dir, fn or "")]
        real = next((p for p in cand if fn and os.path.isfile(p)), None)
        if not real:
            bad["archivo"] += 1
            L.append(f"  FALTA {fn!r} (clip {c['nombre']!r})")
            continue
        ok["archivo"] += 1
        w = wav_info(real)
        if not w or not a:
            bad["cabecera"] += 1
            continue
        for k_cpr, k_wav in (("frames", "frames"), ("canales", "canales"), ("hz", "hz"), ("bits", "bits"), ("bytes_por_frame", "block")):
            (ok if a[k_cpr] == w[k_wav] else bad)[k_cpr] += 1
            if a[k_cpr] != w[k_wav]:
                L.append(f"  DIFIERE {fn}: {k_cpr} cpr={a[k_cpr]} wav={w[k_wav]}")
        (ok if c["nombre"] and fn.startswith(c["nombre"]) else bad)["nombre_clip=archivo"] += 1
        (ok if os.path.dirname(real) == os.path.normpath(r.get("carpeta", "")) else bad)["carpeta_absoluta"] += 1
    for e in evs:
        a = (e["clip"] or {}).get("audio")
        if not a:
            bad["evento_sin_clip"] += 1
            continue
        over = e["offset_muestras"] + e["longitud_muestras"] - a["frames"]
        # tolerancia de media muestra: offset/longitud son f64 fraccionarios (excesos observados < 0,05)
        fit = e["offset_muestras"] >= 0 and over <= 0.5
        (ok if fit else bad)["evento offset+long<=frames"] += 1
        if fit:
            exc[0] = max(exc[0], over)
        if abs(over) < 0.5:
            ok["evento termina en fin de archivo (+-0,5)"] += 1
    L.append(f"exceso maximo aceptado sobre el fin del archivo: {exc[0]:.6f} muestras")
    names = sorted(f for f in os.listdir(audio_dir) if f.lower().endswith(".wav"))
    used = {(c["ruta"] or {}).get("archivo") for c in clips}
    L.append(f"WAV en carpeta: {len(names)}; sin clip en el .cpr: {[n for n in names if n not in used]}")
    L.insert(0, f"coincidencias: {dict(ok)}")
    L.insert(1, f"discrepancias: {dict(bad)}")
    return L


def diff(pa, pb):
    da, _, Aa = load(pa)
    db, _, Ab = load(pb)
    L = [f"A: {pa}", f"B: {pb}"]
    A, B = Aa["Arrangement1"], Ab["Arrangement1"]
    ta = [track_name(da, A, o) for o in A.objs if o.cls == "MAudioTrackEvent"]
    tb = [track_name(db, B, o) for o in B.objs if o.cls == "MAudioTrackEvent"]
    if ta != tb:
        L.append(f"pistas A={ta}\npistas B={tb}")
    ea, eb = read_events(da, A), read_events(db, B)
    key = lambda e: (e["pista"], (e["clip"] or {}).get("nombre"))
    ga, gb = collections.defaultdict(list), collections.defaultdict(list)
    for e in ea:
        ga[key(e)].append(e)
    for e in eb:
        gb[key(e)].append(e)
    for k in sorted(set(ga) | set(gb), key=str):
        xa, xb = ga.get(k, []), gb.get(k, [])
        for i in range(max(len(xa), len(xb))):
            a = xa[i] if i < len(xa) else None
            b = xb[i] if i < len(xb) else None
            if a is None or b is None:
                L.append(f"evento {k}: {'solo B' if a is None else 'solo A'}")
                continue
            ch = [f"{f}: {a[f]:.4f} -> {b[f]:.4f}" for f in ("inicio_ticks", "longitud_muestras", "offset_muestras") if a[f] != b[f]]
            if ch:
                L.append(f"evento {k}: " + "; ".join(ch))
    # bytes propios por objeto en todas las secciones, emparejando por (clase, ordinal).
    # Se enmascaran ids de objeto y referencias de clase que solo cambian porque el flujo se desplazo.
    for sec in sorted(set(Aa) | set(Ab)):
        if sec not in Aa or sec not in Ab:
            L.append(f"seccion {sec}: solo en {'A' if sec in Aa else 'B'}")
            continue
        L += [f"  [{sec}] {x}" for x in diff_objects(da, Aa[sec], db, Ab[sec])]
    return L


def _keys(A):
    cnt, out = collections.Counter(), {}
    for o in A.objs:
        out[o.oid] = (o.cls, cnt[o.cls])
        cnt[o.cls] += 1
    return out


def _tok(A, keys, v):
    if v in keys:
        return ("obj",) + keys[v]
    if v & A.REF and (v & ~A.REF) in A.classes:
        return ("cls", A.classes[v & ~A.REF][0])
    return None


def diff_objects(da, A, db, B):
    L = []
    ka, kb = _keys(A), _keys(B)
    oa, ob = collections.defaultdict(list), collections.defaultdict(list)
    for o in A.objs:
        oa[o.cls].append(o)
    for o in B.objs:
        ob[o.cls].append(o)
    for cls in sorted(set(oa) | set(ob)):
        if len(oa[cls]) != len(ob[cls]):
            L.append(f"clase {cls}: instancias {len(oa[cls])} -> {len(ob[cls])}")
            continue
        if cls == "MHitPointEvent":
            continue
        for x, y in zip(oa[cls], ob[cls]):
            bx = b"".join(da[s:e] for s, e in A.own_ranges(x))
            by = b"".join(db[s:e] for s, e in B.own_ranges(y))
            if bx == by:
                continue
            if len(bx) != len(by):
                sa = [s for _, s in A.strings(x)]
                sb = [s for _, s in B.strings(y)]
                extra = f" cadenas {sorted(set(sa) - set(sb))} -> {sorted(set(sb) - set(sa))}" if sa != sb else ""
                L.append(f"{cls} @A0x{x.at:x}/B0x{y.at:x}: bytes propios {len(bx)} -> {len(by)}{extra}")
                continue
            W, fmt = A.W, A.fmt
            mask = bytearray(len(bx))
            for i in range(len(bx) - W + 1):
                if bx[i:i + W] != by[i:i + W]:
                    ta = _tok(A, ka, struct.unpack_from(fmt, bx, i)[0])
                    if ta is not None and ta == _tok(B, kb, struct.unpack_from(fmt, by, i)[0]):
                        mask[i:i + W] = b"\1" * W
            dif = [i for i in range(len(bx)) if bx[i] != by[i] and not mask[i]]
            if dif:
                i0 = dif[0]
                L.append(f"{cls} @A0x{x.at:x}/B0x{y.at:x}: {len(dif)} bytes distintos en {dif[:16]}; "
                         f"contexto[{max(0, i0 - 8)}:] A={bx[max(0, i0 - 8):i0 + 12].hex(' ')} | B={by[max(0, i0 - 8):i0 + 12].hex(' ')}")
    return L


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cpr")
    ap.add_argument("--classes", action="store_true")
    ap.add_argument("--tree", action="store_true")
    ap.add_argument("--validate", metavar="DIR_AUDIO")
    ap.add_argument("--mixdown", metavar="WAV")
    ap.add_argument("--diff", metavar="OTRO_CPR")
    ap.add_argument("--out")
    a = ap.parse_args()
    if a.out:
        bad = {os.path.realpath(p) for p in (a.cpr, a.diff) if p}
        if os.path.realpath(a.out) in bad or a.out.lower().endswith((".cpr", ".bak", ".wav")):
            raise SystemExit("--out no puede apuntar a un proyecto, copia de seguridad o audio")
    if a.diff:
        L = diff(a.cpr, a.diff)
    else:
        d, wide, archs = load(a.cpr)
        L, A, evs, tp = summary(a.cpr, d, archs)
        if a.classes:
            L += ["", "TABLA DE CLASES (Arrangement1)"] + classes_table(A)
        if a.tree:
            L += ["", "ARBOL"] + tree_lines(A)
        if a.validate:
            L += ["", f"VALIDACION contra {a.validate}"] + validate(evs, A, d, a.validate)
        if a.mixdown:
            w = wav_info(a.mixdown)
            sec = w["frames"] / w["hz"]
            L.append(f"mezcla: {w['frames']} frames / {w['hz']} Hz = {sec:.6f} s")
            if tp and "bpm" in tp:
                L.append(f"  a {tp['bpm']:.4f} BPM = {sec * tp['bpm'] / 60:.4f} negras; a 120 BPM = {sec * 2:.4f} negras")
    text = "\n".join(L) + "\n"
    if a.out:
        with open(a.out, "w", encoding="utf-8") as f:
            f.write(text)
    sys.stdout.write(text if not a.out else f"escrito {a.out}\n")


if __name__ == "__main__":
    main()

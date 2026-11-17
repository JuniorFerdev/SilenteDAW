#!/usr/bin/env python3
"""Volcado de solo lectura del contenedor Cubase .cpr (herramienta de investigacion).

Fuera de la app: no se enlaza a CMake ni a src/. Abre siempre en 'rb'.

Estructura verificada (ver docs/CPR_INVESTIGACION.md):
  cabecera: "RIFF" + u32 BIG-endian (tamano) + "NUND"  (12 bytes)
  bloques : tag(4 ASCII) + u32 BIG-endian (tamano del payload) + payload
  ROOT/ARCH son bloques planos; el payload de ARCH es un flujo serializado
  con cadenas "u32-BE longitud + bytes" y marcadores FFFFFFFE / FFFFFFFF.
NO VERIFICADO: la semantica de cada campo dentro de ARCH; los nombres de clase
se extraen por patron (cadena con longitud previa, aspecto de identificador).
"""
import argparse
import collections
import re
import struct
import sys

CLASS_RE = re.compile(rb"^[MPCG][A-Za-z][A-Za-z0-9_]{2,60}$")
NEWCLASS = b"\xff\xff\xff\xfe"
OLDCLASS = b"\xff\xff\xff\xff"


def u32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def parse(data):
    """Devuelve (cabecera, bloques, avisos). bloque = (off, tag, size, estado)."""
    warns = []
    magic = data[:4]
    if magic == b"RIFF" and data[8:12] == b"NUND":
        wide, hl = False, 12   # tamanos u32 BE
    elif magic == b"RIF2" and data[12:16] == b"NUND":
        wide, hl = True, 16    # tamanos u64 BE (variante nueva, verificado en yo.cpr y sulphuric)
    else:
        raise SystemExit("No es un .cpr (RIFF/RIF2 + NUND esperado)")
    riff = struct.unpack_from(">Q", data, 4)[0] if wide else u32(data, 4)
    hdr = {"magic": magic.decode(), "riff_size": riff, "form": "NUND", "file_size": len(data), "wide": wide}
    ext = 8 if wide else 4
    base = hl - 8 if wide else 8  # candidato: tamano = archivo - base
    # observado: tamano declarado = archivo - longitud de cabecera (12 u32 / 16 u64), NO archivo-8 como en RIFF clasico
    if riff + hl != len(data):
        warns.append(f"{magic.decode()}: tamano declarado {riff} + cabecera {hl} = {riff + hl} != archivo {len(data)}")
    blocks = []
    o = hl
    while o + 4 + ext <= len(data):
        tag = data[o:o + 4]
        if not re.match(rb"^[A-Za-z0-9 ]{4}$", tag):
            warns.append(f"@0x{o:x}: etiqueta no ASCII {tag!r}; recorrido detenido")
            break
        size = struct.unpack_from(">Q", data, o + 4)[0] if wide else u32(data, o + 4)
        end = o + 4 + ext + size
        state = "ok"
        if end > len(data):
            state = "EXCEDE ARCHIVO"
            warns.append(f"@0x{o:x} {tag.decode()}: fin 0x{end:x} > archivo 0x{len(data):x}")
        blocks.append((o, tag.decode(), size, state, 4 + ext))
        if end > len(data):
            break
        o = end
    else:
        if o != len(data):
            warns.append(f"{len(data) - o} bytes sobrantes tras el ultimo bloque en 0x{o:x}")
    if o < len(data) and not warns:
        warns.append(f"{len(data) - o} bytes sobrantes tras el ultimo bloque en 0x{o:x}")
    return hdr, blocks, warns


def strings_in(data, start, end):
    """Cadenas u32-BE-longitud + texto con aspecto de clase. Devuelve [(off, nombre, es_definicion)]."""
    out = []
    i = start
    while i + 5 <= end:
        if data[i] == 0 and data[i + 1] == 0 and data[i + 2] == 0 and 3 <= data[i + 3] <= 64:
            n = data[i + 3]
            s = data[i + 4:i + 4 + n]
            nul = s.endswith(b"\x00")  # en ARCH la longitud incluye un NUL final (verificado: CmObject len 9)
            s = s.rstrip(b"\x00")
            if i + 4 + n <= end and nul and CLASS_RE.match(s):
                # marcador previo FFFFFFFE (nueva clase) o FFFFFFFF: semantica NO verificada
                isdef = i >= start + 4 and data[i - 4:i] in (NEWCLASS, OLDCLASS)
                out.append((i + 4, s.decode(), isdef))
                i += 4 + n
                continue
        i += 1
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cpr")
    ap.add_argument("--out", help="escribe el volcado de texto en esta ruta")
    ap.add_argument("--summary", action="store_true", help="solo conteo por nombre de clase")
    ap.add_argument("--hex", metavar="OFFSET", help="vuelca 64 bytes desde OFFSET (decimal o 0x..)")
    a = ap.parse_args()
    with open(a.cpr, "rb") as f:
        data = f.read()
    hdr, blocks, warns = parse(data)
    lines = [f"magia: {hdr['magic']} (tamanos {'u64' if hdr['wide'] else 'u32'} big-endian)", f"archivo: {a.cpr}", f"tamano archivo: {hdr['file_size']}",
             f"tamano declarado en cabecera: {hdr['riff_size']} (declarado + cabecera = {hdr['riff_size'] + (16 if hdr['wide'] else 12)}) forma: {hdr['form']}"]
    counts = collections.Counter()
    defs = collections.Counter()
    tagcount = collections.Counter()
    first = {}
    tree = []
    for off, tag, size, st, bh in blocks:
        tagcount[tag] += 1
        tree.append(f"0x{off:08x} {tag} size={size} fin=0x{off + bh + size:08x} [{st}]")
        if tag == "ROOT":
            p = off + bh
            # cadenas planas con longitud previa
            i, e = p, p + size
            txt = []
            while i + 4 <= e:
                n = u32(data, i)
                if 0 < n <= e - i - 4:
                    txt.append(data[i + 4:i + 4 + n].decode("latin1"))
                    i += 4 + n
                else:
                    break
            tree.append(f"    cadenas: {txt}")
        if tag == "ARCH" and st == "ok":
            for so, name, isdef in strings_in(data, off + bh, off + bh + size):
                counts[name] += 1
                first.setdefault(name, so)
                if isdef:
                    defs[name] += 1
                tree.append(f"    0x{so:08x} {'MRK ' if isdef else 'str '}{name}")
    summ = [f"bloques por tipo: {dict(tagcount)}", "clase: apariciones (definiciones) primer_offset"]
    for n, c in sorted(counts.items(), key=lambda x: (-x[1], x[0])):
        summ.append(f"  {n}: {c} ({defs.get(n, 0)}) 0x{first[n]:x}")
    lines += [f"AVISO: {w}" for w in warns] or ["cuadre: bloques consecutivos hasta el final del archivo; declarado + cabecera == archivo"]
    if not a.summary:
        lines += tree
    lines += summ
    if a.hex:
        o = int(a.hex, 0)
        lines.append(f"hex @0x{o:x}: {data[o:o + 64].hex(' ')}")
    text = "\n".join(lines) + "\n"
    if a.out:
        with open(a.out, "w", encoding="utf-8") as f:
            f.write(text)
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()

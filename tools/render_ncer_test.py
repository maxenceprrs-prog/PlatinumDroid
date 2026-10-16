#!/usr/bin/env python3
"""Host-side visual test for a NCER + NCGR + NCLR triplet.

This is deliberately a verification utility, not part of the PSP executable.
It mirrors the supported normal 1D OAM path in nds_sprite.c and writes all
NCER cells as a vertically stacked PNG.
"""
from __future__ import annotations
import argparse
from pathlib import Path
from PIL import Image

SIZES = (((8, 8), (16, 16), (32, 32), (64, 64)),
         ((16, 8), (32, 8), (32, 16), (64, 32)),
         ((8, 16), (8, 32), (16, 32), (32, 64)))

def u16(b: bytes, o: int) -> int: return b[o] | b[o + 1] << 8
def u32(b: bytes, o: int) -> int: return u16(b, o) | u16(b, o + 2) << 16

def palette(b: bytes):
    if b[:4] not in (b"RLCN", b"RPCN") or b[16:20] != b"TTLP": raise ValueError("invalid NCLR")
    count, o = min(256, u32(b, 32) // 2), 40
    out = []
    for i in range(count):
        c = u16(b, o + i * 2); out.append((((c & 31) * 255 // 31), (((c >> 5) & 31) * 255 // 31), (((c >> 10) & 31) * 255 // 31), 255))
    return out

def tiles(b: bytes):
    if b[:4] != b"RGCN" or b[16:20] != b"RAHC": raise ValueError("invalid NCGR")
    eight = b[28] != 3; width = 64 if eight else 32; raw = b[48:16 + u32(b, 20)]
    result = []
    for start in range(0, len(raw) - width + 1, width):
        src = raw[start:start + width]
        result.append(list(src) if eight else [v for byte in src for v in (byte & 15, byte >> 4)])
    return result, eight

def cells(b: bytes):
    if b[:4] != b"RECN" or b[16:20] != b"KBEC": raise ValueError("invalid NCER")
    count, extended, mapping, end = u16(b, 24), b[26] == 1, u32(b, 32), 16 + u32(b, 20)
    header_size = 16 if extended else 8; headers = 48; oam = headers + count * header_size
    output = []
    for i in range(count):
        n = u16(b, headers + i * header_size)
        if oam + n * 6 > end: raise ValueError("truncated NCER OAM")
        objs = []
        for _ in range(n):
            a0, a1, a2 = u16(b, oam), u16(b, oam + 2), u16(b, oam + 4); oam += 6
            if a0 & 0x300: raise ValueError("affine/disabled OAM is intentionally unsupported")
            shape, size = a0 >> 14, a1 >> 14
            if shape == 3: raise ValueError("invalid OAM shape")
            x, y = a1 & 511, a0 & 255
            objs.append((x - 512 if x >= 256 else x, y - 256 if y >= 128 else y, *SIZES[shape][size], a2 & 1023, a2 >> 12, bool(a1 & 0x1000), bool(a1 & 0x2000)))
        output.append(objs)
    return output, mapping

def render(objects, mapping, image_tiles, eight, colors):
    minx = min(x for x, y, w, h, *_ in objects); miny = min(y for x, y, w, h, *_ in objects)
    maxx = max(x + w for x, y, w, h, *_ in objects); maxy = max(y + h for x, y, w, h, *_ in objects)
    image = Image.new("RGBA", (maxx - minx, maxy - miny))
    for x, y, w, h, name, pal, hflip, vflip in objects:
        unit, tile_bytes = 1 << mapping, 64 if eight else 32
        row_units = ((w // 8 * tile_bytes // 32 + unit - 1) // unit) * unit
        base, row_tiles = name * unit * 32 // tile_bytes, row_units * 32 // tile_bytes
        for py in range(h):
            for px in range(w):
                sx, sy = (w - 1 - px if hflip else px), (h - 1 - py if vflip else py)
                tile = base + sy // 8 * row_tiles + sx // 8
                if tile >= len(image_tiles): continue
                index = image_tiles[tile][sy % 8 * 8 + sx % 8]; color = index if eight else pal * 16 + index
                if index and color < len(colors): image.putpixel((x - minx + px, y - miny + py), colors[color])
    return image

p = argparse.ArgumentParser(); p.add_argument("ncer", type=Path); p.add_argument("ncgr", type=Path); p.add_argument("nclr", type=Path); p.add_argument("output", type=Path); a = p.parse_args()
objects, mapping = cells(a.ncer.read_bytes()); image_tiles, eight = tiles(a.ncgr.read_bytes()); colors = palette(a.nclr.read_bytes())
frames = [render(o, mapping, image_tiles, eight, colors) for o in objects if o]
w, h = max(i.width for i in frames), sum(i.height + 1 for i in frames) - 1
result = Image.new("RGBA", (w, h)); y = 0
for frame in frames: result.alpha_composite(frame, ((w - frame.width) // 2, y)); y += frame.height + 1
result.save(a.output)
print(f"Rendered {len(frames)} cells, mapping {mapping}, to {a.output} ({w}x{h})")

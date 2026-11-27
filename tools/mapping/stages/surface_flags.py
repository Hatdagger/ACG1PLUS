#!/usr/bin/env python3
"""Count stage collision polygons by surface flag (bits 0-2 of polygon +0x0E) on every map / stage.

  python3 -I tools/mapping/stages/surface_flags.py WORK     (WORK holds pp-disc/ and ma-disc/, set up)

Reads chunk 6 of each stage data file (FDAT 5 x n + 4; MoA has one extra chunk in front): 1,024 pairs of
offsets, the second of each pair = a collision block: u16 polygon count, three 32-byte bit masks, then the
polygons, 20 bytes each from +0x64 (stages.md 5a). Prints counts only, no game data.
"""
import collections, os, struct, sys
W = sys.argv[1].rstrip('/') + '/'

def chunks(d, extra):
    o, out = 0, []
    while o + 4 <= len(d) and len(out) < 21:
        n = struct.unpack_from('<I', d, o)[0]; out.append(d[o + 4:o + 4 + n]); o += 4 + n
    return out[extra:]

def survey(g, s):
    p = W + g + '-disc/unpacked/GG/COM/FDAT/%03d.bin' % (5 * s + 4)
    if not os.path.exists(p) or os.path.getsize(p) < 1000: return None
    ch = chunks(open(p, 'rb').read(), 1 if g == 'ma' else 0)
    if len(ch) < 7: return None
    c6 = ch[6]; blocks = set()
    for i in range(1024):
        b = struct.unpack_from('<II', c6, i * 8)[1]
        if b and b < len(c6): blocks.add(b)
    cnt = collections.Counter()
    for b in blocks:
        n = struct.unpack_from('<H', c6, b)[0]
        if b + 0x64 + n * 20 > len(c6): continue
        for k in range(n): cnt[struct.unpack_from('<H', c6, b + 0x64 + k * 20 + 0xE)[0] & 7] += 1
    return len(blocks), dict(sorted(cnt.items()))

for g in ('pp', 'ma'):
    for s in range(48):
        r = survey(g, s)
        if r: print(g, s, 'blocks', r[0], 'flags', r[1])

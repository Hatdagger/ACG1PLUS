#!/usr/bin/env python3
"""Stage files of Project Phantasma (PP) and Master of Arena (MoA): a read-only survey.

Reads the player's extracted work packs (pp-disc/, ma-disc/ after setup.py) from the folder in ACG_WORK and
prints sizes, counts and record fields only (nothing from the disc is stored; don't commit the output).
Notes: docs/research/mapping/stages.md.

  python3 -I stage_survey.py chunks [pp|ma]   # chunk sizes of every stage data file (FDAT 5s + 4) and its code files
  python3 -I stage_survey.py objects          # PP stage object types / classes per stage, AI markers
  python3 -I stage_survey.py moa-map          # PP stage -> MoA stage with the same geometry chunk size
"""
import collections, os, struct, sys

W = os.environ.get("ACG_WORK", ".").rstrip("/") + "/"
ROOT = {"pp": W + "pp-disc/", "ma": W + "ma-disc/"}
NCHUNK = {"pp": 13, "ma": 14}      # MoA stage data starts with one extra 80-byte chunk
u32 = lambda d, o: struct.unpack_from("<I", d, o)[0]


def fdat(g, e):
    p = ROOT[g] + "unpacked/GG/COM/FDAT/%03d.bin" % e
    return open(p, "rb").read() if os.path.exists(p) else b""


def chunks(g, s):
    d, o, out = fdat(g, 5 * s + 4), 0, []
    if not d:
        return None
    for _ in range(NCHUNK[g]):
        n = u32(d, o); out.append(d[o + 4:o + 4 + n]); o += 4 + n
    return out[NCHUNK[g] - 13:]          # drop MoA's leading chunk so both line up


def objects(s):
    c = chunks("pp", s)
    t = c[8]; types = [struct.unpack_from("<12h", t, 4 + 24 * i) for i in range(u32(t, 0))]
    objs = []
    for i in range(len(c[9]) // 32):
        r = c[9][32 * i:32 * i + 32]; ti = struct.unpack_from("<h", r, 6)[0]
        if ti >= 0:
            objs.append(dict(i=i, pos=struct.unpack_from("<3h", r, 0), type=ti, param=r[0x1A]))
    return types, objs


def main(cmd, g="pp"):
    if cmd == "chunks":
        for s in range(40):
            c = chunks(g, s)
            code = "".join("x" if fdat(g, 5 * s + k) else "." for k in range(4))
            print(s, "code k0..3", code, "chunks", [len(x) for x in c] if c else None)
    elif cmd == "objects":
        for s in range(20, 40):
            types, objs = objects(s)
            cls = collections.Counter(types[o["type"]][3] for o in objs)
            ai = collections.Counter(o["param"] for o in objs if types[o["type"]][3] == 248)
            print(s, "types", len(types), "objects", len(objs), "classes", dict(cls), "AI markers (class 248) by param", dict(ai))
    elif cmd == "moa-map":
        ma = {t: chunks("ma", t) for t in range(20, 40)}
        for s in range(20, 40):
            a = chunks("pp", s)
            hit = [t for t, b in ma.items() if b and len(b[0]) == len(a[0])]
            same = [t for t in hit if ma[t][0] == a[0]]
            print("PP", s, "-> MoA", hit, "(geometry byte-identical: %s)" % same)
    else:
        print(__doc__)


if __name__ == "__main__":
    main(*(sys.argv[1:] or [""]))

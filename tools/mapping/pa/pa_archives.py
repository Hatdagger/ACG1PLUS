#!/usr/bin/env python3
"""Body archives GG/P0-P3/PA00-PA71.T on Project Phantasma (PP) and Master of Arena (MoA): a read-only survey.

Reads the player's own extracted work packs (pp-disc/ and ma-disc/ after each pack's setup.py) from the folder in
ACG_WORK. Prints addresses, entry numbers, sizes, counts and part names only: nothing from the disc is stored and
the output must not be committed (it is small and easy to rerun). Notes: docs/research/mapping/pa-archives.md.

  python3 -I pa_archives.py sets       # body combination table (menu) + what each archive holds
  python3 -I pa_archives.py check      # archive contents against the combination lists (entry = base + part id)
  python3 -I pa_archives.py bodies     # entry 0 header of every set (joints, mesh slots, animations, hit size)
  python3 -I pa_archives.py diff       # PP vs MoA, entry by entry (MoA numbers mapped to PP's layout)
  python3 -I pa_archives.py moa-only   # models MoA has that PP lacks: the import inventory
"""
import glob, hashlib, os, struct, sys, collections

W = os.environ.get("ACG_WORK", ".").rstrip("/") + "/"
DISC = {"pp": W + "pp-disc/", "ma": W + "ma-disc/"}
MOD_BASE = {"pp": 0x800490D0, "ma": 0x80050DD8}
MENU = {"pp": 201, "ma": 206}
COMBO = {"pp": 0x800BC7B8, "ma": 0x800D4A64}           # 72 x 12 bytes: core, pad, arms list ptr, legs list ptr
# Entry number = base + part id. PP from the fight engines' body loader (VS 0x8004D820), MoA from 0x80055214.
BASES = {"pp": dict(core=2, legs=15, head=57, booster=81, hand=99, arms=122, back_l=150, back_r=209),
         "ma": dict(core=2, legs=15, head=57, booster=81, hand=99, arms=127, back_l=155, back_r=215)}
# Part name tables in the menu module (start, stride); content-data.md section 1.
TABLES = {"pp": dict(head=(0x800B64FC, 60), core=(0x800B6934, 56), legs=(0x800B6B9C, 72), arms=(0x800B76DC, 52),
                     hand=(0x800B7BBC, 44), back=(0x800B7F58, 52), booster=(0x800B8D28, 48)),
          "ma": dict(head=(0x800CE690, 60), core=(0x800CEAC8, 56), legs=(0x800CED30, 72), arms=(0x800CF870, 52),
                     hand=(0x800CFD50, 44), back=(0x800D01C8, 52), booster=(0x800D0F30, 48))}
u32 = lambda d, o: struct.unpack_from("<I", d, o)[0]
u16 = lambda d, o: struct.unpack_from("<H", d, o)[0]
_menu = {}


def menu(g):
    if g not in _menu:
        _menu[g] = open(DISC[g] + "unpacked/GG/COM/FDAT/%03d.bin" % MENU[g], "rb").read()
    return _menu[g]


def name(g, cat, i):
    a, st = TABLES[g]["back" if cat.startswith("back") else cat]
    o = a - MOD_BASE[g] + st * i
    return menu(g)[o:o + 18].split(b"\0")[0].decode("latin1")


def combos(g):
    m, b = menu(g), MOD_BASE[g]
    def lst(p):
        out = []
        while m[p - b] != 255:
            out.append(m[p - b]); p += 1
        return out
    t = COMBO[g] - b
    return [(m[t + 12 * i], lst(u32(m, t + 12 * i + 4)), lst(u32(m, t + 12 * i + 8))) for i in range(72)]


def ent(g, n, e):
    p = glob.glob(DISC[g] + "unpacked/GG/P*/PA%02d/%03d.bin" % (n, e))
    return open(p[0], "rb").read() if p else b""


def entries(g, n):
    d = glob.glob(DISC[g] + "unpacked/GG/P*/PA%02d" % n)[0]
    return {int(f[:3]) for f in os.listdir(d) if f.endswith(".bin") and os.path.getsize(d + "/" + f)}


def cat_of(g, e):
    best = None
    for c, b in BASES[g].items():
        if e >= b and (best is None or b > BASES[g][best]):
            best = c
    return (best, e - BASES[g][best]) if best else ("body", e)


def to_ma(e):
    """PP entry -> MoA entry (None = no MoA twin). MoA inserted back IDs 16/17: PP back n >= 17 = MoA n + 1."""
    c, i = cat_of("pp", e) if e >= 2 else ("body", e)
    if c == "body":
        return e
    if c.startswith("back"):
        if i == 16:
            return None
        i = i + 1 if i >= 17 else i
    return BASES["ma"][c] + i


def body(g, n):
    d = ent(g, n, 0)
    jt, ms = u32(d, 12), u32(d, 8)
    p, nodes, slots = jt + 2, 0, 0
    while d[p] == 8:
        nodes += 1; p += 2
    while d[p] in (2, 3):
        slots += d[p] == 2 and u16(d, p + 2) < 0x8000; p += 4
    r = ent(g, n, 1)
    return dict(size=u32(d, 0), anims=d[4], joints=d[5], aim_tables=d[6], layer2=d[7], slots_per_level=u32(d, ms),
                levels=u32(d, ms + 8) // max(1, u32(d, ms)), present=sum(1 for k in range(57)
                if k == 0 or u32(d, u32(d, 16) + 4 * k)), hit_r=u16(r, 8) // 4, hit_h=u16(r, 10) // 4)


def main(cmd):
    C = combos("pp")
    if cmd == "sets":
        for i, (c, a, l) in enumerate(C):
            print("%2d P%d\\PA%02d core %-7s arms %-28s legs %s" % (i, i // 20, i, name("pp", "core", c),
                  ",".join(name("pp", "arms", x) for x in a), ",".join(name("pp", "legs", x) for x in l)))
    elif cmd == "check":
        B = BASES["pp"]
        for i, (c, a, l) in enumerate(C):
            want = {0, 1, c + 2} | ({5} if c == 0 else set()) | {x + B["legs"] for x in l} | \
                   {x + B["arms"] for x in a} | set(range(57, 68)) | set(range(81, 88))
            want |= {B["back_l"] + k for k in range(59) if name("pp", "back", k) != "DUMMY"}
            want |= {B["back_r"] + k for k in range(59) if name("pp", "back", k) != "DUMMY"}
            if min(a) < 12:
                want |= set(range(99, 120))
            have = entries("pp", i)
            if have != want:
                print(i, "extra", sorted(have - want), "missing", sorted(want - have))
        print("checked 72 sets")
    elif cmd == "bodies":
        for i in range(72):
            print(i, body("pp", i))
    elif cmd == "diff":
        by = collections.defaultdict(list)
        for n in range(72):
            for e in sorted(entries("pp", n)):
                me = to_ma(e)
                a, b = ent("pp", n, e), (ent("ma", n, me) if me is not None else b"")
                if a != b:
                    by[e].append((n, sum(x != y for x, y in zip(a, b)) + abs(len(a) - len(b))))
        for e in sorted(by):
            c, i = cat_of("pp", e) if e >= 2 else ("body", e)
            nm = name("pp", c, i) if c not in ("body",) else ("entry %d" % e)
            v = by[e]
            print("PP entry %3d %-8s %-14s differs in %2d sets (bytes %d-%d): %s" % (e, c, nm, len(v),
                  min(x[1] for x in v), max(x[1] for x in v), [x[0] for x in v][:12]))
    elif cmd == "moa-only":
        inv = {to_ma(e): e for e in range(268) if to_ma(e) is not None}
        found = collections.defaultdict(list)
        for n in range(72):
            for e in entries("ma", n):
                pe = inv.get(e)
                if pe is None or pe not in entries("pp", n):
                    found[cat_of("ma", e)].append((n, len(ent("ma", n, e))))
        for (c, i), v in sorted(found.items(), key=lambda x: (BASES["ma"].get(x[0][0], 0), x[0][1])):
            print("MoA %-8s %2d %-12s in %2d sets, %6d bytes in all: %s" % (c, i, name("ma", c, i), len(v),
                  sum(x[1] for x in v), [x[0] for x in v]))
    else:
        print(__doc__)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "")

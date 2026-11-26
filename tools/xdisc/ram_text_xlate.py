#!/usr/bin/env python3
"""Put the text of PP rows copied from MoA in docs/research/ram-map.toml into PP terms: the addresses
written inside 'what' (routines via xdisc/functions.tsv, data via globals.tsv / runs.tsv / the row's own shift).
Only rows whose text is still the MoA row's text word for word (rows a person rewrote are left alone).
  python3 -I tools/xdisc/ram_text_xlate.py .            # report
  python3 -I tools/xdisc/ram_text_xlate.py . --write    # apply
"""
import re, sys, bisect, collections, tomllib
ROOT = sys.argv[1]; WRITE = "--write" in sys.argv
TOML = ROOT + "/docs/research/ram-map.toml"; X = ROOT + "/docs/research/xdisc/"
BASE = 0x80050DD8; EXE = "SLUS_010.30"
MOD = {"menu": "FDAT 206", "hub": "FDAT 206", "garage": "FDAT 206", "shop": "FDAT 206", "missions": "FDAT 207",
       "mission": "FDAT 207", "link": "FDAT 208", "VS": "FDAT 209", "arena": "FDAT 210", "replay": "FDAT 211"}
onlymoa = set(); funcs = collections.defaultdict(list)
for l in open(X + "functions.tsv"):
    if l.startswith("#") or l.startswith("moa_disc"): continue
    f = l.rstrip("\n").split("\t")
    if not f[2]: continue
    a = int(f[2], 16)
    if not f[6]:
        if f[9] == "only_moa": onlymoa.add((f[1], a))
        continue
    if f[12] == "data": continue
    if f[12] == "low":  # keep a low twin only when its calls mostly pair too and it is mostly alike
        c = f[11].split("/")
        if len(c) != 2 or int(c[1]) == 0 or int(c[0]) * 2 < int(c[1]) or float(f[10] or 0) < 0.6: continue
    funcs[f[1]].append((a, int(f[3] or 0), int(f[6], 16), f[9], f[12]))
for k in funcs: funcs[k].sort()
glob = collections.defaultdict(dict)
for l in open(X + "globals.tsv"):
    if l.startswith("#") or l.startswith("moa_disc"): continue
    f = l.rstrip("\n").split("\t")
    if f[9] == "low": continue
    a, b = int(f[1], 16), int(f[3], 16)
    for m in (["exe"] if a < BASE else f[5].split(",")): glob[m][a] = b
runs = []
for l in open(X + "runs.tsv"):
    if l.startswith("#") or l.startswith("moa_first"): continue
    f = l.rstrip("\n").split("\t")
    runs.append((int(f[0], 16), int(f[1], 16), int(f[3], 16) - int(f[0], 16), f[6], int(f[5])))
d = tomllib.load(open(TOML, "rb"))
moatext = {v["name"]: v.get("what", "") for v in d["var"] if v.get("disc") == "MoA-US"}
rowmoa = collections.defaultdict(set)
for v in d["var"]:
    if v.get("disc") == "PP" and v.get("moa"): rowmoa[v["moa"]].add(v["addr"])

def one(a, mod):
    fm = EXE if a < BASE else mod
    if fm:
        L = funcs.get(fm, []); i = bisect.bisect_right([x[0] for x in L], a) - 1
        if i >= 0:
            s, n, p, kind, conf = L[i]
            if s == a: return (p, "routine") if conf != "low" else (p, "routine-likely")
            if s < a < s + n and kind in ("same", "values") and conf == "high": return p + a - s, "in-routine"
    gm = "exe" if a < BASE else mod
    if gm and a in glob.get(gm, {}): return glob[gm][a], "data"
    if a < BASE and len(rowmoa.get(a, ())) == 1: return next(iter(rowmoa[a])), "row"
    if a < BASE:
        for lo, hi, dl, m, n in runs:
            if lo <= a <= hi and n >= 3 and "exe" in m: return a + dl, "run"
    return None

def xl(a, mod):
    if a >= BASE and not mod:
        res = {one(a, m) for m in set(MOD.values())} - {None}
        if len({r[0] for r in res}) == 1: r = res.pop(); return r[0], r[1] if r[1] == "routine-likely" else "any-module"
        return None
    return one(a, mod)

SIZES = {"mission_placements": (11264, "struct[256]", "64 × 44", "256 × 44"),
         "mission_trigger_zones": (26624, None, None, None), "mission_code": (55296, None, None, None)}
ABS = {0x801FE000, 0x80200000, 0x1F800000}   # same on both discs (RAM top, scratchpad)
blocks = open(TOML).read().split("\n[[var]]\n")
stats = collections.Counter(); unres = []; changed = 0
for bi, b in enumerate(blocks):
    if 'disc = "PP"' not in b or "\nmoa = " not in b: continue
    name = re.search(r'^name = "(.*)"$', b, re.M).group(1)
    nb = b
    if name in SIZES:
        sz, ty, o, n = SIZES[name]
        nb = re.sub(r"^size = \d+$", "size = %d" % sz, nb, count=1, flags=re.M)
        if ty: nb = re.sub(r'^type = ".*"$', 'type = "%s"' % ty, nb, count=1, flags=re.M)
        if o: nb = nb.replace(o, n, 1)
        stats["size-fix"] += 1
    m = re.search(r'^what = "(.*)"$', nb, re.M)
    what = m.group(1); head, sep, tail = what.partition(" [Was")
    if head != moatext.get(name):
        stats["skip-rewritten"] += 1; blocks[bi] = nb; continue
    rmoa = int(re.search(r"^moa = (0x[0-9A-Fa-f]+)", nb, re.M).group(1), 16)
    raddr = int(re.search(r"^addr = (0x[0-9A-Fa-f]+)", nb, re.M).group(1), 16)
    rsize = int(re.search(r"^size = (\d+)", b, re.M).group(1))
    eng = re.match(r"\[([^\]]+)\]", head); rowmod = MOD.get(eng.group(1).split()[0]) if eng else None
    h = re.sub(r"0x(8[0-9A-Fa-f]{5})([0-9A-Fa-f]{2})/([0-9A-Fa-f]{2})\b", r"0x\1\2 / 0x\1\3", head)
    def rep(mt):
        a = int(mt.group(0), 16)
        if a in ABS: return mt.group(0)
        if a == BASE: stats["module-base"] += 1; return "0x800490D0"
        before = h[max(0, mt.start() - 16):mt.start()]; mod = rowmod; best = -1
        for w, fm in MOD.items():
            for mm in re.finditer(r"\b" + w + r"\b", before):
                if mm.start() > best: best, mod = mm.start(), fm
        r = xl(a, mod)
        if r is None and rmoa <= a <= rmoa + max(rsize, 1): r = (a + raddr - rmoa, "own-span")
        if r is None and (EXE if a < BASE else mod, a) in onlymoa:
            stats["no-twin"] += 1; return "MoA " + mt.group(0) + " (no PP twin)"
        if r is None:
            stats["unresolved"] += 1; unres.append((name, mt.group(0))); return "MoA " + mt.group(0)
        stats[r[1]] += 1
        return "0x%08X" % r[0] + (" (likely)" if r[1] == "routine-likely" else "")
    nh = re.sub(r"0x8[0-9A-Fa-f]{7}", rep, h)
    if nh != head:
        nb = nb.replace(m.group(0), 'what = "' + nh + sep + tail + '"')
        nb = re.sub(r'^(src = ".*)"$', r'\1; text addresses put into PP terms 2026-11-26 (MoA where marked)"', nb, count=1, flags=re.M)
        changed += 1
    blocks[bi] = nb
print(changed, "rows changed", dict(stats))
for u in unres: print("UNRES", *u)
if WRITE: open(TOML, "w").write("\n[[var]]\n".join(blocks))

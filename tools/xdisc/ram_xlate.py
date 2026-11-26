#!/usr/bin/env python3
"""Add Project Phantasma (PP) rows to docs/research/ram-map.toml by carrying each Master of Arena
(MoA-US) row across with the cross-disc table (docs/research/xdisc/globals.tsv).

  python3 -I tools/xdisc/ram_xlate.py            # report what it would add
  python3 -I tools/xdisc/ram_xlate.py --write    # append the new PP rows to the TOML

Only rows whose name has no PP row yet are added, so rows a person has since edited are kept.
How an address is found, best first:
  exact   the MoA address itself is used by paired code (globals.tsv)        -> status code
  inside  addresses inside the row's span are used, all with one shift        -> status code
  between paired addresses just below and above share one shift (< 0x400)     -> status guess
  near    only a paired address just below (< 0x40) gives the shift           -> status guess
Rows where nothing fits are listed, not added.
"""
import argparse, bisect, collections, os, re, sys, tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOML = os.path.join(ROOT, "docs/research/ram-map.toml")
GLOB = os.path.join(ROOT, "docs/research/xdisc/globals.tsv")
MOA_MOD = {"menu": "FDAT 206", "missions": "FDAT 207", "link": "FDAT 208", "VS": "FDAT 209",
           "arena": "FDAT 210", "replay": "FDAT 211"}
ENGINES = ["VS", "arena", "link", "replay", "missions", "menu"]
MOA_MODULE_BASE = 0x80050DD8


def load_pairs():
    by_mod = collections.defaultdict(list)   # module (or 'exe') -> sorted [(moa, pp, conf)]
    for line in open(GLOB):
        if line.startswith("#") or line.startswith("moa_disc"):
            continue
        f = line.rstrip("\n").split("\t")
        a, b, mods, conf = int(f[1], 16), int(f[3], 16), f[5].split(","), f[9]
        if a < MOA_MODULE_BASE:
            by_mod["exe"].append((a, b, conf))
        else:
            for m in mods:
                by_mod[m].append((a, b, conf))
    for k in by_mod:
        by_mod[k].sort()
    return by_mod


def translate(addr, size, pairs):
    keys = [p[0] for p in pairs]
    i = bisect.bisect_left(keys, addr)
    if i < len(pairs) and pairs[i][0] == addr and pairs[i][2] != "low":
        return pairs[i][1], "exact", pairs[i][2]
    inner = [p for p in pairs if addr <= p[0] < addr + max(size, 1) and p[2] != "low"]
    shifts = {p[1] - p[0] for p in inner}
    if inner and len(shifts) == 1:
        return addr + shifts.pop(), "inside", "medium" if len(inner) > 1 else "low"
    good = [p for p in pairs if p[2] != "low"]
    gk = [p[0] for p in good]
    j = bisect.bisect_left(gk, addr)
    lo = good[j - 1] if j > 0 else None
    hi = good[j] if j < len(good) else None
    def aligned(d):  # a shift that breaks the address's alignment is not believable
        return addr % 4 != 0 or d % 4 == 0
    if lo and hi and hi[0] - lo[0] < 0x400 and hi[1] - hi[0] == lo[1] - lo[0] and aligned(lo[1] - lo[0]):
        return addr + lo[1] - lo[0], "between", "low"
    if lo and addr - lo[0] < 0x40 and aligned(lo[1] - lo[0]):
        return addr + lo[1] - lo[0], "near", "low"
    return None, "none", ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    data = tomllib.load(open(TOML, "rb"))
    rows = data["var"]
    have_pp = {v["name"] for v in rows if v.get("disc") == "PP"}
    pairs = load_pairs()
    out, missing, how = [], [], collections.Counter()
    for v in rows:
        if v.get("disc", "MoA-US") != "MoA-US" or v["name"] in have_pp:
            continue
        a, size = v["addr"], v.get("size", 0)
        if a < MOA_MODULE_BASE:
            prim, others = ("exe", a), []
        else:
            m = re.match(r"\[([^\]]+)\]", v["what"])
            eng = [x.strip() for x in m.group(1).replace("/", ",").split(",")] if m else []
            eng = [e for e in eng if e in MOA_MOD]
            if not eng:
                missing.append((v["name"], a, "module-area row without an [engine] tag"))
                continue
            prim = (MOA_MOD[eng[0]], a)
            others = [(e, MOA_MOD[e], v[e]) for e in ENGINES if e in v]
        if a < 0x80000000:          # scratchpad: same layout on both games so far
            pp, kind, conf = a, "same", "medium"
        else:
            pp, kind, conf = translate(prim[1], size, pairs[prim[0]])
        if pp is None:
            missing.append((v["name"], a, "no paired address near it"))
            continue
        how[kind] += 1
        r = {"addr": pp, "size": size, "type": v["type"], "name": v["name"], "disc": "PP",
             "what": v["what"], "status": "code" if kind in ("exact", "inside", "same") and conf != "low" else "guess",
             "src": v["src"] + "; PP address from xdisc/globals.tsv (%s, %s)" % (kind, conf), "moa": a}
        if v.get("owner"):
            r["owner"] = v["owner"]
        for e, mm, ea in others:
            ep, ek, ec = translate(ea, size, pairs[mm])
            if ep is not None:
                r[e] = ep
        out.append(r)
    print("PP rows to add: %d (%s); not carried: %d" % (len(out), dict(how), len(missing)))
    for n, a, why in missing:
        print("  not carried: %-28s %08X  %s" % (n, a, why))
    if not args.write or not out:
        return

    def q(s):
        return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'
    lines = ["", "# ───────────── Project Phantasma (PP, SLUS-00670) rows ─────────────",
             "# Carried over from the MoA-US rows by tools/xdisc/ram_xlate.py: the address is PP's, the meaning",
             "# is MoA's until someone checks it on PP (then fix status/what and drop the 'from xdisc' note).",
             "# moa = the MoA-US row's address. Module-area engines: PP menu FDAT 201, missions 202, link 203,",
             "# VS 204, arena 205, replay 206 (base 0x800490D0)."]
    for r in out:
        lines.append("")
        lines.append("[[var]]")
        for k in ("addr", "size", "type", "name", "disc", "what", "status", "src", "moa", "owner") + tuple(ENGINES):
            if k not in r:
                continue
            val = r[k]
            if k in ("addr", "moa") or k in ENGINES:
                lines.append("%s = 0x%08X" % (k, val))
            elif isinstance(val, int):
                lines.append("%s = %d" % (k, val))
            else:
                lines.append("%s = %s" % (k, q(val)))
    with open(TOML, "a", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("appended to", os.path.relpath(TOML, ROOT))


if __name__ == "__main__":
    main()

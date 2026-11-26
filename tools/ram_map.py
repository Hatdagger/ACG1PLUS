#!/usr/bin/env python3
"""Build docs/research/ram-map.md from docs/research/ram-map.toml.

Also scans the generated boot-exe code (generated/*_full_*.c, the instruction words in its
comments) for every absolute RAM address the exe reads, writes or takes the address of, so the
doc can show which exe routines touch each named address and list what is touched but unnamed.

  python3 tools/ram_map.py                 # rewrite docs/research/ram-map.md (MoA-US rows)
  python3 tools/ram_map.py --disc PP       # rewrite docs/research/ram-map-pp.md (Project Phantasma rows)
  python3 tools/ram_map.py --header x.h    # also write C #defines (MOA_RAM_<NAME>, PP_RAM_<NAME>)

Static only: code loaded from the disc at run time (menus, fight engines) is not in generated/,
so this sees the exe's own code and the PsyQ libraries, not the modules.
"""
import argparse, bisect, collections, glob, json, os, re, sys, tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOML = os.path.join(ROOT, "docs/research/ram-map.toml")
OUT = os.path.join(ROOT, "docs/research/ram-map.md")
GEN = os.path.join(ROOT, "generated")
PERSIST_END = 0x80050DD8
LIB_CODE = 0x80017800          # exe code from here on is PsyQ library code
GP = 0x8003EA28

REGIONS = [
    (0x80000000, 0x80010000, "Kernel / BIOS (not the game's)"),
    (0x80010000, 0x80011000, "Below the exe"),
    (0x80011000, 0x8003EA88, "Exe image: code and data (survives every load)"),
    (0x8003EA88, PERSIST_END, "Exe bss (zeroed at boot, survives every load)"),
    (PERSIST_END, 0x80200000, "Module area (rebuilt on every module load; depends on the engine)"),
    (0x1F800000, 0x1F800400, "Scratchpad (1 KB fast RAM; fight engines only, nothing lasts past a routine)"),
]


# Per-disc layout. The TOML's `disc` field picks the rows (rows without it are MoA-US).
DISCS = {
    "MoA-US": dict(title="Master of Arena — RAM map (US disc 1, SLUS-01030 v1.0)", out="ram-map.md",
                   cache="ram-xref-modules.json", gp=0x8003EA28, module_base=0x80050DD8,
                   lib_code=0x80017800, image_end=0x8003EA88, prefix="MOA_RAM_",
                   modules={206: "menu", 207: "missions", 208: "link", 209: "VS", 210: "arena", 211: "replay"}),
    # PP: module base and gp from the code (foundations.md); the game/library border is MoA's 0x80017800
    # carried over by the cross-disc table (approximate); image end = exe header text size.
    "PP": dict(title="Project Phantasma — RAM map (US, SLUS-00670 v1.0)", out="ram-map-pp.md",
               cache="ram-xref-modules-pp.json", gp=0x8003BC7C, module_base=0x800490D0,
               lib_code=0x80017FC4, image_end=0x8003C000, prefix="PP_RAM_",
               modules={201: "menu", 202: "missions", 203: "link", 204: "VS", 205: "arena", 206: "replay"}),
}


def use_disc(name, generated=None):
    global GP, MODULE_BASE, PERSIST_END, LIB_CODE, MODULES, REGIONS, OUT, XREF_CACHE, GEN, DISC
    d = DISCS[name]
    DISC = name
    GP, MODULE_BASE, PERSIST_END, LIB_CODE, MODULES = d["gp"], d["module_base"], d["module_base"], \
        d["lib_code"], d["modules"]
    OUT = os.path.join(ROOT, "docs/research", d["out"])
    XREF_CACHE = os.path.join(ROOT, "docs/research", d["cache"])
    if generated:
        GEN = generated
    elif name != "MoA-US":
        GEN = os.path.join(ROOT, "generated-pp")
    REGIONS = [
        (0x80000000, 0x80010000, "Kernel / BIOS (not the game's)"),
        (0x80010000, 0x80011000, "Below the exe"),
        (0x80011000, d["image_end"], "Exe image: code and data (survives every load)"),
        (d["image_end"], PERSIST_END, "Exe bss (zeroed at boot, survives every load)"),
        (PERSIST_END, 0x80200000, "Module area (rebuilt on every module load; depends on the engine)"),
        (0x1F800000, 0x1F800400, "Scratchpad (1 KB fast RAM; fight engines only, nothing lasts past a routine)"),
    ]


def s16(x):
    return x - 0x10000 if x & 0x8000 else x


MODULES = {206: "menu", 207: "missions", 208: "link", 209: "VS", 210: "arena", 211: "replay"}
MODULE_BASE = PERSIST_END
# Module-area rows named from one engine may give the same thing's address in the others
# (found by pairing identical routines between engines): arena = 0x…, link = 0x… in the TOML.
OTHER_ENGINES = ["VS", "arena", "link", "replay", "missions", "menu"]
XREF_CACHE = os.path.join(ROOT, "docs/research/ram-xref-modules.json")


def exe_words():
    words = {}
    pat = re.compile(r"/\* 0x(8[0-9A-F]{7}): 0x([0-9A-F]{8}) \*/")
    for f in glob.glob(os.path.join(GEN, "*_full_*.c")):
        for m in pat.finditer(open(f, encoding="utf-8", errors="replace").read()):
            words[int(m.group(1), 16)] = int(m.group(2), 16)
    rng = glob.glob(os.path.join(GEN, "*.ranges"))
    funcs = sorted(int(l.split()[1], 16) for l in open(rng[0]) if l.startswith("F ")) if rng else []
    return words, funcs


def module_words(path, base=None, skip=0):
    """All words of a module file at its load address; function starts guessed from jal targets
    inside the module and from the instruction after each 'jr ra' + delay slot. With base/skip it
    reads the boot exe file itself (base 0x80011000, skip the 0x800-byte header)."""
    MODULE_BASE = globals()["MODULE_BASE"] if base is None else base
    data = open(path, "rb").read()[skip:]
    n = len(data) // 4
    words = {MODULE_BASE + 4 * i: int.from_bytes(data[4 * i:4 * i + 4], "little") for i in range(n)}
    end = MODULE_BASE + 4 * n
    starts = {MODULE_BASE}
    for a, w in words.items():
        if w >> 26 == 3:
            t = (a & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
            if MODULE_BASE <= t < end:
                starts.add(t)
        elif w == 0x03E00008:
            starts.add(a + 8)
    return words, sorted(x for x in starts if x < end)


def in_ram(v):
    """main RAM, scratchpad or I/O ports; anything else is data read as code"""
    return 0x80000000 <= v < 0x80200000 or 0x1F800000 <= v < 0x1F803000


def scan(words, funcs):
    """Return refs = list of (addr, func, pc, kind, size)."""
    fset = set(funcs)
    loads = {0x20: 1, 0x24: 1, 0x21: 2, 0x25: 2, 0x23: 4, 0x22: 4, 0x26: 4, 0x32: 4}
    stores = {0x28: 1, 0x29: 2, 0x2B: 4, 0x2A: 4, 0x2E: 4, 0x3A: 4}
    refs, regs, idx, prev_f, prev = [], {28: GP}, set(), None, None
    for a in sorted(words):
        i = bisect.bisect_right(funcs, a) - 1
        fn = funcs[i] if i >= 0 else None
        if fn != prev_f or prev is None or a != prev + 4 or a in fset:
            regs, idx = {28: GP}, set()
        prev_f, prev = fn, a
        w = words[a]
        op, rs, rt, imm = w >> 26, (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
        if op == 0x0F:                                   # lui
            regs[rt] = imm << 16; idx.discard(rt); continue
        if op in (0x09, 0x0D) and rs == 0 and rt:         # li: a small constant (e.g. ori v0,zero,0x8000)
            regs[rt] = imm if op == 0x0D else s16(imm) & 0xFFFFFFFF; idx.discard(rt); continue
        if op in (0x08, 0x09, 0x0D) and rs in regs:      # addi(u) / ori on a known base
            v = (regs[rs] | imm) if op == 0x0D else (regs[rs] + s16(imm)) & 0xFFFFFFFF
            if 0x80000000 <= v < 0x80200000 and rs not in idx:
                refs.append((v, fn, a, "P", 0))
            if rt:
                regs[rt] = v
                (idx.add if rs in idx else idx.discard)(rt)
            continue
        if op in loads or op in stores:
            kind, size = ("R", loads[op]) if op in loads else ("W", stores[op])
            if rs in regs and in_ram((regs[rs] + s16(imm)) & 0xFFFFFFFF):
                v = (regs[rs] + s16(imm)) & 0xFFFFFFFF
                refs.append((v, fn, a, kind + ("i" if rs in idx else ""), size))
            if op in loads:
                regs.pop(rt, None)
            continue
        if op == 0:
            rd, f = (w >> 11) & 31, w & 63
            if f in (0x20, 0x21) and rd and rs in regs and rt in regs and rs and rt:
                regs[rd] = (regs[rs] + regs[rt]) & 0xFFFFFFFF          # base + constant offset
                # a register constant may be a loop counter, so keep the result marked "indexed"
                idx.add(rd); continue
            if f in (0x20, 0x21) and rd and ((rs in regs) != (rt in regs)):
                regs[rd] = regs[rs] if rs in regs else regs[rt]; idx.add(rd); continue
            regs.pop(rd, None)
        elif op in (0x0A, 0x0B, 0x0C, 0x0E, 0x08, 0x09, 0x0D):
            regs.pop(rt, None)
        elif op == 3:                                    # jal: caller-saved registers die
            for r in [r for r in regs if r not in (16, 17, 18, 19, 20, 21, 22, 23, 28, 29, 30)]:
                del regs[r]
    refs = [r for r in refs if not (r[3] == "P" and r[0] in fset)]   # drop pointers to code
    return refs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--header")
    ap.add_argument("--disc", default="MoA-US", choices=sorted(DISCS))
    ap.add_argument("--exe", help="the disc's boot exe (SLUS_010.30 / SLUS_006.70) from your own disc, scanned "
                    "when there is no generated code; refs are cached in docs/research/ram-xref-exe*.json")
    ap.add_argument("--generated", help="folder with that disc's generated exe code (default: generated/ for "
                    "MoA-US, generated-pp/ for PP; never committed)")
    ap.add_argument("--modules", help="folder with FDAT entries as NNN.bin (from your own disc, e.g. "
                    "tools/extract_fdat.py output or the disc kit's unpacked/GG/COM/FDAT); the "
                    "address lists are cached in docs/research/ram-xref-modules.json")
    args = ap.parse_args()
    use_disc(args.disc, args.generated)
    data = tomllib.load(open(TOML, "rb"))
    vars_ = sorted((v for v in data["var"] if v.get("disc", "MoA-US") == DISC), key=lambda v: v["addr"])
    names = [v["name"] for v in vars_]
    dup = [n for n, c in collections.Counter(names).items() if c > 1]
    if dup:
        sys.exit("duplicate names: %s" % dup)

    words, funcs = exe_words()
    if not words and args.exe:
        words, funcs = module_words(args.exe, 0x80011000, 0x800)
    exe_cache = XREF_CACHE.replace("modules", "exe")
    if words:
        refs = scan(words, funcs)
        json.dump(sorted([["%08X" % r[0], "%08X" % r[1] if r[1] else "", r[3], r[4]] for r in refs]),
                  open(exe_cache, "w"), separators=(",", ":"))
    elif os.path.exists(exe_cache):
        refs = [(int(a, 16), int(f, 16) if f else None, 0, k, n) for a, f, k, n in json.load(open(exe_cache))]
    else:
        refs = []
    by_addr = collections.defaultdict(list)
    for r in refs:
        by_addr[r[0]].append(r)

    # module scans: fresh from --modules, else from the committed cache
    mods = {}
    if args.modules:
        for k, label in MODULES.items():
            p = next((q for q in (os.path.join(args.modules, "%d.bin" % k),
                                  os.path.join(args.modules, "%03d.bin" % k)) if os.path.exists(q)), None)
            if p is None:
                hits = glob.glob(os.path.join(args.modules, "fdat_%03d_*.bin" % k))
                p = hits[0] if hits else None
            if p is None:
                continue
            mw, mf = module_words(p)
            mods[label] = scan(mw, mf)
        # cache: engine -> address -> {kind: [routines]}; addresses only, no game code
        cache = {}
        for lab, rr in mods.items():
            d = collections.defaultdict(lambda: collections.defaultdict(set))
            for r in rr:
                d["%08X" % r[0]][r[3][0]].add("%08X" % r[1])
            cache[lab] = {a: {k: sorted(v) for k, v in sorted(kv.items())} for a, kv in sorted(d.items())}
        json.dump(cache, open(XREF_CACHE, "w"), separators=(",", ":"), sort_keys=True)
        print("cached module refs:", {k: len(v) for k, v in cache.items()})
    if os.path.exists(XREF_CACHE):
        mods = {}
        for lab, d in json.load(open(XREF_CACHE)).items():
            mods[lab] = [(int(a, 16), int(f, 16), 0, k, 0) for a, kv in d.items() for k, fs in kv.items() for f in fs]
    mod_by = {lab: collections.defaultdict(list) for lab in mods}
    for lab, rr in mods.items():
        for r in rr:
            mod_by[lab][r[0]].append(r)

    def span(v):
        return v["addr"], v["addr"] + max(v.get("size", 0), 1)

    def xref(v):
        lo, hi = span(v)
        rr = [r for a in by_addr if lo <= a < hi for r in by_addr[a]
              if v.get("owner") == "lib" or not r[1] or r[1] < LIB_CODE or r[3][-1:] != "i"]
        if not rr:
            return "—"
        c = collections.Counter(r[3][0] for r in rr)
        fs = sorted({r[1] for r in rr if r[1]})
        parts = ["%s%d" % (k, c[k]) for k in "RWP" if c[k]]
        show = ", ".join("`%08X`" % f for f in fs[:4]) + (" …" if len(fs) > 4 else "")
        return "%s in %d fn: %s" % (" ".join(parts), len(fs), show)

    def engines_for(v):
        if v["addr"] < MODULE_BASE:
            return list(mods)
        m = re.match(r"\[([^\]]+)\]", v["what"])
        want = [x.strip() for x in m.group(1).replace("/", ",").split(",")] if m else []
        alias = {"missions": "missions", "menu": "menu", "VS": "VS", "arena": "arena", "link": "link"}
        return [alias[x] for x in want if x in alias and alias[x] in mods]

    def mxref(v):
        bits = []
        labs = engines_for(v) + [k for k in OTHER_ENGINES if k in v and k in mods and k not in engines_for(v)]
        for lab in labs:
            lo, hi = span(v)
            if lab in v and v["addr"] >= MODULE_BASE:
                lo, hi = v[lab], v[lab] + (hi - lo)
            rr = [r for a, l in mod_by[lab].items() if lo <= a < hi for r in l]
            if rr:
                kinds = "".join(k for k in "RWP" if any(r[3] == k for r in rr))
                nf = len({r[1] for r in rr})
                bits.append("%s %s %d" % (lab, kinds, nf))
        return "; ".join(bits) or "—"

    def what_text(v):
        other = ["%s `%08X`" % (k, v[k]) for k in OTHER_ENGINES if k in v]
        return v["what"] + (" — **Other engines:** " + ", ".join(other) if other else "")

    out = []
    w = out.append
    w("# %s\n" % DISCS[DISC]["title"])
    w("Generated by `tools/ram_map.py` from `docs/research/ram-map.toml`. **Edit the TOML, not this file.**\n")
    w("One place for every named RAM address. The details stay in the lane docs named in *Source*;")
    w("this map only says what each address is, how sure we are, and where to read more.\n")
    w("**Status:** `tested` = checked in a running game · `code` = read from the game's code ·")
    w("`cheat` = from cheat codes only · `guess` = needs a check. **Exe refs:** reads (R), writes (W) and")
    w("address-takes (P) found by a static scan of the boot exe's own code (`generated/`, or the exe file with `--exe`), with the")
    w("routines. **Module refs:** the same scan over the disc-loaded engines (%s):" % ", ".join(
        "%s FDAT %d" % (lab, k) for k, lab in MODULES.items()))
    w("engine, access kinds, number of routines. For module-area")
    w("entries only the engine named in [brackets] is shown, since other engines put other data there;")
    w("where a row lists **Other engines**, the same thing's address there (routines paired by code), and")
    w("its refs are counted at that address.\n")
    if DISC == "MoA-US":
        w("**Version note:** these are v1.0 addresses. The cheat lists' v1.1 set is shifted (+0x64 save data,")
        w("+0x60 fight data); the Japanese disc (SLPS-01855) needs its own pass. See `disc-versions.md`.\n")
    else:
        w("**PP rows:** most were carried over from the MoA-US rows by `tools/xdisc/ram_xlate.py` (cross-disc")
        w("table `xdisc/globals.tsv`): the address is Phantasma's, the meaning is Master of Arena's until checked on")
        w("PP. *Source* says how each address was found (exact / inside / between / near). See")
        w("`mapping/foundations.md`.\n")
    w("## Memory layout\n")
    w("| From | To | What |\n|---|---|---|")
    for lo, hi, t in REGIONS:
        w("| `%08X` | `%08X` | %s |" % (lo, hi - 1, t))
    w("")
    w("Inside the exe: code `80011000`–`%08X` is the game's own, `%08X`–`%08X` is PsyQ library" % (
        LIB_CODE - 1, LIB_CODE, GP - 1))
    w("code and data (libcd, libspu/libsnd, libgte, libetc/libapi, libgpu, libpad, libc). Library state")
    w("is marked *lib*: read it if useful, never write it.\n")
    w("The scratchpad is used only by the fight engines (VS, arena, link, missions, replay share one")
    w("layout); the menu and the exe's game code never touch it, and no value in it outlives the routine")
    w("that wrote it. Details and the stretches no code was seen using: `mapping/scratchpad.md`.\n")
    for lo, hi, title in REGIONS:
        rows = [v for v in vars_ if lo <= v["addr"] < hi]
        if not rows:
            continue
        w("## %s\n" % title)
        w("| Address | Size | Type | Name | What | Status | Exe refs | Module refs | Source |")
        w("|---|---|---|---|---|---|---|---|---|")
        for v in rows:
            name = v["name"] + (" *(lib)*" if v.get("owner") == "lib" else "")
            size = str(v.get("size", "")) if v.get("size") else "—"
            w("| `%08X` | %s | %s | `%s` | %s | %s | %s | %s | %s |" % (
                v["addr"], size, v["type"], name, what_text(v).replace("|", "/"),
                v["status"], xref(v), mxref(v), v["src"]))
        w("")

    # unnamed, game-owned addresses the exe touches
    named = [span(v) for v in vars_]
    lib_spans = [span(v) for v in vars_ if v.get("owner") == "lib"]
    def inside(a, spans):
        return any(lo <= a < hi for lo, hi in spans)
    loose = collections.defaultdict(list)
    for a, rr in by_addr.items():
        if not (0x80011000 <= a < PERSIST_END) or inside(a, named):
            continue
        game = [r for r in rr if r[1] and r[1] < LIB_CODE]
        if game:
            loose[a] = game
    w("## Touched by the exe's game code but not named yet\n")
    w("Addresses in the persistent area that the exe's own (non-library) code reads or writes and that")
    w("no entry above covers. Good first targets. (`P` below `80011D84` = a message string the code prints.)\n")
    if loose:
        w("| Address | Access | Routines |\n|---|---|---|")
        for a in sorted(loose):
            rr = loose[a]
            acc = " ".join(sorted({"%s%d" % (r[3], r[4]) if r[4] else r[3] for r in rr}))
            fs = sorted({r[1] for r in rr})
            w("| `%08X` | %s | %s |" % (a, acc, ", ".join("`%08X`" % f for f in fs)))
    else:
        w("None.")
    w("")
    if mods:
        hot = collections.defaultdict(lambda: collections.defaultdict(set))
        for lab, d in mod_by.items():
            for a, rr in d.items():
                if GP <= a < PERSIST_END and not inside(a, named) and not inside(a, lib_spans):
                    for r in rr:
                        hot[a][lab].add(r[1])
        ranked = sorted(hot, key=lambda a: -sum(len(x) for x in hot[a].values()))
        w("## Persistent addresses the modules use but nobody has named\n")
        w("From the module scan: bss addresses (gp area up to `%08X`) that the menus or fight" % (PERSIST_END - 1))
        w("engines read or write, not covered by any entry. Sorted by how many routines use them;")
        w("%d left (top 60 shown). These are the best next targets.\n" % len(ranked))
        w("| Address | Used by (engine: routines) |\n|---|---|")
        for a in ranked[:60]:
            w("| `%08X` | %s |" % (a, "; ".join("%s %d" % (lab, len(f)) for lab, f in sorted(hot[a].items()))))
        w("")
    w("## Next\n")
    w("- **Module scan:** *Module refs* come from `--modules` (FDAT 206–211 from your own disc, see")
    w("  `tools/extract_fdat.py`), cached in `ram-xref-modules.json` so the doc rebuilds without the")
    w("  disc. Module routine starts are guessed (jal targets, after `jr ra`), and data inside a")
    w("  module can read as code, so treat single hits with care.")
    w("- **Check the `guess` and `cheat` rows** in a running game (recomp debug tools or DuckStation).")
    w("- **Japanese disc:** once SLPS-01855 is generated, add a `jp` address per row instead of a second file.")
    w("- **Adding an entry:** append a `[[var]]` to the TOML, run `python3 tools/ram_map.py`, commit both.\n")
    st = collections.Counter(v["status"] for v in vars_)
    w("## Counts\n")
    w("%d named entries (%s). Exe scan: %d references to %d addresses." % (
        len(vars_), ", ".join("%s %d" % (k, st[k]) for k in ("tested", "code", "cheat", "guess") if st[k]),
        len(refs), len(by_addr)))
    open(OUT, "w", encoding="utf-8").write("\n".join(out) + "\n")
    print("wrote", os.path.relpath(OUT, ROOT), len(vars_), "entries,", len(loose), "unnamed")

    if args.header:
        h = ["/* Generated by tools/ram_map.py from docs/research/ram-map.toml. Do not edit. */",
             "/* %s. Module-area addresses hold only while that engine is loaded. */" % DISCS[DISC]["title"],
             "#pragma once", ""]
        for v in vars_:
            h.append("#define " + DISCS[DISC]["prefix"] + "%-28s 0x%08Xu  /* %s */" % (
                v["name"].upper(), v["addr"], v["what"][:70].replace("*/", "")))
            for k in OTHER_ENGINES:
                if k in v:
                    h.append("#define " + DISCS[DISC]["prefix"] + "%-28s 0x%08Xu" % ((v["name"] + "_" + k).upper(), v[k]))
        open(args.header, "w").write("\n".join(h) + "\n")
        print("wrote", args.header)


if __name__ == "__main__":
    main()

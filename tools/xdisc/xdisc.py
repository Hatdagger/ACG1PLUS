#!/usr/bin/env python3
"""Cross-disc address table: Master of Arena USA (MoA-US) <-> Project Phantasma USA (PP).

Reads the two games' code from the work-pack folders (never from this repo: no game code is kept
here), pairs routines with mipscmp.py, then lines up the paired routines instruction by instruction
and collects what the paired instructions point at:

  functions.tsv  every routine on both sides, its partner, how alike, a confidence, and whether the
                 "routine" is really a data table (mipscmp cuts after each `jr ra`, so tables between
                 routines show up as fake routines)
  globals.tsv    RAM addresses the code uses (lui/addiu pairs, gp-relative, jal targets), MoA -> PP,
                 with how many paired instructions agree
  runs.tsv       the same globals squeezed into runs that moved by one fixed amount
  fields.tsv     struct field offsets that differ between paired loads/stores (MoA +x -> PP +y)

Output is addresses and counts only (no instruction words), so it is safe to commit.

  python3 -I tools/xdisc/xdisc.py --moa WORK/ma-disc --pp WORK/pp-disc --out docs/research/xdisc

Static only. A pair is only as good as the routine pairing under it; see the confidence column.
"""
import argparse, collections, difflib, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mipscmp  # noqa: E402

EXE_BASE = 0x80011000
GP = {"MoA-US": 0x8003EA28, "PP": 0x8003BC7C}
MOD_BASE = {"MoA-US": 0x80050DD8, "PP": 0x800490D0}
# (MoA FDAT entry, PP FDAT entry, name). MoA names from mode-launch.md; PP paired by code.
MODULES = [(206, 201, "menu"), (207, 202, "missions"), (208, 203, "link"),
           (209, 204, "VS"), (210, 205, "arena"), (211, 206, "VS replay")]

LOADS = {0x20: 1, 0x21: 2, 0x23: 4, 0x24: 1, 0x25: 2, 0x22: 4, 0x26: 4, 0x32: 4}
STORES = {0x28: 1, 0x29: 2, 0x2B: 4, 0x2A: 4, 0x2E: 4, 0x3A: 4}
MEMOPS = {**LOADS, **STORES}
# opcodes the R3000A never runs (used to spot data tables cut out as "routines")
BAD_OPS = {0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x27,
           0x2C, 0x2D, 0x2F, 0x30, 0x31, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3B, 0x3C,
           0x3D, 0x3E, 0x3F}
BAD_SPECIAL = {1, 5, 0x0A, 0x0B, 0x0E, 0x14, 0x15, 0x16, 0x17, 0x1C, 0x1D, 0x1E, 0x1F, 0x28,
               0x29, 0x2C, 0x2D, 0x2E, 0x2F} | set(range(0x30, 0x40))


# Offset pairs that identify a pointer as one of the known structs (from claude/phantasma-base.md and the
# first runs of this tool). Every access through the same register value in a paired routine that also
# uses a marker is counted for that struct.
STRUCT_MARKERS = {
    "ac_object": {(0xD4, 0xCC), (0x45, 0x3C), (0xD8, 0xD0), (0x164, 0x15C), (0x4A, 0x41)},
    "player_block": {(0x20C, 0x214), (0x398, 0x3A0), (0x298, 0x2A0), (0x218, 0x220), (0x20E, 0x216)},
}


def s16(x):
    return x - 0x10000 if x & 0x8000 else x


def read_words(path, skip=0):
    b = open(path, "rb").read()[skip:]
    return list(struct.unpack("<%dI" % (len(b) // 4), b[:len(b) // 4 * 4]))


def data_score(ws):
    """share of words that cannot be code or look like pointers/small numbers (0 = clean code)"""
    if not ws:
        return 0.0
    bad = 0
    for w in ws:
        op = w >> 26
        if 0x80010000 <= w < 0x80200000 or w < 0x10000 and w != 0:
            bad += 1
        elif op in BAD_OPS or (op == 0 and (w & 0x3F) in BAD_SPECIAL):
            bad += 1
    return bad / len(ws)


def refs(ws, base, gp):
    """Per instruction index: list of (kind, address). kinds: hi/lo pair 'abs', 'gp', 'jal'.
    Also returns field accesses: index -> (opcode, offset) for loads/stores off a plain register."""
    out, fields = {}, {}
    hi = {}  # reg -> value from lui (or lui+addiu: full address)
    prev_dest = None
    gen = [0] * 32  # bumped whenever a register is written: same (reg, gen) = same pointer value
    for i, w in enumerate(ws):
        op = w >> 26; rs = (w >> 21) & 31; rt = (w >> 16) & 31; rd = (w >> 11) & 31
        imm = w & 0xFFFF
        dest = rd if op == 0 else rt if op in (8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF) or op in LOADS else \
            31 if op == 3 else None
        if prev_dest is not None:
            gen[prev_dest] += 1
        prev_dest = dest
        if w == 0x03E00008 or op == 3:
            for r in range(32):
                gen[r] += 1 if (w == 0x03E00008 or r not in range(16, 24)) else 0
        if w == 0x03E00008:      # jr ra: new routine soon
            hi = {}
        if op == 3:
            out.setdefault(i, []).append(("jal", 0x80000000 | ((w & 0x3FFFFFF) << 2)))
            continue
        if op == 0xF:
            hi[rt] = imm << 16
            continue
        if op in (9, 0xD) and rs in hi:
            a = (hi[rs] + s16(imm)) & 0xFFFFFFFF if op == 9 else hi[rs] | imm
            if 0x80000000 <= a < 0x80200000 or 0x1F800000 <= a < 0x1F801000:
                out.setdefault(i, []).append(("abs", a))
            hi[rt] = a
            continue
        if op in MEMOPS:
            if rs == 28:
                out.setdefault(i, []).append(("gp", (gp + s16(imm)) & 0xFFFFFFFF))
            elif rs in hi:
                a = (hi[rs] + s16(imm)) & 0xFFFFFFFF
                if 0x80000000 <= a < 0x80200000 or 0x1F800000 <= a < 0x1F801000:
                    out.setdefault(i, []).append(("abs", a))
            elif rs != 29:
                fields[i] = (op, s16(imm), rs, gen[rs])
            if op in LOADS:
                hi.pop(rt, None)
            continue
        if op == 9 and rs == 28:
            out.setdefault(i, []).append(("gp", (gp + s16(imm)) & 0xFFFFFFFF))
        # anything else writing a register drops what we knew about it
        if op == 0:
            hi.pop(rd, None)
        elif op in (8, 9, 0xA, 0xB, 0xC, 0xD, 0xE):
            hi.pop(rt, None)
        elif op == 0x12 and rs in (0, 2):  # mfc2 / cfc2
            hi.pop(rt, None)
    return out, fields


def module_list(moa, pp):
    exe = [("SLUS_010.30", read_words(os.path.join(moa, "SLUS_010.30"), 0x800), EXE_BASE,
            "SLUS_006.70", read_words(os.path.join(pp, "SLUS_006.70"), 0x800), EXE_BASE, "boot exe")]
    for ma, pa, name in MODULES:
        exe.append(("FDAT %d" % ma,
                    read_words(os.path.join(moa, "unpacked/GG/COM/FDAT/%03d.bin" % ma)),
                    MOD_BASE["MoA-US"],
                    "FDAT %d" % pa,
                    read_words(os.path.join(pp, "unpacked/GG/COM/FDAT/%03d.bin" % pa)),
                    MOD_BASE["PP"], name))
    return exe


def align(A, a, B, b):
    """instruction index pairs (absolute indices) for two paired routines"""
    la, lb = a[1] - a[0], b[1] - b[0]
    if la == lb and mipscmp.key(A[a[0]:a[1]], False) == mipscmp.key(B[b[0]:b[1]], False):
        return [(a[0] + k, b[0] + k) for k in range(la)]
    na = mipscmp.norm(A[a[0]:a[1]], False); nb = mipscmp.norm(B[b[0]:b[1]], False)
    sm = difflib.SequenceMatcher(None, na, nb, autojunk=False)
    out = []
    for i, j, n in sm.get_matching_blocks():
        out += [(a[0] + i + k, b[0] + j + k) for k in range(n)]
    return out


def confidence(kind, ratio, calls_ok, calls_all, dscore):
    if dscore > 0.35:
        return "data"
    if kind == "same":
        base = 3
    elif kind == "values":
        base = 3
    elif kind == "changed":
        base = 2 if ratio >= 0.85 else 1
    else:
        return "-"
    if calls_all >= 2 and calls_ok / calls_all < 0.5:
        base -= 1
    return {3: "high", 2: "medium", 1: "low", 0: "low"}[max(base, 0)]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--moa", required=True, help="ma-disc folder (setup.py already run)")
    ap.add_argument("--pp", required=True, help="pp-disc folder (setup.py already run)")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    func_rows, glob_votes, field_votes, call_votes = [], collections.defaultdict(collections.Counter), \
        collections.Counter(), collections.defaultdict(collections.Counter)
    field_where = collections.defaultdict(collections.Counter)
    struct_votes = {k: collections.Counter() for k in STRUCT_MARKERS}
    struct_where = {k: collections.defaultdict(set) for k in STRUCT_MARKERS}
    pending = []
    for ma_name, A, abase, pp_name, B, bbase, what in module_list(args.moa, args.pp):
        print("comparing", ma_name, "<->", pp_name, what, file=sys.stderr)
        rows = mipscmp.compare(A, B)
        ra, fa = refs(A, abase, GP["MoA-US"])
        rb, fb = refs(B, bbase, GP["PP"])
        for kind, a, b, ratio in rows:
            ent = {"moa_mod": ma_name, "pp_mod": pp_name, "what": what, "kind": kind, "ratio": ratio,
                   "a": a, "b": b, "abase": abase, "bbase": bbase,
                   "ds": max(data_score(A[a[0]:a[1]]) if a else 0, data_score(B[b[0]:b[1]]) if b else 0),
                   "calls_ok": 0, "calls_all": 0}
            if a and b:
                pa = abase + a[0] * 4; pb = bbase + b[0] * 4
                groups = collections.defaultdict(list)
                for i, j in align(A, a, B, b):
                    x = ra.get(i, []); y = rb.get(j, [])
                    for (k1, t1), (k2, t2) in zip(x, y):
                        if k1 == k2:
                            if k1 == "jal":
                                call_votes[t1][t2] += 1
                                ent["calls_all"] += 1
                                ent["_calls"] = ent.get("_calls", []) + [(t1, t2)]
                            else:
                                glob_votes[t1][(t2, ma_name, pp_name)] += 1
                    if i in fa and j in fb and fa[i][0] == fb[j][0]:
                        oa, ob = fa[i][1], fb[j][1]
                        field_votes[(oa, ob)] += 1
                        field_where[(oa, ob)]["%s %08X" % (pp_name, pb)] += 1
                        groups[fa[i][2:]].append((oa, ob, MEMOPS[fa[i][0]]))
                for g in groups.values():
                    pairs_g = {(x, y) for x, y, _ in g}
                    for sname, marks in STRUCT_MARKERS.items():
                        if len(pairs_g & marks) >= 1 and all(x == y or (x, y) in pairs_g for x, y in marks & pairs_g):
                            for x, y, sz in g:
                                struct_votes[sname][(x, y)] += 1
                                struct_where[sname][(x, y)].add("%s %08X" % (pp_name, pb))
            func_rows.append(ent)

    # routine pairs as the calls see them: every aligned jal pair votes for its callee pair
    start_pair = {}
    for e in func_rows:
        if e["a"] and e["b"]:
            start_pair[e["abase"] + e["a"][0] * 4] = e["bbase"] + e["b"][0] * 4
    for e in func_rows:
        for t1, t2 in e.pop("_calls", []):
            if start_pair.get(t1) == t2:
                e["calls_ok"] += 1

    # calls into routines the code pairing missed ("MoA only") but whose call sites line up
    by_call = {}
    for t1, c in call_votes.items():
        t2, n = c.most_common(1)[0]
        if t1 not in start_pair and n >= 2 and n / sum(c.values()) >= 0.8:
            by_call[t1] = (t2, n)

    with open(os.path.join(args.out, "functions.tsv"), "w") as f:
        f.write("# MoA-US routine -> PP routine (tools/xdisc/xdisc.py). Static. kind: same = identical, values = same\n"
                "# shape (numbers/field offsets differ), changed = logic differs (ratio = how alike), only_moa / only_pp.\n"
                "# conf: high / medium / low; 'data' = this cut is a data table, not a routine. calls = paired call\n"
                "# sites inside the routine whose callees also pair (ok/all). via_calls = MoA-only routine whose\n"
                "# callers line up, giving its likely PP twin.\n")
        f.write("moa_disc\tmoa_module\tmoa_addr\tmoa_bytes\tpp_disc\tpp_module\tpp_addr\tpp_bytes\twhat\tkind\tratio\tcalls\tconf\n")
        kinds = {"same": "same", "values": "values", "changed": "changed", "only_a": "only_moa", "only_b": "only_pp"}
        for e in func_rows:
            a, b = e["a"], e["b"]
            aa = "%08X" % (e["abase"] + a[0] * 4) if a else ""
            bb = "%08X" % (e["bbase"] + b[0] * 4) if b else ""
            kind = kinds[e["kind"]]
            conf = confidence(e["kind"], e["ratio"], e["calls_ok"], e["calls_all"], e["ds"])
            if not b and a and (e["abase"] + a[0] * 4) in by_call:
                t2, n = by_call[e["abase"] + a[0] * 4]
                bb, kind, conf = "%08X" % t2, "via_calls", "low" if n < 3 else "medium"
            f.write("\t".join([
                "MoA-US", e["moa_mod"], aa, str((a[1] - a[0]) * 4) if a else "",
                "PP", e["pp_mod"], bb, str((b[1] - b[0]) * 4) if b else "",
                e["what"], kind, "%.3f" % e["ratio"] if a and b else "",
                "%d/%d" % (e["calls_ok"], e["calls_all"]) if e["calls_all"] else "", conf]) + "\n")

    # globals
    glines = []
    for t1, c in glob_votes.items():
        best = collections.Counter()
        mods = collections.defaultdict(set)
        for (t2, mm, pm), n in c.items():
            best[t2] += n; mods[t2].add((mm, pm))
        t2, n = best.most_common(1)[0]
        tot = sum(best.values())
        mm = ",".join(sorted({m for m, _ in mods[t2]})); pm = ",".join(sorted({p for _, p in mods[t2]}))
        agree = n / tot
        conf = "high" if n >= 3 and agree >= 0.9 else "medium" if n >= 2 and agree >= 0.75 else "low"
        alts = ";".join("%08X" % k for k, _ in best.most_common(3)[1:])
        glines.append((t1, t2, mm, pm, n, tot, conf, alts))
    glines.sort()
    with open(os.path.join(args.out, "globals.tsv"), "w") as f:
        f.write("# RAM addresses used by paired code, MoA-US -> PP (tools/xdisc/xdisc.py). votes = paired instructions\n"
                "# that give this PP address, seen = all paired instructions using the MoA address. module = the code\n"
                "# that uses it (an address in the module area 0x80050DD8+/0x800490D0+ only holds while that module is\n"
                "# loaded). Addresses point at the start of what the code names (a table, a record or a field).\n")
        f.write("moa_disc\tmoa_addr\tpp_disc\tpp_addr\tdelta\tseen_in_moa\tseen_in_pp\tvotes\tseen\tconf\tother_pp\n")
        for t1, t2, mm, pm, n, tot, conf, alts in glines:
            f.write("MoA-US\t%08X\tPP\t%08X\t%+d\t%s\t%s\t%d\t%d\t%s\t%s\n" % (t1, t2, t2 - t1, mm, pm, n, tot, conf, alts))

    # runs: consecutive (by MoA address) high/medium pairs with the same delta and module
    with open(os.path.join(args.out, "runs.tsv"), "w") as f:
        f.write("# Runs of globals that moved by one fixed amount (high/medium pairs only). Inside a run, a MoA-US\n"
                "# address between first and last most likely sits at MoA + delta on PP; outside runs, check globals.tsv.\n")
        f.write("moa_first\tmoa_last\tdelta\tpp_first\tpp_last\tpairs\tmodule_moa\tmodule_pp\n")
        cur = None
        for t1, t2, mm, pm, n, tot, conf, alts in glines:
            if conf == "low":
                continue
            d = t2 - t1
            region = "scratch" if t1 < 0x80000000 else "exe" if t1 < MOD_BASE["MoA-US"] else mm
            if cur and cur[2] == d and cur[6] == region and t1 - cur[1] < 0x2000:
                cur[1] = t1; cur[3] += 1
            else:
                if cur:
                    f.write("%08X\t%08X\t%+d\t%08X\t%08X\t%d\t%s\t%s\n" % (cur[0], cur[1], cur[2], cur[0] + cur[2], cur[1] + cur[2], cur[3], cur[4], cur[5]))
                cur = [t1, t1, d, 1, mm if region not in ("exe", "scratch") else "(exe area: every module)",
                       pm if region not in ("exe", "scratch") else "", region]
        if cur:
            f.write("%08X\t%08X\t%+d\t%08X\t%08X\t%d\t%s\t%s\n" % (cur[0], cur[1], cur[2], cur[0] + cur[2], cur[1] + cur[2], cur[3], cur[4], cur[5]))

    with open(os.path.join(args.out, "fields.tsv"), "w") as f:
        f.write("# Struct field offsets that differ between paired loads/stores, MoA-US +x -> PP +y (all structs mixed:\n"
                "# check the routines listed). Only pairs seen 2+ times and shifts up to 0x80. Same offsets are not listed.\n"
                "# For the AC object and the player block see struct-*.tsv, which keeps those two apart.\n")
        f.write("moa_off\tpp_off\tshift\tcount\ttop_pp_routines\n")
        for (oa, ob), n in sorted(field_votes.items(), key=lambda kv: (kv[0][0], -kv[1])):
            if oa == ob or n < 2 or abs(ob - oa) > 0x80:
                continue
            where = ",".join(k for k, _ in field_where[(oa, ob)].most_common(3))
            f.write("%+#x\t%+#x\t%+d\t%d\t%s\n" % (oa, ob, ob - oa, n, where))
    for sname in STRUCT_MARKERS:
        with open(os.path.join(args.out, "struct-%s.tsv" % sname.replace("_", "-")), "w") as f:
            f.write("# %s field offsets, MoA-US -> PP, from accesses through a pointer that also touches a marker field\n"
                    "# (tools/xdisc/xdisc.py STRUCT_MARKERS). share = this PP offset's share of all PP offsets seen for the\n"
                    "# MoA offset; below 0.8 the register was probably reused for another struct, check the routines.\n" % sname)
            f.write("moa_off\tpp_off\tshift\tcount\tshare\troutines\tsample_pp_routines\n")
            per = collections.defaultdict(collections.Counter)
            for (x, y), n in struct_votes[sname].items():
                per[x][y] += n
            for x in sorted(per):
                if x < 0:
                    continue
                tot = sum(per[x].values())
                y, n = per[x].most_common(1)[0]
                where = sorted(struct_where[sname][(x, y)])
                f.write("%+#x\t%+#x\t%+d\t%d\t%.2f\t%d\t%s\n" % (x, y, y - x, n, n / tot, len(where), ",".join(where[:3])))
    print("done", file=sys.stderr)


if __name__ == "__main__":
    main()

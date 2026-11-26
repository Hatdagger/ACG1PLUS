#!/usr/bin/env python3
"""Cross-disc address table: Armored Core 1 USA (AC1, SCUS-94182) <-> Project Phantasma USA (PP).

Same idea and same output columns as xdisc.py (MoA-US <-> PP), with one change: AC1 was built with an older
compiler setup than PP, so the same routine often uses other registers and a slightly different instruction
order. The pairing here is therefore **register-blind**: an instruction is compared by its operation (and its
small constants), not by which registers it uses. Everything else (globals votes, runs, struct fields) is
collected the same way as in xdisc.py, whose helpers are imported, not copied.

Reads the code from the work-pack folders (never from this repo: no game code is kept here):

  python3 -I tools/xdisc/ac1_xdisc.py --ac1 WORK/ac1-disc --pp WORK/pp-disc --out docs/research/xdisc/ac1-pp

Output is addresses and counts only (no instruction words), so it is safe to commit. Static only.
"""
import argparse, collections, difflib, hashlib, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mipscmp  # noqa: E402
import xdisc  # noqa: E402  (refs, data_score, read_words, MEMOPS, s16)

EXE_BASE = 0x80011000
# AC1 values from code (docs/research/ac1/foundations.md): gp set at 0x80011EEC, module base by jal voting.
GP = {"AC1": 0x80039C5C, "PP": 0x8003BC7C}
MOD_BASE = {"AC1": 0x8004ADA0, "PP": 0x800490D0}
EXE = {"AC1": "SCUS_941.82", "PP": "SLUS_006.70"}
# (AC1 FDAT entry, PP FDAT entry, name). Paired by code likeness (foundations.md §3). AC1 has no arena
# engine and no VS replay, so PP 205 and 206 have no AC1 twin.
MODULES = [(201, 201, "menu"), (202, 202, "missions"), (203, 203, "link"), (204, 204, "VS")]

# PP-side offsets that mark a pointer as the AC object / player block (known PP fields, foundations.md §5).
# A register group counts for the struct when its PP side touches at least two of these.
PP_MARKERS = {
    "ac_object": {0xCC, 0x3C, 0xD0, 0x15C, 0x41, 0x160},
    "player_block": {0x214, 0x3A0, 0x2A0, 0x220, 0x216, 0x3A4},
}

ITYPE_KEEP_IMM = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E}  # slti sltiu andi ori xori: constants are logic, keep them


def token(w):
    """one instruction, registers dropped"""
    op = w >> 26
    if op == 0:
        f = w & 0x3F
        return (0, f, (w >> 6) & 31 if f in (0, 2, 3) else 0)
    if op == 1:
        return (1, (w >> 16) & 31)
    if op in (2, 3):
        return (op,)
    if op == 0x12:                      # GTE: commands keep their word, moves keep the kind
        return (0x12, w & 0x03FFFFFF) if w & 0x02000000 else (0x12, (w >> 21) & 31)
    if op in ITYPE_KEEP_IMM:
        return (op, w & 0xFFFF)
    if op in xdisc.MEMOPS and (w >> 21) & 31 == 29:
        return (op, "sp")               # stack slot: offsets differ with the frame, ignore
    return (op,)


def toks(seg):
    return [token(w) for w in seg]


def key(seg):
    return hashlib.md5(repr(toks(seg)).encode()).digest()


def compare(A, B):
    """like mipscmp.compare, register-blind. rows: (kind, a_range, b_range, ratio)"""
    fa, fb = mipscmp.funcs(A), mipscmp.funcs(B)
    ka = [key(A[s:e]) for s, e in fa]
    kb = [key(B[s:e]) for s, e in fb]
    sm = difflib.SequenceMatcher(None, ka, kb, autojunk=False)
    rows = []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            for d in range(i2 - i1):
                a, b = fa[i1 + d], fb[j1 + d]
                same = mipscmp.key(A[a[0]:a[1]], True) == mipscmp.key(B[b[0]:b[1]], True)
                rows.append(("same" if same else "values", a, b, 1.0))
            continue
        ua, ub = list(range(i1, i2)), list(range(j1, j2))
        la = [toks(A[fa[i][0]:fa[i][1]]) for i in ua]
        lb = [toks(B[fb[j][0]:fb[j][1]]) for j in ub]
        pairs, lastj = [], -1
        for x in range(len(ua)):
            best, bj = 0.0, None
            for y in range(lastj + 1, min(len(ub), lastj + 1 + 60)):
                if abs(len(la[x]) - len(lb[y])) > 0.6 * max(len(la[x]), len(lb[y]), 1):
                    continue
                s = difflib.SequenceMatcher(None, la[x], lb[y], autojunk=False)
                if s.real_quick_ratio() <= best or s.quick_ratio() <= best:
                    continue
                r = s.ratio()
                if r > best:
                    best, bj = r, y
            if bj is not None and best >= 0.5:
                pairs.append((x, bj, best)); lastj = bj
        pa = {x for x, _, _ in pairs}; pb = {y for _, y, _ in pairs}
        for x, y, r in pairs:
            rows.append(("changed", fa[ua[x]], fb[ub[y]], r))
        for x, i in enumerate(ua):
            if x not in pa:
                rows.append(("only_a", fa[i], None, 0.0))
        for y, j in enumerate(ub):
            if y not in pb:
                rows.append(("only_b", None, fb[j], 0.0))
    return rows


def align(A, a, B, b):
    ta, tb = toks(A[a[0]:a[1]]), toks(B[b[0]:b[1]])
    if ta == tb:
        return [(a[0] + k, b[0] + k) for k in range(len(ta))]
    sm = difflib.SequenceMatcher(None, ta, tb, autojunk=False)
    out = []
    for i, j, n in sm.get_matching_blocks():
        out += [(a[0] + i + k, b[0] + j + k) for k in range(n)]
    return out


def confidence(kind, ratio, calls_ok, calls_all, dscore):
    if dscore > 0.35:
        return "data"
    if kind in ("same", "values"):
        base = 3
    elif kind == "changed":
        base = 3 if ratio >= 0.95 else 2 if ratio >= 0.8 else 1
    else:
        return "-"
    if calls_all >= 2 and calls_ok / calls_all < 0.5:
        base -= 1
    elif calls_all >= 3 and calls_ok == calls_all and base == 2:
        base = 3
    return {3: "high", 2: "medium", 1: "low", 0: "low"}[max(base, 0)]


def module_list(ac1, pp):
    out = [("exe", xdisc.read_words(os.path.join(ac1, EXE["AC1"]), 0x800), EXE_BASE,
            "exe", xdisc.read_words(os.path.join(pp, EXE["PP"]), 0x800), EXE_BASE, "boot exe")]
    for ma, pa, name in MODULES:
        out.append(("FDAT %d" % ma, xdisc.read_words(os.path.join(ac1, "unpacked/GG/COM/FDAT/%03d.bin" % ma)),
                    MOD_BASE["AC1"],
                    "FDAT %d" % pa, xdisc.read_words(os.path.join(pp, "unpacked/GG/COM/FDAT/%03d.bin" % pa)),
                    MOD_BASE["PP"], name))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ac1", required=True, help="ac1-disc folder (setup.py already run)")
    ap.add_argument("--pp", required=True, help="pp-disc folder (setup.py already run)")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    func_rows = []
    glob_votes = collections.defaultdict(collections.Counter)
    call_votes = collections.defaultdict(collections.Counter)
    field_votes, field_where = collections.Counter(), collections.defaultdict(collections.Counter)
    struct_votes = {k: collections.Counter() for k in PP_MARKERS}
    struct_where = {k: collections.defaultdict(set) for k in PP_MARKERS}
    for an, A, abase, pn, B, bbase, what in module_list(args.ac1, args.pp):
        print("comparing AC1", an, "<-> PP", pn, what, file=sys.stderr)
        rows = compare(A, B)
        ra, fa = xdisc.refs(A, abase, GP["AC1"])
        rb, fb = xdisc.refs(B, bbase, GP["PP"])
        for kind, a, b, ratio in rows:
            ent = {"am": an, "pm": pn, "what": what, "kind": kind, "ratio": ratio, "a": a, "b": b,
                   "abase": abase, "bbase": bbase, "calls_ok": 0, "calls_all": 0, "_calls": [],
                   "ds": max(xdisc.data_score(A[a[0]:a[1]]) if a else 0, xdisc.data_score(B[b[0]:b[1]]) if b else 0)}
            if a and b:
                pb = bbase + b[0] * 4
                groups = collections.defaultdict(list)
                for i, j in align(A, a, B, b):
                    for (k1, t1), (k2, t2) in zip(ra.get(i, []), rb.get(j, [])):
                        if k1 != k2:
                            continue
                        if k1 == "jal":
                            call_votes[t1][t2] += 1
                            ent["calls_all"] += 1
                            ent["_calls"].append((t1, t2))
                        else:
                            glob_votes[t1][(t2, what)] += 1
                    if i in fa and j in fb and fa[i][0] == fb[j][0]:
                        oa, ob = fa[i][1], fb[j][1]
                        field_votes[(oa, ob)] += 1
                        field_where[(oa, ob)]["%s %08X" % (pn, pb)] += 1
                        groups[fb[j][2:]].append((oa, ob))
                for g in groups.values():
                    pps = {y for _, y in g}
                    for sname, marks in PP_MARKERS.items():
                        if len(pps & marks) >= 2:
                            for x, y in g:
                                struct_votes[sname][(x, y)] += 1
                                struct_where[sname][(x, y)].add("%s %08X" % (pn, pb))
            func_rows.append(ent)

    start_pair = {e["abase"] + e["a"][0] * 4: e["bbase"] + e["b"][0] * 4 for e in func_rows if e["a"] and e["b"]}
    for e in func_rows:
        for t1, t2 in e.pop("_calls"):
            if start_pair.get(t1) == t2:
                e["calls_ok"] += 1
    by_call = {}
    for t1, c in call_votes.items():
        t2, n = c.most_common(1)[0]
        if t1 not in start_pair and n >= 2 and n / sum(c.values()) >= 0.8:
            by_call[t1] = (t2, n)

    kinds = {"same": "same", "values": "values", "changed": "changed", "only_a": "only_ac1", "only_b": "only_pp"}
    counts = collections.Counter()
    with open(os.path.join(args.out, "functions.tsv"), "w") as f:
        f.write("# AC1 routine -> PP routine (tools/xdisc/ac1_xdisc.py, register-blind pairing). Static.\n"
                "# kind: same = identical words (after address masking), values = same operations with other registers or\n"
                "# constants, changed = logic differs (ratio = how alike), only_ac1 / only_pp, via_calls = unpaired AC1\n"
                "# routine whose callers all line up. conf: high / medium / low; 'data' = the cut is a data table.\n"
                "# calls = paired call sites inside the routine whose callees also pair (ok/all).\n")
        f.write("ac1_disc\tac1_module\tac1_addr\tac1_bytes\tpp_disc\tpp_module\tpp_addr\tpp_bytes\twhat\tkind\tratio\tcalls\tconf\n")
        for e in func_rows:
            a, b = e["a"], e["b"]
            aa = "%08X" % (e["abase"] + a[0] * 4) if a else ""
            bb = "%08X" % (e["bbase"] + b[0] * 4) if b else ""
            kind = kinds[e["kind"]]
            conf = confidence(e["kind"], e["ratio"], e["calls_ok"], e["calls_all"], e["ds"])
            if not b and a and (e["abase"] + a[0] * 4) in by_call:
                t2, n = by_call[e["abase"] + a[0] * 4]
                bb, kind, conf = "%08X" % t2, "via_calls", "low" if n < 3 else "medium"
            counts[(e["what"], kind, conf)] += 1
            f.write("\t".join(["AC1", e["am"], aa, str((a[1] - a[0]) * 4) if a else "",
                               "PP", e["pm"], bb, str((b[1] - b[0]) * 4) if b else "", e["what"], kind,
                               "%.3f" % e["ratio"] if a and b else "",
                               "%d/%d" % (e["calls_ok"], e["calls_all"]) if e["calls_all"] else "", conf]) + "\n")

    glines = []
    for t1, c in glob_votes.items():
        best, mods = collections.Counter(), collections.defaultdict(set)
        for (t2, w), n in c.items():
            best[t2] += n; mods[t2].add(w)
        t2, n = best.most_common(1)[0]
        tot = sum(best.values())
        agree = n / tot
        conf = "high" if n >= 3 and agree >= 0.9 else "medium" if n >= 2 and agree >= 0.75 else "low"
        glines.append((t1, t2, ",".join(sorted(mods[t2])), n, tot, conf,
                       ";".join("%08X" % k for k, _ in best.most_common(3)[1:])))
    glines.sort()
    with open(os.path.join(args.out, "globals.tsv"), "w") as f:
        f.write("# RAM addresses used by paired code, AC1 -> PP (tools/xdisc/ac1_xdisc.py). votes = paired instructions giving\n"
                "# this PP address, seen = all paired instructions using the AC1 address. code = which code uses it (an\n"
                "# address in the module area, AC1 0x8004ADA0+ / PP 0x800490D0+, only holds while that module is loaded).\n")
        f.write("ac1_disc\tac1_addr\tpp_disc\tpp_addr\tdelta\tcode\tvotes\tseen\tconf\tother_pp\n")
        for t1, t2, w, n, tot, conf, alts in glines:
            f.write("AC1\t%08X\tPP\t%08X\t%+d\t%s\t%d\t%d\t%s\t%s\n" % (t1, t2, t2 - t1, w, n, tot, conf, alts))
            counts[("globals", conf)] += 1

    with open(os.path.join(args.out, "runs.tsv"), "w") as f:
        f.write("# Runs of globals (high/medium only) that moved by one fixed amount, AC1 -> PP. Inside a run an AC1 address\n"
                "# between first and last most likely sits at AC1 + delta on PP; outside runs, check globals.tsv.\n")
        f.write("ac1_first\tac1_last\tdelta\tpp_first\tpp_last\tpairs\tregion\n")
        cur = None

        def flush(c):
            f.write("%08X\t%08X\t%+d\t%08X\t%08X\t%d\t%s\n" % (c[0], c[1], c[2], c[0] + c[2], c[1] + c[2], c[3], c[4]))
            counts[("runs",)] += 1
        for t1, t2, w, n, tot, conf, alts in glines:
            if conf == "low":
                continue
            d = t2 - t1
            region = "scratchpad" if t1 < 0x80000000 else "exe area" if t1 < MOD_BASE["AC1"] else w
            if cur and cur[2] == d and cur[4] == region and t1 - cur[1] < 0x2000:
                cur[1] = t1; cur[3] += 1
            else:
                if cur and cur[3] >= 2:
                    flush(cur)
                cur = [t1, t1, d, 1, region]
        if cur and cur[3] >= 2:
            flush(cur)

    with open(os.path.join(args.out, "fields.tsv"), "w") as f:
        f.write("# Struct field offsets that differ between paired loads/stores, AC1 +x -> PP +y (all structs mixed; check\n"
                "# the routines listed). Pairs seen 3+ times, shifts up to 0x80. For the AC object and player block see\n"
                "# struct-*.tsv.\n")
        f.write("ac1_off\tpp_off\tshift\tcount\ttop_pp_routines\n")
        for (oa, ob), n in sorted(field_votes.items(), key=lambda kv: (kv[0][0], -kv[1])):
            if oa == ob or n < 3 or abs(ob - oa) > 0x80:
                continue
            f.write("%+#x\t%+#x\t%+d\t%d\t%s\n" % (oa, ob, ob - oa, n,
                                                  ",".join(k for k, _ in field_where[(oa, ob)].most_common(3))))
    for sname in PP_MARKERS:
        with open(os.path.join(args.out, "struct-%s.tsv" % sname.replace("_", "-")), "w") as f:
            f.write("# %s field offsets, AC1 -> PP, from accesses through a register whose PP side touches 2+ known PP\n"
                    "# fields (ac1_xdisc.py PP_MARKERS). share = this AC1 offset's share of all AC1 offsets seen for the PP\n"
                    "# offset; below 0.8 the register probably held another struct too, check the routines.\n" % sname)
            f.write("pp_off\tac1_off\tshift\tcount\tshare\troutines\tsample_pp_routines\n")
            per = collections.defaultdict(collections.Counter)
            for (x, y), n in struct_votes[sname].items():
                per[y][x] += n
            for y in sorted(per):
                if y < 0:
                    continue
                tot = sum(per[y].values())
                x, n = per[y].most_common(1)[0]
                if n < 2:
                    continue
                where = sorted(struct_where[sname][(x, y)])
                f.write("%+#x\t%+#x\t%+d\t%d\t%.2f\t%d\t%s\n" % (y, x, y - x, n, n / tot, len(where), ",".join(where[:3])))
    with open(os.path.join(args.out, "summary.tsv"), "w") as f:
        f.write("# counts from the last run of ac1_xdisc.py\nwhat\tkind\tconf\tcount\n")
        for k in sorted(counts, key=str):
            k3 = (list(k) + ["", ""])[:3]
            f.write("%s\t%s\t%s\t%d\n" % (k3[0], k3[1], k3[2], counts[k]))
    print("done", file=sys.stderr)


if __name__ == "__main__":
    main()

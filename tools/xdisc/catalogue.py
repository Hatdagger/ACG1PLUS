#!/usr/bin/env python3
"""Project Phantasma (PP) routine catalogue and psxrecomp seed list.

  python3 -I tools/xdisc/catalogue.py --pp WORK/pp-disc

Writes (addresses only, safe to commit):
  docs/research/xdisc/pp-catalogue.tsv  every PP routine (boot exe + FDAT 201-206): size, its Master of
                                        Arena twin, how alike, confidence, and the research notes that
                                        mention the twin's MoA-US address (read those to learn what it does)
  seeds/pp/ghidra_funcs.txt             seeds for the PP boot exe: entry, every jal target in the exe,
                                        and the PP twins of the MoA seeds (catches routines only reached
                                        through pointers)

Needs docs/research/xdisc/functions.tsv (tools/xdisc/xdisc.py) and seeds/ghidra_funcs.txt (MoA).
"""
import argparse, collections, glob, os, re, struct

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FUN = os.path.join(ROOT, "docs/research/xdisc/functions.tsv")
PP_ENTRY, EXE_BASE = 0x80011E94, 0x80011000


def doc_mentions():
    """MoA-US address -> set of research docs that mention it (any engine; old notes are MoA-only)"""
    m = collections.defaultdict(set)
    pat = re.compile(r"0x(8[0-9A-Fa-f]{7})\b")
    for p in glob.glob(os.path.join(ROOT, "docs/research/**/*.md"), recursive=True):
        rel = os.path.relpath(p, os.path.join(ROOT, "docs/research"))
        if rel.startswith("xdisc") or rel.startswith("ram-map"):
            continue
        for a in pat.findall(open(p, encoding="utf-8", errors="replace").read()):
            m[int(a, 16)].add(rel)
    return m


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pp", required=True, help="pp-disc folder (for the boot exe)")
    args = ap.parse_args()
    docs = doc_mentions()
    rows = []
    for line in open(FUN):
        if line.startswith("#") or line.startswith("moa_disc"):
            continue
        f = line.rstrip("\n").split("\t")
        rows.append(f)

    out = os.path.join(ROOT, "docs/research/xdisc/pp-catalogue.tsv")
    with open(out, "w") as o:
        o.write("# Project Phantasma US (PP) routines, boot exe and FDAT 201-206 (tools/xdisc/catalogue.py).\n"
                "# twin = the Master of Arena US routine it pairs with (functions.tsv). notes = research docs that\n"
                "# mention the twin's MoA-US address; for module routines that mention may be about another engine,\n"
                "# check the doc. conf 'data' = a data table cut out as a routine.\n")
        o.write("pp_disc\tpp_module\tpp_addr\tpp_bytes\twhat\tmoa_disc\tmoa_module\tmoa_twin\tkind\tratio\tconf\tnotes\n")
        pp_rows = [r for r in rows if r[6]]
        pp_rows.sort(key=lambda r: (r[5] != "SLUS_006.70", r[5], r[6]))
        for r in pp_rows:
            twin = int(r[2], 16) if r[2] else None
            notes = ",".join(sorted(docs.get(twin, ())))[:200] if twin else ""
            o.write("\t".join(["PP", r[5], r[6], r[7], r[8], "MoA-US" if twin else "", r[1] if twin else "",
                               r[2], r[9], r[10], r[12], notes]) + "\n")
    print("wrote", os.path.relpath(out, ROOT), len(pp_rows), "routines")

    # seeds for the PP boot exe
    b = open(os.path.join(args.pp, "SLUS_006.70"), "rb").read()
    text = struct.unpack_from("<I", b, 0x1C)[0]
    ws = struct.unpack_from("<%dI" % (text // 4), b, 0x800)
    end = EXE_BASE + text
    jal = {0x80000000 | ((w & 0x3FFFFFF) << 2) for w in ws if w >> 26 == 3}
    jal = {t for t in jal if EXE_BASE <= t < end}
    moa_seeds = [int(l, 16) for l in open(os.path.join(ROOT, "seeds/ghidra_funcs.txt"))
                 if l.strip() and not l.startswith("#")]
    twin = {int(r[2], 16): int(r[6], 16) for r in rows
            if r[1] == "SLUS_010.30" and r[2] and r[6] and r[12] in ("high", "medium")}
    carried = {twin[a] for a in moa_seeds if a in twin} - jal - {PP_ENTRY}
    os.makedirs(os.path.join(ROOT, "seeds/pp"), exist_ok=True)
    sp = os.path.join(ROOT, "seeds/pp/ghidra_funcs.txt")
    with open(sp, "w") as o:
        o.write("# Seeds for Project Phantasma USA (SLUS_006.70), made by tools/xdisc/catalogue.py\n")
        o.write("# entry=0x%08x load=0x%08x text_size=0x%x\n" % (PP_ENTRY, EXE_BASE, text))
        o.write("# entry + %d jal targets in the exe + %d PP twins of the MoA seeds (high/medium pairs).\n"
                % (len(jal), len(carried)))
        o.write("# First pass only: overlays (FDAT 201-206, load at 0x800490D0) need their own seeds.\n")
        for a in sorted(jal | carried | {PP_ENTRY}):
            o.write("0x%08X\n" % a)
    print("wrote", os.path.relpath(sp, ROOT), len(jal | carried | {PP_ENTRY}), "seeds")


if __name__ == "__main__":
    main()

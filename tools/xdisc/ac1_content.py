#!/usr/bin/env python3
"""AC1 content inventory helpers (AC1 mapping, topic 3: docs/research/ac1/content.md).

Reads the owner's work packs (ac1-work, pp-work, both with setup.py run) and PRINTS comparisons to the screen.
Never commit its output: it quotes game data. Our doc keeps only counts, names and addresses.

  python3 -I tools/xdisc/ac1_content.py --work WORK parts      # part tables AC1 vs PP (names, ids, stats)
  python3 -I tools/xdisc/ac1_content.py --work WORK stock      # new-game owned parts and shop stock (AC1)
  python3 -I tools/xdisc/ac1_content.py --work WORK missions   # MIS.T 0 mission list
  python3 -I tools/xdisc/ac1_content.py --work WORK mail       # MIS.T 818 mail list
  python3 -I tools/xdisc/ac1_content.py --work WORK stages     # AC1 stage / mission data vs PP stage data
  python3 -I tools/xdisc/ac1_content.py --work WORK pa         # body kits: AC1 entry -> PP entry, AC1-only entries
  python3 -I tools/xdisc/ac1_content.py --work WORK archives   # MENU_TIM, MENU_TMD, RTIM, part archives vs PP
  python3 -I tools/xdisc/ac1_content.py --work WORK sound      # XA voices and the fight sound bank vs PP

WORK holds ac1-disc/ and pp-disc/ (default: $ACG1_WORK or the current folder).
"""
import argparse
import collections
import hashlib
import os
import struct

AC1_BASE = 0x8004ADA0  # AC1 module load address (foundations.md §3)
PP_BASE = 0x800490D0

CATS = ["head", "core", "legs", "arms", "hand", "back", "blade", "booster", "fcs", "gen", "opt"]
STRIDES = [60, 56, 72, 52, 44, 52, 52, 48, 96, 48, 44]
# table starts (menu FDAT 201); the last value is where the option table ends
AC1_TABLES = [0x800B91F4, 0x800B944C, 0x800B94F4, 0x800B9DF4, 0x800BA134, 0x800BA3F4, 0x800BAADC,
              0x800BABAC, 0x800BACCC, 0x800BAFCC, 0x800BB11C, 0x800BB300]
PP_TABLES = [0x800B64FC, 0x800B6934, 0x800B6B9C, 0x800B76DC, 0x800B7BBC, 0x800B7F58, 0x800B8B54,
             0x800B8D28, 0x800B8FC8, 0x800B95C8, 0x800B9898, 0x800B9BDC]
# owned-part index order (scenario.md §6b); AC1 bases at 0x8004CC2C, PP at 0x800B5094
INDEX_ORDER = ["head", "core", "arms", "legs", "gen", "fcs", "opt", "booster", "back", "hand", "blade"]
AC1_NEWGAME_OWNED = 0x800B70D4
AC1_NEWGAME_STOCK = 0x800B7174


def md5(b):
    return hashlib.md5(b).digest()


class Work:
    def __init__(self, work):
        self.a = os.path.join(work, "ac1-disc")
        self.p = os.path.join(work, "pp-disc")

    def entry(self, disc, arc, i):
        root = self.a if disc == "ac1" else self.p
        with open(os.path.join(root, "unpacked/GG", arc, "%03d.bin" % i), "rb") as f:
            return f.read()

    def entries(self, disc, arc):
        root = os.path.join(self.a if disc == "ac1" else self.p, "unpacked/GG", arc)
        out = {}
        for f in os.listdir(root):
            if f.endswith(".bin"):
                with open(os.path.join(root, f), "rb") as h:
                    out[int(f[:3])] = h.read()
        return out


def name(rec):
    return rec[:18].split(b"\0")[0].decode("latin1").strip()


def tables(data, base, starts):
    out = {}
    for i, c in enumerate(CATS):
        n = (starts[i + 1] - starts[i]) // STRIDES[i]
        o = starts[i] - base
        out[c] = [data[o + k * STRIDES[i]:o + (k + 1) * STRIDES[i]] for k in range(n)]
    return out


def part_tables(w):
    a = tables(w.entry("ac1", "COM/FDAT", 201), AC1_BASE, AC1_TABLES)
    p = tables(w.entry("pp", "COM/FDAT", 201), PP_BASE, PP_TABLES)
    return a, p


def cmd_parts(w):
    a, p = part_tables(w)
    for c in CATS:
        an = [name(r) for r in a[c]]
        pn = [name(r) for r in p[c]]
        print("== %s: AC1 %d slots (%d real), PP %d slots (%d real)" % (
            c, len(an), sum(x != "DUMMY" for x in an), len(pn), sum(x != "DUMMY" for x in pn)))
        for i, n in enumerate(an):
            if n == "DUMMY":
                continue
            if n not in pn:
                print("  AC1 only: id %d %s" % (i, n))
                continue
            j = pn.index(n)
            note = "" if i == j else "  (PP id %d)" % j
            diff = [o for o in range(36, len(a[c][i]), 2) if a[c][i][o:o + 2] != p[c][j][o:o + 2]]
            if a[c][i][18:36] != p[c][j][18:36]:
                diff.append("type text")
            if note or diff:
                print("  %2d %-16s%s%s" % (i, n, note, ("  differs: %s" % diff) if diff else ""))
        print("  PP only:", [(j, n) for j, n in enumerate(pn) if n != "DUMMY" and n not in an])
    # weapon detail tables (24 bytes): AC1 weapon arms (8) + hand (16) + back (34) right after the options
    ad = w.entry("ac1", "COM/FDAT", 201)
    pd = w.entry("pp", "COM/FDAT", 201)
    o_a = AC1_TABLES[-1] - AC1_BASE
    o_p = PP_TABLES[-1] - PP_BASE
    for c, na, npp, first_a, first_p in (("arms", 8, 16, 8, 12), ("hand", 16, 21, 0, 0), ("back", 34, 59, 0, 0)):
        an = [name(r) for r in a[c]]
        pn = [name(r) for r in p[c]]
        for k in range(na):
            n = an[first_a + k]
            j = pn.index(n) - first_p
            x = ad[o_a + 24 * k:o_a + 24 * k + 24]
            y = pd[o_p + 24 * j:o_p + 24 * j + 24]
            if x != y:
                print("detail differs: %s %s, bytes %s" % (c, n, [i for i in range(24) if x[i] != y[i]]))
        o_a += 24 * na
        o_p += 24 * npp
    print("AC1 detail tables end at %#x" % (o_a + AC1_BASE))


def ac1_index_names(w):
    a, _ = part_tables(w)
    out = []
    for c in INDEX_ORDER:
        out += [(c, name(r)) for r in a[c]]
    return out


def cmd_stock(w):
    d = w.entry("ac1", "COM/FDAT", 201)
    names = ac1_index_names(w)
    for label, addr in (("new-game owned counts", AC1_NEWGAME_OWNED), ("new-game shop stock", AC1_NEWGAME_STOCK)):
        t = d[addr - AC1_BASE:addr - AC1_BASE + 160]
        g = collections.defaultdict(list)
        for i, (c, n) in enumerate(names):
            if n != "DUMMY":
                g[t[i]].append(n)
        print("== %s at %#x: %s" % (label, addr, {k: len(v) for k, v in sorted(g.items())}))
        for k, v in sorted(g.items()):
            if len(v) < 40:
                print("  %d: %s" % (k, ", ".join(v)))


def cmd_missions(w):
    e0 = w.entry("ac1", "MS/MIS", 0)
    for i in range(64):
        lo, hi = struct.unpack_from("<bb", e0, 4 * i)
        nm = e0[0x100 + 32 * i:0x120 + 32 * i].split(b">")[0].split(b"\0")[0].decode("latin1").replace("{4", " ")
        adv = struct.unpack_from("<I", e0, 0x900 + 4 * i)[0]
        rew = struct.unpack_from("<I", e0, 0xA00 + 4 * i)[0]
        pics = [x + 1 for x in struct.unpack_from("<8h", e0, 0xB80 + 32 * i) if x != -1]
        txt = [x + 1 for x in struct.unpack_from("<8h", e0, 0xB90 + 32 * i) if x != -1]
        print("%2d %-26r window %d-%d advance %d reward %d engine %d music %d | MIS.T pictures %s texts %s" % (
            i, nm, lo, hi, adv, rew, e0[0xB00 + i], e0[0xB40 + i], pics, txt))


def cmd_mail(w):
    m = w.entry("ac1", "MS/MIS", 818)
    for i in range(64):
        k, v = m[2 * i], m[2 * i + 1]
        if k == 0xFF:
            continue
        subj = m[0x80 + 32 * i:0xA0 + 32 * i].split(b">")[0].decode("latin1")
        pics = [x + 819 for x in struct.unpack_from("<8h", m, 0x880 + 32 * i) if x != -1]
        txt = [x + 819 for x in struct.unpack_from("<8h", m, 0x890 + 32 * i) if x != -1]
        print("%2d kind %d value %2d %-28r MIS.T pictures %s texts %s" % (i, k, v, subj, pics, txt))


def windows(b, step, k=256):
    return {md5(b[i:i + k]) for i in range(0, len(b) - k, step)}


def cmd_stages(w):
    pp = {s: w.entry("pp", "COM/FDAT", 5 * s + 4) for s in range(40)}
    ppw = {s: windows(v, 4) for s, v in pp.items() if len(v) > 1000}
    slots = list(range(50)) + [60, 70] + list(range(80, 85)) + list(range(90, 95))
    for n in slots:
        data = w.entry("ac1", "COM/FDAT", 2 * n + 1)
        if len(data) < 1000:
            print("slot %d: empty" % n)
            continue
        sa = windows(data, 256)
        best = sorted(((len(sa & ppw[s]) / max(1, len(sa)), s) for s in ppw), reverse=True)[:2]
        print("slot %2d: data FDAT %d %7d B, code %5d B; closest PP stages %s" % (
            n, 2 * n + 1, len(data), len(w.entry("ac1", "COM/FDAT", 2 * n)),
            ", ".join("%d (%.0f%%)" % (s, 100 * f) for f, s in best)))


def cmd_pa(w):
    m = collections.defaultdict(collections.Counter)
    for n in range(72):
        arc = "P%d/PA%02d" % (n // 20, n)
        pp = collections.defaultdict(list)
        for k, b in w.entries("pp", arc).items():
            if b:
                pp[md5(b)].append(k)
        for i, b in sorted(w.entries("ac1", arc).items()):
            if b:
                js = pp.get(md5(b))
                m[i][js[0] if js else None] += 1
    for i in sorted(m):
        print("AC1 entry %3d -> %s" % (i, ", ".join("%s x%d" % ("PP %d" % j if j is not None else "AC1 only", c)
                                                     for j, c in m[i].most_common())))


def cmd_archives(w):
    for arc in ("MS/MENU_TIM", "MS/MENU_TMD", "COM/RTIM", "MS/HEAD_T", "MS/CORE_T", "MS/LEG_T", "MS/ARMS_T",
                "MS/BWL_T", "MS/BWR_T", "MS/WEL_T", "MS/WER_T", "MS/SPEC_T", "MS/GENE_T", "MS/COMP_T", "MS/BST_T"):
        a = w.entries("ac1", arc)
        ph = {}
        for k, v in w.entries("pp", arc).items():
            if v:
                ph.setdefault(md5(v), k)
        row = []
        for k in sorted(a):
            if a[k]:
                row.append("%d=%s" % (k, ph.get(md5(a[k]), "-")))
        print("== %s (AC1 entry=PP entry, - = AC1 only):\n  %s" % (arc, " ".join(row)))


def xa_sectors(path):
    with open(path, "rb") as f:
        d = f.read()
    out = []
    for i in range(0, len(d), 2336):
        s = d[i:i + 2336]
        out.append(s[8:8 + 2324] if s[2] & 0x20 else s[8:8 + 2048])  # skip subheader and EDC
    return out


def blocks(b):
    o, out = 0, []
    while o + 4 <= len(b):
        n = struct.unpack_from("<I", b, o)[0]
        if n == 0 or o + 4 + n > len(b):
            break
        out.append(b[o + 4:o + 4 + n])
        o = (o + 4 + n + 3) & ~3
    return out


def cmd_sound(w):
    for f in sorted(os.listdir(os.path.join(w.a, "GG/BGM"))):
        a = xa_sectors(os.path.join(w.a, "GG/BGM", f))
        pp = os.path.join(w.p, "GG/BGM", f)
        if not os.path.exists(pp):
            print(f, "not on PP")
            continue
        p = xa_sectors(pp)
        ua = {md5(x) for x in a}
        print("%-10s AC1 %5d sectors, PP %5d; same sector at same place %5d; AC1 distinct sectors found on PP %d / %d"
              % (f, len(a), len(p), sum(x == y for x, y in zip(a, p)), len(ua & {md5(x) for x in p}), len(ua)))
    A = blocks(w.entry("ac1", "COM/FDAT", 200))
    P = blocks(w.entry("pp", "COM/FDAT", 200))
    for i in range(max(len(A), len(P))):
        x = A[i] if i < len(A) else b""
        y = P[i] if i < len(P) else b""
        print("FDAT 200 block %d: AC1 %d B, PP %d B, %s" % (i, len(x), len(y), "same" if x == y else "differ"))
    ps, ts, vs = struct.unpack_from("<HHH", A[1], 0x12)
    qs, us, ws = struct.unpack_from("<HHH", P[1], 0x12)
    print("fight bank: AC1 %d programs / %d tones / %d samples, PP %d / %d / %d" % (ps, ts, vs, qs, us, ws))
    pw = windows(P[2], 16)
    hit = sum(md5(A[2][i:i + 256]) in pw for i in range(0, len(A[2]) - 256, 256))
    print("AC1 bank bytes found in PP bank: %d of %d 256-byte pieces" % (hit, (len(A[2]) - 256) // 256 + 1))
    same = sum(A[3][448 + 10 * i:458 + 10 * i] == P[3][448 + 10 * i:458 + 10 * i] for i in range(256))
    print("sound-id records (block 3 +448, 10 B each): %d of 256 the same" % same)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--work", default=os.environ.get("ACG1_WORK", "."))
    ap.add_argument("what", choices=["parts", "stock", "missions", "mail", "stages", "pa", "archives", "sound"])
    a = ap.parse_args()
    globals()["cmd_" + a.what](Work(a.work))


if __name__ == "__main__":
    main()

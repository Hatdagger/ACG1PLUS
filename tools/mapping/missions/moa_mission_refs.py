#!/usr/bin/env python3
"""What a Master of Arena (MoA-US) mission module would need to run on Project Phantasma's (PP) missions engine.

Reads the owner's work packs (never commits their bytes) and the cross-disc tables in docs/research/xdisc/.
For each MoA mission code file (FDAT 5n, linked at 0x800A0DD0) it walks the code from the module's own
pointer table (recursive descent), then sorts every address the module uses:
  internal   points inside the module            -> moves by PP base - MoA base (-0x9398)
  call       jal / pointer to MoA engine or exe code -> PP twin from functions.tsv (conf, kind)
  data       RAM address outside the module      -> PP address from globals.tsv / runs.tsv / engine-area shift
  field      load/store at an AC-object offset that moved (struct-ac-object.tsv) -> needs a type decision
  python3 -I tools/mapping/missions/moa_mission_refs.py [--work /home/claude/work] [--tsv out.tsv] [--mods 0,1]
Prints a summary per module; --tsv writes one row per distinct reference (addresses and counts only).
"""
import argparse, bisect, collections, os, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
X = os.path.join(ROOT, 'docs/research/xdisc')
MOA_BASE, PP_BASE = 0x800A0DD0, 0x80097A38
DELTA = PP_BASE - MOA_BASE                       # -0x9398
MOA_ENG = (0x80050DD8, 0x80050DD8 + 294912)      # FDAT 207 loaded
ENG_DATA = (0x800AE5D0, 0x800C2D30)              # MoA engine data after the mission area (PP = MoA + DELTA, checked)
EXE = (0x80011000, 0x80050DD8)

def s16(x): return x - 0x10000 if x & 0x8000 else x

def load_funcs():
    moa = collections.defaultdict(dict)         # module -> addr -> (pp_module, pp_addr, kind, conf)
    for l in open(os.path.join(X, 'functions.tsv')):
        if l.startswith('#') or l.startswith('moa_disc'): continue
        f = l.rstrip('\n').split('\t')
        if len(f) < 13 or not f[2]: continue
        moa[f[1]][int(f[2], 16)] = (f[5], int(f[6], 16) if f[6] else None, f[9], f[12])
    starts = {k: sorted(v) for k, v in moa.items()}
    return moa, starts

def load_globals():
    g = {}
    for l in open(os.path.join(X, 'globals.tsv')):
        if l.startswith('#') or l.startswith('moa_disc'): continue
        f = l.rstrip('\n').split('\t')
        mods = f[5].split(',')
        a = int(f[1], 16)
        if a < MOA_ENG[0] or 'FDAT 207' in mods:
            g[a] = (int(f[3], 16), f[9])
    runs = []
    for l in open(os.path.join(X, 'runs.tsv')):
        if l.startswith('#') or l.startswith('moa_first'): continue
        f = l.rstrip('\n').split('\t')
        if f[6].startswith('(exe') or 'FDAT 207' in f[6]:
            runs.append((int(f[0], 16), int(f[1], 16), int(f[2])))
    runs.sort()
    return g, runs

def load_fields():
    moved = {}
    for l in open(os.path.join(X, 'struct-ac-object.tsv')):
        if l.startswith('#') or l.startswith('moa_off'): continue
        f = l.split('\t')
        if f[2] != '+0': moved[int(f[0], 16)] = int(f[1], 16)
    return moved

LOADSTORE = set(range(32, 47)) | {50, 58}

class Mod:
    def __init__(s, data):
        s.d = data.rstrip(b'\0'); s.d += b'\0' * (-len(s.d) % 4)
        s.n = len(s.d) // 4
        s.w = struct.unpack('<%dI' % s.n, s.d)
        s.end = MOA_BASE + 4 * s.n
    def inside(s, a): return MOA_BASE <= a < s.end
    def word(s, a): return s.w[(a - MOA_BASE) // 4]

def descend(m, dead=None):
    """recursive descent over the module's code; returns set of code word addresses"""
    code = set(); todo = []
    # pointer table at the start: hooks +0..+0x1C, type handlers +0x24..+0x5C, section +0x60.., points +0xA0..
    i = 0
    while i < m.n and i < 0xE0 // 4:
        v = m.w[i]
        if m.inside(v) and v % 4 == 0 and v >= MOA_BASE + 0xA0 and (
                i < 0xA0 // 4 or (m.word(v) >> 16) == 0x27BD or m.word(v) == 0x03E00008):
            todo.append(v)
        elif v != 0: break
        i += 1
    seen_root = set()
    def run(a):
        hi = {}
        while m.inside(a) and a not in code:
            w = m.word(a); code.add(a); op = w >> 26; rs = (w >> 21) & 31; rt = (w >> 16) & 31
            fn = w & 63
            if op == 0 and fn == 8:            # jr
                code.add(a + 4); return
            if op == 2:                        # j
                code.add(a + 4); t = ((w & 0x3FFFFFF) << 2) | (a & 0xF0000000)
                if m.inside(t): todo.append(t)
                return
            if op == 3:
                t = ((w & 0x3FFFFFF) << 2) | 0x80000000
                if m.inside(t): todo.append(t)
            if op in (4, 5, 6, 7) or (op == 1 and rt in (0, 1, 16, 17)):
                t = a + 4 + 4 * s16(w & 0xFFFF)
                if m.inside(t): todo.append(t)
                if op == 4 and rs == 0 and rt == 0:   # b (beq zero,zero)
                    code.add(a + 4); return
            if op == 15: hi[rt] = (w & 0xFFFF) << 16
            elif op in (9, 13) and rs in hi:
                v = (hi[rs] + (w & 0xFFFF if op == 13 else s16(w & 0xFFFF))) & 0xFFFFFFFF
                if m.inside(v) and v % 4 == 0 and looks_code(m, v): todo.append(v)
            a += 4
    def drain():
        while todo:
            a = todo.pop()
            if a in seen_root: continue
            seen_root.add(a); run(a)
    drain()
    # pointers kept in data (callback tables, jump tables) that land on code
    while True:
        new = [v for i, v in enumerate(m.w) if MOA_BASE + 4 * i not in code and m.inside(v) and v % 4 == 0
               and v not in seen_root and (m.word(v) >> 16) == 0x27BD]
        if not new: break
        todo.extend(new); drain()
    if dead is not None:
        # routines nothing points at (library code linked in but unused): walk them too, count apart
        before = set(code)
        todo.extend(MOA_BASE + 4 * i for i, w in enumerate(m.w)
                    if (w >> 16) == 0x27BD and (w & 0x8000) and MOA_BASE + 4 * i not in code)
        drain(); dead.update(code - before)
    return code

def looks_code(m, a):
    # a routine start usually opens with addiu sp,sp,-n or is a leaf; accept if the first 4 words decode as non-data
    w = m.word(a)
    return (w >> 16) == 0x27BD or (w >> 26) in (0, 9, 15, 35, 43, 3, 2, 4, 5, 8, 13, 12)

def scan(m, funcs, starts, glob, runs, moved):
    dead = set()
    code = descend(m, dead)
    refs = collections.Counter()      # (kind, moa_target, extra) -> count
    fields = collections.Counter()
    hi = {}
    for a in sorted(code):
        w = m.word(a); op = w >> 26; rs = (w >> 21) & 31; rt = (w >> 16) & 31; im = w & 0xFFFF
        if op == 3:
            t = ((w & 0x3FFFFFF) << 2) | 0x80000000
            refs[('jal', t)] += 1
        elif op == 15:
            hi[rt] = im << 16
            continue
        elif rs in hi and (op in (9, 13) or op in LOADSTORE):
            v = (hi[rs] + (im if op == 13 else s16(im))) & 0xFFFFFFFF
            if op in (9, 13):
                hi[rt] = v          # register now holds the full address; later loads add their offset to it
                if not (MOA_BASE <= v < MOA_BASE + 0x10000 or (v & 0xFFFF) or True): pass
                refs[('ptr', v)] += 1
                continue
            refs[('mem', v)] += 1
        elif op in LOADSTORE and rs not in (29, 28, 0) and im < 0x400:
            if im in moved or 0x3A <= im <= 0x42:
                fields[(im, a in dead)] += 1
        if op in (9, 13, 15) or (op == 0 and (w & 63) in (32, 33, 37)) or op in (35, 32, 33, 36, 37):
            if op not in LOADSTORE or rt != rs: pass
        # a register written by anything else drops its lui value
        if op in (9, 13) and rt != rs: hi.pop(rt, None)
        elif op in (35, 32, 33, 36, 37, 34, 38): hi.pop(rt, None)
        elif op == 0: hi.pop((w >> 11) & 31, None)
    # data words (not code) that point somewhere interesting
    data = collections.Counter()
    for i, v in enumerate(m.w):
        a = MOA_BASE + 4 * i
        if a in code: continue
        if m.inside(v) or MOA_ENG[0] <= v < MOA_ENG[1] or EXE[0] <= v < EXE[1] or 0x80000000 <= v < 0x80200000:
            data[v] += 1
    return code, refs, fields, data, dead

# missions-engine record arrays, MoA-US -> PP (bases from globals.tsv, strides and counts from the loaders,
# see docs/research/mapping/missions-import-pp.md section 3)
ARRAYS = [  # name, moa base, moa stride, moa count, pp base, pp stride
    ('object', 0x801E7248, 376, 16, 0x801E2DE0, 368),
    ('section', 0x801CFAB0, 64, 512, 0x801BC328, 64),
    ('point', 0x801E49D8, 52, 128, 0x801D9250, 52),
    ('point class', 0x801E46D8, 24, 32, 0x801D8F50, 24),
    ('object type', 0x801E65E8, 44, 8, 0x801DFC60, 44),
    ('placement', 0x801E6748, 44, 64, 0x801E01E0, 44),
]
FIELDS = None

def array_addr(v):
    global FIELDS
    if FIELDS is None: FIELDS = load_fields()
    for name, mb, ms, mc, pb, ps in ARRAYS:
        if mb <= v < mb + ms * mc:
            k, f = divmod(v - mb, ms)
            if name == 'object':
                pf = FIELDS.get(f, f)
                if 0x3A <= f <= 0x42: return None, '%s %d +0x%X (MoA-only field)' % (name, k, f)
                return pb + k * ps + pf, '%s %d +0x%X' % (name, k, f)
            return pb + k * ps + f, '%s %d +0x%X' % (name, k, f)
    return None, None

def classify(v, m, funcs, starts, glob, runs):
    """returns (class, pp_addr or None, note)"""
    if m.inside(v): return 'internal', v + DELTA, ''
    pa, note = array_addr(v)
    if note: return ('data' if pa else 'data?'), pa, note
    mod = 'SLUS_010.30' if EXE[0] <= v < EXE[1] else ('FDAT 207' if MOA_ENG[0] <= v < MOA_ENG[1] else None)
    if mod:
        f = funcs.get(mod, {}).get(v)
        if f: return 'code', f[1], '%s %s' % (f[2], f[3])
        st = starts.get(mod, [])
        i = bisect.bisect_right(st, v) - 1
        # data address inside the exe / engine image?
        g = glob.get(v)
        if g: return 'data', g[0], 'globals ' + g[1]
        if i >= 0:
            f = funcs[mod][st[i]]
            if f[2] == 'data' or f[3] == 'data':
                return 'data', (f[1] + v - st[i]) if f[1] else None, 'in data cut %08X' % st[i]
            return 'mid', None, 'inside %08X' % st[i]
        return 'unknown', None, ''
    if ENG_DATA[0] <= v < ENG_DATA[1]:
        g = glob.get(v)
        return 'data', v + DELTA, 'engine data shift' + (' (globals %08X)' % g[0] if g and g[0] != v + DELTA else '')
    g = glob.get(v)
    if g: return 'data', g[0], 'globals ' + g[1]
    i = bisect.bisect_right(runs, (v, 0xFFFFFFFF, 0)) - 1
    if i >= 0 and runs[i][0] <= v <= runs[i][1] + 0x40:
        return 'data', v + runs[i][2], 'run %+d' % runs[i][2]
    return 'data?', None, 'no pair'

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--work', default='/home/claude/work')
    ap.add_argument('--tsv')
    ap.add_argument('--mods', default=','.join(map(str, range(20))))
    a = ap.parse_args()
    funcs, starts = load_funcs(); glob, runs = load_globals(); moved = load_fields()
    rows = []; tot = collections.Counter(); allfields = collections.Counter()
    for n in map(int, a.mods.split(',')):
        m = Mod(open(os.path.join(a.work, 'ma-disc/unpacked/GG/COM/FDAT/%03d.bin' % (5 * n)), 'rb').read())
        code, refs, fields, data, dead = scan(m, funcs, starts, glob, runs, moved)
        c = collections.Counter(); calls = collections.Counter()
        for (k, v), cnt in refs.items():
            cl, pp, note = classify(v, m, funcs, starts, glob, runs)
            if k == 'jal' and cl == 'code':
                conf = note.split()[-1]; kind = note.split()[0]
                calls[(conf, kind)] += 1
                if pp is None: cl = 'code-noPP'
            c[(k, cl)] += 1
            rows.append((n, k, '%08X' % v, cl, '%08X' % pp if pp else '', note, cnt))
        for v, cnt in data.items():
            cl, pp, note = classify(v, m, funcs, starts, glob, runs)
            c[('word', cl)] += cnt
            rows.append((n, 'word', '%08X' % v, cl, '%08X' % pp if pp else '', note, cnt))
        allfields.update(fields)
        tot.update(c)
        print('mission %2d: %5d bytes, code %5d words (%2d%%, unreferenced %4d) | ' % (n, 4 * m.n, len(code), 100 * len(code) // m.n, len(dead)) +
              ' '.join('%s/%s=%d' % (k, cl, x) for (k, cl), x in sorted(c.items())) +
              ' | calls by conf: ' + ' '.join('%s-%s=%d' % (k[0], k[1], x) for k, x in sorted(calls.items())) +
              ' | moved-field sites live %d, unreferenced %d' % (
                  sum(v for (o, dd), v in fields.items() if not dd), sum(v for (o, dd), v in fields.items() if dd)))
    print('TOTAL', ' '.join('%s/%s=%d' % (k, cl, x) for (k, cl), x in sorted(tot.items())))
    live = collections.Counter()
    for (o, dd), v in allfields.items():
        if not dd: live[o] += v
    print('moved-field sites in live code: %d (unreferenced code: %d)' % (sum(live.values()),
          sum(v for (o, dd), v in allfields.items() if dd)))
    print('moved-field offsets used in live code (MoA offset: sites):', ' '.join('+0x%X:%d' % kv for kv in sorted(live.items())))
    if a.tsv:
        with open(a.tsv, 'w') as f:
            f.write('# moa_mission_refs.py output: MoA-US mission module references -> PP. Addresses only.\n')
            f.write('mission\tkind\tmoa_addr\tclass\tpp_addr\tnote\tcount\n')
            for r in rows: f.write('\t'.join(map(str, r)) + '\n')

if __name__ == '__main__':
    main()

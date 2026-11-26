#!/usr/bin/env python3
"""What an AC1 mission code file would need to run on Project Phantasma's (PP) missions engine.

Reads the owner's work packs (never commits their bytes) and the AC1 <-> PP tables in docs/research/xdisc/ac1-pp/.
AC1 mission code = FDAT 2n (slots 0-49, 60, 70), linked at 0x801C4B40; PP's mission code area is 0x80097A38.
For each file it walks the code from the module's pointer table (same table layout as PP / MoA), then sorts every
address it uses:
  internal  inside the module                       -> moves by PP base - AC1 base
  call      jal / code pointer into AC1 engine or exe -> PP twin from functions.tsv (kind, conf)
  data      RAM address outside the module           -> PP address: record arrays, block shifts, globals, runs
  field     load/store at an AC-object offset that moved (+0xF2/+0xF4/+0xF6 -> -2), counted per site

  python3 -I tools/xdisc/ac1_mission_refs.py [--work WORK] [--slots 1,2] [--tsv out.tsv] [--list]
  python3 -I tools/xdisc/ac1_mission_refs.py --maps      # AC1 maps vs PP's limits, item points (hidden parts)
  python3 -I tools/xdisc/ac1_mission_refs.py --common    # FDAT 200: messages, sound ids, common models AC1 vs PP
--list prints every engine routine the files call with its PP twin; --tsv writes one row per distinct reference
(addresses and counts only). Prints to the screen; never commit output (it is counts and addresses, but the
--maps item list names parts).
"""
import argparse, bisect, collections, os, struct

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
X = os.path.join(ROOT, 'docs/research/xdisc/ac1-pp')
AC1_BASE, PP_BASE = 0x801C4B40, 0x80097A38
DELTA = PP_BASE - AC1_BASE
AC1_ENG = (0x8004ADA0, 0x8004ADA0 + 286720)       # FDAT 202 as loaded
EXE = (0x80011000, 0x8004ADA0)
SLOTS = [s for s in range(50) if s not in (16, 29, 39)] + [60, 70]

# missions-engine record arrays, AC1 -> PP (AC1 bases from the loaders in FDAT 202, see ac1/import-outlook.md §2)
ARRAYS = [  # name, ac1 base, stride, count, pp base
    ('mission state M', 0x8019F518, 32, 1, 0x80097A18),
    ('point class', 0x80198808, 24, 32, 0x801D8F50),
    ('event point', 0x80198B08, 52, 512, 0x801D9250),
    ('object type', 0x8019F538, 44, 32, 0x801DFC60),
    ('placement', 0x8019FAB8, 44, 256, 0x801E01E0),
    ('object', 0x801A26B8, 368, 16, 0x801E2DE0),
    ('section', 0x801D0B68, 64, 512, 0x801BC328),
]
OBJ_MOVED = {0xF2: 0xF0, 0xF4: 0xF2, 0xF6: 0xF4}
# twins read by hand where functions.tsv has no pair or a wrong one (import-outlook.md §3)
FIX = {
    ('exe', 0x80014D30): 0x80015730,       # rand() * a0 >> 15 helper
    ('exe', 0x80014DFC): 0x800157F0,       # weighted random pick (count, byte table, stride)
    ('exe', 0x80014AE4): 0x800154D8,       # vector -> angles (ratan2, sqrt); tsv pairs it with 800157F0 (wrong)
    ('exe', 0x800132CC): 0x80013D28,       # turn a 12-bit angle toward another by a step; tsv lands mid-routine
    ('FDAT 202', 0x80073170): 0x80070F10,  # rebuild the event-point touch list
    ('FDAT 202', 0x80073674): 0x8007141C,  # allocate n bytes from the section heap
    ('FDAT 202', 0x8007334C): 0x80070F88,  # event point loader (map block 9)
    ('FDAT 202', 0x800731E8): 0x800710F8,  # point class loader (map block 8)
}
# engine data tables the shared mission helper takes the address of (AC1 -> PP; same order, some PP tables longer)
DATA_FIX = {0x8008E530: 0x8008D42C, 0x8008E610: 0x8008D528, 0x8008E658: 0x8008D580,
            0x8008FB58: 0x8008EA90, 0x8008FC48: 0x8008EB80, 0x8008FEE8: 0x8008EE20}
STUBS = (0x8002A400, 0x8002A5BC, 0x8002CB90)   # BIOS call stubs AC1 first..last, PP first (same order)
BLOCKS = (0x80040FA0, 0x8004133C)              # player blocks 1, 2 (AC1); PP 0x800424E8, 0x80042890
PP_BLOCKS = (0x800424E8, 0x80042890)


def block_off(o):
    """AC1 player-block offset -> PP offset (records.md §2-3, import-outlook.md §2); None if not known"""
    if o <= 0x245: return o
    if o <= 0x253: return o + 6
    if o < 0x390:
        k = o - 0x254
        if k <= 3: return 0x258 + k
        if k <= 0x0F: return 0x258 + k + 1
        if k <= 0x85: return 0x258 + k + 2
        slot, f = divmod(k - 0x86, 60)
        if slot < 3 and f <= 0x2A: return 0x258 + 0x88 + slot * 62 + f
        return None
    return o + 12 if o < 0x39C else None
LOADSTORE = set(range(32, 47)) | {50, 58}


def s16(x): return x - 0x10000 if x & 0x8000 else x


def tsv(name):
    for l in open(os.path.join(X, name)):
        if l.startswith('#') or l.startswith('ac1_') or l.startswith('pp_off'): continue
        yield l.rstrip('\n').split('\t')


def load_tables():
    funcs = collections.defaultdict(dict)        # module -> ac1 addr -> (pp module, pp addr, kind, conf)
    for f in tsv('functions.tsv'):
        if len(f) >= 13 and f[2]:
            funcs[f[1]][int(f[2], 16)] = (f[5], int(f[6], 16) if f[6] else None, f[9], f[12])
    starts = {k: sorted(v) for k, v in funcs.items()}
    glob = {}
    for f in tsv('globals.tsv'):
        a = int(f[1], 16)
        if a < AC1_ENG[0] or 'missions' in f[5] or a >= AC1_ENG[1]:
            glob[a] = (int(f[3], 16), f[8])
    runs = sorted((int(f[0], 16), int(f[1], 16), int(f[2])) for f in tsv('runs.tsv'))
    return funcs, starts, glob, runs


class Mod:
    def __init__(s, data):
        s.d = data.rstrip(b'\0'); s.d += b'\0' * (-len(s.d) % 4)
        s.n = len(s.d) // 4
        s.w = struct.unpack('<%dI' % s.n, s.d) if s.n else ()
        s.end = AC1_BASE + 4 * s.n
    def inside(s, a): return AC1_BASE <= a < s.end
    def word(s, a): return s.w[(a - AC1_BASE) // 4]


def descend(m):
    """recursive descent from the pointer table (+0x00 .. +0xDC) and code pointers kept in data"""
    code, todo, roots = set(), [], set()
    for i in range(min(m.n, 0xE0 // 4)):
        v = m.w[i]
        if m.inside(v) and v % 4 == 0: todo.append(v)
    def run(a):
        hi = {}
        while m.inside(a) and a not in code:
            w = m.word(a); code.add(a); op = w >> 26; rs = (w >> 21) & 31; rt = (w >> 16) & 31
            if op == 0 and (w & 63) == 8: code.add(a + 4); return
            if op == 2:
                code.add(a + 4); t = ((w & 0x3FFFFFF) << 2) | (a & 0xF0000000)
                if m.inside(t): todo.append(t)
                return
            if op == 3:
                t = ((w & 0x3FFFFFF) << 2) | 0x80000000
                if m.inside(t): todo.append(t)
            if op in (4, 5, 6, 7) or (op == 1 and rt in (0, 1, 16, 17)):
                t = a + 4 + 4 * s16(w & 0xFFFF)
                if m.inside(t): todo.append(t)
                if op == 4 and rs == 0 and rt == 0: code.add(a + 4); return
            if op == 15: hi[rt] = (w & 0xFFFF) << 16
            elif op in (9, 13) and rs in hi:
                v = (hi[rs] + (w & 0xFFFF if op == 13 else s16(w & 0xFFFF))) & 0xFFFFFFFF
                if m.inside(v) and v % 4 == 0 and (m.word(v) >> 16) == 0x27BD: todo.append(v)
            a += 4
    def drain():
        while todo:
            a = todo.pop()
            if a not in roots: roots.add(a); run(a)
    drain()
    while True:
        new = [v for i, v in enumerate(m.w) if AC1_BASE + 4 * i not in code and m.inside(v) and v % 4 == 0
               and v not in roots and (m.word(v) >> 16) == 0x27BD]
        if not new: break
        todo.extend(new); drain()
    return code


def scan(m):
    code = descend(m)
    refs = collections.Counter(); fields = collections.Counter(); hi = {}
    for a in sorted(code):
        w = m.word(a); op = w >> 26; rs = (w >> 21) & 31; rt = (w >> 16) & 31; im = w & 0xFFFF
        if op == 3:
            refs[('jal', ((w & 0x3FFFFFF) << 2) | 0x80000000)] += 1
        elif op == 15:
            hi[rt] = im << 16; continue
        elif rs in hi and (op in (9, 13) or op in LOADSTORE):
            v = (hi[rs] + (im if op == 13 else s16(im))) & 0xFFFFFFFF
            if op in (9, 13):
                hi[rt] = v; refs[('ptr', v)] += 1; continue
            refs[('mem', v)] += 1
        elif op in LOADSTORE and rs not in (0, 28, 29) and im in OBJ_MOVED:
            fields[im] += 1
        if op in (9, 13) and rt != rs: hi.pop(rt, None)
        elif op in (32, 33, 34, 35, 36, 37, 38): hi.pop(rt, None)
        elif op == 0: hi.pop((w >> 11) & 31, None)
    data = collections.Counter()
    for i, v in enumerate(m.w):
        if AC1_BASE + 4 * i not in code and 0x80010000 <= v < 0x80200000: data[v] += 1
    return code, refs, fields, data


def classify(v, m, T):
    funcs, starts, glob, runs = T
    if m.inside(v): return 'internal', v + DELTA, ''
    if not (0x80000000 <= v < 0x80200000 or 0x1F800000 <= v < 0x1F800400): return 'const', None, ''
    if v in DATA_FIX: return 'data', DATA_FIX[v], 'engine table (by hand)'
    if 0x8008E530 < v < 0x8008E530 + 8 * 28:      # inside the 28-byte effect records (AC1 8, PP 9)
        return 'data', 0x8008D42C + v - 0x8008E530, 'engine table (by hand)'
    if STUBS[0] <= v <= STUBS[1]: return 'code', STUBS[2] + v - STUBS[0], 'bios stub high'
    for b, pb in zip(BLOCKS, PP_BLOCKS):
        if b <= v < b + 0x39C:
            o = block_off(v - b)
            return ('data', pb + o, 'player block +0x%X' % (v - b)) if o is not None else \
                   ('data?', None, 'player block +0x%X' % (v - b))
    for name, b, st, cnt, pb in ARRAYS:
        if b <= v < b + st * cnt:
            k, f = divmod(v - b, st)
            if name == 'object': f = OBJ_MOVED.get(f, f)
            return 'data', pb + k * st + f, '%s %d +0x%X' % (name, k, f)
    mod = 'exe' if EXE[0] <= v < EXE[1] else ('FDAT 202' if AC1_ENG[0] <= v < AC1_ENG[1] else None)
    if mod and (mod, v) in FIX: return 'code', FIX[(mod, v)], 'fixed high'
    if mod:
        f = funcs[mod].get(v)
        if f and f[2] not in ('only_ac1',) and f[3] != 'data': return 'code', f[1], '%s %s' % (f[2], f[3])
        if f and f[2] == 'only_ac1': return 'code', None, 'only_ac1 -'
        g = glob.get(v)
        if g: return 'data', g[0], 'globals ' + g[1]
        st = starts[mod]; i = bisect.bisect_right(st, v) - 1
        if i >= 0 and (EXE[0] <= v < 0x8003A000 or mod == 'FDAT 202'):
            f = funcs[mod][st[i]]
            if f[3] == 'data' or f[2] == 'data':
                return 'data', (f[1] + v - st[i]) if f[1] else None, 'in data cut %08X' % st[i]
            return 'mid', None, 'inside %08X' % st[i]
        return 'unknown', None, ''
    g = glob.get(v)
    if g: return 'data', g[0], 'globals ' + g[1]
    i = bisect.bisect_right(runs, (v, 0xFFFFFFFF, 0)) - 1
    if i >= 0 and runs[i][0] <= v <= runs[i][1] + 0x40:
        return 'data', v + runs[i][2], 'run %+d' % runs[i][2]
    return 'data?', None, 'no pair'


def blocks(d, n=19):
    out, o = [], 0
    while o + 4 <= len(d) and len(out) < n:
        k = struct.unpack_from('<I', d, o)[0]; out.append(d[o + 4:o + 4 + k]); o += 4 + k
    return out


def cmd_maps(work):
    """block 10 (14-byte records: AC1 256, PP 1024), placements (block 12, 40 B), points (block 9, 32 B), items"""
    f = lambda disc, e: open(os.path.join(work, disc, 'unpacked/GG/COM/FDAT/%03d.bin' % e), 'rb').read()
    mx = collections.Counter()
    for s in SLOTS:
        b = blocks(f('ac1-disc', 2 * s + 1))
        v = len(b[10]) // 14
        used10 = max([i + 1 for i in range(v) if b[10][14 * i:14 * i + 14] != b[10][-14:]] + [0])
        plc = [i for i in range(len(b[12]) // 40) if struct.unpack_from('<h', b[12], 40 * i + 6)[0] != -1]
        pts = [i for i in range(len(b[9]) // 32) if struct.unpack_from('<h', b[9], 32 * i + 6)[0] != -1]
        items = [(i, b[9][32 * i + 0x16], struct.unpack_from('<H', b[9], 32 * i + 0x18)[0]) for i in pts
                 if b[9][32 * i + 0x16] in (5, 6, 7)]
        mx['block10'] = max(mx['block10'], v); mx['block10 used'] = max(mx['block10 used'], used10)
        mx['placement'] = max(mx['placement'], max(plc + [-1])); mx['point'] = max(mx['point'], max(pts + [-1]))
        mx['types'] = max(mx['types'], struct.unpack_from('<H', b[11], 0)[0])
        mx['classes'] = max(mx['classes'], struct.unpack_from('<I', b[8], 0)[0])
        print('slot %2d: block sizes %s | block10 %d records (%d used) | item points (point, kind, owned index) %s' % (
            s, [len(x) for x in b], v, used10, items))
    pb = blocks(f('pp-disc', 4))
    print('max over AC1 maps:', dict(mx), '| PP map 0 block 10 records:', len(pb[10]) // 14)


def cmd_common(work):
    f = lambda disc: blocks(open(os.path.join(work, disc, 'unpacked/GG/COM/FDAT/200.bin'), 'rb').read(), 8)
    A, P = f('ac1-disc'), f('pp-disc')
    def msgs(b):
        first = struct.unpack_from('<H', b, 0x14)[0]; n = (first - 0x14) // 2
        o = [struct.unpack_from('<H', b, 0x14 + 2 * i)[0] for i in range(n)] + [len(b)]
        return [b[o[i]:o[i + 1]] for i in range(n)]
    for k in (4, 5, 6):
        a, p = msgs(A[k]), msgs(P[k])
        print('message set %d: AC1 %d entries, PP %d; AC1 n == PP n+50: %d, AC1 n == PP n: %d, header same %s' % (
            k - 4, len(a), len(p), sum(a[i] == p[i + 50] for i in range(len(a))), sum(a[i] == p[i] for i in range(len(a))),
            A[k][:0x14] == P[k][:0x14]))
    ra = [struct.unpack_from('<hhhhH', A[3], 448 + 10 * i) for i in range(256)]
    rp = [struct.unpack_from('<hhhhH', P[3], 448 + 10 * i) for i in range(256)]
    diff = [i for i in range(256) if ra[i] != rp[i]]
    print('sound-id records the same: %d of 256; first id that differs: %d; ids < 56 that differ: %s' % (
        256 - len(diff), diff[0], [i for i in diff if i < 56]))
    def items(b):
        n = struct.unpack_from('<I', b, 0)[0]; o = 4; out = []
        for i in range(n):
            z = struct.unpack_from('<I', b, o)[0]; out.append(b[o:o + z]); o += z
        return out
    ma, mp = items(A[0]), items(P[0])
    print('common models (slot: AC1 size / PP size / bytes differing):', [
        (hex(0x28 + i), len(x), len(y), sum(c != d for c, d in zip(x, y))) for i, (x, y) in enumerate(zip(ma, mp))
        if len(x) > 4 or len(y) > 4])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--work', default=os.environ.get('ACG1_WORK', '.'))
    ap.add_argument('--slots', default=','.join(map(str, SLOTS)))
    ap.add_argument('--tsv'); ap.add_argument('--list', action='store_true')
    ap.add_argument('--maps', action='store_true'); ap.add_argument('--common', action='store_true')
    a = ap.parse_args()
    if a.maps: return cmd_maps(a.work)
    if a.common: return cmd_common(a.work)
    T = load_tables()
    rows, tot, allf = [], collections.Counter(), collections.Counter()
    called = collections.defaultdict(lambda: [0, set()])
    nopair = collections.defaultdict(lambda: [0, set()])
    for s in map(int, a.slots.split(',')):
        m = Mod(open(os.path.join(a.work, 'ac1-disc/unpacked/GG/COM/FDAT/%03d.bin' % (2 * s)), 'rb').read())
        if not m.n: continue
        code, refs, fields, data = scan(m)
        c = collections.Counter(); conf = collections.Counter()
        for (k, v), cnt in list(refs.items()) + [(('word', v), cnt) for v, cnt in data.items()]:
            cl, pp, note = classify(v, m, T)
            if cl == 'code':
                conf[note.split()[-1]] += cnt
                if k in ('jal', 'ptr', 'word'):
                    called[(v, pp, note)][0] += cnt; called[(v, pp, note)][1].add(s)
            if cl == 'const': continue
            if cl in ('data?', 'mid', 'unknown') or (cl == 'code' and pp is None):
                nopair[(v, cl, note)][0] += cnt; nopair[(v, cl, note)][1].add(s)
            c[(k, cl)] += cnt
            rows.append((s, k, '%08X' % v, cl, '%08X' % pp if pp else '', note, cnt))
        allf.update(fields); tot.update(c)
        print('slot %2d: %5d B, code %4d words | ' % (s, 4 * m.n, len(code)) +
              ' '.join('%s/%s=%d' % (k, cl, x) for (k, cl), x in sorted(c.items())) +
              ' | engine refs by conf: ' + ' '.join('%s=%d' % kv for kv in sorted(conf.items())) +
              ' | moved-field sites %d' % sum(fields.values()))
    print('TOTAL', ' '.join('%s/%s=%d' % (k, cl, x) for (k, cl), x in sorted(tot.items())))
    print('moved-field sites (+0xF2/F4/F6, any record):', dict((hex(k), v) for k, v in sorted(allf.items())))
    by = collections.Counter(n.split()[-1] for (_, _, n) in called)
    print('distinct engine/exe routines referenced: %d  by conf: %s' % (len(called), dict(by)))
    print('unpaired / unsure targets: %d' % len(nopair))
    for (v, cl, note), (cnt, ss) in sorted(nopair.items()):
        print('  %08X %-8s %-22s sites %4d slots %s' % (v, cl, note, cnt, ','.join(map(str, sorted(ss)))))
    # every PP target must be a routine start: previous two words end a routine (jr ra + delay) or it opens a frame
    pp = {'exe': open(os.path.join(a.work, 'pp-disc/SLUS_006.70'), 'rb').read()[0x800:],
          'FDAT 202': open(os.path.join(a.work, 'pp-disc/unpacked/GG/COM/FDAT/202.bin'), 'rb').read()}
    def pw(t):
        m, b = ('exe', 0x80011000) if t < 0x800490D0 else ('FDAT 202', 0x800490D0)
        o = t - b
        return [struct.unpack_from('<I', pp[m], o + 4 * k)[0] for k in (-2, -1, 0)]
    bad = []
    for (v, t, note), (cnt, ss) in sorted(called.items()):
        if t is None: continue
        w2, w1, w0 = pw(t)
        if not (w2 == 0x03E00008 or w1 == 0x03E00008 or (w0 >> 16) == 0x27BD or w2 >> 26 == 2):
            bad.append((v, t, note, cnt))
    print('PP targets that may not be a routine start: %d' % len(bad))
    for v, t, note, cnt in bad: print('  %08X -> PP %08X  %s  sites %d' % (v, t, note, cnt))
    if a.list:
        for (v, pp, note), (cnt, ss) in sorted(called.items()):
            print('  call %08X -> PP %s  %-16s sites %4d slots %d' % (v, '%08X' % pp if pp else '-' * 8, note, cnt, len(ss)))
    if a.tsv:
        with open(a.tsv, 'w') as f:
            f.write('# ac1_mission_refs.py: AC1 mission code references -> PP. Addresses only.\n')
            f.write('slot\tkind\tac1_addr\tclass\tpp_addr\tnote\tcount\n')
            for r in rows: f.write('\t'.join(map(str, r)) + '\n')


if __name__ == '__main__':
    main()

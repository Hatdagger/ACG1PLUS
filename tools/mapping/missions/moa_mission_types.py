#!/usr/bin/env python3
"""Prototype: decide, for every load/store in a MoA-US mission module, which record its base register points at.

Why: PP's mission objects are 368 bytes, MoA's 376, and most fields from +0x43 on sit 8-9 bytes lower on PP
(docs/research/xdisc/struct-ac-object.tsv). Map sections, event points and placement records keep their layout.
A load at +0x48 means a different PP offset if the register holds an object, and the same offset if it holds a
section. The old PP->MoA converter guessed this per register and missed sites; this tool does a small dataflow
pass and reports how many sites it can decide, so the plan can say how much is left for a person.

Types: OBJ (mission object, pool slot), SEC (map section record), CTL (section/point controller block),
PT (event point), PLC (placement record). Sources:
  * module table roles: hooks +0x10 (obj), type handlers +0x24..+0x5C (obj), section handlers +0x60..+0x9C (sec),
    point handlers +0xA0.. (pt);
  * routines stored into an object's callback slots (+0x54..+0x64) get a0 = OBJ; into a controller's +0/+4 get SEC/PT;
  * engine returns: find-slot / spawn / find-by-record -> OBJ, controller alloc -> CTL;
  * the current-object global; internal calls pass argument types on when every caller agrees.
  python3 -I tools/mapping/missions/moa_mission_types.py [--work DIR] [--mods 0,1] [--show-unknown N]
Counts only (no game bytes are printed except addresses).
"""
import argparse, collections, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import moa_mission_refs as R

CALLER_SAVED = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 31, 1}
# MoA-US FDAT 207 engine routines whose return value type we know (missions.md)
RET = {0x8007EA24: 'OBJ', 0x8007ECEC: 'OBJ', 0x8007EACC: 'OBJ', 0x80077D28: 'CTL', 0x8007927C: 'CTL'}
CUR_OBJ = 0x801EA9CC
POOL = (0x801E7248, 0x801E7248 + 16 * 376)
SEC = (0x801CFAB0, 0x801CFAB0 + 512 * 64)
PTS = (0x801E49D8, 0x801E49D8 + 128 * 52)
PLC = (0x801E6748, 0x801E6748 + 64 * 44)
OBJ_CB = {0x54, 0x58, 0x5C, 0x60, 0x64}

def s16(x): return x - 0x10000 if x & 0x8000 else x

def is_delay(w):
    op = w >> 26; fn = w & 63; rt = (w >> 16) & 31
    return op in (2, 3, 4, 5, 6, 7, 20, 21, 22, 23) or (op == 0 and fn in (8, 9)) or (op == 1 and rt in (0, 1, 16, 17))

def run_func(m, entry, argt, code):
    """dataflow over one routine. argt = dict reg->type at entry. Returns base type per memory op,
    argument types passed to internal callees, and callbacks stored into records."""
    states = {entry: dict(argt)}; work = [entry]; base = {}; calls = collections.defaultdict(list); cbs = []
    guard = 0
    while work and guard < 300000:
        a = work.pop(); guard += 1
        if not m.inside(a): continue
        st = dict(states[a]); w = m.word(a); op = w >> 26; rt = (w >> 16) & 31; fn = w & 63
        if is_delay(w):
            if m.inside(a + 4): step(m, a + 4, m.word(a + 4), st, base, calls, cbs)
            if op == 3:
                t = ((w & 0x3FFFFFF) << 2) | 0x80000000
                if m.inside(t): calls[t].append({r: st.get(r) for r in (4, 5, 6, 7)})
                else:
                    for r in (4, 5, 6, 7):
                        f = st.get(('fn', r))
                        if f: calls[('ext', f)].append(None)
                for r in CALLER_SAVED: st.pop(r, None); st.pop(('hi', r), None); st.pop(('fn', r), None); st.pop(('k', r), None)
                if t in RET: st[2] = RET[t]
                nxt = [a + 8]
            elif op == 0 and fn == 9:
                for r in CALLER_SAVED: st.pop(r, None); st.pop(('hi', r), None); st.pop(('fn', r), None); st.pop(('k', r), None)
                nxt = [a + 8]
            elif op == 0 and fn == 8: nxt = []
            elif op == 2: nxt = [((w & 0x3FFFFFF) << 2) | (a & 0xF0000000)]
            else:
                t = a + 4 + 4 * s16(w & 0xFFFF)
                nxt = [t] if (op == 4 and (w >> 16) & 0x3FF == 0) else [a + 8, t]
        else:
            step(m, a, w, st, base, calls, cbs); nxt = [a + 4]
        for n in nxt:
            if not m.inside(n): continue
            if n in states:
                old = states[n]; new = {r: t for r, t in old.items() if st.get(r) == t}
                if new != old: states[n] = new; work.append(n)
            else:
                states[n] = dict(st); work.append(n)
    return base, calls, cbs

def drop(st, r):
    for k in (r, ('hi', r), ('fn', r), ('k', r)): st.pop(k, None)

def step(m, a, w, st, base, calls, cbs):
    op = w >> 26; rs = (w >> 21) & 31; rt = (w >> 16) & 31; rd = (w >> 11) & 31; im = w & 0xFFFF
    if op == 15:
        drop(st, rt); st[('hi', rt)] = im << 16; st[('k', rt)] = im << 16; return
    if op in R.LOADSTORE:
        if rs not in (0, 28, 29):
            k = st.get(('k', rs))
            t = ('ABS', (k + s16(im)) & 0xFFFFFFFF) if k is not None else st.get(rs)
            prev = base.get(a)
            base[a] = t if prev in (None, t) else 'MIX'
        if op == 43 and st.get(rs) == 'OBJ' and im in OBJ_CB and st.get(('fn', rt)):
            cbs.append((st[('fn', rt)], 'OBJ'))
        if op == 43 and st.get(rs) == 'CTL' and im in (0, 4) and st.get(('fn', rt)):
            cbs.append((st[('fn', rt)], 'CTLOWNER'))
        if rs == 29 and op == 43:
            st[('sp', im)] = st.get(rt); return
        if rs == 29 and op == 35:
            v = st.get(('sp', im)); drop(st, rt)
            if v: st[rt] = v
            return
        if op in (32, 33, 34, 35, 36, 37, 38):
            k = st.get(('k', rs)); drop(st, rt)
            if k is not None and op == 35 and ((k + s16(im)) & 0xFFFFFFFF) == CUR_OBJ: st[rt] = 'OBJ'
        return
    if op in (9, 13):
        k = st.get(('k', rs)); t = st.get(rs)
        drop(st, rt)
        if k is not None:
            v = (k + (im if op == 13 else s16(im))) & 0xFFFFFFFF
            st[('k', rt)] = v
            if m.inside(v): st[('fn', rt)] = v
            elif POOL[0] <= v < POOL[1] and (v - POOL[0]) % 376 == 0: st[rt] = 'OBJ'
            elif SEC[0] <= v < SEC[1] and (v - SEC[0]) % 64 == 0: st[rt] = 'SEC'
            elif PTS[0] <= v < PTS[1] and (v - PTS[0]) % 52 == 0: st[rt] = 'PT'
            elif PLC[0] <= v < PLC[1] and (v - PLC[0]) % 44 == 0: st[rt] = 'PLC'
            return
        if op == 9 and im == 0 and t: st[rt] = t
        return
    if op == 0:
        fn = w & 63
        if fn in (33, 37, 32) and (rt == 0 or rs == 0):
            src = rs if rt == 0 else rt
            vals = {k: st.get(k) for k in ('t', 'hi', 'fn', 'k')}
            t = st.get(src); f = st.get(('fn', src)); h = st.get(('hi', src)); k = st.get(('k', src))
            drop(st, rd)
            if t: st[rd] = t
            if f is not None: st[('fn', rd)] = f
            if h is not None: st[('hi', rd)] = h
            if k is not None: st[('k', rd)] = k
            return
        if fn in (8, 9, 24, 25, 26, 27, 17, 19): return
        drop(st, rd); return
    if op == 3:
        t = ((w & 0x3FFFFFF) << 2) | 0x80000000
        if m.inside(t): calls[t].append({})
        else:
            for r in (4, 5, 6, 7):
                f = st.get(('fn', r))
                if f: calls[('ext', f)].append(None)
        return
    if op in (8, 10, 11, 12, 14):
        drop(st, rt)

def analyse(m, code):
    # roles from the module's own table
    entry = {}
    for i in range(0, 0xE0 // 4):
        v = m.w[i]
        if not m.inside(v):
            if v != 0: break
            continue
        off = 4 * i
        if off >= 0xA0 and (m.word(v) >> 16) != 0x27BD and m.word(v) != 0x03E00008: break  # past the point slots
        role = 'OBJ' if (0x24 <= off <= 0x5C or off == 0x10) else ('SEC' if 0x60 <= off <= 0x9C else ('PT' if off >= 0xA0 else None))
        entry.setdefault(v, {})
        if role: entry[v][4] = role
    # internal jal targets and data-pointer roots
    for a in sorted(code):
        w = m.word(a)
        if w >> 26 == 3:
            t = ((w & 0x3FFFFFF) << 2) | 0x80000000
            if m.inside(t): entry.setdefault(t, {})
    for i, v in enumerate(m.w):
        if m.inside(v) and v in code and (m.word(v) >> 16) == 0x27BD: entry.setdefault(v, {})
    for a in code:
        if (m.word(a) >> 16) == 0x27BD and (m.word(a) & 0x8000) and a not in entry:
            entry[a] = {}
    fixed = {k: dict(v) for k, v in entry.items()}
    bases = {}
    for it in range(6):
        callargs = collections.defaultdict(list); cbroles = collections.defaultdict(set)
        bases = {}
        for f, args in entry.items():
            b, calls, cbs = run_func(m, f, args, code)
            for a, t in b.items():
                if a in bases and bases[a] != t: bases[a] = 'MIX'
                else: bases[a] = t
            for t, lst in calls.items():
                if isinstance(t, tuple): cbroles[t[1]].add('OBJ'); continue
                lst = [c for c in lst if c]
                callargs[t].extend(lst)
            for fn, role in cbs:
                if role == 'CTLOWNER': role = args.get(4) if args.get(4) in ('SEC', 'PT') else 'SEC'
                cbroles[fn].add(role)
        new = {}
        for f in entry:
            args = dict(fixed.get(f, {}))
            if f in callargs:
                for r in (4, 5, 6, 7):
                    vals = {c.get(r) for c in callargs[f]}
                    if len(vals) == 1 and None not in vals and r not in args: args[r] = vals.pop()
            if f in cbroles and len(cbroles[f]) == 1 and 4 not in args: args[4] = next(iter(cbroles[f]))
            new[f] = args
        if new == entry: break
        entry = new
    return bases

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--work', default='/home/claude/work'); ap.add_argument('--mods', default=','.join(map(str, range(20))))
    ap.add_argument('--show-unknown', type=int, default=0)
    a = ap.parse_args()
    moved = R.load_fields()
    tot = collections.Counter()
    for n in map(int, a.mods.split(',')):
        m = R.Mod(open(os.path.join(a.work, 'ma-disc/unpacked/GG/COM/FDAT/%03d.bin' % (5 * n)), 'rb').read())
        dead = set(); code = R.descend(m, dead); live = code - dead
        bases = analyse(m, code)
        c = collections.Counter(); unk = []
        for ad in sorted(live):
            w = m.word(ad); op = w >> 26; rs = (w >> 21) & 31; im = w & 0xFFFF
            if op in R.LOADSTORE and rs not in (0, 28, 29) and im < 0x400 and (im in moved or 0x3A <= im <= 0x42):
                t = bases.get(ad) or 'unknown'
                if isinstance(t, tuple): t = 'ABS'
                c[t] += 1
                if t in ('unknown', 'MIX'): unk.append(ad)
        tot.update(c); s = sum(c.values())
        print('mission %2d: %4d moved-field sites | ' % (n, s) + ' '.join('%s %d' % kv for kv in sorted(c.items())))
        if a.show_unknown: print('   unknown e.g.', ' '.join('%08X' % x for x in unk[:a.show_unknown]))
    s = sum(tot.values())
    print('TOTAL %d sites | ' % s + ' '.join('%s %d (%d%%)' % (k, v, 100 * v // s) for k, v in sorted(tot.items())))

if __name__ == '__main__':
    main()

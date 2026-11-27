"""Turn one AC1 mission code file (FDAT 2n, linked at 0x801C4B40) into a file Phantasma's missions engine can run
(loaded at 0x80097A38). Runs on the player's PC from their own disc; the result is game code and never goes on
GitHub (docs/no-game-code.md).

What it changes (docs/research/ac1/import-outlook.md §3, §8):
  - every address inside the file moves by PP base - AC1 base (calls, jumps, lui/lo pairs, pointer words in data)
  - every call into AC1's engine or boot program goes to the Phantasma twin (functions.tsv + the FIX table)
  - every engine data address goes to the Phantasma address (record arrays, player blocks, globals, runs)
The checker refuses to write a file while anything is undecided: an unpaired or unchecked low-confidence address,
a lui whose uses can't all be found, a pointer whose offsets would land in the wrong place, a call that doesn't
land on a routine start. It lists what stopped it instead.

Pairing tables come from tools/xdisc/ac1_mission_refs.py (the AC1 mapping chat's survey tool, imported as is).
"""
import collections
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'xdisc'))
import ac1_mission_refs as R  # noqa: E402

AC1_BASE, PP_BASE, DELTA = R.AC1_BASE, R.PP_BASE, R.DELTA
PP_ROOM = 55296          # PP's mission code room, 0x80097A38 .. 0x800A5237 (missions-import-pp.md §1)

# Low-confidence pairs read side by side by eye (2026-11-26, AC1 import chat). Only these low pairs are accepted;
# any other low pair stops the converter until someone checks it and adds it here with a reason.
CHECKED_LOW = {
    0x8004C318: (0x8004A664, 'set M+0xC = a0, M+0x10 = 100 if zero: same code, M fields'),
    0x800529F0: (0x80050ED0, 'show message n: same code, PP reads the table through 0x800A5240+0xB0'),
    0x80073C2C: (0x800719CC, '780 vs 776 bytes, opcodes 95% alike, load/store offsets 88% alike; reordered'),
    0x800745D0: (0x80072360, 'same body, instructions reordered'),
    0x800781E8: (0x80075F60, 'same body, branch sense flipped'),
    0x8008A80C: (0x80088760, '"combat system engaged": same code, message 101 vs 151 (+50)'),
    0x8004ECA8: (0x8004D280, 'step an animation frame: same code, AC1 keeps an unused stack frame'),
    0x800869F0: (0x80084598, 'same code, registers renamed; callees pair (80086854, 80014D78)'),
    0x800873F8: (0x80084F44, 'same code: other stack layout and engine addresses, all of them paired ones'),
}

# AC1 engine routines whose Phantasma twin behaves differently. A copy of AC1's own routine is put at the end of
# the converted mission file and the calls go there. Only small leaf routines without addresses in them qualify
# (the converter checks: no calls, no lui, jumps only inside the routine).
HELPERS = {
    0x8004EBF8: (84, 'animation at its end? AC1 also answers for animations playing backwards (speed <= 0); '
                     "PP's twin 8004D1EC only checks forwards"),
}
# Addresses whose pair depends on how they are used. Indexed reads (base + computed offset) of these mean the
# table below, not the globals.tsv pair (which is for a register base the engine keeps nearby).
INDEXED = {
    0x801AC860: (0x801A8000, 'bit set, one bit per id: PP engine 0x8005C5C4 reads 0x801A15B8 + 0x6A48 '
                             '(AC1 twin 0x8005E434 reads 0x801AC860 directly); same -0x4860 shift as its neighbours'),
}
ACCEPT_CONF = ('high', 'medium')
CALLEE_SAVED = {16, 17, 18, 19, 20, 21, 22, 23, 28, 29, 30}
LOADSTORE = set(range(32, 47))
LOADS = set(range(32, 39))


class Undecided(Exception):
    pass


def s16(x):
    return x - 0x10000 if x & 0x8000 else x


class Image:
    """a mission file seen at a load address"""
    def __init__(self, data, base):
        self.base = base
        self.size = len(data)
        d = data + b'\0' * (-len(data) % 4)
        self.w = list(struct.unpack('<%dI' % (len(d) // 4), d))
        used = data.rstrip(b'\0')
        self.end = base + len(used) + (-len(used) % 4)
        self.helpers_at = self.end                 # set lower when AC1 helper routines were appended

    def inside(self, a):
        return self.base <= a < self.end

    def word(self, a):
        return self.w[(a - self.base) // 4]

    def bytes(self):
        return struct.pack('<%dI' % len(self.w), *self.w)[:self.size]


def walk(img):
    """routines reached from the module's pointer table (+0x00 .. +0xDC) and from code pointers.
    Same walk as ac1_mission_refs.descend, at any base. Returns (code addresses, labels)."""
    code, todo, roots, labels = set(), [], set(), set()
    for i in range(min(len(img.w), 0xE0 // 4)):
        v = img.w[i]
        if img.inside(v) and v % 4 == 0:
            todo.append(v)

    def run(a):
        hi = {}
        while img.inside(a) and a not in code:
            w = img.word(a); code.add(a); op = w >> 26; rs = (w >> 21) & 31; rt = (w >> 16) & 31
            if op == 0 and (w & 63) == 8:
                code.add(a + 4); return
            if op == 2:
                code.add(a + 4); t = ((w & 0x3FFFFFF) << 2) | (a & 0xF0000000)
                if img.inside(t): todo.append(t); labels.add(t)
                return
            if op == 3:
                t = ((w & 0x3FFFFFF) << 2) | 0x80000000
                if img.inside(t): todo.append(t)
            if op in (4, 5, 6, 7) or (op == 1 and rt in (0, 1, 16, 17)):
                t = a + 4 + 4 * s16(w & 0xFFFF)
                if img.inside(t): todo.append(t); labels.add(t)
                if op == 4 and rs == 0 and rt == 0:
                    code.add(a + 4); return
            if op == 15:
                hi[rt] = (w & 0xFFFF) << 16
            elif op in (9, 13) and rs in hi:
                v = (hi[rs] + (w & 0xFFFF if op == 13 else s16(w & 0xFFFF))) & 0xFFFFFFFF
                if img.inside(v) and v % 4 == 0 and (img.word(v) >> 16) == 0x27BD:
                    todo.append(v)
            a += 4

    def drain():
        while todo:
            a = todo.pop()
            if a not in roots:
                roots.add(a); labels.add(a); run(a)
    drain()
    seen_switch = set()
    while True:
        new = [v for i, v in enumerate(img.w) if img.base + 4 * i not in code and img.inside(v) and v % 4 == 0
               and v not in roots and (img.word(v) >> 16) == 0x27BD]
        for a in sorted(code):
            w = img.word(a)
            if a not in seen_switch and w >> 26 == 0 and w & 63 == 8 and (w >> 21) & 31 != 31:
                seen_switch.add(a)
                for t in switch_table(img, code, a):
                    if t not in roots:
                        new.append(t)
        if not new:
            # routines nobody points at by a frame (leaf callbacks): the code area is one piece between the first
            # and the last routine found, so walk every gap in it from its first non-zero word
            cs = sorted(a for a in code if a < img.helpers_at)
            for x, y in zip(cs, cs[1:]):
                if y - x > 4:
                    g = x + 4
                    while g < y and img.word(g) == 0:
                        g += 4
                    if g < y and g not in roots:
                        new.append(g)
        if not new:
            break
        todo.extend(new); drain()
    walk.switches = seen_switch
    cs = sorted(a for a in code if a < img.helpers_at)
    walk.uncovered = [a for a in range(cs[0], cs[-1], 4) if a not in code and img.word(a) != 0] if cs else []
    return code, labels


def switch_table(img, code, jr):
    """targets of a jump table behind `jr rX`: lw rX, lo(rT) with rT = (lui hi) + index, bound from sltiu"""
    def back(a, reg, limit=12):
        for k in range(1, limit + 1):
            b = a - 4 * k
            if b not in code:
                return None, None
            if dest_reg(img.word(b)) == reg:
                return b, img.word(b)
        return None, None
    rx = (img.word(jr) >> 21) & 31
    b, w = back(jr, rx)
    if w is None or w >> 26 != 35:
        return []
    lo, rt = s16(w & 0xFFFF), (w >> 21) & 31
    b2, w2 = back(b, rt)
    if w2 is None or w2 >> 26 != 0 or w2 & 63 not in (0x20, 0x21):
        return []
    base = None
    for r in ((w2 >> 21) & 31, (w2 >> 16) & 31):
        b3, w3 = back(b2, r, 20)
        if w3 is not None and w3 >> 26 == 15:
            base = (((w3 & 0xFFFF) << 16) + lo) & 0xFFFFFFFF
    if base is None or not img.inside(base):
        return []
    count = None
    for k in range(1, 24):
        c = jr - 4 * k
        if c not in code:
            break
        wc = img.word(c)
        if wc >> 26 == 11:                                  # sltiu rZ, rI, N  (index < N)
            count = wc & 0xFFFF
            break
    out = []
    i = 0
    while (count is None or i < count) and img.inside(base + 4 * i):
        t = img.word(base + 4 * i)
        if not img.inside(t) or t % 4:
            break
        out.append(t); i += 1
    if count is not None and len(out) != count:
        raise Undecided('jump table at %08X for %08X: %d entries, bound says %d' % (base, jr, len(out), count))
    return out


def dest_reg(w):
    """GPR an instruction writes, or None"""
    op = w >> 26
    if op == 0:
        f = w & 63
        if f in (8, 0x18, 0x19, 0x1A, 0x1B, 0x11, 0x13, 0x0C, 0x0D):   # jr, mult/div, mthi/mtlo, syscall, break
            return None
        return (w >> 11) & 31
    if op == 1:
        return 31 if (w >> 16) & 31 in (16, 17) else None
    if op == 3:
        return 31
    if 8 <= op <= 15:
        return (w >> 16) & 31
    if op in (16, 18) and (w >> 21) & 31 in (0, 2):                 # mfc / cfc
        return (w >> 16) & 31
    if op in LOADS:
        return (w >> 16) & 31
    return None


def sites(img, code, labels):
    """every place that builds an address: lui and its uses, pointers made from them, calls and jumps.
    State is dropped at every label and after every jump, so a use is only paired with a lui on the same
    straight run of code; a lui left without a use is reported, not guessed."""
    luis = {}                                   # lui addr -> [(use addr, kind, value)]
    derived = []                                # (use addr, pointer value, offset)
    jumps = []                                  # (addr, op, target)
    gp = []
    hi, ptr, idx = {}, {}, {}
    after_jump = after_call = None
    for a in sorted(code):
        if a in labels or (after_jump is not None and a > after_jump):
            hi, ptr, idx = {}, {}, {}
            after_jump = after_call = None
        elif after_call is not None and a > after_call:
            # past a call and its delay slot: only callee-saved registers still hold their values
            hi, ptr, idx = ({r: v for r, v in d.items() if r in CALLEE_SAVED} for d in (hi, ptr, idx))
            after_call = None
        w = img.word(a); op = w >> 26; rs = (w >> 21) & 31; rt = (w >> 16) & 31; im = w & 0xFFFF
        if op in (2, 3):
            jumps.append((a, op, ((w & 0x3FFFFFF) << 2) | (a & 0xF0000000)))
        if rs == 28 and (op in LOADSTORE or op in (9, 13)):
            gp.append(a)
        rd = dest_reg(w)
        new_hi = new_ptr = new_idx = None
        if op == 15:
            luis.setdefault(a, [])
            new_hi = (a, im << 16)
        elif op in (9, 13) and rs in hi:
            l, h = hi[rs]
            v = (h + (im if op == 13 else s16(im))) & 0xFFFFFFFF
            luis[l].append((a, 'lo', v))
            new_ptr = v
        elif op in LOADSTORE and rs in hi:
            l, h = hi[rs]
            luis[l].append((a, 'lo', (h + s16(im)) & 0xFFFFFFFF))
            if op not in LOADS and rt in hi:        # storing the half-built address itself
                luis[hi[rt][0]].append((a, 'other', None))
        elif op in LOADSTORE and rs in idx:
            l, h = idx[rs]
            luis[l].append((a, 'idx', (h + s16(im)) & 0xFFFFFFFF))
        elif (op in LOADSTORE or op == 9) and rs in ptr:
            derived.append((a, ptr[rs], s16(im)))
            if op == 9:
                new_ptr = (ptr[rs] + s16(im)) & 0xFFFFFFFF
        elif op == 0 and (w & 63) in (0x20, 0x21) and (rs in hi) != (rt in hi) and rs not in idx and rt not in idx:
            new_idx = hi[rs] if rs in hi else hi[rt]
        else:
            # any other read of a register holding a bare high half: we can't follow it
            reads = (rs, rt) if op == 0 or op in (4, 5) or op in LOADSTORE and op not in LOADS else (rs,)
            if op in (2, 3, 15):
                reads = ()
            for r in reads:
                if r in hi:
                    luis[hi[r][0]].append((a, 'other', None))
                if r in idx:
                    luis[idx[r][0]].append((a, 'other', None))
        if rd is not None and rd != 0:
            hi.pop(rd, None); ptr.pop(rd, None); idx.pop(rd, None)
            if new_hi: hi[rd] = new_hi
            if new_ptr is not None: ptr[rd] = new_ptr
            if new_idx: idx[rd] = new_idx
        if op == 3 or (op == 0 and (w & 63) == 9):
            after_call = a + 4
        if op == 2 or (op == 0 and (w & 63) == 8):
            after_jump = a + 4
    return luis, derived, jumps, gp


class Mapper:
    def __init__(self, img):
        self.img = img
        self.T = R.load_tables()
        self.log = collections.Counter()

    def map(self, v, call=False):
        """AC1 address -> PP address, or raise Undecided"""
        img = self.img
        if img.inside(v):
            return v + DELTA, 'internal'
        if v in CHECKED_LOW:
            return CHECKED_LOW[v][0], 'checked low'
        cl, pp, note = R.classify(v, img, self.T)
        if cl == 'const':
            return v, 'const'
        if cl in ('data?', 'mid', 'unknown') or pp is None:
            raise Undecided('%08X: %s %s' % (v, cl, note))
        conf = note.split()[-1]
        if cl == 'code':
            if conf not in ACCEPT_CONF and not note.startswith(('fixed', 'bios')):
                raise Undecided('%08X: %s pair %08X not checked (%s)' % (v, conf, pp, note))
        elif note.startswith('globals') and conf not in ACCEPT_CONF:
            raise Undecided('%08X: globals pair %08X at %s confidence' % (v, pp, conf))
        if call and cl != 'code':
            raise Undecided('%08X: called but is %s' % (v, note))
        return pp, note

    def uniform(self, v, span=0x40):
        """an indexed access through v is safe only where the area around v moves as one piece"""
        if self.img.inside(v):
            return True
        cl, _, note = R.classify(v, self.img, self.T)
        return cl == 'data' and (note.split()[0] in ('point', 'event', 'object', 'placement', 'section', 'run')
                                 or note.startswith(('point class', 'event point', 'object type', 'run')))


def pp_routine_start(t, pp_exe, pp_eng):
    """same check as ac1_mission_refs: previous words end a routine or this one opens a frame"""
    buf, base = (pp_exe, 0x80011000) if t < 0x800490D0 else (pp_eng, 0x800490D0)
    o = t - base
    if o < 8 or o + 4 > len(buf):
        return False
    w2, w1, w0 = (struct.unpack_from('<I', buf, o + 4 * k)[0] for k in (-2, -1, 0))
    return w2 == 0x03E00008 or w1 == 0x03E00008 or (w0 >> 16) == 0x27BD or w2 >> 26 == 2


_AC1_EXE = {}


def src_stub(t):
    return _AC1_EXE.get('exe'), t


def same_bios_stub(src, t, pp_exe):
    """BIOS call stubs: li t2, 0xA0/B0/C0; jr t2; li t1, n. Both sides must be the same table and number."""
    ac1_exe, a = src
    if ac1_exe is None:
        return False
    def words(buf, x):
        o = x - 0x80011000
        return struct.unpack_from('<3I', buf, o) if 0 <= o and o + 12 <= len(buf) else None
    wa, wp = words(ac1_exe, a), words(pp_exe, t)
    stub = lambda w: w and w[0] >> 16 == 0x240A and w[1] == 0x01400008 and w[2] >> 16 == 0x2409
    return stub(wa) and stub(wp) and wa[0] == wp[0] and wa[2] == wp[2]


def add_helpers(data, ac1_eng):
    """append copies of the HELPERS this file calls and point its calls at them (still at AC1's address)"""
    src = Image(data, AC1_BASE)
    code, labels = walk(src)
    want = sorted({((w & 0x3FFFFFF) << 2) | 0x80000000 for a in code for w in [src.word(a)]
                   if w >> 26 == 3 and (((w & 0x3FFFFFF) << 2) | 0x80000000) in HELPERS})
    if not want:
        return data, {}, None
    if ac1_eng is None:
        raise Undecided('helpers needed but no AC1 engine given')
    words = list(src.w)
    end = src.end + (-src.end % 16)
    words += [0] * ((end - AC1_BASE) // 4 - len(words))
    placed = {}
    for t in want:
        n, _ = HELPERS[t]
        o = t - 0x8004ADA0
        body = list(struct.unpack_from('<%dI' % (n // 4), ac1_eng, o))
        new = AC1_BASE + 4 * len(words)
        for i, w in enumerate(body):
            op = w >> 26
            if op == 3 or op == 15 or (op == 0 and w & 63 == 9):
                raise Undecided('helper %08X is not a plain leaf routine' % t)
            if op == 2:
                jt = ((w & 0x3FFFFFF) << 2) | 0x80000000
                if not t <= jt < t + n:
                    raise Undecided('helper %08X jumps outside itself' % t)
                body[i] = (w & 0xFC000000) | (((jt - t + new) >> 2) & 0x3FFFFFF)
        if body[-2] != 0x03E00008:
            raise Undecided('helper %08X does not end in jr ra' % t)
        words += body + [0] * (-len(body) % 4)
        placed[t] = new
    for a in code:
        w = words[(a - AC1_BASE) // 4]
        if w >> 26 == 3:
            jt = ((w & 0x3FFFFFF) << 2) | 0x80000000
            if jt in placed:
                words[(a - AC1_BASE) // 4] = (w & 0xFC000000) | ((placed[jt] >> 2) & 0x3FFFFFF)
    out = struct.pack('<%dI' % len(words), *words)
    return out + b'\0' * (-len(out) % 2048), placed, end


def convert(data, pp_exe, pp_eng, ac1_exe=None, ac1_eng=None):
    """returns (converted bytes, report dict); raises Undecided with every problem listed"""
    _AC1_EXE['exe'] = ac1_exe
    data, helpers, helpers_at = add_helpers(data, ac1_eng)
    src = Image(data, AC1_BASE)
    if helpers_at:
        src.helpers_at = helpers_at
    if src.end - AC1_BASE > PP_ROOM:
        raise Undecided('file uses %d bytes, PP room is %d' % (src.end - AC1_BASE, PP_ROOM))
    code, labels = walk(src)
    if walk.uncovered:
        raise Undecided('code area has %d words the walk could not follow, first at %08X' % (
            len(walk.uncovered), walk.uncovered[0]))
    luis, derived, jumps, gp = sites(src, code, labels)
    M = Mapper(src)
    out = list(src.w)
    problems, rep = [], collections.Counter()

    def put(a, w):
        out[(a - AC1_BASE) // 4] = w

    if gp:
        problems.append('gp-relative accesses at %s' % ', '.join('%08X' % a for a in gp[:8]))
    # calls and jumps
    for a, op, t in jumps:
        try:
            nt, why = M.map(t, call=True)
        except Undecided as e:
            problems.append('call at %08X -> %s' % (a, e)); continue
        if why.startswith('bios'):
            if not same_bios_stub(src_stub(t), nt, pp_exe):
                problems.append('call at %08X -> PP %08X is not the same BIOS stub' % (a, nt)); continue
        elif why != 'internal' and not pp_routine_start(nt, pp_exe, pp_eng):
            problems.append('call at %08X -> PP %08X is not a routine start' % (a, nt)); continue
        put(a, (src.word(a) & 0xFC000000) | ((nt >> 2) & 0x3FFFFFF))
        rep['calls internal' if why == 'internal' else 'calls to engine'] += 1
    # lui pairs
    for l, uses in sorted(luis.items()):
        h = (src.word(l) & 0xFFFF) << 16
        addrish = 0x80000000 <= h < 0x80200000 or h == 0x1F800000
        if not addrish:
            continue
        if not uses or any(k == 'other' for _, k, _ in uses):
            # a lui whose value we can't follow to every use: refuse if the address would move
            try:
                nt, _ = M.map(h)
            except Undecided:
                nt = None
            if nt == h and not uses:
                continue
            problems.append('lui at %08X (0x%04X): uses not all found (%s)' % (
                l, h >> 16, ', '.join('%08X %s' % (u, k) for u, k, _ in uses) or 'none'))
            continue
        new = []
        try:
            for u, k, v in uses:
                if k == 'idx' and v in INDEXED:
                    new.append((u, k, v, INDEXED[v][0])); continue
                nt, why = M.map(v)
                if k == 'idx' and not M.uniform(v):
                    raise Undecided('indexed access at %08X through %08X (%s) does not move as one piece' % (u, v, why))
                new.append((u, k, v, nt))
        except Undecided as e:
            problems.append('lui at %08X: %s' % (l, e)); continue
        ori = [src.word(u) >> 26 == 13 for u, _, _, _ in new]
        his = {(nt >> 16) if o else ((nt + 0x8000) >> 16) & 0xFFFF for (_, _, _, nt), o in zip(new, ori)}
        if len(his) != 1:
            problems.append('lui at %08X: its uses now need different high halves %s' % (
                l, ', '.join('%04X' % x for x in sorted(his)))); continue
        nh = his.pop()
        put(l, (src.word(l) & 0xFFFF0000) | nh)
        for (u, k, v, nt), o in zip(new, ori):
            lo = (nt - (nh << 16)) & 0xFFFFFFFF
            if (o and lo > 0xFFFF) or (not o and not (lo < 0x8000 or lo >= 0xFFFF8000)):
                problems.append('use at %08X: low half out of range' % u); continue
            put(u, (src.word(u) & 0xFFFF0000) | (lo & 0xFFFF))
        rep['address pairs'] += 1
    # pointers made from those: their offsets must land where they should on PP too
    for u, v, off in derived:
        try:
            a0, _ = M.map(v)
            a1, _ = M.map((v + off) & 0xFFFFFFFF)
        except Undecided as e:
            problems.append('pointer use at %08X: %s' % (u, e)); continue
        if (a1 - a0) & 0xFFFFFFFF != off & 0xFFFFFFFF:
            problems.append('pointer use at %08X: %08X%+d lands on PP %08X, offset would give %08X' % (
                u, v, off, a1, (a0 + off) & 0xFFFFFFFF))
    rep['pointer uses checked'] = len(derived)
    # pointer words in data
    for i, v in enumerate(src.w):
        a = AC1_BASE + 4 * i
        if a in code or not (0x80010000 <= v < 0x80200000):
            continue
        try:
            nt, why = M.map(v)
        except Undecided as e:
            problems.append('data word at %08X: %s' % (a, e)); continue
        out[i] = nt
        rep['data words'] += 1
    # code the walk never reached: calls in it would still point at AC1. Report; it is never run
    stray = sum(1 for i, w in enumerate(src.w) if AC1_BASE + 4 * i not in code and w >> 26 == 3
                and 0x80011000 <= (((w & 0x3FFFFFF) << 2) | 0x80000000) < 0x80200000)
    rep['unreached words that look like calls'] = stray
    rep['code words'] = len(code)
    if problems:
        raise Undecided('\n'.join(problems))
    res = Image(struct.pack('<%dI' % len(out), *out)[:src.size], PP_BASE)
    if helpers_at:
        res.helpers_at = helpers_at + DELTA
    verify(src, res, M)
    for t, a in helpers.items():
        rep['AC1 helper %08X at PP %08X' % (t, a + DELTA)] = 1
    return res.bytes(), dict(rep)


def verify(src, res, M):
    """walk the converted file at PP's address and check every address it builds is the mapped original"""
    code0, lab0 = walk(src)
    code1, lab1 = walk(res)
    if {a + DELTA for a in code0} != code1:
        raise Undecided('self-check: the converted file walks differently')
    l0, d0, j0, _ = sites(src, code0, lab0)
    l1, d1, j1, _ = sites(res, code1, lab1)
    for (a, op, t), (b, op2, t2) in zip(j0, j1):
        if b != a + DELTA or M.map(t, call=True)[0] != t2:
            raise Undecided('self-check: call at %08X' % a)
    for l, uses in l0.items():
        h = (src.word(l) & 0xFFFF) << 16
        if not (0x80000000 <= h < 0x80200000 or h == 0x1F800000):
            continue
        for (u, k, v), (u2, k2, v2) in zip(uses, l1.get(l + DELTA, [])):
            want = INDEXED[v][0] if k == 'idx' and v in INDEXED else M.map(v)[0]
            if u2 != u + DELTA or want != v2:
                raise Undecided('self-check: use at %08X builds %08X' % (u, v2 or 0))

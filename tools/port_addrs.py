#!/usr/bin/env python3
"""Find the Japanese (or any other) disc's addresses for a list of US ones.

Each side is a memory image built from recompiled game C (instruction words in
the "/* 0xADDR: 0xWORD */" comments) and/or raw files loaded at an address
(e.g. the menu module, FDAT 206, at 0x80050DD8). Code is cut into pieces at
each "jr ra" (+ delay slot); pieces whose instruction shape (immediates and
jump targets masked) is unique on both sides are paired. Inside each pair,
every address the code makes (lui+addiu/ori/load/store, jal/j targets) is
worked out on both sides and the US->other pairs are tallied, and every
instruction address is paired too. Prints, for each address asked: the best
answer, how many places agree, and the first code word there on each side.

usage: port_addrs.py --us gen=generated --us bin=FDAT-206-menu.bin@0x80050DD8 \
                     --jp gen=generated-jp --jp bin=jp-206.bin@0x80050DD8 ADDR...
"""
import argparse, collections, glob, re, struct, sys

def load(specs):
    img = {}
    for s in specs:
        kind, _, v = s.partition('=')
        if kind == 'gen':
            for f in sorted(glob.glob(v + '/*_full_*.c')):
                for a, w in re.findall(r'/\* 0x([0-9A-F]{8}): 0x([0-9A-F]{8}) \*/',
                                       open(f, encoding='utf-8', errors='replace').read()):
                    img[int(a, 16)] = int(w, 16)
        elif kind == 'bin':
            path, _, base = v.rpartition('@'); base = int(base, 0)
            data = open(path, 'rb').read(); data += b'\0' * (-len(data) % 4)
            for i, (w,) in enumerate(struct.iter_unpack('<I', data)):
                img.setdefault(base + 4 * i, w)
        else: sys.exit('bad --us/--jp: ' + s)
    return img

def pieces(img):
    addrs = sorted(img); out = []; cur = []; prev = None; cut = False
    for a in addrs:
        if prev is not None and a != prev + 4: (out.append(cur) if cur else None); cur = []; cut = False
        cur.append(a); prev = a
        if cut: out.append(cur); cur = []; cut = False
        elif img[a] == 0x03E00008: cut = True          # jr ra: cut after its delay slot
    if cur: out.append(cur)
    return [p for p in out if len(p) >= 4]

def shape(w):
    op = w >> 26
    if op == 0: return w
    if op in (2, 3): return op << 26
    return w & 0xFFFF0000

def sx(i): return i - 0x10000 if i & 0x8000 else i
LOADST = {0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x28,0x29,0x2A,0x2B,0x2E,0x32,0x3A}
def made(ws):
    hi = {}; out = []
    for w in ws:
        op = w >> 26; rs = (w >> 21) & 31; rt = (w >> 16) & 31; imm = w & 0xFFFF; a = None
        if op == 0x0F: hi[rt] = imm << 16; out.append(None); continue
        if op in (2, 3): a = 0x80000000 | ((w & 0x03FFFFFF) << 2)
        elif op in (0x09, 0x0D) and rs in hi: a = (hi[rs] + (sx(imm) if op == 9 else imm)) & 0xFFFFFFFF
        elif op in LOADST and rs in hi: a = (hi[rs] + sx(imm)) & 0xFFFFFFFF
        # forget registers this instruction writes
        if op == 0: hi.pop((w >> 11) & 31, None)
        elif op not in (1, 2, 3, 4, 5, 6, 7, 0x28, 0x29, 0x2A, 0x2B, 0x2E, 0x3A): hi.pop(rt, None)
        out.append(a if a is None or 0x80000000 <= a < 0x80200000 else None)
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--us', action='append', required=True)
    ap.add_argument('--jp', action='append', required=True)
    ap.add_argument('addr', nargs='*')
    ap.add_argument('--file', help='addresses to map, one per line (name addr)')
    o = ap.parse_args()
    us, jp = load(o.us), load(o.jp)
    up, jpp = pieces(us), pieces(jp)
    key = lambda p, im: tuple(shape(im[a]) for a in p)
    jby = collections.defaultdict(list)
    for p in jpp: jby[key(p, jp)].append(p)
    uby = collections.Counter(key(p, us) for p in up)
    tally = collections.defaultdict(collections.Counter); ins = collections.defaultdict(collections.Counter); paired = 0
    for p in up:
        k = key(p, us)
        if uby[k] != 1 or len(jby.get(k, [])) != 1: continue
        q = jby[k][0]; paired += 1
        for a, b in zip(p, q): ins[a][b] += 1             # the instruction itself
        for a, b in zip(made([us[x] for x in p]), made([jp[x] for x in q])):
            if a is not None and b is not None: tally[a][b] += 1
    print(f'# pieces paired: {paired} (US {len(up)}, other {len(jpp)})')
    want = [(x, x) for x in o.addr]
    if o.file:
        for line in open(o.file):
            line = line.split('#')[0].split()
            if line: want.append((line[0], line[-1]))
    for name, x in want:
        a = int(x, 16); c = ins.get(a) or tally.get(a)
        how = 'code' if a in ins else 'refs'
        uw = us.get(a & ~3)
        if c:
            (b, n), *rest = c.most_common(3)
            alt = '  others: ' + ', '.join(f'{y:08X}x{m}' for y, m in rest) if rest else ''
            jw = jp.get(b & ~3)
            print(f'{name:18} {a:08X} -> {b:08X}  {how} {n:<4}' +
                  (f' word US {uw:08X} / other {jw:08X}' if uw is not None and jw is not None else '') + alt)
        else:
            print(f'{name:18} {a:08X} -> ?  (no paired code uses it)')
main()

#!/usr/bin/env python3
"""Survey of mission enemies on Project Phantasma (PP) or Master of Arena (MoA-US): per mission, per object type,
what its routines call and the numbers they pass. Read-only; prints addresses and numbers only (no code bytes).

  python3 -I tools/mapping/enemies/enemy_survey.py [--work /home/claude/work] [--disc pp|ma] [--mis 0,5] [--plc]

Needs capstone. WORK holds pp-disc/ and ma-disc/ (work packs extracted, setup.py run).
For each object type handler (module +0x24 + 4 x (type-1)) it walks the module routines reachable from it
(handler -> callbacks it stores -> their callees, inside the module) and reports:
  sight   calls to the engine's sight test (PP 0x800719CC / MoA 0x80079828) with range, pitch and yaw limits
  rand    rand() (exe 0x8002CC30 / MoA 0x8002C5C8 is not used: both discs' rand found by pairing) compared with a constant
  turn    calls to the exe's turn-toward (0x80013D28 PP / 0x800136E0 MoA) with the step
  fire    calls to the engine's shot routine (PP 0x800758A0 / MoA 0x8007D774)
  burst   small constants stored to the object fields the gun code uses (byte stores)
--plc lists the mission's placements (map file block 12): type, AP, attack, impact, energy class nibble.
"""
import argparse, collections, os, struct, sys
from capstone import Cs, CS_ARCH_MIPS, CS_MODE_MIPS32, CS_MODE_LITTLE_ENDIAN
from capstone.mips import MIPS_OP_IMM, MIPS_OP_MEM, MIPS_OP_REG

CFG = {
 'pp': dict(dir='pp-disc', mod=0x80097A38, eng=(202, 0x800490D0), sight=0x800719CC, fire=0x800758A0,
            turn=0x80013D28, rand=0x8002CC30, pick=0x800714C0, los=0x80075F60, cb=range(0x4C, 0x60, 4)),
 'ma': dict(dir='ma-disc', mod=0x800A0DD0, eng=(207, 0x80050DD8), sight=0x80079828, fire=0x8007D774,
            turn=0x800136E0, rand=None, pick=0x8007931C, los=0x8007DEA4, cb=range(0x54, 0x68, 4)),
}
md = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 | CS_MODE_LITTLE_ENDIAN); md.detail = True

def insn_at(d, b, a):
    o = a - b
    if o < 0 or o + 4 > len(d): return None
    for i in md.disasm(d[o:o + 4], a): return i
    return None

def routine(d, b, start, limit=3000):
    """Linear walk to the last jr ra that no branch jumps past."""
    out, far, a = [], start, start
    while len(out) < limit:
        i = insn_at(d, b, a)
        if i is None: break
        out.append(i)
        if i.mnemonic.startswith('b') or i.mnemonic == 'j':
            for op in i.operands:
                if op.type == MIPS_OP_IMM and b <= op.imm < b + len(d): far = max(far, op.imm)
        if i.mnemonic == 'jr' and i.op_str == '$ra' and a >= far:
            j = insn_at(d, b, a + 4)
            if j: out.append(j)
            break
        a += 4
    return out

def survey(work, disc, mis, plc):
    c = CFG[disc]; root = os.path.join(work, c['dir'], 'unpacked/GG/COM/FDAT')
    d = open(os.path.join(root, '%03d.bin' % (5 * mis)), 'rb').read(); b = c['mod']
    if not d: print('== %s mission %d: empty FDAT %d' % (disc.upper(), mis, 5 * mis)); return
    lo, hi = b, b + len(d)
    print('== %s mission %d (FDAT %d, %d bytes)' % (disc.upper(), mis, 5 * mis, len(d)))
    if plc: placements(os.path.join(root, '%03d.bin' % (5 * mis + 4)), disc)
    for t in range(1, 16):
        h = struct.unpack_from('<I', d, 0x20 + 4 * t)[0]
        if not (lo <= h < hi): continue
        seen, todo, cbs = set(), [h], {}
        rep = collections.defaultdict(list)
        while todo:
            r = todo.pop()
            if r in seen or not (lo <= r < hi): continue
            seen.add(r); body = routine(d, b, r)
            regs = {}
            for k, i in enumerate(body):
                m, ops = i.mnemonic, i.operands
                if m in ('addiu', 'ori') and len(ops) == 3 and ops[2].type == MIPS_OP_IMM:
                    src = i.reg_name(ops[1].reg)
                    if src == 'zero': regs[i.reg_name(ops[0].reg)] = ops[2].imm
                    elif m == 'addiu' and src in regs and isinstance(regs[src], int) and regs[src] & 0xFFFF == 0:
                        regs[i.reg_name(ops[0].reg)] = regs[src] + ops[2].imm
                    else: regs.pop(i.reg_name(ops[0].reg), None)
                elif m == 'lui': regs[i.reg_name(ops[0].reg)] = (ops[1].imm << 16) & 0xFFFFFFFF
                elif m == 'sw' and ops[1].type == MIPS_OP_MEM:
                    rn = i.reg_name(ops[0].reg); base = i.reg_name(ops[1].mem.base)
                    if base == 'sp' and rn in regs: regs['sp%d' % ops[1].mem.disp] = regs[rn]
                    v = regs.get(rn)
                    if base != 'sp' and isinstance(v, int) and lo <= v < hi and ops[1].mem.disp in c['cb']:
                        cbs[ops[1].mem.disp] = v; todo.append(v)
                elif m in ('sb', 'sh') and ops[1].type == MIPS_OP_MEM and i.reg_name(ops[1].mem.base) not in ('sp',):
                    rn = i.reg_name(ops[0].reg)
                    if rn in regs and isinstance(regs[rn], int) and abs(regs[rn]) < 0x10000:
                        rep['store'].append('%s+0x%X=%d' % (m, ops[1].mem.disp, regs[rn]))
                elif m == 'slti' and k >= 2 and any(x.mnemonic == 'jal' and x.operands[0].imm == c['rand']
                                                   for x in body[max(0, k - 3):k]):
                    rep['rand<'].append(ops[2].imm)
                elif m == 'jal':
                    tgt = ops[0].imm
                    # delay slot effect
                    ds = body[k + 1] if k + 1 < len(body) else None
                    if ds is not None and ds.mnemonic in ('addiu', 'ori') and i.reg_name(ds.operands[1].reg) == 'zero':
                        regs[ds.reg_name(ds.operands[0].reg)] = ds.operands[2].imm
                    if lo <= tgt < hi: todo.append(tgt)
                    elif tgt == c['sight']:
                        if disc == 'pp': keys = [('range', 'sp24'), ('pitch-', 'sp28'), ('pitch+', 'sp32'), ('yaw-', 'sp36'), ('yaw+', 'sp40')]
                        else: keys = [('range', 'sp24'), ('pitch-', 'sp28'), ('pitch+', 'sp32'), ('yaw-', 'sp36'), ('yaw+', 'sp40')]
                        rep['sight'].append('@%08X ' % i.address + ' '.join('%s=%s' % (n, regs.get(s, '?')) for n, s in keys))
                    elif tgt == c['turn']: rep['turn'].append(regs.get('a2', '?'))
                    elif tgt == c['fire']: rep['fire'].append('@%08X' % i.address)
                    elif tgt == c['pick']: rep['pick'].append('@%08X' % i.address)
                    elif tgt == c['los']: rep['los'].append('@%08X' % i.address)
                    for r_ in ('v0', 'v1', 'a0', 'a1', 'a2', 'a3'):
                        if r_ != 'a2' or tgt != c['turn']: regs.pop(r_, None)
        print(' type %2d handler %08X callbacks %s  routines %d' % (t, h, ' '.join('+0x%X=%08X' % kv for kv in sorted(cbs.items())), len(seen)))
        for k in ('pick', 'sight', 'turn', 'rand<', 'los', 'fire'):
            if rep[k]:
                v = rep[k]
                if k in ('turn', 'rand<'): v = ['%s x%d' % (x, n) for x, n in collections.Counter(v).most_common()]
                print('   %-6s %s' % (k, '; '.join(map(str, v))[:400]))

def placements(path, disc):
    d = open(path, 'rb').read(); o, bl = 0, []
    while o + 4 <= len(d) and len(bl) < 25:
        n = struct.unpack_from('<I', d, o)[0]
        if o + 4 + n > len(d): break
        bl.append((o + 4, n)); o += 4 + n
    if disc == 'ma': bl = bl[1:]
    if len(bl) < 13: print(' placements: (no map)'); return
    po, pn = bl[12]
    rows = []
    for k in range(pn // 40):
        r = d[po + 40 * k: po + 40 * k + 40]
        typ = struct.unpack_from('<h', r, 6)[0]
        if typ == -1: continue
        f10, ap, atk = struct.unpack_from('<HHH', r, 0x10)[0], struct.unpack_from('<H', r, 0x16)[0], struct.unpack_from('<H', r, 0x18)[0]
        imp = r[0x1C]; nib = (f10 >> 8) & 0xF; nib = nib - 16 if nib & 8 else nib
        rows.append('%d:t%d ap%d atk%d imp%d n%d' % (k, typ, ap, atk, imp, nib))
    print(' placements: ' + ', '.join(rows))

if __name__ == '__main__':
    ap = argparse.ArgumentParser(); ap.add_argument('--work', default='/home/claude/work')
    ap.add_argument('--disc', default='pp'); ap.add_argument('--mis', default=None); ap.add_argument('--plc', action='store_true')
    a = ap.parse_args()
    ms = [int(x) for x in a.mis.split(',')] if a.mis else range(20)
    for m in ms:
        try: survey(a.work, a.disc, m, a.plc)
        except FileNotFoundError: pass

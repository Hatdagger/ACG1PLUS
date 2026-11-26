#!/usr/bin/env python3
"""Read helpers for mapping AC1 / PP code from the work packs (prints to the screen only; never commit output).

  python3 -I tools/xdisc/ac1_dis.py dis   ac1 0x8009C964 40 201   # disassemble 40 instructions (module 201)
  python3 -I tools/xdisc/ac1_dis.py dis   pp  0x800171B4 36       # boot exe when no module is given
  python3 -I tools/xdisc/ac1_dis.py calls ac1 0x800165E4 202 203  # every call to a routine, a0-a2 if constant

Needs capstone (pip install --break-system-packages capstone). Work-pack folders default to WORK/ac1-disc and
WORK/pp-disc with WORK from --work or $ACG1_WORK. Module bases: AC1 0x8004ADA0, PP 0x800490D0 (foundations docs).
"""
import argparse, os, struct

BASES = {"ac1": 0x8004ADA0, "pp": 0x800490D0}
EXES = {"ac1": "SCUS_941.82", "pp": "SLUS_006.70"}


def load(work, disc, mod):
    folder = os.path.join(work, disc + "-disc")
    if mod is None:
        return open(os.path.join(folder, EXES[disc]), "rb").read()[0x800:], 0x80011000
    return open(os.path.join(folder, "unpacked/GG/COM/FDAT/%03d.bin" % mod), "rb").read(), BASES[disc]


def dis(work, disc, addr, n, mod):
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_MIPS, capstone.CS_MODE_MIPS32 + capstone.CS_MODE_LITTLE_ENDIAN)
    data, base = load(work, disc, mod)
    for i in md.disasm(data[addr - base:addr - base + n * 4], addr):
        print("%08x  %s %s" % (i.address, i.mnemonic, i.op_str))


def calls(work, disc, target, mod, back=10):
    data, base = load(work, disc, mod)
    ws = struct.unpack("<%dI" % (len(data) // 4), data[:len(data) // 4 * 4])
    for i, w in enumerate(ws):
        if w >> 26 != 3 or (0x80000000 | ((w & 0x3FFFFFF) << 2)) != target:
            continue
        regs = {0: 0}
        for k in range(max(0, i - back), min(len(ws), i + 2)):
            x = ws[k]; op = x >> 26; rs = (x >> 21) & 31; rt = (x >> 16) & 31; imm = x & 0xFFFF
            s = imm - 0x10000 if imm & 0x8000 else imm
            if op == 0xF:
                regs[rt] = imm << 16
            elif op == 9:
                regs[rt] = (regs[rs] + s) & 0xFFFFFFFF if regs.get(rs) is not None else None
            elif op == 0xD:
                regs[rt] = regs[rs] | imm if regs.get(rs) is not None else None
            elif op == 0:
                regs[(x >> 11) & 31] = None
            elif op in (0x20, 0x21, 0x23, 0x24, 0x25, 0xA, 0xB, 0xC, 0xE):
                regs[rt] = None
        f = lambda r: "?" if regs.get(r) is None else hex(regs[r])
        print("%s %08x a0=%s a1=%s a2=%s" % ("exe" if mod is None else mod, base + i * 4, f(4), f(5), f(6)))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--work", default=os.environ.get("ACG1_WORK", "."))
    ap.add_argument("what", choices=["dis", "calls"])
    ap.add_argument("disc", choices=["ac1", "pp"])
    ap.add_argument("addr")
    ap.add_argument("rest", nargs="*")
    a = ap.parse_args()
    addr = int(a.addr, 16)
    if a.what == "dis":
        n = int(a.rest[0]) if a.rest else 32
        dis(a.work, a.disc, addr, n, int(a.rest[1]) if len(a.rest) > 1 else None)
    else:
        for m in a.rest or ["exe"]:
            calls(a.work, a.disc, addr, None if m == "exe" else int(m))


if __name__ == "__main__":
    main()

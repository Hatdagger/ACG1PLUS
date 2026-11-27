#!/usr/bin/env python3
"""List the code files in Project Phantasma's FDAT.T and where each one loads.

Usage: python3 -I tools/mapping/code_files.py <unpacked FDAT folder>   (pp-disc/unpacked/GG/COM/FDAT)

FDAT entry 5 x stage + k: k 0 = missions (stages 0-19) / link (20-39), 1 = VS, 2 = arena, 3 = VS replay,
4 = stage data (mapping/stages.md section 1). For each code file it checks that the 8 header handlers point
inside the file at the engine's load address, counts where its calls go (in the file, boot exe, its engine,
elsewhere) and marks byte-identical copies. Prints a table; don't commit the output.
See docs/research/mapping/performance.md section 7.
"""
import hashlib
import os
import struct
import sys

MODULE_BASE = 0x800490D0          # FDAT 201-206 load here
EXE_START = 0x80011000
LOADS = {1: (204, 0x8008F5D0), 2: (205, 0x800956F0), 3: (206, 0x8008F828)}
NAMES = {202: "missions", 203: "link", 204: "VS", 205: "arena", 206: "replay"}


def engine_of(entry):
    stage, k = divmod(entry, 5)
    if k == 0:
        return (202, 0x80097A38) if stage < 20 else (203, 0x800904B0)
    return LOADS[k]


def main(folder):
    def read(i):
        return open(os.path.join(folder, "%03d.bin" % i), "rb").read()

    module_size = {m: len(read(m)) for m in range(201, 207)}
    seen = {}
    print("fdat stage k engine    load        size  hdr  in-file exe engine other  copy-of")
    for i in list(range(0, 100, 5)) + [e for e in range(100, 200) if e % 5 != 4]:
        data = read(i)
        if not data:
            continue
        module, base = engine_of(i)
        words = struct.unpack("<%dI" % (len(data) // 4), data[: len(data) // 4 * 4])
        header = sum(1 for w in words[:8] if base <= w < base + len(data))
        counts = [0, 0, 0, 0]
        for w in words:
            if w >> 26 != 3:      # jal
                continue
            target = 0x80000000 | ((w & 0x3FFFFFF) << 2)
            if base <= target < base + len(data):
                counts[0] += 1
            elif EXE_START <= target < MODULE_BASE:
                counts[1] += 1
            elif MODULE_BASE <= target < MODULE_BASE + module_size[module]:
                counts[2] += 1
            else:
                counts[3] += 1
        digest = hashlib.sha256(data).hexdigest()
        copy_of = seen.setdefault(digest, i)
        flag = "" if header == 8 else "  <- header doesn't fit this address (never loaded?)"
        print("%4d %5d %d %-9s 0x%08X %6d %d/8  %7d %3d %6d %5d  %s%s" % (
            i, i // 5, i % 5, NAMES[module], base, len(data), header, *counts,
            "" if copy_of == i else copy_of, flag))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])

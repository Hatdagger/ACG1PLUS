#!/usr/bin/env python3
"""Make the AC1 mission pack on the player's PC, from their own Armored Core and Project Phantasma discs.

  python3 -I tools/ac1import/ac1import.py --ac1 "Disc/AC1/Armored Core.cue" --pp "Disc/Project Phantasma.cue" \\
          --out RecompData/content [--slots 1]

--ac1 / --pp take a .cue, a .bin/.iso, or a folder laid out like the disc (the chats' work packs: ac1-disc/,
pp-disc/). Writes <out>/ac1/ (see PACK below) and prints what it did. Stops without writing a mission while
anything about it is undecided, and lists why.

The pack is game data made from the player's discs: it stays on their PC and never goes on GitHub
(docs/no-game-code.md). The folder is the contract with game/acg_mission_files.c:

  PACK = <out>/ac1/
    pack.txt              first line "acg1plus-ac1-pack 1", then one line per file: name, size, sha256
    a2-e<5k>.bin          mission code for engine number k = AC1 slot + 50 (FDAT 5k on Phantasma's numbering)
    a2-e<5k+4>.bin        its map
    a1-e<k>.bin           its pictures (RTIM k)
    a2-e200.bin           the common file for AC1 missions
    values.bin            AC1's per-mission u16 table (100 entries, AC1 numbering)
    test-slot.txt         only with --test-slot N: every Phantasma mission runs AC1 slot N (testing)
"""
import argparse
import hashlib
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import disc as D            # noqa: E402
import mission_code as C    # noqa: E402
import mission_data as MD   # noqa: E402

FORMAT = 'acg1plus-ac1-pack 1'
FIRST_ENGINE = 50
PILOT = [1]
SKIP = {0: 'dummy slot', 70: 'garage test'}
FDAT, RTIM = 'GG/COM/FDAT.T', 'GG/COM/RTIM.T'


def exe_name(d, want):
    try:
        cnf = d.file('SYSTEM.CNF').decode('ascii', 'replace')
    except FileNotFoundError:
        return want
    for line in cnf.splitlines():
        if line.upper().startswith('BOOT'):
            return line.split(':\\')[-1].split(';')[0].strip()
    return want


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--ac1', required=True)
    ap.add_argument('--pp', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--slots', default=','.join(map(str, PILOT)))
    ap.add_argument('--check', action='store_true', help='convert and report, write nothing')
    ap.add_argument('--test-slot', type=int, help='test switch: every Phantasma mission runs this AC1 slot '
                    '(writes ac1/test-slot.txt); 0 turns it off')
    a = ap.parse_args()

    ac1, pp = D.Disc(a.ac1), D.Disc(a.pp)
    ac1_exe = exe_name(ac1, 'SCUS_941.82')
    pp_exe_name = exe_name(pp, 'SLUS_006.70')
    if ac1_exe.upper() not in ('SCUS_941.82', 'SLUS_013.23'):
        sys.exit('--ac1: boot program %s is not Armored Core USA (SCUS-94182 / SLUS-01323)' % ac1_exe)
    if pp_exe_name.upper() != 'SLUS_006.70':
        sys.exit('--pp: boot program %s is not Project Phantasma USA (SLUS-00670)' % pp_exe_name)
    pp_exe = pp.file(pp_exe_name)[0x800:]
    pp_eng = pp.entry(FDAT, 202)
    ac1_eng = ac1.entry(FDAT, 202)
    ac1_exe_bytes = ac1.file(ac1_exe)[0x800:]

    files = {}
    files['a2-e200.bin'] = MD.common_file(ac1.entry(FDAT, 200), pp.entry(FDAT, 200))
    files['values.bin'] = MD.mission_values(ac1_eng)
    print('common file: %d bytes (AC1 lines at their own numbers, AC1 sound ids 56-255)' % len(files['a2-e200.bin']))

    failed = 0
    for s in map(int, a.slots.split(',')):
        if s in SKIP:
            print('slot %d: skipped (%s)' % (s, SKIP[s])); continue
        k = s + FIRST_ENGINE
        try:
            code, rep = C.convert(ac1.entry(FDAT, 2 * s), pp_exe, pp_eng, ac1_exe_bytes, ac1_eng)
            mp, notes = MD.fix_map(ac1.entry(FDAT, 2 * s + 1))
        except (C.Undecided, ValueError) as e:
            failed += 1
            print('slot %d: NOT converted:\n  %s' % (s, str(e).replace('\n', '\n  ')))
            continue
        files['a2-e%03d.bin' % (5 * k)] = code
        files['a2-e%03d.bin' % (5 * k + 4)] = mp
        files['a1-e%03d.bin' % k] = ac1.entry(RTIM, s)
        print('slot %d -> engine mission %d: %s; map %s' % (
            s, k, ', '.join('%s %d' % kv for kv in sorted(rep.items())), '; '.join(notes) or 'no changes'))

    if a.check:
        print('check only: nothing written'); return 1 if failed else 0
    out = os.path.join(a.out, 'ac1')
    os.makedirs(out, exist_ok=True)
    lines = [FORMAT]
    for name in sorted(files):
        tmp = os.path.join(out, name + '.tmp')
        with open(tmp, 'wb') as f:
            f.write(files[name])
        os.replace(tmp, os.path.join(out, name))
        lines.append('%s %d %s' % (name, len(files[name]), hashlib.sha256(files[name]).hexdigest()))
    with open(os.path.join(out, 'pack.txt'), 'w', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    print('wrote %d files to %s' % (len(files), out))
    ts = os.path.join(out, 'test-slot.txt')
    if a.test_slot:
        if 'a2-e%03d.bin' % (5 * (a.test_slot + FIRST_ENGINE)) not in files:
            print('test switch NOT set: slot %d was not converted' % a.test_slot)
        else:
            with open(ts, 'w', newline='\n') as f:
                f.write('%d\n' % a.test_slot)
            print('test switch on: every Phantasma mission runs AC1 slot %d (delete %s to stop)' % (a.test_slot, ts))
    elif a.test_slot == 0 and os.path.exists(ts):
        os.remove(ts); print('test switch off')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())

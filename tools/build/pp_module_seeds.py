#!/usr/bin/env python3
"""Write seeds/modules/fdat20N.txt: routine starts in Project Phantasma's six disc
modules (FDAT 201-206), taken from mapping round 1's cross-disc catalogue.

Reads only our research table (docs/research/xdisc/pp-catalogue.tsv): addresses, no
game bytes. Kept: rows whose MoA twin pairing is high or medium confidence, plus
PP-only rows, minus rows flagged as data tables. Every module loads at 0x800490D0.

    python3 tools/build/pp_module_seeds.py            # rewrite seeds/modules/
    python3 tools/build/pp_module_seeds.py --check    # fail if they are out of date
"""
import argparse
import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CATALOGUE = ROOT / 'docs/research/xdisc/pp-catalogue.tsv'
OUT = ROOT / 'seeds/modules'
MODULES = {
    201: 'menu (hub, garage, shop, VS menu); entry 0x80098EF0, called directly',
    202: 'missions engine; entry = header word 1',
    203: 'link engine (two consoles); entry = header word 1',
    204: 'VS engine (split screen); entry = header word 1',
    205: 'arena engine; entry = header word 1',
    206: 'VS replay; entry = header word 1',
}
KEEP_CONF = {'high', 'medium'}


def rows():
    with CATALOGUE.open(encoding='utf-8') as f:
        lines = [line for line in f if not line.startswith('#')]
    yield from csv.DictReader(lines, delimiter='\t')


def render():
    picked = {n: [] for n in MODULES}
    counts = {n: dict(kept=0, data=0, low=0) for n in MODULES}
    for row in rows():
        module = row['pp_module']
        if not module.startswith('FDAT '):
            continue
        n = int(module.split()[1])
        if n not in MODULES:
            continue
        conf, kind = row['conf'], row['kind']
        if conf == 'data':
            counts[n]['data'] += 1
            continue
        if conf in KEEP_CONF or kind == 'only_pp':
            picked[n].append(int(row['pp_addr'], 16))
            counts[n]['kept'] += 1
        else:
            counts[n]['low'] += 1
    files = {}
    for n, what in MODULES.items():
        addrs = sorted(set(picked[n]))
        c = counts[n]
        head = [
            f'# Project Phantasma USA (SLUS-00670), GG/COM/FDAT.T entry {n}: {what}.',
            '# Loads at 0x800490D0 (file offset = address - 0x800490D0).',
            '# Made by tools/build/pp_module_seeds.py from docs/research/xdisc/pp-catalogue.tsv',
            f'# (mapping round 1). {len(addrs)} kept (high/medium twin, or Phantasma-only),',
            f'# {c["low"]} low-confidence and {c["data"]} data-table rows left out.',
            '# A "routine" there is cut after each jr ra, so a few starts may be inside a',
            '# function; the AOT pipeline checks each one before using it.',
        ]
        files[OUT / f'fdat{n}.txt'] = '\n'.join(head + [f'0x{a:08X}' for a in addrs]) + '\n'
    return files


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()
    files = render()
    stale = [p for p, text in files.items() if not p.is_file() or p.read_text() != text]
    if args.check:
        for p in stale:
            print(f'out of date: {p.relative_to(ROOT)}')
        return 1 if stale else 0
    OUT.mkdir(parents=True, exist_ok=True)
    for p, text in files.items():
        p.write_text(text)
        print(f'{p.relative_to(ROOT)}: {text.count(chr(10)) - 7} starts')
    return 0


if __name__ == '__main__':
    sys.exit(main())

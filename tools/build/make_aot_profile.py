#!/usr/bin/env python3
"""Write aot/overlays.json: the list of code Project Phantasma loads from its disc
while playing, so Generate can pre-compile it on the player's PC.

The profile holds only facts about the disc: file offsets, sizes, load addresses,
entry points and SHA-256 checksums of each piece. No game bytes. The framework's
tools/aot_overlay_pipeline.py reads the player's own disc, checks every checksum,
and compiles the pieces into generated/overlays_static*.c (never committed).

Needs the owner's disc once, to work out the checksums:

    python3 tools/build/make_aot_profile.py --disc "disc/Armored Core - Project Phantasma (USA).cue"
    python3 tools/build/make_aot_profile.py --disc ... --check   # fail if aot/overlays.json differs

Where the facts come from (mapping round 1, docs/research/mapping/foundations.md §3 and
boot-loads.md PP-10):
- FDAT.T entries 201-206 are the six modules; all load at 0x800490D0. The menu (201) is
  called at 0x80098EF0; the five fight engines (202-206) start with a header whose
  word 1 is the entry point.
- Entries 5n (n = 0..19) are per-mission code, loaded by the missions engine at
  0x80097A38. Entries 100 + 5s + k are per-stage code for one engine: k = 0 link
  (0x800904B0), 1 VS (0x8008F5D0), 2 arena (0x800956F0), 3 VS replay (0x8008F828).
  Each starts with a table of 8 handler addresses; those inside the piece are entries.
  A piece whose handlers all point outside it is left out (it does not load at that slot).
- 5n+4 / 100+5s+4 are map data and 200 is shared data: not code, not listed.
"""
import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'psxrecomp' / 'tools'))
import aot_overlay_pipeline as pipeline  # noqa: E402  (framework: disc reader)

PROFILE = ROOT / 'aot' / 'overlays.json'
FDAT = 'GG/COM/FDAT.T'
SECTOR = 2048
MODULE_BASE = 0x800490D0
MENU_ENTRY = 0x80098EF0
MISSION_BASE = 0x80097A38
STAGE_BASE = {0: (0x800904B0, 'link'), 1: (0x8008F5D0, 'VS'),
              2: (0x800956F0, 'arena'), 3: (0x8008F828, 'VS replay')}
MODULE_NAMES = {201: 'menu (hub, garage, shop, VS menu)', 202: 'missions engine',
                203: 'link engine', 204: 'VS engine', 205: 'arena engine', 206: 'VS replay'}
HANDLERS = 8


def fdat_entries(data):
    """[(index, file offset, size)] from the .T header: halfwords count, start0..end."""
    count = struct.unpack_from('<H', data, 0)[0]
    starts = struct.unpack_from(f'<{count + 1}H', data, 2)
    return [(i, starts[i] * SECTOR, (starts[i + 1] - starts[i]) * SECTOR) for i in range(count)]


def extent(offset, size, body, load):
    return dict(file=FDAT, address=hex(offset), size=hex(size), load_addr=hex(load),
                sha256=hashlib.sha256(body).hexdigest())


def handler_offsets(body, load):
    """Offsets of handler-table words that point inside the piece itself."""
    out = []
    for i in range(HANDLERS):
        ptr = struct.unpack_from('<I', body, i * 4)[0]
        if load <= ptr < load + len(body) and ptr % 4 == 0:
            out.append(i * 4)
    return out


def build(cue):
    disc = pipeline.Disc(cue)
    binary = Path(disc.binary)
    md5 = hashlib.md5(binary.read_bytes()).hexdigest()
    data = disc.read(FDAT)
    entries = fdat_entries(data)
    images, seen = [], {}
    outside = []

    def add(note, offset, size, load, **extra):
        body = data[offset:offset + size]
        key = (load, hashlib.sha256(body).hexdigest())
        if key in seen:                     # same bytes at the same address: one image
            seen[key]['note'] += f'; same bytes as {note}'
            return
        image = dict(note=note, method='fixed_address_extents',
                     extents=[extent(offset, size, body, load)], allow_missing=True, **extra)
        seen[key] = image
        images.append(image)

    for index, offset, size in entries:
        if not size:
            continue
        body = data[offset:offset + size]
        if 201 <= index <= 206:
            if index == 201:
                add(f'FDAT {index}: {MODULE_NAMES[index]}', offset, size, MODULE_BASE,
                    entries=[hex(MENU_ENTRY)])
            else:
                add(f'FDAT {index}: {MODULE_NAMES[index]}', offset, size, MODULE_BASE,
                    entry_pointer_offsets=[4])
        elif index < 100 and index % 5 == 0:
            offs = handler_offsets(body, MISSION_BASE)
            add(f'FDAT {index}: mission {index // 5} code', offset, size, MISSION_BASE,
                entry_pointer_offsets=offs)
        elif 100 <= index < 200 and (index - 100) % 5 in STAGE_BASE:
            stage, k = (index - 100) // 5, (index - 100) % 5
            load, engine = STAGE_BASE[k]
            offs = handler_offsets(body, load)
            if not offs:
                # Not at this engine's stage slot (or not code): left to the runtime's
                # background compiler. See LEFT_OUT below.
                outside.append(index)
                continue
            add(f'FDAT {index}: stage {20 + stage} code for the {engine} engine', offset, size,
                load, entry_pointer_offsets=offs)
    profile = {
        'schema': 'psxrecomp AOT methods v1',
        'game_id': 'SLUS-00670',
        '_comment': (
            'Project Phantasma USA. Made by tools/build/make_aot_profile.py from the owner\'s disc '
            '(facts only: offsets, sizes, load addresses, entries, checksums). Generate compiles '
            'these into generated/overlays_static*.c on the player\'s PC; never commit that output. '
            f'Left out: FDAT {", ".join(map(str, outside))}. Their handler table and calls point '
            'outside the arena stage slot (static guess: they load near 0x801C9DD8; 192 holds '
            'almost no code), so they are compiled while playing instead, as before. '
            'Handed to the mapping (stages topic).'),
        'disc_hashes': {'md5': md5},
        'static_output': 'generated/overlays_static.c',
        'strict_bounds': True,
        'cross_image_call_roots': True,
        'expected_records': len(images),
        'images': images,
    }
    return json.dumps(profile, indent=2) + '\n'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--disc', required=True, help='the Phantasma .cue (with its .bin beside it)')
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--expected-records', type=int,
                    help='override the record count (when the pipeline merges or adds records)')
    args = ap.parse_args()
    text = build(args.disc)
    if args.expected_records is not None:
        profile = json.loads(text)
        profile['expected_records'] = args.expected_records
        text = json.dumps(profile, indent=2) + '\n'
    if args.check:
        same = PROFILE.is_file() and PROFILE.read_text() == text
        print('aot/overlays.json is up to date' if same else 'aot/overlays.json differs')
        return 0 if same else 1
    PROFILE.parent.mkdir(exist_ok=True)
    PROFILE.write_text(text)
    print(f'wrote {PROFILE.relative_to(ROOT)}: {len(json.loads(text)["images"])} images')
    return 0


if __name__ == '__main__':
    sys.exit(main())

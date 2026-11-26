#!/usr/bin/env python3
"""Block list of mission map files (FDAT 5n+4) on PP and MoA-US, and what a MoA map would look like to PP's loader.

  python3 -I tools/mapping/missions/map_blocks.py [--work /home/claude/work]
Per map: block sizes (MoA's leading 72-kind graphics block shown as 'h'), used placements (block 12, type != -1)
and where they sit, used event points (block 9, type != -1), object types (block 11 count), point classes
(block 8 count), and whether every block's checksum word holds. Counts and sizes only.
"""
import argparse, os, struct

def blocks(d):
    out, o = [], 0
    while o + 4 <= len(d):
        n = struct.unpack_from('<I', d, o)[0]
        if n == 0 and o + 4 >= len(d.rstrip(b'\0')): break
        if o + 4 + n > len(d): break
        out.append((o + 4, n)); o += 4 + n
        if len(out) > 40: break
    return out

def survey(d, moa):
    bl = blocks(d)
    head = bl[0] if moa else None
    body = bl[1:] if moa else bl
    r = {'n': len(bl), 'sizes': [n for _, n in bl]}
    if len(body) >= 13:
        o, n = body[12]; recs = n // 40
        used = [i for i in range(recs) if struct.unpack_from('<h', d, o + 40 * i + 6)[0] != -1]
        r['plc'] = (recs, len(used), max(used) if used else -1)
        o, n = body[9]; pts = n // 32
        usedp = [i for i in range(pts) if struct.unpack_from('<h', d, o + 32 * i + 6)[0] != -1]
        r['pts'] = (pts, len(usedp), max(usedp) if usedp else -1)
        r['types'] = struct.unpack_from('<H', d, body[11][0])[0]
        r['classes'] = struct.unpack_from('<I', d, body[8][0])[0]
    return r

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--work', default='/home/claude/work'); a = ap.parse_args()
    for disc, moa in (('pp-disc', False), ('ma-disc', True)):
        print('==', 'MoA-US' if moa else 'PP')
        for s in range(20):
            p = os.path.join(a.work, disc, 'unpacked/GG/COM/FDAT/%03d.bin' % (5 * s + 4))
            d = open(p, 'rb').read()
            if not d.strip(b'\0'): print('%2d empty' % s); continue
            r = survey(d, moa)
            print('%2d blocks %2d  placements %s  points %s  types %s  classes %s  sizes %s' % (
                s, r['n'], r.get('plc'), r.get('pts'), r.get('types'), r.get('classes'), r['sizes']))

if __name__ == '__main__':
    main()

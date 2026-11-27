"""Put a Project Phantasma TEST disc image back together from the split disc packs.

    python3 -I tools/build/packs_to_test_image.py PP_DISC_FOLDER OUT.bin

For chats and tests only, when the owner's full disc image isn't at hand (it is 454 MB; the packs in
the claude.ai Project are not). Never a substitute for the owner's disc in a real build.

- Every file listed in `disc-toc.tsv` goes back to its own sector, byte for byte where the pack has it.
  Files the packs leave out (fight music, movies, DUMMY.DDD) become sectors of zeros of the right kind.
- The packs don't keep the disc's file system (directory sectors), so a fresh ISO9660 one is written:
  volume descriptor at sector 16, directories in the empty sectors after the last file.
- Data sectors get a correct header and EDC; error-correction bytes are left as zeros (readers here
  don't use them).

So the result has the same size and the same file positions as the real disc, but NOT its checksum:
the build rejects it. For a sandbox build, use a throwaway copy of the source and add the md5 this tool
prints to that copy's game.toml (`known_md5`, drop `known_sha1`/`known_crc32`) and aot/overlays.json
(`disc_hashes`). Never commit those copies. The per-file sha256 guards inside the AOT profile still
check the real bytes, so the pre-compiled disc code is the same as from the real disc.
"""
import hashlib
import os
import struct
import sys

SECTORS = 193058          # the real disc's data track
RAW = 2352
SYNC = b'\x00' + b'\xff' * 10 + b'\x00'


def edc_table():
    t = []
    for i in range(256):
        x = i
        for _ in range(8):
            x = (x >> 1) ^ 0xD8018001 if x & 1 else x >> 1
        t.append(x)
    return t


EDC = edc_table()


def edc(buf, start, length):
    x = 0
    for b in buf[start:start + length]:
        x = EDC[(x ^ b) & 0xFF] ^ (x >> 8)
    return x


def header(lba):
    t = lba + 150
    mm, ss, ff = t // 4500, (t // 75) % 60, t % 75
    bcd = lambda v: ((v // 10) << 4) | (v % 10)
    return SYNC + bytes([bcd(mm), bcd(ss), bcd(ff), 2])


def form1(user, lba, submode=0x08):
    user = user.ljust(2048, b'\x00')
    s = bytearray(RAW)
    s[0:16] = header(lba)
    sub = bytes([0, 0, submode, 0])
    s[16:20] = sub
    s[20:24] = sub
    s[24:2072] = user
    struct.pack_into('<I', s, 2072, edc(s, 16, 2056))
    return bytes(s)


def form2(payload, lba):
    s = bytearray(RAW)
    s[0:16] = header(lba)
    if payload:
        s[16:16 + 2336] = payload
    else:   # an empty Form 2 sector
        sub = bytes([0, 0, 0x20, 0])
        s[16:20] = sub
        s[20:24] = sub
    return bytes(s)


def both16(v): return struct.pack('<H', v) + struct.pack('>H', v)
def both32(v): return struct.pack('<I', v) + struct.pack('>I', v)


DATE = bytes([98, 1, 1, 0, 0, 0, 0])


def record(name, lba, size, is_dir, xa_form2=False):
    """One ISO9660 directory record, with the PS1 XA system-use field."""
    xa = struct.pack('>HH', 0, 0) + struct.pack('>H', 0x8D55 if is_dir else (0x3555 if xa_form2 else 0x0D55)) \
        + b'XA' + b'\x00' * 6
    body = (b'\x00' + both32(lba) + both32(size) + DATE + bytes([2 if is_dir else 0, 0, 0])
            + both16(1) + bytes([len(name)]) + name)
    if len(name) % 2 == 0:
        body += b'\x00'
    rec = bytes([len(body) + 1 + len(xa)]) + body + xa
    return rec


def main():
    root, out = sys.argv[1], sys.argv[2]
    rows = []
    for line in open(os.path.join(root, 'disc-toc.tsv'), encoding='utf-8'):
        if line.startswith('#') or line.startswith('path\t') or not line.strip():
            continue
        p, lba, n, size, form, sha, _ = (line.rstrip('\n').split('\t') + [''])[:7]
        rows.append(dict(path=p, lba=int(lba), n=int(n), size=int(size), form=int(form), sha=sha))
    last = max(r['lba'] + r['n'] for r in rows)

    # Directory tree.
    dirs = {'': []}
    for r in rows:
        parts = r['path'].split('/')
        for i in range(1, len(parts)):
            d = '/'.join(parts[:i])
            if d not in dirs:
                dirs[d] = []
                dirs['/'.join(parts[:i - 1])].append(('d', parts[i - 1], d))
        dirs['/'.join(parts[:-1])].append(('f', parts[-1], r))
    order = sorted(dirs, key=lambda d: (d.count('/') + (d != ''), d))   # breadth first
    # One sector per directory is plenty here (largest has well under 40 entries).
    dir_lba = {d: last + 4 + i for i, d in enumerate(order)}
    pt_l, pt_m = last + 1, last + 2
    if dir_lba[order[-1]] >= SECTORS:
        sys.exit('no room for the directories')

    def dir_sector(d):
        parent = d.rsplit('/', 1)[0] if '/' in d else ''
        data = record(b'\x00', dir_lba[d], 2048, True) + record(b'\x01', dir_lba[parent], 2048, True)
        for kind, name, x in sorted(dirs[d], key=lambda e: e[1]):
            if kind == 'd':
                rec = record(name.encode(), dir_lba[x], 2048, True)
            else:
                rec = record((name + ';1').encode(), x['lba'], x['size'], False, x['form'] == 2)
            if len(data) + len(rec) > 2048:
                sys.exit('directory %s needs more than one sector' % d)
            data += rec
        return data

    # Path tables (little and big endian).
    num = {d: i + 1 for i, d in enumerate(order)}
    ptl = ptm = b''
    for d in order:
        name = d.rsplit('/', 1)[-1].encode() if d else b'\x00'
        parent = num[d.rsplit('/', 1)[0] if '/' in d else '']
        pad = b'\x00' if len(name) % 2 else b''
        ptl += bytes([len(name), 0]) + struct.pack('<I', dir_lba[d]) + struct.pack('<H', parent) + name + pad
        ptm += bytes([len(name), 0]) + struct.pack('>I', dir_lba[d]) + struct.pack('>H', parent) + name + pad

    pvd = bytearray(2048)
    pvd[0:8] = b'\x01CD001\x01\x00'
    pvd[8:40] = b'PLAYSTATION'.ljust(32)
    pvd[40:72] = b'SLUS_00670_TEST'.ljust(32)
    pvd[80:88] = both32(SECTORS)
    pvd[120:124] = both16(1)
    pvd[124:128] = both16(1)
    pvd[128:132] = both16(2048)
    pvd[132:140] = both32(len(ptl))
    pvd[140:144] = struct.pack('<I', pt_l)
    pvd[148:152] = struct.pack('>I', pt_m)
    rootrec = record(b'\x00', dir_lba[''], 2048, True)[:34]
    rootrec = bytes([34]) + rootrec[1:]
    pvd[156:190] = rootrec
    pvd[881] = 1
    pvd[1024:1032] = b'CD-XA001'
    term = b'\xffCD001\x01'.ljust(2048, b'\x00')

    by_lba = {}
    for r in rows:
        by_lba[r['lba']] = r
    special = {16: form1(bytes(pvd), 16, 0x09), 17: form1(term, 17, 0x89),
               pt_l: form1(ptl, pt_l), pt_m: form1(ptm, pt_m)}
    for d in order:
        special[dir_lba[d]] = form1(dir_sector(d), dir_lba[d], 0x89)

    h = hashlib.md5()
    missing = []
    with open(out, 'wb') as f:
        lba = 0
        while lba < SECTORS:
            if lba in special:
                s = special[lba]; f.write(s); h.update(s); lba += 1
                continue
            r = by_lba.get(lba)
            if not r:
                s = form1(b'', lba, 0x00 if lba >= 16 else 0x00); f.write(s); h.update(s); lba += 1
                continue
            fp = os.path.join(root, r['path'])
            data = open(fp, 'rb').read() if os.path.isfile(fp) else None
            if data is not None and r['sha'] and hashlib.sha256(data).hexdigest() != r['sha']:
                sys.exit('%s does not match disc-toc.tsv' % r['path'])
            if data is None:
                missing.append(r['path'])
            for i in range(r['n']):
                if r['form'] == 2:
                    chunk = data[i * 2336:(i + 1) * 2336] if data else b''
                    s = form2(chunk, lba + i)
                else:
                    chunk = data[i * 2048:(i + 1) * 2048] if data else b''
                    last_sector = i == r['n'] - 1
                    s = form1(chunk, lba + i, 0x89 if last_sector else 0x08)
                f.write(s); h.update(s)
            lba += r['n']
    stem = os.path.splitext(out)[0]
    with open(stem + '.cue', 'w', newline='\n') as c:
        c.write('FILE "%s" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n' % os.path.basename(out))
    print('wrote %s (%d sectors), md5 %s' % (out, SECTORS, h.hexdigest()))
    print('zero-filled (not in the packs given):', ', '.join(missing) or 'none')


if __name__ == '__main__':
    main()

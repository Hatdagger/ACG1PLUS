"""Read files from the player's own disc: a .cue/.bin (or .bin/.iso) image, or an unpacked work folder.

Ours, written for the AC1 import (no code from the disc packs). Reads only; never writes to the disc.

  Disc(path)            path = .cue, .bin/.img (2352-byte sectors) or .iso (2048-byte sectors)
  Disc(folder)          a folder laid out like the disc (GG/COM/FDAT.T ...), as the chats' work packs are
  d.file('GG/COM/FDAT.T')        -> bytes (Form 1 files)
  d.entry('GG/COM/FDAT.T', 202)  -> one .T archive entry, padded to whole 2048-byte sectors as the game reads it
"""
import os
import re
import struct

SECTOR = 2048


class Disc:
    def __init__(self, path):
        self.path = path
        self.folder = os.path.isdir(path)
        self._tables = {}
        if self.folder:
            return
        if path.lower().endswith('.cue'):
            path = self._bin_from_cue(path)
        self.f = open(path, 'rb')
        self.raw = self._looks_raw()          # raw 2352-byte sectors start with the sync pattern
        pvd = self.sector(16)
        if pvd[1:6] != b'CD001':
            raise ValueError('%s: no ISO 9660 volume at sector 16' % self.path)
        self.root = struct.unpack_from('<I', pvd, 156 + 2)[0], struct.unpack_from('<I', pvd, 156 + 10)[0]

    @staticmethod
    def _bin_from_cue(cue):
        txt = open(cue, encoding='utf-8', errors='replace').read()
        m = re.search(r'FILE\s+"([^"]+)"', txt) or re.search(r'FILE\s+(\S+)', txt)
        if not m:
            raise ValueError('%s: no FILE line' % cue)
        return os.path.join(os.path.dirname(cue), m.group(1))

    def _looks_raw(self):
        self.f.seek(0)
        return self.f.read(12) == b'\x00' + b'\xff' * 10 + b'\x00'

    def sector(self, lba):
        """2048 user bytes of a Form 1 sector"""
        if self.raw:
            self.f.seek(lba * 2352)
            s = self.f.read(2352)
            mode = s[15]
            return s[16:16 + SECTOR] if mode == 1 else s[24:24 + SECTOR]
        self.f.seek(lba * SECTOR)
        return self.f.read(SECTOR)

    def _find(self, path):
        lba, size = self.root
        parts = [p for p in path.replace('\\', '/').split('/') if p]
        for i, name in enumerate(parts):
            want = name.upper()
            found = None
            for k in range((size + SECTOR - 1) // SECTOR):
                s = self.sector(lba + k)
                o = 0
                while o < SECTOR and s[o]:
                    n = s[o]
                    elba, esize = struct.unpack_from('<I', s, o + 2)[0], struct.unpack_from('<I', s, o + 10)[0]
                    flags, nlen = s[o + 25], s[o + 32]
                    ename = s[o + 33:o + 33 + nlen].decode('ascii', 'replace').upper().split(';')[0]
                    if ename == want:
                        found = (elba, esize, bool(flags & 2))
                        break
                    o += n
                if found:
                    break
            if not found:
                raise FileNotFoundError('%s: %s not on the disc' % (self.path, path))
            lba, size, is_dir = found
            if (i < len(parts) - 1) != is_dir:
                raise FileNotFoundError('%s: %s is not a %s' % (self.path, path, 'folder' if is_dir else 'file'))
        return lba, size

    def file(self, path):
        if self.folder:
            return open(os.path.join(self.path, path), 'rb').read()
        lba, size = self._find(path)
        return b''.join(self.sector(lba + k) for k in range((size + SECTOR - 1) // SECTOR))[:size]

    def _table(self, path):
        """.T archive: sector 0 = halfwords [count, start0, start1, ..., end] in sectors.
        AC1's MIS.T writes count = entries + 1; both forms are accepted."""
        if path not in self._tables:
            if self.folder:
                with open(os.path.join(self.path, path), 'rb') as f:
                    head = f.read(SECTOR)
                    total = os.fstat(f.fileno()).st_size
            else:
                lba, total = self._find(path)
                head = self.sector(lba)
            n = struct.unpack_from('<H', head, 0)[0]
            for cnt in (n, n - 1):
                if 0 < cnt and 2 * (cnt + 2) <= SECTOR:
                    idx = struct.unpack_from('<%dH' % (cnt + 2), head, 0)
                    if all(idx[i] <= idx[i + 1] for i in range(1, cnt + 1)) and idx[cnt + 1] * SECTOR <= total:
                        self._tables[path] = (cnt, idx)
                        break
            else:
                raise ValueError('%s is not a .T archive' % path)
        return self._tables[path]

    def entries(self, path):
        return self._table(path)[0]

    def entry(self, path, k):
        cnt, idx = self._table(path)
        if not 0 <= k < cnt:
            raise IndexError('%s has no entry %d' % (path, k))
        s, e = idx[k + 1], idx[k + 2]
        if self.folder:
            with open(os.path.join(self.path, path), 'rb') as f:
                f.seek(s * SECTOR)
                return f.read((e - s) * SECTOR)
        lba, _ = self._find(path)
        return b''.join(self.sector(lba + s + j) for j in range(e - s))

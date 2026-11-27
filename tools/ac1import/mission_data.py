"""Data side of an AC1 mission on Phantasma: the map fixes and the common file (import-outlook.md §2, §4, §9).

Map (AC1 FDAT 2n+1, 19 blocks of [u32 size][bytes]):
  - block 10 (view table, 14-byte records): AC1's engine reads 256, PP's 1,024 -> pad with the filler record
  - block 9 (event points, 32 bytes on disc): hidden-part items carry an owned-parts index at +0x18 -> PP index
Common file (FDAT 200 for an AC1 mission) = Phantasma's FDAT 200 with
  - blocks 4-6 (message sets, one per COM voice) = AC1's lines at AC1's own numbers (see MESSAGES below)
  - block 3 sound-id records 56-255 = AC1's (ids 0-55 are the same on both and stay PP's: decision §9.2)
"""
import struct

FILLER14 = bytes.fromhex('000000000000ffff000000000000')
PP_VIEW_RECORDS = 1024

# AC1 owned-parts index -> PP index for the 11 hidden parts (import-outlook.md §2; content.md §3 bases).
HIDDEN_PART = {95: 159, 72: 112, 2: 2, 142: 231, 122: 201, 67: 99, 136: 225, 109: 178, 89: 145, 125: 209, 146: 240}
ITEM_KINDS = (5, 6, 7)

# Messages. The outlook (§4) proposed AC1's 200 lines placed 50 later, matching the engine's own lines
# (PP 151/155/171/195 = AC1 101/105/121/145). But AC1 mission code, its scripts and the timer record ask for
# lines by AC1's numbers (slot 1 calls line 1 and 104), so shifted lines would show the wrong text.
# So: AC1's lines stay at their own numbers, and the game hook (game/acg_mission_files.c) takes 50 off only
# where the engine itself asks for one of its fixed lines. Lines 200-249 are empty.
MESSAGE_SETS = (4, 5, 6)
SOUND_BLOCK, SOUND_TABLE_AT, SOUND_RECORD, SOUND_SHARED = 3, 448, 10, 56


def blocks(d, n):
    out, o = [], 0
    for _ in range(n):
        k = struct.unpack_from('<I', d, o)[0]
        if o + 4 + k > len(d):
            raise ValueError('block %d runs past the end of the file' % len(out))
        out.append(d[o + 4:o + 4 + k]); o += 4 + k
    return out, d[o:]


def join(bl, tail=b''):
    return b''.join(struct.pack('<I', len(b)) + b for b in bl) + tail


def fix_map(data):
    """returns (new map bytes, notes); raises ValueError on anything it doesn't know"""
    bl, tail = blocks(data, 19)
    notes = []
    v = bl[10]
    if len(v) % 14 or len(v) // 14 > PP_VIEW_RECORDS:
        raise ValueError('block 10 is %d bytes' % len(v))
    if len(v) // 14 < PP_VIEW_RECORDS:
        bl[10] = v + FILLER14 * (PP_VIEW_RECORDS - len(v) // 14)
        notes.append('block 10: %d -> %d records' % (len(v) // 14, PP_VIEW_RECORDS))
    pts = bytearray(bl[9])
    for i in range(len(pts) // 32):
        if struct.unpack_from('<h', pts, 32 * i + 6)[0] == -1:
            continue
        if pts[32 * i + 0x16] in ITEM_KINDS:
            ix = struct.unpack_from('<H', pts, 32 * i + 0x18)[0]
            if ix not in HIDDEN_PART:
                raise ValueError('event point %d: item kind %d with part index %d not in the table' % (
                    i, pts[32 * i + 0x16], ix))
            struct.pack_into('<H', pts, 32 * i + 0x18, HIDDEN_PART[ix])
            notes.append('event point %d: hidden part %d -> %d' % (i, ix, HIDDEN_PART[ix]))
    bl[9] = bytes(pts)
    # the tail after the 19 blocks is never read (sector filler); keep it so the file keeps its shape
    return join(bl, tail), notes


def message_table(ac1, pp, count=250):
    """AC1 message block -> a block with PP's header and `count` entries, AC1 line n at n"""
    def lines(b):
        first = struct.unpack_from('<H', b, 0x14)[0]
        n = (first - 0x14) // 2
        o = [struct.unpack_from('<H', b, 0x14 + 2 * i)[0] for i in range(n)] + [len(b)]
        return [b[o[i]:o[i + 1]] for i in range(n)]
    if ac1[:0x14] != pp[:0x14]:
        raise ValueError('message header differs')
    a = lines(ac1)
    if len(a) > count:
        raise ValueError('%d AC1 lines' % len(a))
    a += [b'\0'] * (count - len(a))
    head = 0x14 + 2 * count
    offs, body, pos = [], b'', head
    for t in a:
        offs.append(pos); body += t; pos += len(t)
    if pos > 0xFFFF:
        raise ValueError('message block too big (%d bytes)' % pos)
    out = pp[:0x14] + struct.pack('<%dH' % count, *offs) + body
    return out + b'\0' * (-len(out) % 4)


def common_file(ac1_200, pp_200):
    abl, _ = blocks(ac1_200, 8)
    pbl, ptail = blocks(pp_200, 8)
    out = list(pbl)
    for k in MESSAGE_SETS:
        out[k] = message_table(abl[k], pbl[k])
        if len(out[k]) > len(pbl[k]):
            # the engine copies the active set next to its own data (0x800A52F0); never make it longer than PP's
            raise ValueError('message set %d: %d bytes, PP has room for %d' % (k - 4, len(out[k]), len(pbl[k])))
    s = bytearray(pbl[SOUND_BLOCK])
    a = abl[SOUND_BLOCK]
    if len(a) != len(s):
        raise ValueError('sound block sizes differ')
    lo = SOUND_TABLE_AT + SOUND_RECORD * SOUND_SHARED
    s[lo:SOUND_TABLE_AT + SOUND_RECORD * 256] = a[lo:SOUND_TABLE_AT + SOUND_RECORD * 256]
    out[SOUND_BLOCK] = bytes(s)
    data = join(out)
    return data + b'\0' * (-len(data) % 2048)


def mission_values(ac1_eng, count=100, at=0x800906CC, base=0x8004ADA0):
    """AC1's per-mission u16 table (import-outlook.md §1, §8): one value per AC1 mission number"""
    o = at - base
    return ac1_eng[o:o + 2 * count]

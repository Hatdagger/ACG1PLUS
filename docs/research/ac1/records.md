# AC1 topic 2 — Main records: AC object, player block, stat block, save, missions, mail

AC1 mapping chat, topic 2 (`../mapping/PLAN-AC1.md`), 2026-11-26. Discs: **AC1** = Armored Core USA (SCUS-94182),
**PP** = Project Phantasma USA (SLUS-00670). Static read only (status per row: `code`, `data`, `guess`); tests in
§8. Every address is AC1 unless tagged PP. Module addresses are in FDAT 201 (menu) unless the row says VS (FDAT 204).
Built on topic 1 (`foundations.md`) and the AC1↔PP tables (`../xdisc/ac1-pp/`); PP layouts from
`../mapping/battle-system.md`, `content-data.md`, `stat-block.md`, `scenario.md`.

## In plain words

- **AC object, object pool, mission table, mail block: same as Phantasma.** Same sizes, same field places
  (AC object: 3 fields 2 bytes apart). PP's notes for these apply to AC1 as they are.
- **Player block: PP's is 12 bytes longer.** 6 bytes were added just before the compiled stat block (the per-fight
  numbers worked out from the build), and the stat block itself is 8 bytes longer on PP: AC1 has no padding bytes
  at its front and each of its three weapon slots is 2 bytes shorter (60 vs 62). The first part of the block
  (+0x000 … +0x221) is in the same place.
- **The save on the memory card holds the same things in the same order as PP's**, just smaller: AC1 has room for
  160 parts (PP 256) and no arena. It is exactly the 3,044-byte block PP's own LOAD already knows how to import.
- **AC1's mission list is in PP's format** (MIS.T entry 0, 64 slots). Mail uses PP's format too (MIS.T 818).

## 1. AC object (code)

| | AC1 | PP | Status |
|---|---|---|---|
| Size | 368 (P1 `0x801AB748`, P2 `0x801AB8B8` in VS) | 368 (`0x801D80E0`, `0x801D8250`) | code (address pairs high; stride) |
| Fields +0x00 … +0x164 | same offsets as PP (93 fields seen) | | code |
| Exception | +0xF2, +0xF4, +0xF6 | +0xF0, +0xF2, +0xF4 | code (9 votes each); meaning not read |

So velocity +0xCC, state bytes +0x3C/+0x3D/+0x41, stability +0x15C, AP +0x160 are where PP has them.

**Object pool** (shots, missiles, effects; VS): **`0x801B645C`, 256 × 224 bytes**, cleared by `0x80081FD0`
(PP `0x801DA47C`, 256 × 224, `0x8007FCF8`) `code`.

## 2. Player block (code)

**AC1 924 bytes (0x39C), PP 936.** Blocks `0x80040FA0` / `0x8004133C` (PP `0x800424E8` / `0x80042890`). The VS engine
keeps backups at `0x8008C2B8` / `0x8008C654` (PP `0x8008AE40` / `0x8008B1F0`). Current-block pointer `0x80039D18`
(PP `0x8003BD00`); round number `0x80039D2A` (PP `0x8003BCF4`).

| AC1 | PP | What | How we know |
|---|---|---|---|
| +0x000 … +0x221 | same | pad words, fight flags, energy, lock slots, movement values, missile cursor (+0x220) | struct table; absolute globals with delta 0 |
| +0x222 … +0x245 | +0x222 … +0x24B | PP has **6 more bytes** somewhere in here (not pinned: few reads) | delta 0 at +0x220, +6 at +0x246 |
| +0x246 … +0x251 | +0x24C … +0x257 | incl. **"HIT" blink +0x24E** (PP +0x254) and **hit indicator +0x250** (PP +0x256) | +0x246 → +0x24C high; +0x24E, +0x250 medium |
| +0x252 … +0x253 | — | 2 bytes with no PP place seen (padding before the stat block `guess`) | |
| **+0x254 … +0x38F** | **+0x258 … +0x39B** | **compiled stat block, AC1 316 bytes, PP 324** (§3) | compile routine base (§3) |
| +0x390 | +0x39C | round history; the boot loop clears it for both blocks (`0x80012480`) like PP | code |
| +0x394 | +0x3A0 | pointer to the AC object | struct table (41 votes) |
| +0x398 | +0x3A4 | pointer to the camera record | struct table (46 votes) |

The struct table's "+6 shift" rows for +0x264 … +0x2D8 and "+4" for +0x254 are the stat block moving 4 bytes along
(6 added before it, 2 AC1 bytes gone) plus its own inner shifts (§3).

## 3. Compiled stat block (code)

**Compile routine `0x8009C964`** (25,504 bytes, no automatic PP twin: PP rewrote it as `0x8009A238`), called twice
from AC1's VS START `0x800AA710` (PP `0x800A654C`) at `0x800AA7C8` (player 0) and `0x800AA938` (player 1). It
starts by setting **block = `0x800411F4` + player × 924** (= player block +0x254). Before it, VS START uses the
garage loadout at **`0x80031A58`** (PP `0x800342B4`), Human PLUS bytes **`0x80039D20`** (PP `0x8003BD24`) and the
option mask **`0x80039CA0`** (PP `0x8003BCCC`).

Layout, AC1 offset → PP offset (from the paired globals inside player block 1; PP field names from
`stat-block.md` / `content-data.md`):

| AC1 | PP | Field (PP meaning) | Shift | Confidence |
|---|---|---|---|---|
| +0x00 … +0x03 | +0x00 … +0x03 | time limit, flags, Human PLUS level | 0 | medium |
| — | +0x04 | (padding on PP) | | |
| **+0x04** | +0x05 | **stage / mission engine number** (`0x800411F8` for player 1) | +1 | low (1 vote) + code (§6) |
| +0x05 … +0x0D | +0x06 … +0x0E | thrust ×3 sets, turn, friction | +1 | high (`0x80041201` → PP `0x8004274E`) |
| +0x0E, +0x0F | +0x0F, +0x10 | missile interceptor reaction, angle | +1 | low |
| — | +0x11 | (padding on PP) | | |
| **+0x10 … +0x83** | +0x12 … +0x85 | energy capacity, drains, refills, radar, AP×4, damage factors (+0x20/+0x21), stability, radar flags, body frame (+0x26), **body combination (+0x27 = `0x8004121B`)**, part ids, … | **+2** | high (12+ high pairs: +0x1E, +0x24, +0x28, +0x32, +0x3C … +0x50, +0x6A) |
| **+0x84** | +0x86 | **number of usable weapons** (MoA +0x87) | +2 | high |
| **+0x86 + slot × 60** | +0x88 + slot × 62 | **weapon slots**, 3 of them | +2, +4, +6 | high for slot 0 and 1 |
| +0x13A … +0x13B | +0x142 … +0x143 | end of block | | |

Inside a weapon slot, offsets +0x00 … +0x2A match PP's (slot 0: +0x0E max ammo, +0x10 ammo, +0x12, +0x24, +0x2A
paired at the same offsets); PP's 2 extra bytes per slot are after +0x2A, exactly where not read yet. Slot 1 at AC1
+0xC2 → PP +0xC6 (both medium/low votes) fits 60 vs 62.

What the extra PP bytes hold, and the formulas (thrust, stability…) in AC1's compile routine, are not compared yet:
AC1's routine has to be read on its own, as PP's was (`content-data.md` §2 compared PP and MoA by running both).

## 4. Save block on the memory card (code)

**SAVE = `0x80066F90`** (PP `0x80065ECC`, 0.94 alike), buffer **3,712 bytes** (0xE80; PP 3,968), one 8 KB card
block. **Checksum `0x80066F68`**: byte sum over **+0x230 … +0xE13 (3,044 bytes)**, stored at **+0x200**. PP has
the same routine, used only for AC1 saves (PP `0x80065EA4`, identical code), and its LOAD (PP `0x80066C44`) holds an
"AC1 import" path for exactly 3,044 bytes: so PP reads this block.

| AC1 offset | Size | From RAM | What | PP offset (size) |
|---|---|---|---|---|
| +0x000 | 4 | — | `S`, `C`, 0x11, 1 | same |
| +0x004 | 64 | `0x800B7628` + filled in | title (+0x1A … +0x2F digits; +0x44 … +0x5F cleared) | same |
| +0x060 | 32 | `0x800B7668` | icon palette | same |
| +0x080 / +0x100 / +0x180 | 128 each | menu data | icon frames | same |
| **+0x200** | 4 | — | **checksum** | same |
| +0x204 | 36 | `0x80031BD4` | names, list copy | same |
| +0x228 | 2 | `0x80041B3C` | missions launched (saved T+0x2C), list copy | same |
| +0x22C | 4 | `0x80039CA4` | credits, list copy | same |
| **+0x230** | 12 | `0x80031A58` | **equipped parts** (checksum starts here) | same |
| +0x23C / +0x23D | 1 + 1 | `0x80039CA2` / `0x80039CA3` | `?` (PP `0x8003BCCE` / `CF`) | same |
| +0x23E | 2 | `0x80039CA0` | option parts mask | same |
| +0x240 | 4 | `0x80039CA4` | **credits** | same |
| +0x244 | 36 | `0x80031BD4` | names (AC, pilot) | same |
| +0x268 | 96 | `0x800485A0` | owned-part bitmasks | same |
| +0x2C8 | 48 | `0x80031A64` | paint | same |
| **+0x2F8** | **160** | `0x80031A94` | **owned-part counts** | +0x2F8 (256) |
| **+0x398** | **160** | `0x80031B34` | **shop stock** | +0x3F8 (256) |
| +0x438 | 2,048 | VRAM rect `0x800B7618` | emblem pixels | +0x4F8 |
| +0xC38 | 32 | VRAM rect `0x800B7620` | emblem palette | +0xCF8 |
| +0xC58 | 120 | `0x80041B10` | **missions**: T+0 … +0x77 | +0xD18 |
| +0xCD0 | 56 | `0x80048630` | settings record | +0xD90 |
| +0xD08 | 64 | `0x80049E70` | the record PP uses for the **arena** (rank + 15 unread words). AC1 has no arena: what AC1 keeps here is not read yet | +0xDC8 |
| +0xD48 | 7 | `0x80039D20` | Human PLUS level and flags | +0xE08 |
| +0xD4F | 196 | `0x80049EC8` | **mail** S+0 … +0xC3 | +0xE0F |
| — | | | (no arena beaten flags) | +0xED3 (52) |
| +0xE13 | | | checksum ends | +0xF07 |

So PP = AC1 + 96 more owned-part counts + 96 more shop entries (all later fields 0xC0 further on) + the 52 arena
flags. The owned-part counts and shop stock are indexed by the same part index on both (PP's category bases end
at blade 237; AC1's 160 entries fit inside: which index is which AC1 part is topic 3).

## 5. Mission table and mail (code + data)

| | AC1 | PP | Status |
|---|---|---|---|
| Mission table T | **`0x801F3750`** | `0x80186A40` | code (pair high) |
| MIS.T entry 0 copy | T+0xC4 (`0x801F3814`), 0x620 words, callback `0x80086E34`, load `0x80086EB0` | T+0xC4, 0x620 words | code |
| Saved part | T+0 … +0x77 → `0x80041B10` (store `0x80086654`, same code as PP `0x800856FC`) | → `0x80043108` | code |
| Mail block S | **`0x8017F6D0`** | `0x8018D5D0` | code |
| MIS.T 818 copy | S+0x110, 0x420 words, then 64 trigger pairs to S+0x40 (callback `0x800963A0`, load `0x80096454`) | same | code |
| Saved part | S+0 … +0xC3 → `0x80049EC8` (store `0x800960C0`, same code as PP `0x80093778`) | → `0x80043AB8` | code |
| Other MIS.T loads | entry 817 (`0x8008C3C4`), briefing pages entry *n*+1 (`0x8008906C` …), mail pages 819+ (*n* + 0x333, `0x800987F8` …) | | code |

**AC1's MIS.T entry 0 in PP's format** (data): windows at 0x000, names at 0x100 (64 × 32), rewards at 0xA00,
engine mission numbers at 0xB00, fight music at 0xB40, page records at 0xB80 all read sensibly. Slots 0–49 run
engine missions 0–49 (slots 16, 29, 39 are "Omitted" placeholders, slot 0 "Dummy00", slot 49 "Destruction of
Ravens" has no window); **slot 50 "AC Battle" runs engine mission 60**; slots 51–63 are unused Japanese placeholder
names. The full list is topic 3.

## 6. Mission number and the boot loop (code)

At launch the engine mission number goes to **stat +0x04 (`0x800411F8`)** (PP stat +0x05 `0x80042745`; same
role). When an engine returns, AC1's boot loop (`0x80012538` …) tests that byte for **21 or 22, 47 or 48, and 49**:
the last missions of the story ("Mop Up Chrome Remnants", "Destroy 'Justice'", the second "Mop Up Chrome
Remnants", "Destroy Floating Mines", "Destruction of Ravens"), then branches; most likely the ending movies
(ACED1–5) `guess`.

## 7. RAM rows
Added to `../ram-map.toml` (`disc = "AC1"`, src `ac1/records.md`): stat block, mission number, VS AC objects, object
pool, VS player-block backups, current-block pointer, round number, garage loadout, paint, owned counts, shop stock,
names, credits, option mask, Human PLUS bytes, owned bitmasks, settings, the 64-byte PP-arena record, mission table,
its saved copy, mail block, its saved copy.

## 8. Tests waiting (also in `../mapping/ROUND1.md`)
- **A4** AC object: in an AC1 VS fight, P1's velocity at `0x801AB748` + 0xCC changes while boosting (same field as
  PP; confirms the whole "same layout" finding).
- **A5** Stat block: in an AC1 mission, `0x800411F8` = the mission's engine number (e.g. 1 for "Stop Terrorist
  Threat"); in VS, P1's energy at `0x80040FA0` + 0x1E starts at the capacity `0x80041204` (stat +0x10).
- **A6** Save: an AC1 save file from a real card: byte sum of file +0x230 … +0xE13 = the word at +0x200.

## 9. Open
- AC1 compile formulas vs PP (thrust, stability, damage factors), and what PP's extra stat-block bytes hold.
- What AC1 keeps in the 64 bytes at `0x80049E70` (PP's arena record) and in MIS.T 817.
- AC object +0xF2…+0xF6 (moved by 2): meaning.
- Engine slot 70 (topic 1) is not used by any mission list slot: engine mission 70 may be the garage test fight or a
  demo `guess` (topic 3).

## Handed over
- To the round 2 mapping chat (`missions-import-pp.md`): AC1's mission list (MIS.T 0) and mail list (MIS.T 818) are
  already in PP's format, and PP's LOAD already imports AC1 saves.

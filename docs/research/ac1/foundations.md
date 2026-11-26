# AC1 topic 1 — Foundations: boot program, FDAT, archives, AC1 ↔ PP tables

AC1 mapping chat, topic 1 (`../mapping/PLAN-AC1.md`), 2026-11-26. Discs: **AC1** = Armored Core USA (SCUS-94182
v1.0, image sha256 `0c7f6350…f4d4d59`; the 2000 rerelease SLUS-01323 has the same boot program byte for byte),
**PP** = Project Phantasma USA (SLUS-00670). Everything here is a **static read** of the owner's discs (work packs
`ac1-work`, `pp-work`): status `code` = read from code, `data` = read from the files, `guess` = inferred. Nothing has
been seen running yet (tests in §9). Every address is AC1 unless tagged PP.

## In plain words

- **AC1 is built the same way as Phantasma**: one boot program, a big archive (FDAT) of modules, the same 19 archive
  numbers with the same names, the same body-kit files (`PAnn.T`). The menu, missions, link-cable and split-screen
  VS engines all have a twin on Phantasma. AC1 just has no arena engine and no VS replay (those came with PP).
- **The code is the same code, compiled differently.** Most AC1 routines line up with their PP twin once you ignore
  which registers the compiler picked and the order it put instructions in. About 3 in 4 routines of the fight
  engines pair up well (`high`/`medium`); the menu much less (it was reworked for PP).
- **The records barely moved.** The AC object has the same layout on AC1 and PP (only 3 fields 2 bytes apart).
  The player block is the same up to +0x220, then grows by 4–12 bytes on PP. That is much closer than MoA↔PP.
- **The disc content is mostly shared.** Almost all of AC1's part models and 97% of its body-kit pieces are
  byte-identical on PP. Missions and stages are packed differently (AC1 uses 2 FDAT entries per mission or stage,
  PP 5), and the per-mission code loads at a high address on AC1.
- So old knowledge **can** be carried over by lookup (tables in `../xdisc/ac1-pp/`), with one warning: on AC1 a
  `low` pair is often still the right routine, just reordered. Check by eye.

## 1. What to use

| Need | Use |
|---|---|
| AC1 routine → PP routine | `../xdisc/ac1-pp/functions.tsv` |
| AC1 global / table / RAM address → PP | `../xdisc/ac1-pp/globals.tsv`, then `runs.tsv` |
| AC object / player block field PP → AC1 | `../xdisc/ac1-pp/struct-ac-object.tsv`, `struct-player-block.tsv` (keyed by PP offset) |
| Other struct fields | `../xdisc/ac1-pp/fields.tsv` |
| Named AC1 RAM addresses | `../ram-map.toml` rows with `disc = "AC1"` → `../ram-map-ac1.md` |
| Rebuild | §8 |

## 2. Boot program and archives (code)

| What | AC1 | PP | Status |
|---|---|---|---|
| Boot exe | `SCUS_941.82`, 169,984 B, text 0x29000 | `SLUS_006.70`, 178,176 B, text 0x2B000 | data |
| Load address / entry | `0x80011000` / `0x80011E6C` | `0x80011000` / `0x80011E94` | data |
| gp | `0x80039C5C` (set at `0x80011EEC`) | `0x8003BC7C` | code |
| Exe image end / bss end (= module base) | `0x8003A000` / `0x8004ADA0` | `0x8003C000` / `0x800490D0` | code |
| Register archive `(file, name)` | `0x80016678` | `0x80017244` | code |
| Loader `(file, entry, dest)` | `0x800165E4` | `0x800171B4` | code |
| Archive table (20 × 12 B: entry-list pointer, CD position, size) | `0x8004A2A4` | `0x8004135C` | code |
| Archive names | `0x80011000` (exe +0x800 in the file) | `0x800111FC` | data |

The register routine glues `cdrom:\GG\` (gp+0x30) + name + `;1`, finds the file (`0x800191C0`), reads its first
sector and copies the entry list (start sectors, halfwords) to a growing list at `[0x8004A394]` (error if it reaches
`0x80048508`). The loader reads entry *n* of archive *f* into *dest*. Both are the same code as PP's (loader pair
likeness 0.96; the register routine differs only in register use and order).

**File numbers are identical to PP** (registered at `0x80012304`–`0x8001241C`): 1 RTIM, 2 FDAT, 4 MENU_TIM, 5 MENU_TMD,
6 MENU_VAB, 7 CORE_T, 8 LEG_T, 9 ARMS_T, 10 HEAD_T, 11 BST_T, 12 BWL_T, 13 BWR_T, 14 WEL_T, 15 WER_T, 16 COMP_T,
17 SPEC_T, 18 GENE_T, 19 MIS. **File 0** = the body kit `GG\Pn\PAnn.T`, registered by each fight engine (§4).
**File 3** is never registered on AC1 (no call with 3 found) `code`.

## 3. Modules (FDAT 200–204) and the boot loop (code)

| AC1 FDAT | Module | Size | Entry | PP twin | Paired code (high / medium / low) |
|---|---|---|---|---|---|
| 200 | common (shared by every engine) | 282,624 | — | PP 200 | data, not compared |
| 201 | menu | 542,720 | `0x8009B414` (called by address) | PP 201 | 5% / 18% / 30% of bytes; 20% data tables, 26% unpaired |
| 202 | missions engine | 286,720 | `0x8004C340` | PP 202 | 47% / 32% / 17% |
| 203 | link engine (two consoles) | 268,288 | `0x8004BF94` | PP 203 | 51% / 31% / 15% |
| 204 | VS engine (split screen) | 268,288 | `0x8004BF3C` | PP 204 | 52% / 28% / 16% |
| — | (no arena engine, no VS replay) | | | PP 205, 206 | |

Every module loads at **`0x8004ADA0`** (PP `0x800490D0`): 4,313 internal calls of the menu and 1,500+ of each engine
land on routine starts at that base and nowhere else `code`. The engines start with the same header as PP's
(word 0 = 4, word 1 = entry, then "ENERGY"…); the menu's word 0 = 5 and the boot code calls its entry by address.
Which engine twins which PP module was decided by code likeness (each AC1 engine is closest to the PP engine of the
same number) `code`.

**Boot loop** (`0x800122A8`, code): register the archives → show RTIM 101 (`0x800121AC`) → load the menu (FDAT 201,
`0x80012458`) and run it (`0x8009B414`) → then loop: by the **mode byte gp+2 = `0x80039C5E`** load **0 → FDAT 202
missions, 1 → FDAT 203 link, 2 → FDAT 204 VS**, call the entry word at `0x8004ADA4`, repeat while it returns non-zero.
Same mode numbers as PP (PP `0x8003BC86`; PP adds 3 = arena and a replay flag). What happens on return (the checks
on `0x800411F8` against 0x15, 0x16, 0x31, 0x2F) is not read yet.

Boot exe pairing: 21% high, 28% medium, 14% low, 35% unpaired (by bytes). Much of the unpaired part is library
code: AC1 links an older PsyQ library, and calls go to it at scattered places (`0x800191C0` file search,
`0x8002A440` string copy), so the game/library border used in `ram_map.py` (`0x80016F7C`, carried over from PP's)
is approximate `guess`.

## 4. FDAT 0–199: missions and stages (code + data)

AC1 does **not** use PP's 5-entries-per-slot layout. It uses **slots of two entries**: **FDAT 2n = code,
FDAT 2n+1 = map/stage data**, and the same slot number picks **RTIM n** (that slot's pictures). Which engine a slot
belongs to was read from the loader calls and checked by where each code module's self-pointers land.

| Slots | FDAT | Engine | Code goes to | Slot byte | Data buffer | Count |
|---|---|---|---|---|---|---|
| 0–49 | 0–99 | missions (202) | `0x801C4B40` | `0x8019F518` | `[0x8019F520]` | 47 (slots 16, 29, 39 empty) |
| 60, 70 | 120–121, 140–141 | missions (202) | `0x801C4B40` | same | same | 2 (not named yet) |
| 80–84 | 160–169 | VS (204) | `0x801C8460` | `0x801AB678` | `[0x801AB680]` | 5 stages |
| 90–94 | 180–189 | link (203) | `0x801B80C8` | `0x8019B480` | `[0x8019B488]` | 5 stages |

Each engine loads, in this order (loader call sites, missions / link / VS): body kit archive 0 entry 1
(`0x8004F244` / `0x8004F88C` / `0x8004F36C`), RTIM 100 (link `0x8004FF5C`, VS `0x8004FA54`; the missions engine passes the
entry in a register, not read yet), RTIM *slot*, FDAT 200 common (`0x8004F5CC` / `0x8004FF98` / `0x8004FA90`), FDAT 2×slot → code
(`0x8004F784` / `0x80050084` / `0x8004FB7C`), FDAT 2×slot+1 → buffer (`0x8004F7A4` / `0x800500A8` / `0x8004FBA0`).
The VS and link stages are separate files: a stage's data is stored once for each engine (e.g. FDAT 183 has the
same header as mission map FDAT 95).

PP for comparison: missions 5 per slot (code at `0x80097A38`), stages 5 per stage with one code entry per engine
(link `0x800904B0`, VS `0x8008F5D0`, …), stage data shared. The AC1↔PP table pairs the slot bytes and buffers
(`0x8019F518` → PP `0x80097A18`, `0x8019B480` → PP `0x80090490`, `0x801AB678` → PP `0x8008F5B0`) and AC1's link
code address with PP's (`0x801B80C8` → PP `0x800904B0`), so the engines do the same job in the same place in
their code; only AC1 puts the per-slot code high in memory.

**Body kit (file 0)**: each engine fills a name string `P0\PA00.T` (missions `0x8008D928`, link `0x800895D0`, VS
`0x800893D4`) with the body number, `P<n ÷ 20>\PA<nn>.T`, and registers it as archive 0 (missions `0x8004F1A8`).
The number is **player block 1 +0x27B = `0x8004121B`** (PP block +0x281 = `0x80042769`, `../mapping/pa-archives.md`).

**RTIM** (104 entries, 63 used): 0–49 mission pictures (16, 29, 39 empty), 60, 70, 80–84 (VS), 90–94 (link), **100**
fight-common pictures (PP's RTIM 60 plays this role), **101** the boot picture, 102 and 103 not placed yet.

## 5. Other archives side by side (data)

Entries total / used; "same bytes" = entries whose bytes also appear somewhere in the PP archive of the same name.

| Archive | AC1 | PP | AC1 entries also on PP |
|---|---|---|---|
| FDAT | 205 / 123 (26.9 MB) | 207 / 123 (19.5 MB) | 0 |
| RTIM | 104 / 63 | 63 / 61 | 5 |
| MENU_TIM (menu pictures) | 122 / 122 | 524 / 321 | 71 of 118 different |
| MENU_TMD (garage models) | 150 / 150 | 249 / 173 | 144 of 145 |
| MENU_VAB (menu sounds) | 5 / 5 | 5 / 5 | 5 of 5 |
| MIS (screens, briefings, mail) | 883 / 547 (3.1 MB) | 833 / 149 (0.8 MB) | 1 |
| HEAD_T / LEG_T / CORE_T / GENE_T / WER_T | 160 / 512 / 48 / 7 / 16, all used | 288 / 640 / 176 / 15 / 21 | all |
| ARMS_T / BWL_T | 256 / 34 | 384 / 59 | all but 1 each |
| PA00–PA71 (body kits) | 176 entries each, 3,883 different pieces | 268 each | 3,780 (97%) |

AC1's `MIS.T` header counts its end marker (one more than the others); the pack's `tarc.py` reads it, the repo's
`tools/` copies don't yet. Content details (which parts, missions, briefings) are topic 3.

## 6. Struct fields, AC1 → PP (code)

**AC object**: every field seen (93, from +0x00 to +0x164, through registers whose PP side touches known PP AC
fields) is at **the same offset on AC1 and PP**, except **AC1 +0xF2, +0xF4, +0xF6 → PP +0xF0, +0xF2, +0xF4** (−2;
9 votes each). PP's velocity +0xCC, state bytes +0x3C/+0x3D/+0x41 and AP +0x160 are the same on AC1. (MoA moved
these by 8–9 bytes.)

**Player block** (AC1 924 bytes, PP 936): blocks at **`0x80040FA0` and `0x8004133C`** (PP `0x800424E8` /
`0x80042890`). Fields up to **+0x220 are the same**; after that PP is higher by:
| AC1 | PP | Shift | Votes |
|---|---|---|---|
| +0x254 | +0x258 | +4 | 14 |
| +0x259 … +0x261 | +0x25E … +0x266 | +5 | 2 each |
| +0x264 … +0x2D8 (incl. +0x27A, +0x27B body number, +0x29A) | +0x26A … +0x2DE | +6 | 2–28 |
| +0x390 … +0x398 (camera pointer) | +0x39C … +0x3A4 | +12 | 41–46 |
So PP added a few bytes inside the compiled stat block area (+0x250…) and 6 more before the camera pointer.
Where exactly is topic 2.

**Other structs** (`fields.tsv`): a menu record seen 124 bytes lower on PP (AC1 +0x80 → PP +0x04, 41 votes), an exe
record +8 (AC1 +0x90 → PP +0x98, 55 votes), an exe −4 family (AC1 +0x4 → PP +0x0, 51 votes). Not named.

## 7. The tool (`tools/xdisc/ac1_xdisc.py`)

Same outputs and columns as `xdisc.py` (MoA↔PP), whose helpers it imports (reference scan, data-table score). The
difference is the pairing: **register-blind**. An instruction is compared by its operation and its logic constants
(and/or/slt immediates, shift amounts, GTE commands), not by registers, stack offsets or address halves. Needed
because AC1's compiler picks other registers and orders instructions differently: the plain comparison found
only 1–5% identical routines, the register-blind one pairs 3 of 4 engine routines `high`/`medium`.
Confidence: `high` = same operations, or likeness ≥ 0.95, or ≥ 0.80 with every call checked; `medium` ≥ 0.80;
`low` ≥ 0.50. Spot checks by eye: a `low` VS pair (`0x8004D524` ↔ PP `0x8004BB58`, 0.75) is the same routine with
three instructions moved; the loader (0.96) and register routine (0.76) pairs are right.

Counts (2026-11-26): routine rows 4,403 (AC1 side high 1,262, medium 1,152, low 681, data 11, unpaired 405;
PP-only 892: code PP added or rewrote, not sorted yet). Globals: 5,068 AC1 addresses (1,095 high,
895 medium, 3,078 low). 225 runs.

## 8. Rebuilding
From a folder with `ac1-work` and `pp-work` extracted and `setup.py` run in each:
```
python3 -I tools/xdisc/ac1_xdisc.py --ac1 WORK/ac1-disc --pp WORK/pp-disc --out docs/research/xdisc/ac1-pp
python3 -I tools/ram_map.py --disc AC1 --generated /none --exe WORK/ac1-disc/SCUS_941.82 \
        --modules WORK/ac1-disc/unpacked/GG/COM/FDAT
```
(`--generated /none` stops it looking for PP's generated folder; AC1 is never generated.)

## 9. Tests waiting (also in `../mapping/ROUND1.md`)
- **A1** mode byte: `0x80039C5E` = 2 in a VS fight, 0 in a mission, 1 in a link fight.
- **A2** slots: in a VS fight `0x801AB678` is 80–84; in a mission `0x8019F518` is the mission's slot (note which
  stage / mission name goes with which number: feeds topic 3).
- **A3** body number: `0x8004121B` changes with core + arms + legs the same way PP's `0x80042769` does.

## 10. Open
- Slots 60 and 70 (mission-engine slots outside 0–49), RTIM 102–103: topic 3.
- The boot loop's return checks (`0x800411F8` = player block 1 +0x258: values 0x15, 0x16, 0x2F, 0x31): topic 2.
- Where inside the stat block PP added its bytes: topic 2.
- The menu module pairs poorly (5% high): expected, PP reworked the menus.

## Handed over
- To the round 2 mapping chat (Lane 1 / foundations open items): PP's file 0 is the body kit too (same `P0\PA00.T`
  string in every PP engine, e.g. `0x8008BFDC` in FDAT 202), as `pa-archives.md` already says; file 3 is not
  registered on AC1 either.

# Lane 1 — Foundations: disc layouts, overlays, cross-disc tools

Mapping lane 1 of the 2026-11-26 restart (`PLAN.md`). Discs: **PP** = Project Phantasma USA (SLUS-00670 v1.0,
image sha256 `1dbedf48…26bd0f`), **MoA-US** = Master of Arena USA disc 1 (SLUS-01030 v1.0). Everything here is
static (read from the files on the disc, nothing run) unless marked `tested`. Status tags: `code` = read from
code, `guess` = inferred.

**In plain words:** Phantasma and Master of Arena are built the same way: one boot program, then one big archive
of game modules where the game swaps in the menu or a fight engine as needed. Almost everything has a twin on the
other disc, but things were moved around in memory. This lane made the tools and tables that turn a Master of
Arena address into the Phantasma one, so every other lane can reuse the old research.

## 1. What to use (for the other lanes)

| Need | Use |
|---|---|
| MoA routine → PP routine | `../xdisc/functions.tsv` (or `../xdisc/pp-catalogue.tsv`, sorted by PP address, with the notes that mention the MoA twin) |
| MoA global / table / RAM address → PP | `../xdisc/globals.tsv` (one row per address, votes + confidence); `../xdisc/runs.tsv` for an address that isn't listed (stretches that all moved by one amount) |
| AC object field MoA → PP | `../xdisc/struct-ac-object.tsv` (summary in §5) |
| Player block field MoA → PP | `../xdisc/struct-player-block.tsv` (summary in §5) |
| Other struct fields | `../xdisc/fields.tsv` (all structs mixed: check the routines it lists) |
| Named RAM addresses on PP | `../ram-map.toml` rows with `disc = "PP"` → `../ram-map-pp.md` |
| Seeds for a PP recomp | `seeds/pp/ghidra_funcs.txt` |
| Rebuild all of the above | §7 |

Rule of thumb for the confidence columns: `high` = the routine is the same code on both discs (only addresses
differ), take it; `medium` = paired with some differences or by fewer votes, fine for reading, check before
patching; `low` = a lead, not an answer. `data` in functions.tsv = the "routine" is really a data table.

## 2. Disc layout, side by side

Both discs: `SYSTEM.CNF`, `LICENSEA.DAT`, `COPY.TXT`, the boot exe, and a `GG/` tree. Sector = first LBA.

| What | PP file (sector, size) | MoA-US file (sector, size) | Notes |
|---|---|---|---|
| Boot exe | `SLUS_006.70` (38, 178,176 B; text 0x2B000) | `SLUS_010.30` (39, 190,464 B; text 0x2E000) | both load at 0x80011000; entry PP 0x80011E94, MoA 0x80011D84; gp PP 0x8003BC7C, MoA 0x8003EA28 |
| — | — | `PDA.BIN` (132, 20,132 B) | MoA only (PocketStation program) `guess` |
| — | — | `GG/COM/CD.DAT` (132529, 4 B) | MoA only: which disc is in the drive (two-disc game) |
| Game modules | `GG/COM/FDAT.T` (71793, 207 entries, 19.4 MB) | `GG/COM/FDAT.T` (132530, 212 entries, 17.7 MB) | §3 |
| Pictures (TIM) | `GG/COM/RTIM.T` (63 entries) | `GG/COM/RTIM.T` (69) | |
| Menu pictures, font, arena data | `GG/MS/MENU_TIM.T` (524 entries, 321 used) | (885, 833 used) | MoA's arena rosters live here |
| Garage part models | `GG/MS/MENU_TMD.T` (249, 173 used) | (252, 192 used) | |
| Menu sound banks | `GG/MS/MENU_VAB.T` (5) | (5) | |
| Mission screens, briefings, mail | `GG/MS/MIS.T` (833 entries, 149 used) | (409, 184 used) | PP's has twice the slots |
| Part model/texture archives | `GG/MS/{ARMS,BST,BWL,BWR,COMP,CORE,GENE,HEAD,LEG,SPEC,WEL,WER}_T.T` | same names | same entry counts except BWL/BWR (59 vs 60), WEL (9 vs 6), WER (21 vs 26); 16 entries per part slot in ARMS/HEAD/LEG/CORE (Lane 5) |
| Stage archives | `GG/P0`–`P3/PA00`–`PA71.T` (72 files, 268 entries each) | same names (72 files, 275 entries each) | not yet tied to code; see §6 |
| Fight music (XA) | `GG/BGM/BGM00`, `BGM01` | `BGM00`–`BGM03` | MoA has twice the fight music |
| Voices (XA) | `CPU00`–`CPU23` (12), `TALK00`–`05` | `CPU00`–`CPU23` (12), `TALK00`–`06` | |
| Movies (STR) | `ASLOGO`, `ACOPA`, `DEMOPLAY`, `ACED5` (= same bytes as ACOPA), `STAFF` | `ACOPA`, `ACEND`, `DEMOPLAY`, `STAFF`, `STAFF2` | |
| Filler | `GG/DUMMY.DDD` (28 MB) | `_29M.DMY` (29 MB) | never read |

**File numbers** (code, both discs identical): the boot exe registers 18 archives with the routine PP
`0x80017244` / MoA `0x80016AE8` (`file, name`), names at PP `0x800111FC` / MoA `0x80011000`. Loads then use
`loader(file, entry, dest)` = PP `0x800171B4` / MoA `0x80016A58` (identical code).

| File | 1 | 2 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Archive | RTIM | FDAT | MENU_TIM | MENU_TMD | MENU_VAB | CORE_T | LEG_T | ARMS_T | HEAD_T | BST_T | BWL_T | BWR_T | WEL_T | WER_T | COMP_T | SPEC_T | GENE_T | MIS |

Files 0 and 3 are not registered at boot. **File 0 = the player's body archive** (round 2 topic 5, code): every
fight engine registers it with `0x80017244(0, name)` just before its "file 0, entry 1" load (missions
`0x8004D7F4`, link `0x8004DE80`, VS `0x8004D8C4`, arena `0x8004E080`, replay `0x8004D814`, all PP), after writing
the digits of player block +0x281 (the body combination 0–71) into a `P0\PA00.T` template (VS template at
`0x800874C4`): folder digit = n ÷ 20, then the two-digit number. So the name is `GG/P(n÷20)/PA(nn).T`
(`pa-archives.md`). **File 3 is never used on PP**: no register call and no load with file 3 anywhere in the exe
or the six modules (loader calls with a fixed file number pass 0, 1 or 2; the menu's file loader `0x8004F2D8`
is called with 5, 6 or a part archive number). Checked by call-site scan, so `code` for the fixed ones. It is a free slot, for
example for our own pack.

## 3. FDAT.T and the overlay table

### Layout (code + header words)
| Entries | PP | MoA-US |
|---|---|---|
| 0–99 | **missions**: 5 per mission, `5n` = mission code (18–55 KB), `5n+4` = mission map (0.3–0.9 MB). 18 missions (slots 17 and 18 = entries 85 and 90 are empty) | same layout, 20 missions |
| 100–199 | **stages**: 5 per stage, 20 stages. `+0` code for the link engine, `+1` for VS, `+2` for arena, `+3` for VS replay, `+4` stage data. Stages 0–9 (entries 100–149) have all four, stages 10–19 (150–199) only link + arena | same layout, 20 stages; which engines each stage has differs |
| 200 | "common" (285 KB; menus' messages, VS sounds): loaded by every engine | — (MoA's common is 205) |
| 201–206 | the six modules below | 201–203 empty; 200 (28 KB) and 204 (231 KB) not named yet; 205 = common |
| — | — | 206–211 = the six modules |

PP's module numbers are MoA's minus 5 (exe loads `0xC9`/`0xCD` on PP where MoA loads `0xCE`/`0xD2`).

### Modules (overlays) — load address and entry
Every module loads at the module base: **PP `0x800490D0`**, MoA `0x80050DD8` (code: the loader calls, and
jal-target voting puts 470–520 internal calls on that base). The fight engines start with a header: word 0 = 4,
**word 1 = entry point**, then names ("ENERGY", "MISSION TIMER", "LOCK"…). The menu's header is different
(word 0 = 5, then four VRAM rectangles x, y, w, h: (448, 192, 16, 64), (368, 225, 16, 1), (768, 448, 64, 64),
(0, 493, 256, 1) that the hub ring copies at `0x8004C018` … and `0x8004E084` …, then texts from `0x800490F4`).
**The menu is not entered through its header**: the exe loads FDAT 201 and calls **`0x80098EF0`** directly
(`jal` at `0x80012E2C`; MoA `0x800A69C8` from `0x80012674`); the fight engines are started through header word 1
(`lw 0x800490D4`, `jalr` at `0x80012FF0`). (round 2 topic 5, code)

| PP FDAT | MoA FDAT | Module | PP size | MoA size | PP entry | MoA entry | Routines paired |
|---|---|---|---|---|---|---|---|
| 201 | 206 | menu (hub, garage, shop, VS menu…) | 548,864 | 618,496 | `0x80098EF0` (direct call) | `0x800A69C8` (direct call) | 1,079 |
| 202 | 207 | missions engine | 290,816 | 294,912 | `0x8004A68C` | `0x800524B8` | 667 |
| 203 | 208 | link engine (two consoles) | 274,432 | 278,528 | `0x8004A2B8` | `0x80051FDC` | 639 |
| 204 | 209 | VS engine (split screen) | 270,336 | 274,432 | `0x8004A228` | `0x80051F4C` | 636 |
| 205 | 210 | arena engine | 294,912 | 303,104 | `0x8004A654` | `0x80052378` | 693 |
| 206 | 211 | VS replay | 270,336 | 274,432 | `0x8004A280` | `0x80051FA4` | 637 |

### What each engine loads at start (code; loader call sites)
| Engine (PP) | file 0, entry 1 | file 1 (RTIM) entry → dest | file 2 (FDAT) | per-mission / per-stage code → dest |
|---|---|---|---|---|
| missions 202 | `0x8004D80C` | `0x8004DB64` (entry from a table) | 200 common `0x8004DB80` | mission code (5 × slot) → **`0x80097A38`** `0x8004DD20`, then the map (5 × slot + 4) `0x8004DD40` |
| link 203 | `0x8004DE98` | 60 → `0x800A3B70` `0x8004E544` | 200 `0x8004E578` | stage code → **`0x800904B0`** `0x8004E658` |
| VS 204 | `0x8004D8DC` | 60 → `0x800A2C88` `0x8004DF9C` | 200 `0x8004DFD0` | stage code → **`0x8008F5D0`** `0x8004E0B4` |
| arena 205 | `0x8004E098` | 60 → `0x800ACA68` `0x8004E798` | 200 `0x8004E7CC` | stage code → **`0x800956F0`** `0x8004E8B0` |
| replay 206 | `0x8004D82C` | 60 → `0x800A2EE8` `0x8004DEC4` | 200 `0x8004DEF8` | stage code → **`0x8008F828`** `0x8004DFDC` |

MoA-US for comparison: mission code → `0x800A0DD0`; stage code link `0x80098E40`, VS `0x80097EC0`, arena
`0x8009EFE8`, replay `0x80098118`; RTIM entry 61 (not 60); common = FDAT 205. The cross-disc table finds these
same pairs on its own (e.g. MoA `0x800A0DD0` → PP `0x80097A38`), which is a good check on it.
The per-mission and per-stage code modules start with an 8-handler pointer table into themselves (or into the
engine for stages that use the default).

### Memory layout (PP, code)
| From | To | What |
|---|---|---|
| `80011000` | `80017FC3` | boot exe: game code (border with the PsyQ library carried over from MoA's `80017800`, approximate) |
| `80017FC4` | `8003BC7B` | PsyQ library code and data |
| `8003BC7C` | | gp; small globals around it (MoA `8003EA28`) |
| `8003C000` | `800490CF` | exe bss: survives module loads (MoA `8003EA88`–`80050DD7`) |
| `800490D0` | `801FFFFF` | module area, rebuilt on each module load |

PP's persistent area is 0x7D08 bytes shorter than MoA's: the exe is smaller and its bss ends lower, which is why
every engine address moved.

## 4. The cross-disc tool (`tools/xdisc/`)

`xdisc.py` pairs routines with `mipscmp.py` (exe↔exe and the six module pairs), then lines up each paired routine
instruction by instruction (exactly when the code has the same shape, by matching blocks when it changed) and
collects what the paired instructions point at: absolute addresses (lui + addiu / load / store), gp-relative
globals, jal targets and struct field offsets. Each pair is a vote; a MoA address takes the PP address most of its
votes give. Improvements over the first comparison (`claude/phantasma-base.md`):
- **Data tables** cut out as routines are flagged `data` (share of words that are pointers, small numbers or
  impossible instructions).
- **Confidence** per routine pair (`high` / `medium` / `low`) and a call check: paired call sites inside a
  routine must call paired routines (`calls` column, ok/all).
- **Routines found by their callers** (`via_calls`): 45 MoA routines the code pairing missed get a PP twin
  because every call to them lines up.
- **Globals, runs and field shifts** as separate tables, with separate AC object and player block tables (a
  pointer counts as an AC object when it also touches a known AC field, e.g. MoA +0xD4 / PP +0xCC).

Counts (2026-11-26): 5,825 routine rows; routines `high` 4,199, `medium` 521, `low` 316, `data` 39.
Globals: 7,773 MoA addresses (2,114 high, 1,252 medium, 4,407 low: mostly a single vote). 81 runs of 10+ globals
that moved together.

Output is addresses and counts only, never instruction words (`docs/no-game-code.md`).

## 5. Struct field shifts (code)

**AC object** (MoA 376 bytes; `struct-ac-object.tsv`, 80 fields seen):
| MoA offsets | PP | Shift |
|---|---|---|
| +0x00 … +0x38 | same | 0 |
| +0x43 … +0x4A (state bytes: MoA +0x45 = PP +0x3C) | +0x3A … +0x41 | **−9** |
| +0x50 … +0x16C (velocity MoA +0xD4 = PP +0xCC, AP +0x168 = +0x160) | +0x48 … +0x164 | **−8** |
So PP's AC object is 8 bytes shorter: MoA has 9 more bytes between +0x38 and +0x43, and PP has one byte more
between +0x4A and +0x50 (likely padding). Two odd rows (+0x10 → +0x14, +0x104 → +0x100, 5 votes each) are most
likely another struct through a reused register.

**Player block** (`struct-player-block.tsv`, 62 fields): every field seen from +0x08 to +0x39C is **+8** on PP
(MoA +0x20C = PP +0x214, +0x298 = +0x2A0, camera pointer +0x39C = +0x3A4), except **+0x2D7 → +0x2DE (+7)**:
inside the compiled stat block (MoA +0x250 … +0x393) one byte goes missing somewhere between +0x2B8 and
+0x2D7 (Lane 5). Fields below +0x08 (pad bits +0x04 / +0x06) were not seen through a marked pointer.

**Other structs** (`fields.tsv`): a large −4 family (e.g. MoA +0x78 → +0x74, 718 votes; +0x16 → +0x12) belongs to
some other record (object pool? stage or camera records?): not named yet.

## 6. Open (for this lane or others)
- ~~Menu module 201: header format and entry point~~; ~~file numbers 0 and 3~~: round 2 topic 5 (§2, §3).
- `GG/Pn/PAnn.T` (72 archives): what each is and which routine reads it. Lane 3 / 7.
- MoA FDAT 200 and 204: what they are (no PP counterpart). Lane 3.
- The −4 struct family in `fields.tsv`. Lane 4.
- PP seeds cover the boot exe only; the overlays need their own seed lists when PP is built (build work, not
  mapping).
- Nothing here has been checked in a running game. A first live check that would confirm the whole table: in
  plain VS on PP, the AC object's velocity at PP +0xCC changes while boosting (Lane 4 will ask).

## 7. Rebuilding the tables
From a folder with both work packs extracted and `setup.py` run in each (`ma-disc/`, `pp-disc/`):
```
python3 -I tools/xdisc/xdisc.py --moa WORK/ma-disc --pp WORK/pp-disc --out docs/research/xdisc
python3 -I tools/xdisc/catalogue.py --pp WORK/pp-disc
python3 -I tools/xdisc/ram_xlate.py            # report; --write adds PP rows for new MoA rows
python3 -I tools/ram_map.py --exe WORK/ma-disc/SLUS_010.30 --modules WORK/ma-disc/unpacked/GG/COM/FDAT
python3 -I tools/ram_map.py --disc PP --exe WORK/pp-disc/SLUS_006.70 --modules WORK/pp-disc/unpacked/GG/COM/FDAT
```
The two `ram_map.py` runs refresh the cached reference lists (`ram-xref-*.json`); without `--exe/--modules` they
rebuild the docs from the cache.

## Handed over
(Other lanes: add one line here for anything that belongs to Lane 1.)


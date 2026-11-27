# AC1 missions on Phantasma: build side

Armored Core 1 missions running on Phantasma's missions engine. Plan and research: `research/ac1/import-outlook.md`
(§8 the pieces, §9 the owner's decisions). This page is what is built, how to use it, and what is left.

**Status (2026-11-26): pilot written, not run yet.** "Stop Terrorist Threat" (AC1 slot 1) converts cleanly. It
needs a Phantasma build (ladder 0.0.x, PR #11) to try in game: test 35 in `research/mapping/ROUND1.md`.

## In plain words

- A **converter** on the player's PC reads their own AC1 and Phantasma discs and writes a small pack of
  Phantasma-ready AC1 mission files into the game folder. Nothing from the discs goes on GitHub.
- The converter **refuses** to write a mission while anything about it is unsure, and lists why. For the pilot
  that list is empty. For the other 45 missions it isn't yet (see "Left to do").
- **Hooks in the game** load those files when an AC1 mission runs, fix the engine's own few message lines, give
  AC1 missions their own per-mission value, and handle the 48 → 49 chain (AC1 98 → 99) with a small table.
- **Engine numbers:** AC1 slot + 50 (slot 1 = mission 51). Phantasma's own are 0–19, Master of Arena's will be
  20–39.
- Until AC1 has its own mission list, a **test switch** makes every Phantasma mission you pick run an AC1
  mission instead.

## Making the pack (for testing)

On the PC with the game folder, from the source folder:

    python3 -I tools/ac1import/ac1import.py --ac1 "<AC1 .cue>" --pp "<Phantasma .cue>" \
        --out "<game folder>/RecompData/content" --test-slot 1

- `--ac1` / `--pp`: a `.cue`, `.bin` or `.iso` of your disc (or a folder laid out like the disc).
  AC1 must be the USA disc (SCUS-94182) or the 2000 rerelease (SLUS-01323).
- `--test-slot 1` turns the test switch on. Run again with `--test-slot 0` (or delete
  `content/ac1/test-slot.txt`) to turn it off.
- `--check` converts and reports without writing. `--slots 1,3` picks slots (default: the pilot).

The game looks for the pack in `RecompData/content/ac1/` (or `$ACG1PLUS_CONTENT_DIR/ac1/`). Its log lines start
with `[acg missions]`.

Later the launcher's first-run build should run this step itself when it finds an AC1 disc (build chat).

## What's in it

| Piece | Where | What it does |
|---|---|---|
| Converter | `tools/ac1import/ac1import.py` | makes `content/ac1/`: per mission `a2-e<5k>.bin` code, `a2-e<5k+4>.bin` map, `a1-e<k>.bin` pictures; `a2-e200.bin` common file; `values.bin`; `pack.txt` (sizes, sha256) |
| Mission code | `tools/ac1import/mission_code.py` | moves the code to Phantasma's address, points calls and data at Phantasma's engine, self-check |
| Map and common file | `tools/ac1import/mission_data.py` | pads map block 10 to 1,024 records, hidden-part numbers, AC1 messages and AC1 sound ids 56–255 |
| Disc reader | `tools/ac1import/disc.py` | reads `.cue`/`.bin`/`.iso` or a disc folder (ours) |
| File-load hook | `game/acg_mission_files.c` | on Phantasma's loader `0x800171B4`, only for the 4 loads in the missions start-up: RTIM *n*, FDAT 200, 5*n*, 5*n*+4. Keyed by game (Phantasma / MoA / AC1); MoA's numbers still load from the disc until its pack exists |
| Message hook | same file | the engine's own lines 151, 155, 171 (and the question 149) are taken from 50 lower while an AC1 mission runs |
| Per-mission value | same file | `0x8008108C` gets AC1's own value (56, or 24 for AC1 missions 8 and 11) instead of reading past Phantasma's 40-entry table |
| Chain table | `game/acg_mission_chain.c` | 98 → 99 now; MoA's 38 → 39 goes in the same table |
| Test switch | same file | `content/ac1/test-slot.txt` |

## Things the build found (and how the plan changed)

1. **Messages stay at AC1's own numbers, not 50 later.** The outlook (§4) said "AC1 line *n* = PP line *n* + 50,
   no code change". That holds for the engine's own lines only. AC1 mission code, its scripts and its timer
   record ask for lines by AC1's numbers (slot 1 asks for lines 1 and 104), so lines placed 50 later would show
   the wrong text. Instead AC1's lines keep their numbers and the hook moves only the engine's few fixed lines.
2. **The survey missed about 40% of the mission code.** `ac1_mission_refs.py` finds routines through the
   module's table and stack frames only; jump tables (switch statements) and small routines without a frame were
   not followed. Slot 1: 1,152 code words counted, 1,990 real. The converter follows jump tables and walks every
   gap in the code area, and refuses if any gap is left. Its counts replace the outlook's §3 numbers.
3. **One engine routine behaves differently.** AC1 `0x8004EBF8` ("animation at its end?") also answers for
   animations playing backwards; Phantasma's twin `0x8004D1EC` only checks forwards. The converter copies AC1's
   own little routine to the end of the mission file and points the calls there (`HELPERS` table).
4. **Low-confidence pairs checked by eye** for slot 1 (9 of them, listed with a reason in `CHECKED_LOW`). Only
   checked low pairs are accepted.
5. **One address means two things.** AC1 `0x801AC860` is paired with PP `0x801A15B8` in `globals.tsv`, which is
   right for a register base; read with an index it is a bit set at PP `0x801A8000` (`INDEXED` table, new
   ram-map row `mission_id_bits`).
6. **The chain needs no code patch.** Just before the results screen, a won mission listed in the table is shown
   to the game's own 48 → 49 check as 48; at the next start-up the 49 becomes the table's next. Two plain
   function hooks, no instruction hooks (those need a mod package).

## Not done yet

- **AC1 radio voices and music (TALK / BGM).** The game finds these by name once at boot and keeps their disc
  position (table at `0x80033FE0`, 0x24 bytes per file). Swapping in AC1's files means putting AC1's audio sectors
  on the virtual disc, which the framework only allows from a mod package with a verified file
  (`psx_mod_append_disc_extent`). That needs a package and launcher support (build chat). Until then AC1
  missions play Phantasma's voices and the Phantasma mission's music.
- The AC1 mission list (source picker, AC1 `MIS.T` 0, briefings), AC1 progress in its own save block,
  endings and movies (later, decision §9.3).
- The other 45 missions. `--check` over all of them (2026-11-26): 2 convert (slots 1 and 3). What stops the rest,
  most frequent first:
  - calls to Phantasma `0x80029760` / `0x8002AFC0` (230 sites) that fail the routine-start check: library
    routines without a frame, or wrong pairs;
  - AC1 `0x801A5DC0` / `0x801A5DC4` (190 sites), paired at low confidence with PP `0x801E64E8` / `EC`
    (the outlook's own note: consistent with the +0x40728 block);
  - indexed reads through the effect tables `0x8008FB58` / `FC48` / `FEE8` (115 sites): PP's tables have other
    record counts, so an index needs checking per table;
  - about a dozen low pairs (94 + 71 + 68 + 29 + … sites), e.g. `0x800889B4`, `0x80075BC0`, `0x800784F8`.
- Pre-compiling: converted mission code isn't in the first-run build's pre-compile list yet, so it runs on the
  framework's slower fallback (build chat, `aot/overlays.json`).
- The pack is checked by size only at start; `pack.txt` carries sha256 for a later check.

## File border (this build chat)

Mine: `tools/ac1import/`, `game/acg_mission_files.*`, `game/acg_mission_chain.c`, this page, project doc
`claude/ac1-import.md`. One block in `CMakeLists.txt` (build chat's file) adds the two game files.
Appended only: `research/ram-map.toml`, `research/mapping/ROUND1.md` test 35.
Imported, not changed: `tools/xdisc/ac1_mission_refs.py`.

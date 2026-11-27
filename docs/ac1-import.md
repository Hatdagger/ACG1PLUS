# AC1 missions on Phantasma: build side

Armored Core 1 missions running on Phantasma's missions engine. Plan and research: `research/ac1/import-outlook.md`
(§8 the pieces, §9 the owner's decisions). This page is what is built, how to use it, and what is left.

**Status (2026-11-26, evening): the pilot runs in the sandbox.** "Stop Terrorist Threat" (AC1 slot 1) converts,
loads from the pack and plays on Phantasma's missions engine (headless sandbox build, test disc). What that run
did and didn't check: "Sandbox run" below. Still waiting for a real PC: tests 37 and 38 in `research/mapping/ROUND1.md`.

## Sandbox run (2026-11-26)

Built with `scripts/linux/build.sh` in a throwaway copy (test disc from the packs), pack made with
`--test-slot 1`, game started headless with the debug server (`PSX_DEBUG_SERVER=1`), driven by injected pad
input and screenshots: New Game → hub → MISSION → first Phantasma mission, which the test switch turns into AC1
slot 1.

**Fixed:** the hooks never switched on at first. The return address of the pictures load was 4 bytes off
(`0x8004DB68` → `0x8004DB6C`; the jal is at `0x8004DB64`), so the "is the missions engine loaded?" check always
failed and every hook stood aside. Found by reading the engine in RAM; all other hook addresses and code words were
checked the same way and match.

**Checked (seen on screen / in the log):**
- the pack is found (`RecompData/content/ac1/`), the test switch fires, the mission code, map, pictures and common
  file come from the pack, engine number 51 runs;
- the AC1 map is drawn (the tunnel and the hangar with crates), the AC moves and fires;
- AC1 enemies are there: they lock on, shoot and take damage;
- the engine's fixed lines show AC1's text through the message hooks: "COM: SYSTEM ENGAGED" (151 → 101) and
  "Do you want to abort?" (149 → 99);
- aborting goes back to the hub cleanly, and starting the mission again loads it again;
- savestates work during the AC1 mission.

**Not checked (needs a person playing, or a later run):**
- winning: the mission end, results screen and money (blind play didn't get far enough);
- the 98 → 99 chain (only slots 1 and 3 convert, so it can't run yet);
- AC1 voices and music (not built yet; the test disc has no music anyway);
- speed: the sandbox can't fetch the background compiler (tcc), so the AC1 mission code ran in the slow
  interpreter here. On a player's PC the background compiler handles it.

**New open question: the "FRIENDLY UNIT" label.** AC1's MTs in this mission show Phantasma's "FRIENDLY UNIT"
label next to them, although they attack. Either AC1 has friendlies here too (unlikely) or the object type
records mean something different on Phantasma for that label. Added to the test list below; the label's code
isn't mapped yet (test 38).

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
  position (table at `0x80033FE0`, 0x24 bytes per file). Until plan A below is built, AC1 missions play
  Phantasma's voices and the Phantasma mission's music.
- The AC1 mission list (source picker, AC1 `MIS.T` 0, briefings), AC1 progress in its own save block,
  endings and movies (later, decision §9.3).
- The other missions: see "Plan for the rest" below.
- Pre-compiling: converted mission code isn't in the first-run build's pre-compile list yet, so it runs on the
  framework's slower fallback (build chat, `aot/overlays.json`).
- The pack is checked by size only at start; `pack.txt` carries sha256 for a later check.
- The loading picture: before a missions start the boot program shows RTIM *n* + 40 (*n* = the next mission
  number). Phantasma's RTIM has 63 pictures, so any number above 22 reads past the end: AC1's 51+ and Master of
  Arena's 20–39 (whose 60–62 would show the wrong pictures). The test switch and the chain don't hit this
  (the number changes after the picture), but a real AC1 / MoA mission list will need a small hook there.

## Plan for the rest (2026-11-26, not started; owner decides the order)

### A. AC1 radio voices and music

What the framework allows: a mod package may add sectors to the virtual disc
(`psx_mod_append_disc_extent`), but only from a **verified file**: one whose size and sha256 the package's
manifest names. The AC1 audio files (`TALK*.XA`, `CPU*.XA`, `BGM*.XA`) are form-2 sectors, 2,336 bytes each,
which is exactly what that call takes.

1. **Converter** (mine): copy AC1's audio files from the player's .bin/.cue into one file in the pack,
   `ac1/audio.xa` (2,336 bytes per sector), with a small index (file name, first sector, sector count). An .iso
   has no form-2 sectors, so audio needs a .bin/.cue (the converter says so and carries on without audio).
2. **Package** `mods/preloaded/packages/acg.ac1-audio/` (new, would be mine; the folder is empty and unowned
   today): a manifest naming `audio.xa` as a hidden, verified resource (size + sha256: numbers only, no game
   data; one pair per supported AC1 disc, US and the 2000 rerelease, if their audio differs), and a small plugin
   in `game/` that appends it at start and keeps the new sector numbers.
3. **Swap while an AC1 mission runs** (mine): at the missions start-up, point the TALK / BGM entries of the table
   at `0x80033FE0` to the appended sectors; put Phantasma's back when the engine returns. Off in netplay.
4. **Needs the launcher chat:** packages are normally switched on and prepared by the framework's own launcher;
   we start the game with `--no-launcher` from our launcher. Question for them: how our launcher enables a
   package and points it at the pack (the framework's "automatic preparation", format 9, or a fixed path).
   Until that is settled, steps 1–3 can be built and tested in the sandbox with the package switched on by hand.

Unknowns to check first: which AC1 audio files the missions really open (by name at boot vs per mission), and
whether the voice lines' order inside AC1's TALK files matches what AC1 mission code asks for (it should, since
the code is AC1's own).

### B. The other missions

`--check --slots 1,…,69` (2026-11-26; the 22 empty slots are now skipped instead of stopping the converter):
**47 tried (60, AC Battle, included), 2 convert (1 and 3), 45 don't.** What stops them (sites / missions):

| Blocker | Sites | Missions | Fix |
|---|---|---|---|
| calls to PP `0x80029760` "not a routine start" | 203 | 39 | find the real PP twin of this AC1 library routine (likely a pairing slip; compare the code) |
| AC1 `0x801A5DC0` / `C4` low-confidence global | 156 + 36 | 40 | confirm PP `0x801E64E8` / `EC` by reading the users on both sides (the +0x40728 block suggests it's right) |
| low-confidence call pairs: `0x800889B4`, `0x80075BC0`, `0x800784F8`, `0x80027480`, `0x8004D178`, `0x8008C2FC`, and 8 smaller ones | 94, 71, 68, 29, 14, 12, … | up to 35 | check each pair by eye (`CHECKED_LOW`, as for slot 1) |
| indexed reads through engine tables `0x8008FB58` / `FC48` / `FEE8` | 29 each | 29 | compare record counts per table; allow the index range the missions use |
| indexed reads through `0x80031A94` / `0x80031B34` | 14 each | 14 | same |
| PP `0x8002AFC0`, `0x80029520` "not a routine start" | 27 + 1 | 27 | as the first row |
| 4 more single globals and one "mid inside" address | ≤ 4 | ≤ 4 | by eye |

Order: the first two rows unblock most missions at once; then the call pairs (each one is a small, separate
check); then the table indexes. After each step, run `--check` again and try one newly converted mission in the
sandbox (headless, as above). Missions with the 48 → 49 chain (AC1 48/49) go last, since they need everything.

### C. Running the converter in the launcher's first-run build

1. **Launcher chat:** a new optional stage after "translate": if an AC1 disc (.cue/.bin, US or 2000 rerelease)
   is found (beside the Phantasma disc, in `Disc/`, or picked), run `tools/ac1import/ac1import.py` with the
   compiler pack's Python: `--ac1 <cue> --pp <cue> --out RecompData/content`. No AC1 disc: skip, no error.
2. **Mine:** a machine-readable result for the launcher (exit code + a short summary line: converted /
   not converted / audio yes-no), and keep the converter fast (it takes seconds per mission now).
3. **Build chat (later):** pre-compiling the converted code. The pre-compile list (`aot/overlays.json`) only
   reads from the disc today; converted code lives in the pack. Until there is a way to list pack files, the
   background compiler takes it while playing (on the player's PC; the sandbox can't fetch that compiler).

## File border (this build chat)

Mine: `tools/ac1import/`, `game/acg_mission_files.*`, `game/acg_mission_chain.c`, this page, project doc
`claude/ac1-import.md`. One block in `CMakeLists.txt` (build chat's file) adds the two game files.
Appended only: `research/ram-map.toml`, `research/mapping/ROUND1.md` tests 37–38 (35 was taken by round 3 at the same time).
Imported, not changed: `tools/xdisc/ac1_mission_refs.py`.

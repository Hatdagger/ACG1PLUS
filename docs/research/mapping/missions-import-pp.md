# Missions import plan: Master of Arena missions on Phantasma's missions engine

Mapping round 2, topic 4 (2026-11-26). **Static read** of the owner's discs: **PP** = Project Phantasma USA
(SLUS-00670), **MoA-US** = Master of Arena USA disc 1 (SLUS-01030). Nothing here was seen running. Every address
carries its disc. PP missions engine = **FDAT 202** loaded at `0x800490D0` (file offset = address − 0x800490D0);
MoA-US missions engine = **FDAT 207** at `0x80050DD8`. Mission code file *n* = FDAT 5*n*, map = FDAT 5*n* + 4 on
both discs.

Tools (ours, no game data; rerun them, don't commit their output):
- `tools/mapping/missions/moa_mission_refs.py`: every address the 20 MoA mission code files use, sorted into
  internal / engine call / data, with the PP twin from `docs/research/xdisc/`.
- `tools/mapping/missions/moa_mission_types.py`: prototype dataflow pass that decides which record a load/store
  goes through (object, map section, controller …), to count how many moved-field sites a tool can settle.
- `tools/mapping/missions/map_blocks.py`: block list and record counts of every mission map on both discs.

Background: `missions.md` (MoA-US missions in depth), `scenario.md` §4 (PP mission table and files),
`stages.md` §2 (the map chunk format), project doc `phantasma-missions-crashes.md` (why the old PP → MoA import broke).

## In plain words (for the owner)

- **It can be done, and the hard part is now measured instead of guessed.** Phantasma's missions engine is the
  same engine MoA used, with Phantasma's numbers. The memory layout around a mission is the same, just shifted:
  everything MoA keeps from `0x800A0DB0` up sits exactly `0x9398` lower on PP, and the room for a mission's code
  is the same 55,296 bytes on both. Every MoA mission fits.
- **PP's engine is the bigger one.** It keeps 256 enemy placements and 512 event points per map; MoA cut those
  to 64 and 128. Every MoA map fits inside PP's limits with room to spare.
- **The map files are almost the same format.** MoA puts one extra block of models at the front. In 18 of 20
  missions that block is empty; in 2 (Destroy Cannon, Recover Meteorite) it holds 3 small models that can move
  into our own copy of the shared data file. Textures load into the same places on screen memory.
- **The mission code is the real work.** Each MoA mission is 32–55 KB of MIPS code that calls MoA's engine by
  address and reads MoA's object records. Three kinds of fixes:
  1. **Calls into the engine** (about 4,100 call sites, 116 different engine routines): 92 of the 116 have a
     high-confidence Phantasma twin, 13 medium, 11 low; 5 exist only in MoA and need our own replacement
     (small: an AP-level helper, a screen-rectangle effect, the "Raven beaten in a mission" flag, two effects).
  2. **Data addresses** (about 2,700 sites): all but about a dozen addresses have a PP twin now.
  3. **Object fields** (about 5,750 places that read or write an object field that moved by 8–9 bytes): this
     is what broke the old import. A first automatic pass settles about 68%; the rest need more rules or a
     person. 42% of MoA's mission code also exists, in Phantasma's layout, inside Phantasma's own missions, so
     those routines can be swapped for Phantasma's copy instead of converted.
- **Recommended way**: convert the MoA mission code into Phantasma-ready mission files on the player's PC (from
  their own MoA disc, like the rest of the build), using tables and rules we write here, with a checker that
  refuses to produce a file while any call, address or field is undecided. A small set of engine hooks loads them
  from our own pack. Start with one easy mission (Eliminate Fugitive) to prove the chain end to end.
- **Decisions for you** are in §9 (mission list and chapter rules, what to do with MoA-only effects).

---

## 1. The two engines side by side (code)

Engine start-up: PP `0x8004DAD4` (FDAT 202 +0x4A04) = MoA-US `0x80055628` (FDAT 207 +0x4850), same order of work:

| Step | PP | MoA-US |
|---|---|---|
| Mission state record **M** | `0x80097A18` | `0x800A0DB0` |
| M+0 mission number from | player block 1 (`0x800424E8`) **+0x25D** | player block (`0x80045198`) +0x255 |
| Mission code loaded to (M+4) | **`0x80097A38`** (call `0x8004DD20`) | `0x800A0DD0` |
| Load buffer start (M+8) | `0x800BA5B0` | `0x800C2D30` |
| Shared fight textures | RTIM **60** | RTIM **61** (MoA inserted a 22 KB RTIM 60; MoA 62/63 = PP 61/62) |
| Mission textures | RTIM *n* | RTIM *n* |
| Shared data file ("common") | FDAT **200** | FDAT **205** |
| Map | FDAT 5*n* + 4 (`0x8004DD40`) | FDAT 5*n* + 4 |
| File loader (exe) | `0x800171B4(file, entry, dest)` | `0x80016A58` |
| Byte that picks the message set (common blocks 4–6) and the map's sound bank (0, 1, 2); likely the COM voice option `guess` | player block +0x282 (`0x8004276A`) | +0x27A |

**The region around a mission is MoA's shifted by −0x9398** (checked on M, the code base, the message table copy,
the first engine data word after the mission area, and in the paired engine code):

| What | PP | MoA-US |
|---|---|---|
| M (state record) | `0x80097A18` | `0x800A0DB0` |
| mission code area | `0x80097A38` … `0x800A5237` | `0x800A0DD0` … `0x800AE5CF` |
| first engine data after it | `0x800A5238` | `0x800AE5D0` |
| message table copy | `0x800A52F0` | `0x800AE688` |

So the **room for a mission's code is 55,296 bytes on both** (0xD800). MoA's largest (Assist Marine Lab, slot 6)
is exactly 55,296; all 20 fit. (The ram-map row `mission_code` says 57,344: that is the FDAT entry size rounded
up, not the room.)

### 1a. Record arrays the mission code uses (code + data)

From the loaders and `docs/research/xdisc/globals.tsv` (high confidence unless marked):

| Record | PP base | MoA-US base | Size each | PP count | MoA count | Loader PP / MoA |
|---|---|---|---|---|---|---|
| map sections | `0x801BC328` | `0x801CFAB0` | 64 (52 on disc) | 512 | 512 | `0x8006FBA8` / `0x80077A00` |
| section data heap | `0x801C4328` | `0x801D7AB0` | — | 96 KB | 64 KB | same |
| point classes (map block 8) | `0x801D8F50` (medium) | `0x801E46D8` | 24 | 32 | 32 | `0x800710F8` / `0x80078F58` |
| **event points** (block 9; also trigger zones) | `0x801D9250` | `0x801E49D8` | 52 (32 on disc) | **512** | **128** | `0x80070F88` / `0x80078DE8` |
| touch list / count | `0x801DFA50` / `0x801DFC50` | `0x801E63D8` / `0x801E65D8` | | | | |
| object types (block 11) | `0x801DFC60` | `0x801E65E8` | 44 | 32 | 8 | `0x8007174C` / `0x800795A8` |
| **placements** (block 12) | `0x801E01E0` | `0x801E6748` | 44 (40 on disc) | **256** | **64** | `0x80071918` / `0x80079774` |
| **object pool** (slot 0 = player AC) | `0x801E2DE0` | `0x801E7248` | **368** | 16 | 16 | |
| active-section bit set | `0x801E6500` | `0x801EA9E8` | 512 bits | | | |
| timer message / end code; tallies +0x24 / +0x28 | `0x801F4588` | `0x801F8E70` | | | | |
| map block 4 pointer | `0x801F45A8` | `0x801F8E90` | | | | |
| map block 6 copy (area finder) | `0x801AD328` | `0x801BFAB0` | | | | |
| model table (slot *k* at +0x11C + 4*k*) | `0x801A15B8` | `0x801B3D38` | | | | `0x80051D50` / `0x8005999C` |

Read from the loops: PP's placement loader runs 0x100 records, MoA's 0x40; PP's point loader 0x200, MoA's 0x80.
Records keep their layout (the loaders are the same code with different bases and counts; the section loader
differs only in bases and heap size). **Only the object record changed shape** (next section).

**Corrections for existing PP rows in `ram-map.toml`** (rows were carried over from MoA by tool; not edited here
because that file is append-only for the mapping chats): `mission_placements` is 256 × 44 = 11,264 bytes, not
64 × 44; `mission_trigger_zones` = the event points, 512 × 52 = 26,624 bytes; `mission_code` room is 55,296.
*Fixed in `ram-map.toml` 2026-11-26 (round 2 review, copied rows may now be corrected).*

### 1b. The object record: MoA 376 bytes, PP 368 (code)

`docs/research/xdisc/struct-ac-object.tsv` has the full list. In short: offsets up to +0x39 are the same; MoA has
**9 bytes PP doesn't** at +0x3A … +0x42 (smoke timer +0x3A, burn timer +0x3C, burn damage +0x3E, used by MoA's
per-object update `0x8007E6A8`, which has no PP twin); so MoA +0x43 … +0x4A = PP +0x3A … +0x41 (−9); from +0x50 on
**−8** (MoA +0x54 movement, +0x58 set state, +0x5C message, +0x60 per-frame, +0x64 hit callbacks = PP +0x4C …
+0x5C; AP MoA +0x168 = PP +0x160). A few byte fields move by −4 (+0x81, +0x104, +0x107) and the placement copy
lands record bytes at object +0x14 on PP (+0x10 on MoA).

### 1c. Engine behaviours tied to the mission number (code)

The mission number (M+0) is read in five places in PP FDAT 202:

| Where | What | MoA-US |
|---|---|---|
| `0x8004DB60`, `0x8004DD14`, `0x8004DD2C` | file loads: RTIM *n*, FDAT 5*n*, 5*n* + 4 | same |
| `0x800810A4` | u16 from a **per-mission table `0x8008F874`** (40 entries, values 24 or 56) → `0x8008DE76`. Meaning not read yet `?` | table `0x80098C9C` (41 entries); stock values differ per mission |
| `0x8004A914` | AC destroyed: message **171**, or **195** "Test mode disengaged" when the number is **19** | 155 / 156 when the number is **40** |
| `0x80088E30` (results screen, RTIM 62) | skipped when the number is **19**; for **10** (The Final Battle) only shown when M+0xC bit 0x100 is set | `0x800918E8`: no such skips |
| main loop end `0x8004AA48` | after a success, number **48** becomes **49** and the engine returns 1 (start the next mission at once). No PP mission is 48, so this is dormant on PP | **18 → 19** (Enter Factory → Final Mission) |

Other fixed ends: out of area = message **155** on PP (MoA 157), end code 0x101; the timer end uses `0x801F4588`.
The results screen calls the mission module's **hook +0x0C** with two pointers and adds the two tallies
(`0x801F4588` +0x24 / +0x28) to what it returns; MoA does the same (`0x800918E8`). So module hook +0x0C =
**extra results lines** (it was "not found yet" in `missions.md`).

## 2. Map files (data; consumer code)

`map_blocks.py` output, both discs:

- **PP maps: 19 blocks.** 3 model lists (slots 0x104, 0x80, 0xA0), backdrop model, block 4, block 5 (to M+8),
  area finder, sections, point classes, points (16,384 B), view table (14,336 B), object types, placements
  (10,240 B), then **3 sound banks** (VAB header + body each); only the one for the COM voice byte is loaded.
- **MoA maps: 20 blocks** = the same 19 with **one model list in front, for model slots 0x48 …** (19 entries).
  PP's engine has no call for it (MoA `0x800558A0`, `0x8005999C(0x48, …)`).
  - 18 maps: that list is empty (80 bytes, every entry size 4). **Drop it.**
  - **Destroy Cannon (3)**: entries 0, 1, 2 (348, 2,764, 348 B). **Recover Meteorite (4)**: entries 0, 1, 3
    (4,884, 2,564, 636 B).
  - PP's common file fills slots 0x28 … 0x5A (51 entries) but **only 8 … 22 are non-empty**, so slots 0x48 … 0x5A
    (common entries 32 … 50) are free on PP. The models of missions 3 and 4 can go into **our own common file**
    at entries 32+, landing in the same slots with no engine change.
- **Counts:** MoA maps use at most 52 placements (index ≤ 52) and 88 points (index ≤ 88); everything beyond is
  type −1 in the files. PP reads 256 / 512 of them, so the unused MoA records stay unused. (PP's own maps use up
  to 93 placements, Destroy Main Facility, and point 265, Disrupt Shipping Lanes.)
- **Sound banks:** both discs have 3 COM-voice banks in the same place; nothing to convert.
- **Hidden parts:** item events (block 9, kinds 5/6/7) carry an **owned-parts index**. HD-HELM (MoA 12 → PP 12)
  and P/CV (MoA 118 → PP 118) keep their numbers; **WX-ED2 (MoA 212)** gets whatever PP back slot it is given
  (`content-data.md` §5). That is a data patch of the map's block 9. The pickup also checks owned/shop bytes,
  MoA `0x80036FEC` / `0x800370EC` = PP `0x800342F0` / `0x800343F0` (shop: guess, `content-data.md` §4b).

## 3. Textures (data)

MoA mission textures (RTIM *n*) load after the shared set (MoA RTIM 61, PP RTIM 60). Cut into 16 × 16 VRAM cells:
PP 60 and MoA 61 cover nearly the same area (PP 60 has 32 cells MoA 61 doesn't, MoA 61 has 7 PP 60 doesn't), and
**each MoA mission set overlaps PP 60 exactly where it overlaps MoA 61** (16–23 cells, by design), except
missions 9 (+2 cells) and 16 (+4). So MoA RTIM *n* can load as it is. Small risks to look at in game: MoA models
drawing from the 7 MoA-only shared cells, and missions 9 / 16 covering a few PP-only cells.

## 4. Messages (data + code)

Mission code shows message *n* through `0x80050ED0` (PP) = `0x80058B1C` (MoA). The text table comes from the
common file, blocks 4 / 5 / 6 (one per COM voice), 250 entries on both.

- **The numbers mean different things.** PP's engine uses 151 "Combat system engaged" (same text on MoA),
  **155 out of area**, **171 defenses damaged**, **195 test mode disengaged**; MoA uses 157 / 155 / 156 for those.
  MoA missions use 3–76, 92–123, 147–166 for their own lines.
- So **an MoA mission needs MoA's table**, with PP's fixed slots filled in: 155 ← MoA 157, 171 ← MoA 155,
  195 ← MoA 156 (MoA 171 and 195 are empty). That table lives in our own common file for MoA missions.
- **Text codes differ.** MoA brackets speaker names with `F0 81 … F0 82` (294 times in its common file, never on
  PP); PP uses `9A … 9B`. Header codes differ too (MoA `M4N0X0 C02`, PP `M3X2N0 C02`). The old import converted
  PP → MoA; the reverse needs PP's message parser read first (open, §10). Letters use the same code (A = 0xAA).

## 5. The mission screen and pages (data + code)

- PP's mission table T (`scenario.md` §4a) has **64 slots, 17 used**; MoA's 20 missions fit in slots 17–36. A row
  is: chapter window, name, advance pay, reward, **engine number**, music, page record (8 pictures + 8 text pages,
  each a `MIS.T` entry − 1). T+0xC4 … is rebuilt from `MIS.T` entry 0 every time (callback `0x80085AAC`), so rows
  are added either in our own entry 0 or right after that copy.
- **Engine numbers 20–39 for MoA missions 0–19** fit best: PP never runs the missions engine with those numbers
  (they are stage numbers for the other engines), they avoid PP's specials 10 and 19, and the per-mission table
  `0x8008F874` has exactly entries 20–39 to set to MoA's values (MoA missions 3, 4 and 15 use 24, the rest 56).
  The loads for those numbers (RTIM, FDAT 5*n*, 5*n* + 4) must then come from our pack (§6).
- **Enter Factory → Final Mission**: give them 38 and 39 and change PP's dormant 48 → 49 check (two immediates,
  `0x8004AA4C` / `0x8004AA58`) to 38 → 39.
- **Briefing pages:** MoA pages are the same kind of `MIS.T` entries (2 KB text ending in `>`, 12 KB TIM picture);
  PP texts also use colour codes (`%0`, `@1`). Page records take any entry number, so MoA pages can be numbered
  after PP's last entry (833+) and loaded from our pack.
- **Chapters:** PP's chapter is a plain win count (`scenario.md` §4c); MoA's flow has jumps, arena-rank gates,
  credit and part gifts (`missions.md`, "Mission results and chapters"). MoA windows can't be used as they are.

## 6. What has to change, piece by piece

*Added in the round 2 review:* **surface flag 2** (`stages.md` §5a). Seven MoA maps have collision polygons
with flag 2, which MoA's AC stops on and PP's AC passes through (mask 5 vs 7): Defend Submarine, Destroy Berserk
MTs, Retrieve Secret Files, Enter Abandoned Mine, **Destroy Giant Tank (104 polygons, the second pilot)**, Enter
Factory, Final Mission. The map converter should clear flag 2 in those polygons (data only).

| Piece | Work | Kind |
|---|---|---|
| MoA mission code (FDAT 5*n*) | relocate, retarget engine calls and data, convert object fields (§7) → a PP-ready mission file | converter, run on the player's PC |
| MoA map (FDAT 5*n* + 4) | drop the front model list; fix WX-ED2's index in block 9 | data |
| MoA textures (RTIM *n*) | none | copy |
| Common file for MoA missions | PP FDAT 200 + MoA message blocks (converted, PP fixed slots filled) + models of missions 3 / 4 at entries 32+ | data |
| Briefing pages | MoA `MIS.T` pages as entries 833+ | data |
| Mission rows | 20 rows in T slots 17–36, engine numbers 20–39 | data (or menu hook) |
| **File-load redirect** | one hook on `0x800171B4`: missions engine + number 20–39 → our pack for RTIM *n*, FDAT 5*n*, 5*n* + 4 and FDAT 200; `MIS.T` 833+ → our pack | engine hook (recomp runtime) |
| Per-mission table `0x8008F874` [20…39] | MoA values | data patch of FDAT 202 |
| Mission chain | 48 → 49 becomes 38 → 39 | 2 immediates in FDAT 202 |
| MoA-only engine routines | our own versions, linked where the converter points the calls (§7b) | new code |
| MoA-only object fields +0x3A … +0x42 | find room or drop the effect (§9) | decision |
| Chapter flow / mission list | §9 | decision + menu work |

Nothing here puts game code on GitHub: the converter, its tables and the hooks are ours; the pack is built from
the player's own MoA disc at install time, like the rest of the build (`docs/no-game-code.md`).

## 7. Converting the code (code)

Counts from `moa_mission_refs.py` over the 20 MoA mission files (code found by walking from each module's table;
about 37,000 words of routines nothing points at — library leftovers — are counted apart and can stay as they are).

### 7a. Inside the module (mechanical)

Every reference into the module moves by **−0x9398**: 1,216 internal calls, 1,104 address pairs (`lui` + `addiu`;
the pair has to be patched together because the low half can carry into the high half), 771 loads/stores through
such pairs, and **7,213 pointer words in data** (callback tables, jump tables, route lists). Plus every `j`.

### 7b. Engine calls (tables + a check)

About 4,100 call sites to **116 different engine routines**: **92 high, 13 medium, 11 low** confidence twins in
`functions.tsv`. Low ones to read by hand (MoA-US → PP): `0x8009105C` → `0x80088760`, `0x80093844` →
`0x8008A558` (script runner), `0x8008D914` → `0x800852EC`, `0x8008FA6C` → `0x80086C50`, `0x800730A4` →
`0x8006B334`, `0x80053320` → `0x8004B80C` (play sound; MoA adds a once-per-frame check), `0x80090734` →
`0x80087E88`, `0x80090294` → `0x80087BDC`, `0x8007358C` → `0x8006B6A0`, `0x800731F4` → `0x8006B420`.

Also:
- **BIOS calls** (A0 table stubs in the exe): MoA `0x8002C0F4` (A0:2F rand, 269 sites) = **PP `0x8002CC30`**;
  MoA `0x8002C0E4` (A0:2B) = **PP `0x8002CC20`**. (The old notes called PP `0x8002CC20` a weak match for
  `0x8002C0F4`; it is the A0:2B stub, the twin of MoA `0x8002C0E4`.)
- **Callbacks handed to the engine**: MoA `0x8007C088` (shot callback, 20 sites) = PP `0x800741D0`,
  `0x8007BBFC` = PP `0x80073D44`, `0x8007BC5C` = PP `0x80073DA4`.
- **MoA-only engine routines** (no PP twin), with call sites: `0x8007B354` (54; AP as a 0–5 damage level:
  AP × 256 ÷ max AP +0x16C), `0x800907D0` (12; "Raven beaten in a mission" → MoA arena rank table), `0x8005CCF0`
  (11; allocates and fills a screen rectangle record, then `setRECT`), `0x8008E794` (1), `0x8009043C` (1; spark
  effect with MoA's extra type 4). Each needs our own small routine.
- An engine **data table** the code takes the address of: MoA `0x80096740` (28-byte effect records) = **PP
  `0x8008D42C`** (same bytes).
- The check that caught the old bug: **every call or code pointer must land on a routine start and pair with
  the routine it replaces** (the old import sent 44 calls into the middle of `0x8007D50C`).

### 7c. Data addresses (tables + struct rules)

About 2,700 sites. Most pair through `globals.tsv` (233 of them only at low/medium confidence: check), the
engine-area shift (−0x9398), or the record arrays of §1a: an address inside an array is translated as *base +
index × size + field*, with the object field map for the pool (this is what the old import missed: it moved bases
but not "type table + offset" reaching into the pool). Left without a pair: **MoA `0x8009D8C0` … `0x8009D8CF`**
(mission 6), **`0x800A000A`, `0x800A0020`** (1, 3, 6), **`0x801F1CE0`, `0x801F1D08`** (9 missions; may be a
tool artefact), and player object **+0x42** (`0x801E728A`, MoA-only field). Read these by hand.

### 7d. Object fields (the hard part)

**5,749 loads/stores in live code use an offset that is a moved object field** (and 3,251 more in unreferenced
code). The same offset means a different PP offset when the register holds an object and the same offset when it
holds a section, point or placement, so each site needs a decision. `moa_mission_types.py` (prototype) types
registers from: the module table (type handlers and hook +0x10 get an object, section handlers a section, point
handlers a point), callbacks stored into an object's +0x54 … +0x64 or handed to the engine, engine routines that
return objects (find slot `0x8007EA24`, spawn `0x8007ECEC`, find by record `0x8007EACC`) or controllers
(`0x80077D28`, `0x8007927C`), pool constants, and arguments passed between the module's own routines. Result:

| Decided as | Sites |
|---|---|
| object (field moves) | 3,930 (68%) |
| section / controller / absolute address | 82 (1%) |
| **undecided** | **1,749 (30%)** |

Two things the next rules must cover (seen while checking undecided sites): object pointers read out of another
object's fields (targets), and **interior pointers**: the code often passes *object + 0x6C* to engine routines
(a sub-record), and offsets relative to that pointer don't move.

**Reuse instead of converting:** 42% of MoA's live mission code has a same-shape twin inside Phantasma's own 17
mission files (7% byte-identical apart from addresses). Those routines (door, area box, pickup and message
helpers) can be replaced by Phantasma's own copy, already in PP's layout.

**The old import's lesson, as rules for the converter:** no guessing per register without a recorded reason; any
site the tool cannot decide stops the build of that mission and is listed; a debug build traps writes below
`0x80010000` from mission code (both old crashes started with an object pointer like 0xF9 writing into BIOS RAM).

## 8. Approaches considered

| Approach | Verdict |
|---|---|
| **A. Convert MoA mission code to PP's engine** (§7), plus **B. swap in PP's own copies** where they exist | **Recommended.** Keeps Phantasma's engine and feel (import rule, `feel-diff.md`). The work is tables and a checker, done once |
| C. Rewrite each mission's logic in our own code from `missions.md` | Only for the per-mission hooks if conversion stalls; enemy and object behaviour is too much code to redo |
| D. Run MoA's own missions engine (FDAT 207) inside PP | No: brings MoA's movement and hit rules into missions, and FDAT 207 expects MoA's exe globals (moved by 0x2DA4 and more) and MoA's player block |

Pilot order: **Eliminate Fugitive (0)** (70 moved-field sites, 12 undecided, no MoA-only routines, empty front
model list) → **Destroy Giant Tank (15)** (118 / 9, none MoA-only) → the rest by undecided count. The two old
lists (missions 3 and 4 need the common-file models; 7 and 13 have route followers calling hook +0x08 directly).

## 9. Decisions for the owner

1. **Mission list:** MoA missions in their own list (the earlier "mission source picker" idea: Phantasma / Master
   of Arena rows), or mixed into Phantasma's list by chapter? The engine side is the same either way; it decides
   the menu work and the chapter rules.
2. **Chapter flow for MoA missions:** MoA's own flow (wins + arena-rank gates + gifts, needs our own counter in
   the save's free space) or a simple Phantasma-style win count?
3. **MoA-only object effects** (smoke after damage, burning damage over time): rebuild them on PP (needs spare
   object space and a per-frame step), or drop them for MoA missions?
4. **"Raven beaten in a mission" counts in the arena** (MoA): keep it (needs mapping MoA ranks to PP's arena)
   or leave it out?

## 10. Open

- PP message parser: which codes PP understands (needed to convert MoA's `F0 81` / header codes).
- Meaning of the per-mission u16 table (`0x8008F874` PP / `0x80098C9C` MoA) and its reader `0x8008DE76` users.
- The 11 low-confidence engine twins (§7b), the 233 low/medium data pairs, the unpaired addresses (§7c), and what
  MoA `0x8008E794` does.
- More typing rules (§7d) until the undecided count is near zero; then a per-site report for what is left.
- Sound effect ids: MoA mission code plays sounds by id; whether MoA's and PP's id lists agree belongs to
  topic 5 (sound).
- Mail for MoA missions (mail kinds 1 / 2 = after a slot is completed / played, `missions.md`): later, with
  the mail work.

## How we know

- Engine layout: PP `0x8004DAD4` and MoA `0x80055628` read side by side (every load call, buffer and block
  consumer); loaders `0x80071918` / `0x80079774` and `0x80070F88` / `0x80078DE8` (counts), `0x8006FBA8` /
  `0x80077A00` (diffed: bases and heap size only); end checks `0x8004A8A0` … and `0x80088E30`, chain
  `0x8004AA3C`.
- Data: `map_blocks.py` over all 37 maps; model lists parsed from FDAT 200 / 205 and the maps; RTIM TIM rectangles;
  message tables (block 4 of each common file); `MIS.T` pages 1–3 on both discs.
- Code counts: `moa_mission_refs.py` and `moa_mission_types.py`; twin share with `tools/xdisc/mipscmp.py` keys.

## RAM rows added (PP)

`mission_state` `0x80097A18`, `mission_engine_data` `0x800A5238`, `mission_messages` `0x800A52F0`,
`mission_load_buffer` `0x800BA5B0`, `mission_model_table` `0x801A15B8`, `mission_section_heap` `0x801C4328`,
`mission_point_classes` `0x801D8F50`, `mission_touch_list` `0x801DFA50`, `mission_object_types` `0x801DFC60`,
`mission_objects` `0x801E2DE0`, `mission_active_sections` `0x801E6500`, `mission_timer_end` `0x801F4588`,
`mission_number_table` `0x8008F874`, `mission_effect_table` `0x8008D42C`, `com_voice` `0x8004276A`.

## Handed over to the build chat

- No new routine starts (every routine named here is already in `functions.tsv`).
- Hooks the import will need, when it comes: file loader `0x800171B4` (PP exe; redirect by file + entry while the
  missions engine runs), `MIS.T` entry 0 copy callback `0x80085AAC` (PP FDAT 201) if rows are added in RAM.

# Stages (FDAT 0–199) — Project Phantasma

Mapping round 2, topic 2 (2026-11-26). Disc **PP** (Project Phantasma USA) unless a line says **MoA-US**. Static read
of the fight engines' stage loaders and the stage files. Tool: `tools/mapping/stages/stage_survey.py` (prints sizes,
counts and record fields; rerun, don't commit its output).

## In plain words

- **A stage is two kinds of file in FDAT.T**: a small **code file per fight engine** (4–6 KB: the stage's own
  behaviour, such as area boxes and moving parts) and one **data file** (110–520 KB on fight stages) that holds
  everything else: the stage's 3D models, its area map, its object list, its sound bank and a few tables, packed as
  13 chunks one after another.
- **Stage numbers 20–29** are the VS stages (all engines have code for them). **30–39** have code only for the
  arena and the link-cable engine. The arena uses 12 of them (20, 23, 24, 25, 28, 29, 30, 32, 33, 34, 35, 36).
  **0–19** are the mission maps (missions engine). Stage 19 is also the garage test fight.
- **The level geometry is the same kind of model as the AC parts** (TMD-style), split into up to 128 pieces, one
  per area. The stage is cut into **areas** (boxes that know their neighbours), which drive what is drawn and
  what collides.
- **The "stage objects" the arena AI looks for are invisible marker boxes** (class 248) placed by the level
  designers: on stages **23 and 29** pairs of markers at ledges (param 10 / 11) switch on AI behaviour 12 ("go
  there and boost up"); on stage **32** one big marker (param 20) feeds behaviour 2. All other arena stages have
  none, so those behaviours stay off there. **Area boxes** (class 249, the player-block area bits) are objects too.
- **MoA keeps the same format** with one extra chunk at the front. 9 of PP's 20 fight stages are on MoA under
  other numbers (3 byte-identical, 6 with small edits); MoA has 11 fight stages PP doesn't, and PP has 11 MoA
  dropped. Importing a MoA stage means: its data file (drop the extra chunk), its textures (RTIM), and **new PP
  builds of its code files** (MoA's call MoA's engine addresses).

## 1. Files and numbering (code)

FDAT entry **5 × stage + k**. Which k each engine loads, read from the loaders (same order in every engine:
RTIM 60, RTIM stage, FDAT 200, then the stage code, then the stage data):

| k | Engine | PP loader call | Loaded to |
|---|---|---|---|
| 0 | **missions** (stages 0–19) and **link** (stages 20–39) | missions `0x8004DD20`, link `0x8004E658` | missions 0x80097A38, link `0x800904B0` |
| 1 | VS | `0x8004E0B4` | `0x8008F5D0` |
| 2 | arena | `0x8004E8B0` | `0x800956F0` |
| 3 | VS replay | `0x8004DFDC` | `0x8008F828` |
| 4 | stage data (every engine) | VS `0x8004E0D4` | the engine's load buffer (VS from `0x800A2C88`) |

So the stage files are: 0–19 mission maps (k 0 + 4; slots 17 and 18 empty), 20–29 all of k 0–4, 30–39 k 0, 2, 4.
The stage number comes from player block +0x25D (VS: P1's choice on even rounds, P2's on odd; arena:
table `0x800BD6D4`; test fight: 19). **Corrections:** k = 0 is shared by missions and link (not "VS 1, link 1"
as `boot-loads.md` PP-10 says), and FDAT 100 is stage 20's **link** code, so the box handler `battle-system.md`
places at `0x80090070` "in FDAT 100" is really FDAT 101 +0xAA0 (VS); FDAT 100 has the same routine for the link engine.

**Stage code file** (data at its start, read by the loaders): words +0x00 … +0x1C = 8 handlers (on every fight
stage all 8 point to one `jr ra`; the loader calls +0x14 as the stage's init after the data is in, VS `0x8004E228`),
then from **+0xA0 one init routine per object type** (0 = none). The link files also use word +0x60.

**Never-loaded arena files (round 3 topic 1, `performance.md` §7):** the arena code files (k = 2) of the 7 stages the
arena doesn't use (21, 22, 26, 27, 31, 37, 38) were built for another address (about `0x801C9DD8`), not the arena's
`0x800956F0`, so they would crash if loaded. Adding one of these stages to the arena needs a fresh arena file. (code)

## 2. The stage data file: 13 chunks (data; consumers code)

Each chunk is `u32 size` + data. VS reads them in this order at `0x8004E0D4` … `0x8004E214` (the arena does the same
at `0x8004E8D0` …):

| # | Size (fight stages) | Consumer (VS) | What | Lands |
|---|---|---|---|---|
| 0 | 9–226 KB | `0x80050C50(0x104, …)` | **Area models**: u16 count (128), then models (u32 size, model; size < 5 = empty). TMD-style, the same model format as PA part models (`pa-archives.md` §3), fixed up by `0x800548C0`. 6–57 real pieces per stage | model table `0x80199F24`[0x104 + n] |
| 1 | 8 | `0x80050C50(0x80, …)` | model list for slot 0x80: empty on fight stages (mission maps put models here) | |
| 2 | 0–6 KB | `0x80050C50(0xA0, …)` | **Object models**, one per object type | model table [0xA0 + type] |
| 3 | 3–45 KB | `0x80054D78(0, …)` | one model without a list header (sky / backdrop `guess`) | `0x80199F1C` |
| 4 | 0–176 B | stored as a pointer | unknown; no VS code reads the pointer except the store (stage code `guess`) | `0x801E84A0` |
| 5 | 2.6–81 KB | `0x8004AF28` | **Sound bank** (VAB, "pBAV"); 10 different banks over the 20 fight stages; 2,608 bytes = the smallest, shared by 6 | sound memory |
| 6 | 10–53 KB | copied, then `0x8006A700` | **Area finder**: 1,024 pairs of offsets (8 KB) + bit-packed data, read by the bit reader `0x8006C784` to find which area a point is in (used by the collision sweep `0x8006D1F8` and three other VS routines) | `0x801A56F0` |
| 7 | 28–87 KB | `0x8006EA18` | **Areas**: 512 records of 52 bytes, then extra data (neighbour lists, collision `guess`; 2–60 KB). At load each record becomes 64 bytes (the words at +0x28 / +0x2C move to +0x30 / +0x34, the draw-distance limit is squared) | records `0x801B46F0`, extra `0x801BC6F0` |
| 8 | 28–148 B | `0x8006FF68` | **Object types**: u32 count, then 24 bytes each (12 halfwords) | `0x801D1318` |
| 9 | 16,384 | `0x8006FDF8` | **Objects**: 512 × 32 bytes (§3) | `0x801D1618`, 52 bytes each |
| 10 | 14,336 | `0x80053E0C` | a fixed-size table (pairs of offsets / counts), the same bytes on 11 of 20 stages; MoA name: "record read by the view-cone test" `?` | |
| 11 | 4 | skipped | | |
| 12 | 10,240 | `0x8007046C` | per-stage records for the fight heap (`vs_loaded_files` records 2 / 3), mostly zeros `?` | `0x801D8088` … |

## 3. Stage objects (data + code)

**On the disc** (chunk 9, 32 bytes): +0 / +2 / +4 centre (s16), +6 **type index** (−1 = unused slot), +8 … +0x0F
copied as is, +0x10 … +0x15 **box** as signed bytes × 256 around the centre (min x, y, z, max x, y, z), +0x1A
**parameter byte**, rest copied. **In RAM** (52 bytes): disc +0 … +0x0F at +0, disc +0x10 … +0x1F at +0x22 …
(so the box is +0x22 … +0x27 and the parameter **+0x2C**); +0x1C / +0x1E / +0x20 = type halfwords 0 / 1 / 2;
+0x14 = behaviour block from the type's init. Loader `0x8006FDF8` (arena `0x800719C8`) then calls the stage code's
init for that type (table +0xA0 + 4 × type). Model = model table [0xA0 + type].

**Type record** (chunk 8, 24 bytes): +0 draw flags (bit 15 = not drawn; `renderer.md` "Stage objects"), +2 / +4,
**+6 class**. Classes seen on the 20 fight stages (`stage_survey.py objects`):

| Class | How many | What (how we know) |
|---|---|---|
| **249** | 3 per stage on most stages | **Area box**: its init (e.g. FDAT 101 `0x80090160`) installs the box handler (FDAT 101 `0x80090070`) that sets player block +0x10 bit n while the AC is inside (`battle-system.md` "Stage area boxes") `code` |
| **248** | stages 23 (4), 29 (4), 32 (1) | **AI marker**, invisible (flags 0xC044), no init: read only by the arena AI (`0x80082DA8`, `0x800820F0`) `code` |
| 250, 241, 247, 242, 246, 240, 245 | one or a few per stage | unknown; 250 is on every stage (object 0) |
| 230, 40, 34 | many | scenery pieces `guess` (stage 20 has 90 of class 40; 21, 24, 34 only class 230) |

**AI markers by stage** (class 248; parameter = the byte the AI calls "kind"):

| Stage | In the arena? | Markers | AI effect |
|---|---|---|---|
| 23 | yes | two pairs: param 10 at y −3,000 with a small box, param 11 just below with a tall box, on both sides of the map | behaviour 12 on ("go to the nearest param-10 marker, boost up") |
| 29 | yes | one 10 / 11 pair, two param 0 | behaviour 12 on |
| 32 | yes | one param 20 with a box ~23,000 wide around the centre | behaviour 2 on |
| 28 | yes | none (behaviour 3 is hard-wired to stage 28 in code) | |
| 20, 24, 25, 30, 33, 34, 35, 36 | yes | none | behaviours 2 and 12 off |

So `ai.md`'s "kinds 0x0A / 0x14" are the parameter byte of class-248 markers: 10 = the spot to climb from, 11 =
its partner below `guess`, 20 = a big zone on stage 32 `guess`.

## 4. Areas and drawing (data; the MoA reading carried over)

The 64-byte RAM area record (box min +0 / +2 / +4, max +8 / +0xA / +0xC, centre +0x10, model +0x1E, neighbour count
+0x26 / list +0x28, flags +0x30, limits +0x34) and how the stage is drawn from the eye's area through its neighbours,
with a near and a far level, are in `renderer.md` ("Stage") and the `vs_stage_areas` row. PP's draw routines pair
one to one with MoA's (one area: PP `0x80059BCC`, 0.99). Area model numbers on PP run 0 … 56 (+0x1E; 0xFFFF = none).

## 5. MoA stages (data)

**Format:** MoA's stage data has **one extra 80-byte chunk in front** (14 chunks); the other 13 line up with PP's
(chunk 4 is always empty on MoA). Stage numbering differs. Matched by the size of the geometry chunk
(`stage_survey.py moa-map`):

| PP | MoA-US | Geometry |
|---|---|---|
| 20 | 33 | byte-identical (whole data file except the extra chunk) |
| 25 | 37 | byte-identical geometry |
| 27 | 38 | byte-identical geometry |
| 21, 22, 23, 33, 38, 39 | 34, 35, 36, 31, 32, 27 | same size, a few bytes changed |
| 24, 26, 28, 29, 30, 31, 32, 34, 35, 36, 37 | none (not among MoA 0–39) | PP only |
| — | 20–26, 28–30, 39 | MoA only (11 stages) |

**To bring a MoA stage over:** its data file minus the first chunk; its stage textures (RTIM entry = stage number on
MoA); and its code files rebuilt for PP, because they call engine routines by MoA address (e.g. the per-type
inits allocate behaviour blocks through the engine). The 4–6 KB code files are small; the function map
(`docs/research/xdisc/functions.tsv`) gives the PP address for each engine call. A stage slot: below 40 only mission slots 17
and 18 are free (no files), so a MoA stage needs a number ≥ 40 (checks on the
stage byte in the engines: open) or replaces a PP stage. For the disc patch track: FDAT.T would need new entries.

## 5a. Collision polygons and surface flags (round 2 review, 2026-11-26)

**Where the stage's collision lives (code + data).** Not in the area models: in **chunk 6** (the area finder;
mission maps block 6 too, MoA one later). Its 1,024 offset pairs are turned into pointers at load (`0x8006A700`);
the second of each pair points at a **collision block**: u16 polygon count at +0, three 32-byte bit masks at
+4 / +0x24 / +0x44 (x / y / z slices, tested by `0x8006C3CC`), then the **polygons, 20 bytes each from +0x64**.
An area record's +0x1E picks its block. The hit-list builder (`0x8006E6E4`, VS) walks the block (`0x8006E368`)
and fills the hit list `0x801CC6FC` (28-byte records, +0 = polygon). **Polygon +0x0E bits 0–2 = surface flags.**

**Two walkers read the flags (code, VS; same in every engine):**
- `0x8006E958` (via `0x8006E9D0`): nearest hit, **skipping** surfaces that share a bit with the mask. The AC
  passes its +0x3A (PP 7) / +0x43 (MoA 5); one missions-engine caller passes a fixed 7 (PP `0x8008A148`) / 5
  (MoA `0x8009331C`). So: flags 1 and 4 are passed through on both discs; **flag 2 stops MoA ACs, PP ACs pass it.**
- `0x8006A23C` (via `0x80074B44`, missions `0x800762F4` / MoA `0x8007E238`): the opposite, nearest hit
  **with** flag 1 (called with mask 1, next to a ground-height query). Flag 1 = some special ground `?`.

**Which maps have which flags (data; `tools/mapping/stages/surface_flags.py`, polygon counts):**
- **Flag 2 appears only on MoA mission maps**: 1 Defend Submarine (15), 2 Destroy Berserk MTs (32), 7 Retrieve
  Secret Files (6), 9 Enter Abandoned Mine (2), **15 Destroy Giant Tank (104)**, 18 Enter Factory (22),
  19 Final Mission (4). **No PP map and no fight stage on either disc has any.** What they are in the level
  (invisible walls, barriers `guess`) is not read yet.
- Flag 1: on many maps of both discs (e.g. PP 13: 557 of 1,261). Flag 4: mission maps of both, PP 21 / MoA 34.

**What it means.** PP's mask 7 against MoA's 5 changes nothing on any PP map or any fight stage (so imported
MoA *fight* stages feel the same). It matters only for **imported MoA missions**: on PP's engine the AC would pass
surfaces MoA built to stop it. Simplest fix in the import: rewrite flag 2 → 0 in those polygons of our converted
map (data only; no other reader of bit 2 found so far `?`), instead of patching the engine's masks.

## 6. Open

- What chunk 4, 10 and 12 hold; what classes 250, 241, 247 … are.
- The extra data after the 512 area records (neighbour lists) and the area finder's bit format (the collision
  blocks behind it are read, §5a); what MoA's flag-2 surfaces are in the level; what flag 1 is.
- The 80-byte chunk MoA added.
- Whether the engines accept stage numbers ≥ 40 (table sizes, RTIM index = stage).
- Mission maps (0–19) follow the same chunk scheme with more chunks (16 on PP) — topic 4.

## Tests (added to `ROUND1.md`)

- **#10** AI marker check: arena on stage 23 or 29 vs stage 20, opponent with any AI: on 23 / 29 the AI should
  sometimes run to a ledge and boost up; never on 20.

## Handed over to the build chat (routine starts for PP module seeds; all `code`)

- FDAT 204 (VS): `0x80050C50`, `0x80054D78`, `0x8004AF28`, `0x8006A700`, `0x8006EA18`, `0x8006FF68`, `0x8006FDF8`,
  `0x80053E0C`, `0x8007046C`, `0x8006C784`, `0x8006D1F8`, `0x8004DECC`.
- FDAT 205 (arena): `0x800705E8`, `0x80071B38`, `0x800719C8`, `0x80071950`.

## Handed over (other docs)

- `boot-loads.md` PP-10 (Lane 3): stage code k = 0 is missions **and link**; link does not use k = 1.
- `battle-system.md` (Lane 4): the box handler `0x80090070` is FDAT 101 +0xAA0 (VS code of stage 20), installed by
  the class-249 type init; FDAT 100 is the link copy.
- `ai.md` (Lane 6): open item 1 answered in §3 above.
- `renderer.md` (Lane 7): PP stage objects are 512 (not 128); stage geometry = chunk 0 model list (§2).

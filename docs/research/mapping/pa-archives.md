# PA body archives (`GG/P0`–`P3/PA00`–`PA71.T`) — Project Phantasma

Mapping round 2, topic 1 (2026-11-26). Disc: **PP** (Project Phantasma USA, SLUS-00670) unless a line says
**MoA-US** (Master of Arena USA disc 1). Everything here is a **static read** of the owner's discs: the fight
engines' loader code plus the archive bytes. Nothing has been seen running yet (tests at the end).
Tool: `tools/mapping/pa/pa_archives.py` (prints numbers and names only; rerun it, don't commit its output).

## In plain words

- **Each of the 72 files is one "body kit"**: everything a fight needs to draw and move one AC whose core, arms and
  legs fall into one combination. It holds the **skeleton**, the **joint animations** (57 moves), the AC's **hit size**
  and **how far each move carries it** (the root motion where Kaze lives), and **a 3D model of every part that
  fits that body**, each one pre-fitted to that skeleton: every head, booster, back weapon, and the hand weapons,
  arms, legs and core of that combination. No textures (those come from the menu's part archives).
- **Which file an AC uses** is worked out in the menu when a fight starts: the game looks up core + arms + legs in
  a 72-row table and takes the first row that matches. That number (0–71) picks `PA<nn>.T`.
- **Only the fight engines read these files** (VS, arena, missions, link, replay). The garage and shop never do.
- **MoA's files are the same bodies with the same skeletons**, so a MoA part model can drop into the PP file of
  the same number. About 94% of entries are byte-identical between the two discs. MoA only rebuilt a few weapon
  models and the four body sets Lane 2 already found (Kaze), and it added 33 new part models (1,746 copies,
  ~11.9 MB, because each part is stored once per body that can wear it).
- **Consequences:** the "Both sides" Kaze setting only needs a few more bytes in three files (there is room).
  Importing MoA parts needs extra model entries, and PP's numbering has no space for MoA's hand weapons 23–25 and
  its 60th back weapon, so the recomp should serve part models by part, not by PP's entry number (below).

## 1. How a body file is picked (code)

| Step | What | PP | MoA-US | Status |
|---|---|---|---|---|
| 1 | VS START / arena start compiles each player's stat block; the body combination goes to **stat +0x29** (player block +0x281) | menu `0x8009A238`, store at `0x8009A5EC` | `0x800A7BB4` | code |
| 2 | Lookup: walk the **combination table** (72 × 12 bytes: core id, 3 pad, pointer to an arms list, pointer to a legs list; lists end in 255) and take the first row whose core equals the build's core and whose lists hold its arms and legs. **No end check**: a build that matches no row runs past the table | table `0x800BC7B8` (FDAT 201 +0x736E8) | `0x800D4A64` | code |
| 3 | Core 3 (XXA_S0) is looked up as core 0 (the build's core byte is set to 0 for the search and put back after) | `0x8009A290` … `0x8009A39C` | cores 3 and 4 | code |
| 4 | Each fight engine writes the number into its name string as `P<n ÷ 20>\PA<nn>.T`, registers it as **archive 0** with `0x80017244`, and reads entries with the usual loader `0x800171B4` | VS `0x8004D820` (string `0x800874C4`) | VS `0x80055214` | code |

The other engines do the same with their own copies (code, same base numbers in each):

| Engine (PP FDAT) | Body loader | String | Entry-1 copier | Model load | Load + attach |
|---|---|---|---|---|---|
| missions 202 (player 1 only, block `0x800424E8`) | `0x8004D75C` | `0x8008BFDC` | `0x8007187C` | `0x80051B1C` | `0x80051D08` |
| link 203 | `0x8004DDDC` | `0x80088728` | `0x80070BEC` | `0x800517E8` | `0x800519D4` |
| VS 204 | `0x8004D820` | `0x800874C4` | `0x800703D0` | `0x80050A1C` | `0x80050C08` |
| arena 205 | `0x8004DFDC` | `0x8008C7D8` | `0x80071FA0` | `0x80052B9C` | `0x80052D88` |
| VS replay 206 | `0x8004D770` | `0x800876FC` | `0x80070374` | `0x80050F80` | `0x8005116C` |

The menu module (201) never registers archive 0, so the garage, shop and assembly previews don't use these files
(they use `GG/MS/MENU_TMD.T`).

### The 72 combinations (data, menu `0x800BC7B8`)

Arms groups: **A** = AN-101, AN-201, AN-D-7001, AN-863-B, AN-25 (ids 0, 1, 3, 6, 7); **B** = AN-3001, ANKS-1A46J (4, 5);
**K** = AN-K1 (2); **W** = weapon arms AW-… (12–20). Legs groups: **M** medium two-legged {0, 3, 6, 9, 10, 11},
**H** heavy two-legged {2, 5, 8, 13}, **L** light two-legged {1, 4, 7, 12}, **R** reverse-joint {14, 16–19},
**F** four-legged {21–23, 26}, **C** caterpillar {27, 28, 30, 31}. (Leg ids 7 and 9 are `DUMMY` on PP but still in the lists.)

| Core \ legs | M | H | L | R | F | C |
|---|---|---|---|---|---|---|
| **XCA-00** (0; XXA_S0 too) | A 0, B 10, K 11, W 31 | A 8, B 4, K 13, W 32 | A 9, B 12, K 5, W 33 | A 48, B 49, K 50, W 66 | A 6, B 42, K 43, W 63 | A 1, B 2, K 3, W 69 |
| **XCL-01** (1) | A 7, B 20, K 19, W 34 | A 16, B 14, K 18, W 35 | A 17, B 21, K 15, W 36 | A 51, B 52, K 53, W 67 | A 40, B 44, K 45, W 64 | A 57, B 58, K 59, W 70 |
| **XCH-01** (2) | A 22, B 29, K 28, W 37 | A 25, B 23, K 27, W 38 | A 26, B 30, K 24, W 39 | A 54, B 55, K 56, W 68 | A 41, B 46, K 47, W 65 | A 60, B 61, K 62, W 71 |

So the number is "which core × which arms family × which legs family"; the same table is on MoA (MoA's lists also
hold its new parts, `feel-diff.md` §4.3). File `P0` holds 0–19, `P1` 20–39, `P2` 40–59, `P3` 60–71.

## 2. What is inside one archive (data + code)

268 entries on PP (275 on MoA). **Entry number = a fixed base + the part's id**, read in the body loader (VS
`0x8004D820`; part ids from player block +0x283 … +0x28A = stat +0x2B … +0x32):

| Entries (PP) | What | Player block byte | Loaded | Entries (MoA) |
|---|---|---|---|---|
| 0 | **Body**: skeleton, joint animations, mesh-slot table (§3) | +0x281 picks the file | always | 0 |
| 1 | **Body record**: hit size + root-motion tracks (§4) | | always | 1 |
| 2 + core | core model (2 XCA-00, 3 XCL-01, 4 XCH-01, 5 XXA_S0) | +0x283 | if ≠ 255 | 2 + core |
| 15 + legs | legs model | +0x284 | if ≠ 255 | 15 + legs |
| 57 + head | head model (57 … 67) | +0x285 | if ≠ 255 | 57 + head |
| 81 + booster | booster model (81 … 87) | +0x287 | if ≠ 255 | 81 + booster |
| 99 + hand weapon | hand weapon (99 = NO WEAPON … 119 WA-Finger) | +0x288 | only when arms id < 12, ≠ 255 | 99 + hand |
| **122** + arms | arms model (weapon arms 12–20 = 134–142) | +0x286 | if ≠ 255 | **127** + arms |
| **150** + back | back weapon, left side (all 41 real back parts) | +0x289 | if ≠ 255 | **155** + back |
| **209** + back | back weapon, right side | +0x28A | if ≠ 255 | **215** + back |

Each archive holds exactly the models its combination can wear: its core (plus XXA_S0 in the XCA-00 sets), its
arms and legs groups, all heads, boosters and real back weapons, and the 21 hand weapons unless the arms are
weapon arms (checked on all 72 files with `pa_archives.py check`). Leftovers: sets 66–71 still carry entry 99,
and set 0 carries entry 24, a 26 KB model for leg id 9, which is `DUMMY` on PP (MoA put LN-S3 in that id).

**Copies are per body.** A head exists 72 times; the copies are not all equal (28 different versions of HD-01-SRVT
across the 72 files), but they differ only in a few bytes, mostly inside one small mesh `guess: where the part sits
on that body`. Hand weapons are the exception: 54 copies, all different.

Sizes: body 16–64 KB, a part 2–28 KB. The biggest per-player load is ~177 KB (set 20).

## 3. Entry 0: the body (data; code `identical` PP ↔ MoA)

The PP routines that read it are byte-identical to the MoA ones the old notes decoded (`renderer.md`
"Model", the decoder `reference/ac1recomp/.../acr_models_moa.c`); the layout was re-read here on the PP files.

| Offset | Size | Meaning |
|---|---|---|
| +0x00 | u32 | bytes used |
| +0x04 | u8 | main animation count (57 on every set) |
| +0x05 | u8 | joint count |
| +0x06 | u8 | aim tables (7; 8 with weapon arms) |
| +0x07 | u8 | second-layer animations (3) |
| +0x08 | u32 | offset of the **mesh-slot table** |
| +0x0C | u32 | offset of the **joint table** (always 0x1C): u16 count, then op words (op 8 = a joint, op 3 + u16 = parent, op 2 + u16 = mesh slot; read by VS `0x80050D58`) |
| +0x10 | u32 | offset of the main animation list (57 offsets, relative to the list; 0 = the set has no such move) |
| +0x14 | u32 | offset of the aim-table list (absolute offsets) |
| +0x18 | u32 | offset of the second-layer animation list (absolute offsets) |

**Mesh-slot table**: u32 slots per level, u32 0, u32 total, then one 28-byte record per slot per level. A body
starts with every record empty (flag +0x0E bit 0x8000). **Three detail levels** of every slot `guess: near / middle
/ far`.

The four body families (all 72 sets fall into these):

| Family | Sets | Joints | Mesh slots | Main animations present | Hit radius / height |
|---|---|---|---|---|---|
| two-legged (also reverse-joint) | 0, 4, 5, 7–30, 48–56 | 45 | 23 | 57 | 100 / 225 (set 0: 212) |
| two-legged, weapon arms | 31–39, 66–68 | 47 | 23 | 55 | 100 / 225 |
| four-legged | 6, 40–47 (weapon arms 63–65) | 52 | 25 | 35 (33) | 112 / 150 |
| caterpillar | 1–3, 57–62 (weapon arms 69–71) | 36 | 17 | 35 (33) | 112 / 150 |

**Part models** (entries ≥ 2) are TMD-style models (the PlayStation's standard model layout): at +0x0C a header
(meshes per level, levels = 3, object count), then 28-byte objects (vertex list, count, normal list, count,
primitive list, primitive count + **body mesh slot** at +0x16, a fourth list). After loading, `0x800548C0` turns
the offsets into addresses and prepares the primitives; `0x80050A98` then copies each object into the body's
mesh-slot record named by +0x16 (per level) and marks both. So **a part model works on any body file whose slot
numbers it was made for**, which is why it is stored once per body.

**Where it goes in RAM** (VS): body → model table `0x80199F24`[0x80 + player], parts → `0x80199F24`[0xE0 + 16 ×
player + k] (k: 0 core, 1 legs, 2 head, 3 arms, 4 booster, 5 hand, 6 back L, 7 back R), loaded one after another
from the engine's load pointer (VS `0x8008F5B0` +8). The AC's pose routine (VS `0x80051B44`) reads the model as
`renderer.md` describes.

## 4. Entry 1: hit size and root motion (data + code)

One 2,048-byte sector. +0 u32 bytes used; the engine copies the rest into the fight heap (VS `0x800703D0` →
heap `0x801D83C0`, record `0x801D8030` + player × 44):

| File offset | Meaning | Used by |
|---|---|---|
| +0x04 | u32 57 (track count) | |
| +0x08 / +0x0A | u16 **hit radius / height × 4** (model units; 400 / 900 two-legged, 450 / 600 four-legged and caterpillars, 400 / 850 set 0) | record +0 / +2 (÷ 4) → AC object +0x6A / +0x6C (hit cylinder) and +0x28 / +0x2A, set by VS `0x80075144` |
| +0x0C … | the 57 **root-motion tracks** (how far a move carries the AC, keyed by frame): format in `tools/feeldiff/anim.py`, Kaze tracks in `feel-diff.md` §4.3 | record +0x28 → AC object +0x80 |

So the AC's hit cylinder comes from the body file, not from the parts: every two-legged body is 100 wide and 225
tall in world units except the default AC's set 0 (212). `code` for the copy and the AC setup; that +0x6A/+0x6C is
the hit cylinder is Lane 4's reading (`battle-system.md`).

## 5. PP vs MoA (data)

Entry numbers differ (arms +5, back weapons +5 / +6 on MoA; MoA back ids 16–17 are new, so PP back id n ≥ 17 =
MoA n + 1). Matched that way, **8,738 of 9,210 PP entries are byte-identical on MoA**. Joint tables and mesh-slot
tables are identical on all 72 sets. What differs (`pa_archives.py diff`):

| What | PP entries | Sets | Change |
|---|---|---|---|
| Body animations | entry 0 | 11, 13, 27 | **anim 18** (back-right start) is a longer move on PP (9 / 11 frames) than MoA (7 / 8, the length the rest of that family uses), and anim 56; offsets after them shift. Nothing else |
| | entry 0 | 12 | nearly all moves (the light-legs walk timing Lane 2 found) |
| | entry 0 | 54 | anim 28, 3 bytes |
| Root motion | entry 1 | 11, 12, 13, 27 | Lane 2's Kaze / walk rows |
| Weapon models | 100 WG-RF35 (all 54), 107 WG-HG512 (4 sets) | | remade (~1.5 KB of bytes) |
| Small edits (3–10 bytes) | 102 WG-MG500; back WM-S60/4, WM-L201, WM-AT, WM-TO100 (every set); LB-4401, LF-205-SF; a few back weapons in sets 6, 9, 13, 26; caterpillar legs in set 71 | | `guess`: a field fix |

**MoA-only models** (the import inventory; `pa_archives.py moa-only`): core XXL-D0 (24 sets); legs LN-S3,
LB-H230, LF-TR-0, LC-HTP-H5 (12 each); heads HD-12-RADAR, HD-HELM, HD-H10, HD-4004 and booster B-PT000 (72 each);
hand WG-HG770, WG-XW11, WG-FGI-00, WG-RF/E, WG-MG500/E (54 each); arms AN-891-S and AW-R/4 (18 each); back
WM-MVG812, WM-X15-EX, WR-RS7, WRR-10, WX-ED2, WX-C/4, P77-ST (72 each, both sides). 1,746 entries, 11.9 MB of
sectors. Because the skeletons match, **MoA's copy in its file n fits PP's file n as it is**.

## 6. What this means for the planned work

**Kaze setting (`feel-diff.md` §8b).** All three choices are entry-1 data in PA11, PA13, PA27:
- *Both sides*: anim 16 (back-left start) gets the mirrored slide keys. Room: entry 1 uses 1,532 / 1,556 / 1,556 of
  its 2,048 bytes (492+ free), so the extra ~24 bytes per file fit in the same sector; offsets of the tracks after
  anim 16 move. Note the body animation: PP's anim 18 in these sets is a longer move (9 / 11 frames) than anim 16
  (7 / 9), so a left slide plays the normal step pose over a longer slide `guess` (test).
- *Off*: MoA's entry 1 for these sets; for a matching pose also MoA's anim 18 from entry 0 (shorter move).
- In the recomp this is a swap of loaded bytes for archive 0 entry 1 (and optionally entry 0), keyed on the set number.

**MoA part import.**
- Models: take MoA's entry from MoA's file of the same number. PP's numbering has free entries for MoA's extra
  core (XXL-D0 → 6; 6–14 free), legs (ids up to 41), heads (68–80 free), booster (88–98 free), hand ids 21–22
  (120–121) and arms (AN-891-S → 130, AW-R/4 → 143); but **hand ids 23–25 would land on PP's arms entries
  122–124, and PP's back range is full at 59 ids** (MoA has 60). MoA solved it by moving the bases (275 entries). For us the cleanest route is a **recomp hook on
  the body loader's part loads** (the five engines' load + attach calls above): look up (category, part id, set)
  in our own pack first, fall back to the disc. No archive renumbering, no change to the engines' numbers.
- Combinations: every new arms / legs / core id **must be added to a list** in the combination table, or the
  lookup runs off the table (step 2). MoA's lists show where each new part goes.
- Hit size and animations come from the body set, so new MoA parts don't change how big an AC is or how it moves.
- **Disc patch track**: adding entries means rebuilding the PA archives (their sector tables). Per-file growth would
  be ~0.2 MB on average; the engines' fixed numbers would also need MoA's bases. Not cheap; the recomp route first.

## 7. Disc patch track

- Files: `GG/P0/PA00.T` (disc LBA 95,895) … `GG/P3/PA71.T`; each starts with its sector table (u16 count, starts,
  end; `tools/tarc.py`). Kaze files: PA11 at LBA 101,021 (entry 1 = sector 33 of the file), PA13 at 101,949
  (sector 30), PA27 at 108,504 (sector 31). Free bytes: see §6.
- Code constants (if the numbering ever changes): the bases 2, 15, 57, 122, 81, 99, 150, 209 are `addiu a2, a2, N`
  immediates in the five body loaders (VS: `0x8004D960`, `0x8004D9A4`, `0x8004D9E8`, `0x8004DA2C`, `0x8004DA70`,
  `0x8004DAC8`, `0x8004DB18`, `0x8004DB64`; FDAT 204 + address − 0x800490D0).
- Combination table: FDAT 201 +0x736E8, 864 bytes; the lists it points to sit **before** it (`0x800BC380`, 1,080
  bytes; MoA's lists fit there in place: `parts-pp.md` §4).

## 8. Open

- What each of the 57 main animations is (14, 16, 18 = back / back-left / back-right start, 15 / 17 the walks
  after them, 2 / 4 / 6 / 9 / 12 walk and strafe tracks are known; Lane 2). Which AC state starts which.
- The three detail levels: when the engine switches level `guess` (by distance).
- The small per-body differences between copies of one part, and the 3–10-byte MoA edits.
- Engine 203 (link) and 206 (replay) were read only as far as the loader.

## Tests (added to `ROUND1.md`)

- **#8** Hit size from the body file: plain VS, default AC (set 0) vs an AC with AN-K1 arms (set 11): in
  DuckStation's memory view, `0x801D8030` / `0x801D8032` read 100 / 212 for the default AC and 100 / 225 for the
  AN-K1 one; a caterpillar build reads 112 / 150.
- **#9** Body number: in VS, byte `0x80042769` (player 1 block `0x800424E8` + 0x281) = 11 with XCA-00 + AN-K1 + LN-1001, = 0 with the
  default AC.

## Handed over to the build chat (routine starts for the PP module seeds)

All `code`, found in this topic:
- FDAT 201 (menu): `0x8009A238` (stat compile; already known).
- FDAT 202: `0x8004D75C`, `0x8007187C`, `0x80051B1C`, `0x80051D08`.
- FDAT 203: `0x8004DDDC`, `0x80070BEC`, `0x800517E8`, `0x800519D4`.
- FDAT 204: `0x8004D820`, `0x800703D0`, `0x80050A1C`, `0x80050A98`, `0x80050C08`, `0x800548C0`, `0x80050D58`,
  `0x80075144`.
- FDAT 205: `0x8004DFDC`, `0x80071FA0`, `0x80052B9C`, `0x80052D88`.
- FDAT 206: `0x8004D770`, `0x80070374`, `0x80050F80`, `0x8005116C`.

## Handed over (other topics)

- Topic 5 / Lane 4: AC object +0x28 / +0x2A get the same values as +0x6A / +0x6C at setup (VS `0x80075144`).
- Topic 5 / Lane 5: the 20-byte per-body table at VS `0x80088B24` (player block +0x4C) is not decoded.

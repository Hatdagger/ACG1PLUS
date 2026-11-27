# Parts on Project Phantasma: part archives, shop stock, the data after the weapon names

Mapping round 3, topic 3 (2026-11-26). Disc: **PP** = Project Phantasma USA (SLUS-00670) unless a line says
**MoA-US** (Master of Arena USA disc 1). Everything here is a **static read** of the owner's discs (code and archive
bytes); nothing was seen running yet (tests 35–36 in `ROUND1.md`). Status words as in `content-data.md`:
**code** = read from the instructions, **data** = read from the disc files.

Addresses in the menu module are PP **FDAT 201**, loaded at `0x800490D0` (file offset = address − `0x800490D0`).
Data findings (§1, §3, §4 tables) and code findings (the routines named beside them) are kept apart in each table.

## In plain words (for the owner)

- **The `*_T` archives are part *textures*, not models.** Each body part has **16 versions, one per paint
  pattern** (CUSTOM plus the 15 named patterns like WOODLAND or TIGER STRIPE). They are stored pattern by pattern:
  all 18 heads in pattern 0, then all 18 heads in pattern 1, and so on. So a MoA head, core, arms or legs needs
  **16 textures**, one per pattern. Weapons, boosters, FCS, generators and options have one texture each.
- **The garage / shop 3D models are in `MENU_TMD`**, one per part, numbered by kind. PP has no free numbers for
  MoA's last 5 hand weapons or its 60th back weapon, so the recomp should serve MoA parts from our own pack by
  "kind + part number" rather than squeeze them into PP's numbering.
- **PP's filled `DUMMY` slots are leftovers:** some are copies of a real part (legs 9, 15, 20, 29 are LN-1001's
  textures; back 16 is M118-TD's), two are reskins of LF-205-SF (legs 24, 25), one is a cut leg (7). MoA put its
  own parts in 9, 20 and 24. Nothing in the game shows them, so we can overwrite them.
- **Shop stock is confirmed** at `0x800343F0` (right after the owned counts). A new game: you own the 11 starting
  parts; the shop has **1 of most parts and 2 of each back weapon** (one per shoulder; for the two starting back
  weapons that's 1 owned + 1 in the shop). Parts with stock 0 are never sold (KARASAWA, MOONLIGHT, WA-Finger …):
  they only come as prizes. **Nothing on PP ever adds stock later.** A count of **127 means "never changes"**
  (buying and selling skip it); nothing on the disc sets 127, so it's a ready-made "unlimited" marker we can use.
- **The bytes after the weapon-name table** are the **body layouts** (which model joints guns and missiles fire
  from, what the alternate camera hides) and a 72-entry **body table** (one per body combination). They are byte
  for byte MoA's. MoA put its weapon names *after* that table instead, which is how it fit 5 more names. For us:
  the five new names (SPECIAL MISSILE … FLAME SHELL) go in our own copy of the name table; only one routine reads it.
- **One import trap found:** the table that decides a fight's body file only knows PP's arms and legs. MoA added
  its new arms and legs to it (and moved leg 9 from the medium to the heavy group for LN-S3). A MoA arm or leg
  dropped into PP without that change makes the game run off the end of the table. Good news: **MoA's version fits
  in PP's table in place**, every list has room.

## 1. Part texture archives `GG/MS/*_T.T` (data, code)

**Layout (data):** HEAD_T 288, CORE_T 176, ARMS_T 384, LEG_T 640 entries = **16 × the part count** (18, 11, 24, 40).
**Entry = pattern × part count + part id**, pattern 0–15. Every entry is one PS1 TIM, 8-bit, no palette, the size of
that part's VRAM page (head 6,144 bytes, a 128 × 32 image placed at (768, 352); core 18,432; arms and legs
14,336). The 16 versions of a part are 16 different images. The other archives have one entry per part id:
BST_T 14, BWL_T / BWR_T 59 (left / right back weapon), WEL_T 9 (blade), WER_T 21 (hand), COMP_T 16 (FCS),
SPEC_T 19 (options), GENE_T 15. A part with no texture has a 0-byte entry.

**Reader (code):** the AC texture queue **`0x800730A4`** (twin of MoA `0x800822B0`, "same") reads the pattern byte
**`0x8003BCCE`** (`paint_pattern`) and queues HEAD_T (archive 10) head + 18 × pattern, CORE_T (7) + 11 ×, ARMS_T (9)
+ 24 ×, LEG_T (8) + 40 ×, then WEL_T, WER_T, BWL_T, BWR_T, BST_T by part id. The pattern rules (pattern 0 = custom
colours, 1–15 ready-made colour schemes) are as on MoA, `menus-screens.md` §2l; the pattern list is PP `0x80080044`
(twin of `0x8008E70C`).

**Filled slots per part kind, PP vs MoA-US (data, every entry compared byte for byte):**

| Kind | PP has data, part is `DUMMY` on PP | MoA has data, PP empty (MoA's new parts) | Both have data, different |
|---|---|---|---|
| head | — | 11–14 (HD-12-RADAR, HD-HELM, HD-H10, HD-4004) | — |
| core | — | 4 (XXL-D0) | — |
| arms | — | 8 (AN-891-S; PP has a garage model there, below), 21 (AW-R/4) | 7 pattern 9, 20 pattern 8 (repaints) |
| legs | **7** (a leg no other slot uses), **15**, **25** | 32 (LC-HTP-H5) | **9** (LN-S3), **20** (LB-H230), **24** (LF-TR-0) |
| back | 16 (copy of 15, M118-TD) | MoA 16/17 (MVG812, X15-EX), MoA 23/24, 55–57 (PP numbers: see `content-data.md` 1a) | — |
| blade / hand / booster / FCS / option | — | blade 4 (LS-1000W), hand 21–25, booster 7 (B-PT000), FCS 10 (P/CV), option 12 (SP-ABS_Re) | — |
| generator | — | — | — |

What PP's leg leftovers are: **9, 15, 20, 29 = LN-1001's 16 textures** (leg 0; MoA's 29 is the same copy),
**24, 25 = the same new image in pattern 0 and LF-205-SF's (leg 21) in the other patterns**, **7 = its own image**
(a cut part). Every other back weapon from PP 17 up equals MoA's n + 1 (the known shift).

**MENU_TMD (garage and shop models, data):** the kind table **`0x800BC284`** (FDAT 201 +0x731B4; twin of MoA
`0x800D4544`), 11 × 16 bytes (u16 part count, u16 first MENU_TMD entry, then owned mask, framing table and one more
table pointer), gives PP's layout: entries 0–2 menu models, **core 3, legs 14, arms 54, head 78, booster 96, back
110, blade 169, hand 178, FCS 199, options 215, generator 234** (249 entries). MoA: the same up to back (60 ids),
then blade 170, hand 176, FCS 202, options 218, generator 237 (252). Compared part by part (data), PP has a model
where the part is `DUMMY` for **legs 7, 9, 15, 20, 24, 25, arms 8 and back 16**; MoA's new parts are empty on PP.
Entries 1 and 2 are equal on both discs; entry 0 differs.

**For the import (0.7 parts):** a MoA part needs its MENU_TMD model (garage, shop), its textures (16 for body
parts, 1 for the rest) and its fight models (`pa-archives.md` §6). PP's own `DUMMY` slots can be overwritten:
nothing lists them (stock 0, no table record). Hand 21–25 and MoA's 60th back weapon have no MENU_TMD number on
PP (the next kind starts right after), so serve models and textures **by (kind, part id) from our pack** in the
recomp's archive reads, the same route `pa-archives.md` §6 gives for the fight files.

## 2. Shop stock and owned counts (data, code)

| What | PP | MoA-US | Status |
|---|---|---|---|
| Owned count per part | `0x800342F0` (save +760) | `0x80036FEC` | code |
| **Stock per part** | **`0x800343F0` = owned + 0x100** (save +1016) | `0x800370EC` | code |
| BUY list builder | `0x8008B0D8`: `lbu 0x100(owned + id)`, keeps ids with stock ≠ 0 | `0x80099734` | code |
| SELL list builder | `0x8008B334`: owned ≠ 0 | `0x80099990` | code |
| A deal (buy / sell) | `0x80091940`: buy = owned +1 then stock −1, sell = owned −1; **each step is skipped when that count is 127** | `0x800A01B4` | code |
| Leaving the shop (unfit sold parts) | `0x8008B86C` | `0x80099EAC` | code (twin, "values 1.000") |
| New-game owned counts | `0x800B41D4` (FDAT 201 +0x6B104), 256 bytes | — | data |
| New-game stock | `0x800B42D4` (FDAT 201 +0x6B204), 256 bytes | — | data |

**Order of both tables (code, from `0x8008B0D8`):** head 0–17, core 18–28, arms 29–52, legs 53–92, generator
93–107, FCS 108–123, options 124–142, booster 143–156, back weapon 157–215, **arm weapons 216–245** (hand 216–236,
then blades 237–245). 246 bytes used, 10 spare at the end. Same order as MoA, but MoA's 256 entries hold 60 back
weapons and 26 + 6 arm weapons, so **MoA's index n is not PP's** from the back weapons on. The 10 shop kinds are
these ranges in this order (the list builder's `slti` bounds 0x12, 0x1D, 0x35, 0x5D, 0x6C, 0x7C, 0x8F, 0x9D, 0xD8,
0xF6), kept per kind at shop work +0xAC (counts) / +0xB6 (starts); the list length is at **+0x7C** on PP (the
`menu_shop_work` row copied from MoA says +0x5C, MoA's offset).

**New-game values (data):** owned = 1 for the 11 starting parts (HD-GRY-NX, XCA-00, AN-201, LN-1001-PX-0, GPS-VVA,
COMDEX-C7, B-P320, WM-S40/1, RXA-01WE, WG-RF35, LS-2001). Stock: 121 parts, **1 for every non-back part on sale,
2 for back weapons** except the two starting ones (WM-S40/1, RXA-01WE: 1 owned + 1 stock), so you can always end up
with 2 of a back weapon. **Stock 0 (never sold):** HD-X1487, HD-G780, AW-DC/2, LN-SSVT, LN-3001, LN-D-8000R,
LFH-X5X, LC-MOS4545, GBG-XR, GBX-TL, QX-AF, SP-SAP, SP-AXL, B-T001, WM-S60/4, WM-L201, WM-AT, M118-TD, WR-L24,
WC-LN350, WC-XC8000, WC-SPGUN, RZT-333, RZ-Fw2, WX-S800-GF, WG-FG99, WG-1-KARASAWA, WG-PB26, WA-Finger,
LS-99-MOONLIGHT (and every `DUMMY`). Bytes 246–255 are 0 in both tables.

**What adds parts later (code):** stock is written only by the new game (copy of the table) and **PP's AC1 save
import** in LOAD (`0x80066C44`, PP only): it clears owned and stock, then refills both from AC1's 160-byte owned and
stock tables (load buffer +0x2F8 / +0x398) through PP's per-kind index tables at `0x800B5094` … `0x800B50E8`
(lead for the AC1 chat: that is PP's own AC1 → PP part map). The save routine `0x80065ECC` copies the tables out.
Prizes go to **owned**, not stock: every fight engine has a 4-instruction "add one part" helper (missions
`0x80088008`, link `0x80087F68`, VS `0x80086C74`, arena `0x8008C020`, replay `0x80086EF4`: owned[id] += 1 when
id ≠ 0, **no 127 check**). Nothing else in the exe or the six modules writes either table.

**127 (code):** the deal compares owned and stock with 127 before each change and leaves a 127 alone; no code on
the disc ever stores 127 there (the two 127s in the hub code are sound volumes). So 127 = "unlimited" in the shop,
by design or as a leftover, and is safe to use for our own "always in stock" parts. Owned 127 stays 127 when bought
or sold, but a prize would still raise it to 128.

## 3. The data after the weapon-name table (data, code)

PP's menu module holds this run of tables (data):

| Address (FDAT 201 +) | Size | What | MoA-US |
|---|---|---|---|
| `0x800BC380` (+0x732B0) | 1,080 | 144 id lists for the combination table, one private list per row and side, each ending in 255 and padded to 4 bytes | before `0x800D4A64` |
| `0x800BC7B8` (+0x736E8) | 864 | body combination table (existing row) | `0x800D4A64` |
| `0x800BCB18` (+0x73A48) | 448 | weapon type names, 28 × 16 (existing row) | **`0x800D5488`, 33 names, after the body table** |
| **`0x800BCCD8`** (+0x73C08) | 580 | **body layouts**: 6 used ones (+0x00, +0x3A, +0xAE, +0x196, +0x1D0, +0x20A) and 3 layouts in between that no body-table entry points at | `0x800D4DC4` |
| **`0x800BCF1C`** (+0x73E4C) | 1,152 | **body table**, 72 × 16 bytes: layout pointer, u16 body frame (0 two-legged, 1 caterpillar, 2 four-legged), u16 650, u16 40 (0 on combination 1), u16 100, u32 2 | `0x800D5008` |
| `0x800BD39C` | … | next data (starts like MoA's bytes after its name table) | `0x800D5698` |

The layouts and the body table are **byte for byte MoA's** (pointers compared relative to their block), so
`stat-block.md`'s "Body layout" holds on PP: the layout bytes are model joint numbers (missile-interceptor joints,
alternate-camera joints, muzzle joints per weapon slot, the joints the alternate camera hides), copied into the stat
block +0x56 … +0x85; the body table's four numbers go to +0x36 … +0x3C (camera distance and height). MoA's layout
and body tables sit right after its combination table; **PP put the names in between**, MoA moved them to the end
of the run, which made room for 5 more without moving anything that has pointers.

**Readers (code):** only the stat compile **`0x8009A238`** reads any of it. It looks up the combination (§4), then
indexes the body table by it 14 times (`0x8009AF9C`, `0x8009B064`, `0x8009B300`, `0x8009B9D8`, `0x8009C1B4`,
`0x8009C6E8` …) and follows the layout pointer. The weapon names are read at 5 places, all in the same routine
(`0x8009B944`, `0x8009BF80`, `0x8009CDB0`, `0x8009D454`, `0x8009DFAC`: `lui/addiu 0x800BCB18`, then + 16 × type).
No other module or the exe uses these addresses.

**MoA's 5 extra names:** a type 28–32 on PP would read the first bytes of the layouts (not text). Two ways:
- **Recomp (recommended):** our own 33-name table (MoA's names 28–32 appended to PP's 28) and the 5 name-table
  loads in `0x8009A238` pointed at it. One routine, five sites, nothing else moves.
- **Disc patch track:** do what MoA did inside FDAT 201: move the names after the body table and shift the layouts
  and body table down by 448 bytes, updating the 72 layout pointers and the 5 name loads. Needs 80 free bytes after
  `0x800BD39C` (not checked) or a moved block.

## 4. Combination lists for MoA's new arms and legs (data, code)

The stat compile (`0x8009A238`, `0x8009A2B4` … `0x8009A388`) walks the 72 combination rows until the core matches
and the build's arms and legs are both in that row's lists; **there is no end check** (`pa-archives.md` §1). PP's
lists hold PP's parts only. MoA's lists (data, compared list by list) differ in 66 of 72 rows, only by:

- arms group A gains **8** (AN-891-S); weapon arms gain **21** (AW-R/4);
- legs: **9 moves from the medium group {0, 3, 6, 9, 10, 11} to the heavy group {2, 5, 8, 9, 13}** (PP keeps the
  `DUMMY` 9 with the medium legs; MoA's LN-S3 is heavy); reverse-joint gains **20** (LB-H230), four-legged **24**
  (LF-TR-0), caterpillar **32** (LC-HTP-H5).

Every one of MoA's 144 lists fits the space PP's list already has (the padding to 4 bytes leaves room), so the
import can **rewrite the 1,080 bytes at `0x800BC380` in place with MoA's lists** (same pointers, same 72 rows, same
body files). Without it, a MoA arm 8 / 21 or leg 20 / 24 / 32 matches no row and the lookup runs into the weapon
names, and LN-S3 in slot 9 would get a medium body.

(Correction for `pa-archives.md` §7: the lists sit *before* the combination table, not after it.)

## 5. Open

- Arms 8: PP has a garage model in this `DUMMY` slot but no textures; what it shows `?`.
- Owned can pass 127 through prizes (no cap in the fight helpers); whether the garage or save care `?`.
- Bytes free after `0x800BD39C` for the disc-patch name move (§3).
- The kind table's owned-mask, framing and +0x0C pointers on PP (only counts and MENU_TMD starts read here).

## Handed over

- **AC1 chat:** PP's LOAD `0x80066C44` maps AC1's owned / stock tables into PP's through the tables at
  `0x800B5094` … `0x800B50E8` (PP's own AC1 → PP part numbering).
- **Build chat (0.7 parts):** serve MoA part models and textures by (kind, part id) in the archive reads (MENU_TMD,
  the `*_T` archives, the PA files); rewrite the combination lists (§4); our own 33-name table for `0x8009A238`.
  No new routine starts for seeds from this topic.

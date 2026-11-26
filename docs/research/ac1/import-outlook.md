# AC1 topic 4 — Import outlook: AC1 missions, stages and parts on Phantasma

AC1 mapping chat, topic 4 (`../mapping/PLAN-AC1.md`), 2026-11-26. Discs: **AC1** = Armored Core USA (SCUS-94182),
**PP** = Project Phantasma USA (SLUS-00670). **Static read** of the owner's work packs; nothing here was seen
running. Status per finding: `code` (read from code), `data` (read from the files), `guess`. Every address is AC1
unless tagged PP. Module addresses: AC1 modules load at `0x8004ADA0`, PP at `0x800490D0`; "FDAT 202" = missions
engine, "FDAT 201" = menu. AC1 mission code = FDAT 2*n* (linked at `0x801C4B40`), map = FDAT 2*n* + 1.
Written to stay in step with round 2's MoA plan, `../mapping/missions-import-pp.md` (§8 says where they meet).

Tool: **`tools/xdisc/ac1_mission_refs.py`** (ours; reads the packs, prints counts and addresses; rerun, never
commit output): default = every address the 49 AC1 mission files use, sorted and paired with PP; `--list` = the
117 engine routines with their PP twins; `--maps` = AC1 maps against PP's limits, item points; `--common` =
messages, sound ids and shared models in FDAT 200.

## In plain words (for the owner)

- **AC1 missions are much easier to bring over than MoA's.** Phantasma's missions engine grew out of AC1's,
  and the parts that broke the old MoA import are not there: the object record is the same size and layout
  (no field fixes at all in the mission code), the map files have the same 19 blocks, the module's table of
  hooks is the same, and the record limits are the same (256 placements, 512 event points).
- **What does need doing:**
  1. **Move the code to Phantasma's address.** AC1 runs mission code high in memory (`0x801C4B40`), Phantasma
     lower (`0x80097A38`). Every mission fits (largest 43 KB, room 54 KB). Mechanical, a tool does it.
  2. **Point the calls at Phantasma's engine.** The 49 files call 117 engine routines (3,970 calls). Every one
     now has a Phantasma twin: 97 sure, 20 to check by eye. I found and fixed 6 wrong or missing pairs on the way.
  3. **Point the data at Phantasma's addresses.** 188 addresses, all paired; most are inside the engine's record
     arrays, which are the same shape on both, just in other places.
  4. **Small data fixes:** one map block (a 256-entry table that PP keeps at 1,024) is padded; the 11 hidden
     parts get Phantasma's part numbers; AC1's mission texts go in at 50 + their number (PP put 50 new lines in
     front of AC1's 200 and kept the rest in order, so the engine's own messages land right with no code change).
  5. **Load AC1's own files while an AC1 mission runs:** its mission files, pictures, briefings, radio voices
     (TALK) and, for the right songs, its music (BGM). One file-load hook, the same one the MoA plan needs.
  6. **The story glue lives in AC1's menu, not the mission files**, so it has to be written again for
     Phantasma's menu: the mission list and chapter windows, the parts that arrive at chapters 11 and 21 and
     after "Remove Gun Emplacement", the score that triggers the "Above Ground" mail, and which ending movie
     plays after the last missions. All of this is now located (§5).
- **Stages: nothing to bring over.** AC1's VS and link stages are early versions of Phantasma's.
- **Parts and saves: already done by Phantasma itself.** Every AC1 part is on Phantasma, and Phantasma's own
  LOAD screen reads an AC1 memory-card save and converts the part numbers (§6). AC1 builds come across as they are.
- **Recommended way:** the same converter idea as the MoA plan (convert on the player's PC from their own disc,
  a checker refuses any undecided address), **starting with AC1**, because it is the easy case and proves the
  chain: pilot "Stop Terrorist Threat" (slot 1).
- **Decisions for you** are in §9 (own AC1 mission list or mixed in, AC1 sounds or Phantasma's, endings).

| Kind of AC1 content | How hard | Why |
|---|---|---|
| Missions (code + maps) | **medium** — tables and a converter, no hand fixes expected | same engine design, same object layout |
| Briefings, mails, pictures | easy | same file formats as PP; load from our pack |
| Mission texts | easy | same text codes; AC1 line *n* = PP line *n* + 50 |
| Radio voices, music | easy | same XA file names and channel numbering; redirect the file while an AC1 mission runs |
| Story rules (chapters, gifts, score, endings) | medium | AC1's menu code, to be rewritten for PP's menu |
| Movies | easy once the menu hook exists | AC1's own STR files, played by name |
| Stages | nothing to do | AC1's are early versions of PP stages 20–24 and 30–34 |
| Parts, saves, builds | nothing to do | PP has every AC1 part; PP's LOAD converts AC1 saves |

---

## 1. The two missions engines side by side (code)

AC1 engine start-up **`0x8004F508`** (PP `0x8004DAD4`, pair medium): same work in the same order.

| What | AC1 | PP | Status |
|---|---|---|---|
| Mission state record **M** (+0 number, +4 code, +8 buffer, +0xC flags, +0x16 = 0x80, +0x1A = 0x40) | **`0x8019F518`** | `0x80097A18` | code (same fields) |
| Number from | stat block +0x04 `0x800411F8` | stat +0x05 `0x80042745` | code |
| Mission code to (M+4) | `0x801C4B40` | `0x80097A38` | code |
| Room for mission code | `0x801C4B40` … `0x801D0B27` (49,128 B) | 55,296 B | code; **AC1's largest file is 43,008 (slot 49): every AC1 mission fits PP's room** |
| Load buffer (M+8) | `0x800B17C8` | `0x800BA5B0` | code |
| Fight-common pictures | RTIM **100** | RTIM **60** | code |
| Common file | FDAT 200 | FDAT 200 | code |
| Map | FDAT 2*n* + 1 | FDAT 5*n* + 4 | code |
| Map block consumers, in order | 3 model lists (slots 0x104, 0x80, 0xA0), backdrop, block 4, block 5 → M+8, area finder `0x80091228`, view table, sections, point classes, points, object types, placements, then one of 3 sound banks by the COM voice byte | same | code |
| COM voice byte | player block +0x27C `0x8004121C` | +0x282 `0x8004276A` | code (pair high) |

### 1a. Record arrays (code)

Same strides and counts as PP; read from AC1's loaders (`0x80073B74` placements, `0x8007334C` points,
`0x800731E8` point classes, `0x800739AC` object types, `0x80071E14` sections, `0x80056AD0` view table).

| Record | AC1 base | PP base | Size × count | AC1 → PP |
|---|---|---|---|---|
| point classes (block 8) | `0x80198808` | `0x801D8F50` | 24 × 32 | +0x40748 |
| event points (block 9) | `0x80198B08` | `0x801D9250` | 52 × **512** | +0x40748 |
| touch list / count | `0x8019F308` / `0x8019F508` | `0x801DFA50` / `0x801DFC50` | | +0x40748 |
| *(M sits here on AC1)* | `0x8019F518` | `0x80097A18` | 32 | |
| object types (block 11) | `0x8019F538` | `0x801DFC60` | 44 × 32 | +0x40728 |
| placements (block 12) | `0x8019FAB8` | `0x801E01E0` | 44 × **256** | +0x40728 |
| object pool (slot 0 = player AC) | `0x801A26B8` | `0x801E2DE0` | **368** × 16 | +0x40728 |
| active-section bits | `0x801A5DD8` | `0x801E6500` | 512 bits | +0x40728 |
| view table (block 10, 14 B on disc) | `0x801AD82C` | `0x801A8FCC` | 16 × **256** (PP **1,024**) | |
| timer / end code record | `0x801D0B44` | `0x801F4588` | | |
| map sections | `0x801D0B68` | `0x801BC328` | 64 × 512 | −0x14840 |
| section heap / its pointer | `0x801D8B68` (64 KB) / `0x801E8B68` | `0x801C4328` / `0x801D4328` | | |
| message table copy | `0x801ED850` | `0x800A52F0` | | medium |
| effect records (28 B) | `0x8008E530` (8) | `0x8008D42C` (9) | | by hand |
| per-mission u16 table | `0x800906CC` (**100** entries; 24 for missions 8 and 11, else 56) | `0x8008F874` (40) | | code |

So the **AC object is the same record** on both (`records.md` §1; AC1 +0xF2 … +0xF6 = PP +0xF0 … +0xF4) and **no
AC1 mission file touches those three fields** (0 sites). MoA needed 5,749 field fixes; AC1 needs none.

### 1b. Engine behaviours tied to the mission number (code)

| | AC1 (FDAT 202) | PP (FDAT 202) |
|---|---|---|
| Out of area | message **105**, end 0x101 (`0x8004C580`) | message 155 |
| AC destroyed | message **121**; **145** "Test mode disengaged" for number **70**; **no message** for 0, 21, 22, 49 (`0x8004C590` …) | 171; 195 for 19 |
| Results screen (`0x8008AF2C`) | skipped for **70, 48, 49**; for **47, 21, 22** only when M+0xC bit 0x200 is clear | skipped for 19; 10 needs M+0xC bit 0x100 |
| Chain at the end (`0x8004C6DC`) | after a success **48 → 49**, return 1 (start at once) | **the same 48 → 49 check**, dormant on PP |
| Per-mission u16 → `0x8008EF4A` (`0x800835A8`) | 100-entry table | 40 entries; numbers ≥ 40 read the next data |

**How slot 49 "Destruction of Ravens" is reached** (open from topic 3): by this chain. Winning engine mission
48 "Destroy Floating Mines" starts 49 straight away; it never needs a window `code`. Phantasma kept AC1's check
(that is why PP has a dormant 48 → 49).

## 2. Map files (data; tool `--maps`)

- **Same 19 blocks as PP** in all 49 maps (PP's own maps have them too; MoA's have one extra in front).
- **One difference: block 10 (view table)**: AC1's loader reads **256** records of 14 bytes, PP's **1,024**.
  AC1 maps use up to 255 of them. Filler record is the same on both discs (`000000000000ffff000000000000`), so the
  fix is to **pad block 10 to 1,024 records** with it (3,584 → 14,336 bytes). Slot 48 already ships a 1,024-record
  block (AC1 reads only the first 256).
- **Limits:** AC1 maps use at most placement 88, event point 132, 6 object types, 12 point classes: inside PP's
  256 / 512 / 32 / 32. Section heap: AC1 64 KB, PP 96 KB.
- **Sound banks:** the same 3 COM-voice banks at the end; nothing to convert.
- **Hidden parts** (item points, kind 7, owned index at disc +0x18 = RAM +0x24; the mission checks "not owned and
  not in the shop", the engine's pickup `0x8008A088` sets it): **11 parts in 11 missions**, all on PP. The index
  is a data patch of the map's block 9 (AC1 index → PP index, `content.md` §3 bases):

| Slot | Mission | Part | AC1 index | PP index |
|---|---|---|---|---|
| 6 | Secret Factory Recon | WM-S60/4 | 95 | 159 |
| 7 | Exterminate Organisms | QX-AF | 72 | 112 |
| 8 | Guard Freight Train | HD-X1487 | 2 | 2 |
| 9 | Destroy Fuel Depot | WG-1-KARASAWA | 142 | 231 |
| 17 | Guard Wharf Warehouse | RZT-333 (kind 6) | 122 | 201 |
| 24 | Destroy Plus Escapee | GBG-XR | 67 | 99 |
| 25 | Destroy Intruders | WG-FG99 | 136 | 225 |
| 34 | Eliminate Strikers | WR-L24 | 109 | 178 |
| 40 | Guard Factory Entrance | B-T001 | 89 | 145 |
| 43 | Retake Air Cleaner | WX-S800-GF | 125 | 209 |
| 44 | Kill "Struggle" Leader | LS-99-MOONLIGHT | 146 | 240 |

- **Enemy ACs** in missions: no engine call loads a body kit or part data for them (only the player's kit,
  `0x8004F1A8`), so they come as models in the map plus code in the mission file, with no part numbers to
  convert `guess` (test 29 would show it).

## 3. Mission code (code; tool default)

Counts over the 49 files (slots 0–49 without 16/29/39, plus 60 and 70), code found by walking from each module's
pointer table:

| Reference | Sites | What to do |
|---|---|---|
| inside the module | 17,866 (2,386 in code, 15,480 pointer words in data) | move by PP base − AC1 base (`0x80097A38 − 0x801C4B40`); `lui`/`addiu` pairs patched together |
| calls to engine and exe routines | 4,036 sites, **117 routines** | PP twin: **51 high** (6 of them fixed by hand), **46 medium**, **20 low** (`--list`) |
| data addresses | 2,651 sites, **188 addresses** | record arrays (1,144 sites), player block rules (535), globals (high 328, medium 319, low 219), engine tables by hand |
| left undecided | **8 sites, 3 addresses** (`0x8008DCC4` … `0x8008DCC8`, slots 20 and 42, inside the engine's data near `0x8008DCA0` = PP `0x8008C380`) | read by hand |
| AC-object fields that moved | **0** | — |

**Pairs fixed by hand** (functions.tsv had none, or a wrong one; the tool's `FIX` table):

| AC1 | PP | What |
|---|---|---|
| `0x80073674` (101 calls) | `0x8007141C` | allocate *n* bytes from the section heap |
| `0x80073170` (32) | `0x80070F10` | rebuild the event-point touch list |
| `0x800132CC` (97) | `0x80013D28` | turn a 12-bit angle toward another by a step (the tsv pointed into the middle, PP `0x80013D70`: the old import's bug) |
| `0x80014AE4` (11) | `0x800154D8` | vector → angles (tsv paired it with `0x800157F0`, wrong) |
| `0x80014DFC` (6) | `0x800157F0` | weighted random pick (count, byte table, stride) |
| `0x80014D30` | `0x80015730` | random × n ÷ 32,768 |
| `0x8002A400` … (BIOS stubs) | `0x8002CB90` … | same stub order: AC1 `0x8002A4A0` rand (325 calls) = PP `0x8002CC30` |

Also by hand: the effect-record tables the shared mission helper passes to the engine, AC1 `0x8008E530`,
`0x8008E610`, `0x8008E658`, `0x8008FB58`, `0x8008FC48`, `0x8008FEE8` = PP `0x8008D42C`, `0x8008D528`,
`0x8008D580`, `0x8008EA90`, `0x8008EB80`, `0x8008EE20` (same order; the first two are longer on PP: 9 records
vs 8, 0x58 vs 0x48 bytes) `code`.

**Player block from mission code** (rule in the tool, from `records.md` §2–3 and this topic): offsets up to
+0x245 are the same; **PP's 6 extra bytes sit exactly at +0x246** (AC1 +0x242 = PP +0x242, AC1 +0x246 = PP
+0x24C), which pins the open item of `records.md` §2. 25 missions copy two 16-byte records to +0x226 and +0x236
(same offsets on PP).

**Still to check by eye**: the 20 low pairs; four have different sizes on the two sides and may start at the
wrong place (AC1 `0x8004EBF8`, `0x8008A798`, `0x8008A778`, `0x80074280`); 6 globals at low confidence
(`0x801A5DC0` / `C4` 155 sites, consistent with the +0x40728 block; `0x801987FC` 1 site). The checker rule from
the MoA plan applies as it is: every call must land on a routine start (the tool checks this; 3 targets without
a frame are library stubs, fine).

## 4. Shared data: messages, sounds, models, textures (data; tool `--common`)

- **Messages:** AC1 has **200** lines per COM-voice set, PP **250**. **PP line *n* + 50 = AC1 line *n*** for
  the engine's own lines (151/155/171/195 = AC1 101/105/121/145) and 93–98 of AC1's 200 lines are byte-identical
  there; same header and same text codes (no conversion like MoA's `F0 81`). So: a message table for AC1
  missions = 50 filler lines + AC1's 200 lines, and **mission code needs no message changes**.
- **Sound ids:** ids 0–55 are the same records on both; **137 of ids 56–255 differ** (AC1 points them at map
  bank slots 3–53 and at other fight-bank tones; PP at slots 0–60). So AC1 missions need **AC1's sound-id table**
  (FDAT 200 block 3) in their common file. The fight bank itself is a different recording set (no sample bytes
  shared, `content.md` §6): decision §9.
- **Shared models** (FDAT 200 block 0, slots 0x30 … 0x3D): same 14 models, same sizes; 13 identical or 2–4
  bytes apart, one (slot 0x3C) 660 bytes apart; PP adds slot 0x3E. PP's common models can serve AC1 missions.
- **Textures:** AC1 RTIM 100 and PP RTIM 60 hold the same 33 pictures at the same screen-memory places (31
  byte-identical; two 64 × 256 pages at x 832 and 896 differ). **Every AC1 mission picture set (RTIM *n*) overlaps
  PP 60 exactly where it overlaps AC1 100**, so AC1 RTIM *n* loads as it is. Whether the two differing pages
  matter: test 28.
- **XA audio:** same file list in the same order on both exes (BGM00–01, TALK00–05, CPU00–23), so XA ids mean
  the same file and channel. AC1's TALK and BGM files have to be the ones open while an AC1 mission runs.

## 5. The story side, in AC1's menu (code, FDAT 201)

None of this is in the mission files; it is AC1 menu code that PP's menu does differently or not at all.

| What | AC1 | Status |
|---|---|---|
| **Gift after slot 2** "Remove Gun Emplacement" (reward 0 credits) | after the results, if the slot is 2: owned count = 1 for **LN-SSVT, SP-SAP, SP-AXL** (`0x800869BC` …) | code |
| **"New Parts Added" (mails 0 and 1)** | after a mission, chapter (T+2) ≥ **11**: shop stock = 1 for **LN-3001, LN-D-8000R, LFH-X5X, LC-MOS4545** if not owned; ≥ **21**: **WM-L201, WC-LN350, WC-XC8000** (`0x80086BF4` …) | code |
| Hidden parts | the 11 of §2; the menu checks the same 11 indices at `0x80052690` … | code |
| | 3 + 4 + 3 + 11 = the **21 parts not on sale at the start** (`content.md` §3), all accounted for | |
| **Score T+0x28** (mail 18 "Above Ground", kind 3 = score below 0) | results code `0x80086AD8` …: the score goes **up by a per-mission signed byte (T+5)** on one outcome and **down by it** on the other; the counter T+0x34 (cap 999) goes up by 1 | code (which outcome is which `guess`) |
| **Movies** | table **`0x800BDC68`** (9 × {name, frame count}): 0 ACOPA, 1 DEMOPLAY, 2 ACOPB, 3 ACED1, 4 ACED2, 5 ACED3, 6 ACED4, 7 ACED5, 8 STAFF; player **`0x800A448C`(index, …)** | code |
| | ACED5 after a won mission 21, 22, 47 or 48 (`0x8009BB5C`; result word `0x8004A610` = 2) | code |
| | ACED1 / ACED2 picked by flags of slots 21 / 22 (saved mission bytes `0x80041B5D` / `0x80041B5E`, bit 2) (`0x8009C660` …) | code (meaning `guess`) |
| | ACED4 at the end of `0x8009C7xx` (an ending chosen by `0x80039D14` & 0xF) | code (when `guess`) |
| | ACED3 then STAFF (`0x8004FFF4` …): the final ending and credits | code (when `guess`) |
| Boot loop after a fight | missions 21/22/49 and 47/48 with result `0x8004A610` decide whether the menu restarts at once | code (`0x80012538`) |

## 6. Parts, saves and builds (code, PP FDAT 201)

**PP's LOAD already imports AC1 saves**: the AC1 path (`0x8006750C` …, checksum PP `0x80065EA4` over 3,044 bytes
from +0x230) copies the equipped parts and **converts them**: arms 8–15 → +4; back weapons 12–16 → +5, 17–23 →
+10, 24–30 → +15, 31+ → +20 (exactly the rule of `content.md` §3), then rebuilds owned counts and shop stock
category by category with tables at PP `0x800B50AC` (AC1 base − PP base: 0, −8, −16, −24, −32, −40, −48, −56,
−64, −89, −94), `0x800B50C4` (AC1 counts per category: 10, 3, 16, 32, 7, 8, 11, 6, 34, 16, 4) and special group
tables for arms and back weapons (`0x800B50DC`, `0x800B50E8`). It also adds **100,000 credits** (cap
99,999,999), sets the arena rank record to 50, and copies AC1's mission record (+0xC58) into PP's, with T+4 = 1.
So **nothing to write for AC1 saves or builds**; AC1 parts need no import. Test 27.

## 7. Stages (data)

Nothing to import (`content.md` §4). AC1's VS / link stage files would only matter for "classic" versions of
PP stages 20–24 / 30–34; they are AC1 engine builds like the missions, so the same method would apply. Not planned.

## 8. What has to change, piece by piece (and where it meets the MoA plan)

| Piece | Work | Kind |
|---|---|---|
| AC1 mission code (FDAT 2*n*) | relocate, retarget calls and data with the tables of §3; **no field work**; checker refuses undecided sites | converter on the player's PC |
| AC1 map (FDAT 2*n* + 1) | pad block 10 to 1,024; hidden-part index in block 9 (§2) | data |
| AC1 pictures (RTIM *n*) | none | copy |
| Common file for AC1 missions | PP FDAT 200 + AC1 message sets (+50) + AC1 sound-id table (and maybe AC1's bank, §9) | data |
| Briefings, mails | AC1 `MIS.T` pages / mails, numbered after MoA's in our pack | data |
| **File-load redirect** | the MoA plan's hook on PP `0x800171B4`, keyed by **mission source** (PP / MoA / AC1), plus the XA file open for TALK / BGM while an AC1 mission runs | engine hook (shared) |
| Engine numbers | **AC1 slot + 50** (slots 0–49 → 50–99, AC Battle 60 → 110) keeps clear of PP's specials 10, 19, 48 and of MoA's 20–39 | data |
| Per-mission u16 | numbers ≥ 40 read past PP's 40-entry table: one small hook on the read (PP `0x800810A4`) | engine hook |
| **Mission chain** | AC1 needs 98 → 99; the MoA plan wants to patch PP's 48 → 49 into 38 → 39. **Both can't patch the same two immediates**: use one small chain-table hook at PP `0x8004AA48` | engine hook (shared) |
| AC1 ending missions | AC1 suppresses the "defenses damaged" line and results for 21/22/47–49; PP doesn't | hook or accept (§9) |
| Story rules | §5: gifts, chapter parts (PP indices), score, endings, movies, chapter windows | menu work |
| Slots 0 (Dummy) and 70 (garage test) | skip | — |

Consistency with `missions-import-pp.md`: same converter shape, same checker rule, same file-load hook, same
"pack from the player's own disc" rule (`docs/no-game-code.md`). Differences: AC1 needs no object-field pass and
no message-code conversion; the chain needs a table instead of the two-immediate patch.

Pilot: **Stop Terrorist Threat (slot 1)**: 21 KB, no hidden part, 1 event point, no story rules.

## 9. Decisions for the owner

1. **Mission list:** AC1 missions in their own list (mission source picker: Phantasma / Master of Arena /
   Armored Core), or mixed in? AC1's list is already in PP's format (64 slots, its own chapter windows), so its
   own list is the natural fit; mixing needs new windows for 49 missions.
2. **Sound for AC1 missions:** AC1's whole fight bank and sound table (AC1 sound throughout, including your own
   guns and boosters), or PP's bank with AC1's sound table (PP's gun sounds, AC1's mission sounds; small risk on 4
   ids, 122–125)?
3. **Ending missions:** rebuild AC1's endings (movies ACED1–5 and STAFF, needs the movie packs at build time) or
   leave the last missions as ordinary missions first?
4. **Progress:** keep AC1 story progress apart from Phantasma's (needs space in the save's unused 4 KB)?

## 10. Open

- The 20 low pairs and 4 size-mismatched pairs (§3), 3 undecided addresses in slots 20 / 42.
- Meaning of the per-mission u16 (AC1 24 for missions 8 and 11) and of the 2 differing RTIM 100 / PP 60 pages.
- Which outcome raises or lowers the score; which conditions pick ACED1 / ACED2 / ACED4; where T+5 is set.
- Fight music: whether AC1's BGM songs equal PP's no longer matters for the import (AC1 missions take AC1's file);
  the `ac1-music-bgm00/01` packs are not needed for this topic.

## How we know

- Engine: start-up `0x8004F508` read beside PP `0x8004DAD4`; loaders listed in §1a; main loop `0x8004C340`
  (messages 0x69 / 0x79 / 0x91, chain at `0x8004C6DC`) beside PP `0x8004A8F0` … `0x8004AA64`; results
  `0x8008AF2C`; per-mission table read `0x800835A8`.
- Data: `--maps` over 49 maps, `--common` (FDAT 200 blocks 0, 3, 4–6), RTIM TIM rectangles (AC1 100, PP 60, AC1 1–70).
- Menu: owned/stock writes with constant indices in FDAT 201; movie names at FDAT 201 +0x23E4, table `0x800BDC68`,
  callers of `0x800A448C`; PP LOAD `0x80067500` …

## RAM rows added (AC1)

`ac1_mission_point_classes` `0x80198808`, `ac1_mission_event_points` `0x80198B08`, `ac1_mission_touch_list`
`0x8019F308`, `ac1_mission_touch_count` `0x8019F508`, `ac1_mission_object_types` `0x8019F538`,
`ac1_mission_placements` `0x8019FAB8`, `ac1_mission_objects` `0x801A26B8`, `ac1_mission_active_sections`
`0x801A5DD8`, `ac1_mission_view_table` `0x801AD82C`, `ac1_mission_timer_end` `0x801D0B44`, `ac1_mission_sections`
`0x801D0B68`, `ac1_mission_section_heap` `0x801D8B68`, `ac1_mission_heap_pointer` `0x801E8B68`,
`ac1_mission_messages` `0x801ED850`, `ac1_mission_effect_table` `0x8008E530`, `ac1_mission_number_table`
`0x800906CC`, `ac1_com_voice` `0x8004121C`, `ac1_fight_result` `0x8004A610`, `ac1_movie_table` `0x800BDC68`.

## Tests waiting (also in `../mapping/ROUND1.md`)

- **A10** (#26) Chain: in AC1, win "Destroy Floating Mines" → "Destruction of Ravens" starts without the menu;
  `0x8019F518` goes 48 → 49.
- **A11** (#27) PP import: load an AC1 save in PP's LOAD screen; the garage shows the same AC (e.g. an AC with
  back weapon WC-XP4000, AC1 id 21, shows it on PP as id 31) and credits are AC1's + 100,000.
- **A12** (#28) Hidden part and pictures: in AC1 "Secret Factory Recon" pick up the item → `0x80031AF3` (owned
  index 95, WM-S60/4) becomes 1. While flying, note anything drawn from screen-memory pages x 832–959, y 256–511.
- **A13** (#29) Gift: in AC1, after "Remove Gun Emplacement" `0x80031AB2`, `0x80031AE4`, `0x80031AE6` are 1
  (LN-SSVT, SP-SAP, SP-AXL owned); in a mission with an enemy AC, `0x8004133C` (player block 2) stays unused.

## Handed over

- To the round 2 mapping chat (`missions-import-pp.md`): the 48 → 49 chain is AC1's and is needed by AC1
  missions, so a chain table at PP `0x8004AA48` serves both imports better than patching the two immediates (§8);
  PP's 6 extra player-block bytes are at +0x246 (from AC1 +0x246 = PP +0x24C).

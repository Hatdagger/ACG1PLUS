# AC1 topic 3 — Content inventory: what AC1 has that Phantasma doesn't

AC1 mapping chat, topic 3 (`../mapping/PLAN-AC1.md`), 2026-11-26. Discs: **AC1** = Armored Core USA (SCUS-94182),
**PP** = Project Phantasma USA (SLUS-00670). Static read of the owner's work packs; status per finding: `data`
(read from the files), `code` (read from code), `guess`. Every address is AC1 unless tagged PP; addresses in
`0x8004ADA0`… are in a module (FDAT 201 = menu unless the row says otherwise), "FDAT 201 +x" = offset in that entry.
Tool: `tools/xdisc/ac1_content.py` (prints the comparisons; rerun it, never commit its output). Tests in §10.

## In plain words

- **Missions: all of AC1's are new to PP.** 47 story missions + the "AC Battle" mission + the garage test, about
  23 MB of mission files, 480 briefing pages, 23 mails (PP: 17 missions, 14 mails). None of them share a name or a
  map with PP's missions; a few reuse bits of the VS stages.
- **Parts: AC1 has nothing PP doesn't.** Every AC1 part is on PP with **exactly the same numbers**. The only
  difference is a label: six weapons are called "laser" on AC1 and "plasma" on PP (or the other way round). PP
  added 22 parts. Catch: two lists are numbered differently (weapon arms and back weapons), so AC1 part numbers
  need converting.
- **VS and link stages are early versions of PP's stages.** AC1's 5 VS stages are PP stages 20–24, its 5 link
  stages PP 30–34, each 36–95% the same bytes. Nothing new to import there.
- **Body kits (PA files): almost identical.** Same 72 files; AC1 just has fewer slots. The few changed pieces are
  small edits (one cannon model, three leg models, nine bodies). One leftover: a model for a leg that AC1 never
  sells (leg slot 12, which PP uses for LN-2KZ-SP).
- **Sound: the mission voices are new, the rest is close.** The 6 mission radio files (TALK) are AC1's own. The
  AC computer voice is the same as PP's except one of the three voice types (partly re-recorded). The fight
  sound bank is a different recording set. Fight music: same file sizes as PP, not compared (packs not here).
- **Movies: all 7 are AC1's** (two openings, five endings, the demo and the credits; PP's are different files).
- **Menus: AC1 has a Raven ranking screen PP lacks** (25 profile texts and 16 portraits). Title and menu art
  is AC1's own; paint sets and emblem sample sheet 2 are the same as PP's.

So the **AC1 import list** for 0.8.5–0.8.7 is: the 49 mission slots (code, maps, pictures, briefings, mission
music choices), the 23 mails, the TALK voices, the CPU1x voice type, the movies, the ranking screen. Parts and
stages need no import (PP has them), only an id translation for AC1 saves and builds.

## 1. Missions (data)

From `MIS.T` 0 (PP's format, `records.md` §5; tool `missions`). Window = chapters the mission is offered in.
Pages = `MIS.T` entries (pictures / texts); every slot *s* uses entries 16*s* + 1 … 16*s* + 16, the same
16-per-slot scheme as PP. Music = XA id (BGM00 = 0–15, BGM01 = 16–31, `../mapping/scenario.md` §7; 255 = none).

| Slot | Name | Window | Advance | Reward | Music |
|---|---|---|---|---|---|
| 0 | Dummy00 (placeholder, no window; map ≈ PP garage-test stage 19) | — | 0 | 200,000 | 0 |
| 1 | Stop Terrorist Threat | 7–11 | 0 | 22,000 | — |
| 2 | Remove Gun Emplacement | 6–10 | 0 | 0 | 18 |
| 3 | Rescue Survey Team | 9–13 | 0 | 25,000 | 5 |
| 4 | Terrorist Pursuit | 10–11 | 0 | 23,000 | 22 |
| 5 | Worker Robot Removal | 7–9 | 7,000 | 10,000 | — |
| 6 | Secret Factory Recon | 16 | 0 | 32,000 | 3 |
| 7 | Exterminate Organisms | 19–23 | 0 | 30,000 | 4 |
| 8 | Guard Freight Train | 12–14 | 12,000 | 20,000 | — |
| 9 | Destroy Fuel Depot | 11–15 | 0 | 21,000 | — |
| 10 | Prototype MT Test | 13–15 | 0 | 28,000 | 19 |
| 11 | Guard Airplane | 23–25 | 0 | 34,000 | — |
| 12 | Stop Gas Exposure | 28–31 | 20,000 | 25,000 | — |
| 13 | Prototype MT Test | 22–25 | 0 | 0 | 19 |
| 14 | Repulse Enemy Attack | 19–22 | 0 | 32,000 | — |
| 15 | Exterminate Organisms | 29–31 | 0 | 40,000 | — |
| 16 | "16 Omitted" (no window, no files) | — | | | |
| 17 | Guard Wharf Warehouse | 17–21 | 0 | 32,000 | — |
| 18 | Remove Base Occupants | 27–29 | 35,000 | 0 | 5 |
| 19 | Destroy Space Catapult | 29–31 | 0 | 35,000 | — |
| 20 | Destroy Base Generator | 33 | 20,000 | 28,000 | — |
| 21 | Mop Up Chrome Remnants | 34 | 0 | 50,000 | — |
| 22 | Destroy "Justice" | 34 | 0 | 50,000 | — |
| 23 | Chrome Uprising | 33 | 20,000 | 28,000 | 1 |
| 24 | Destroy Plus Escapee | 19–20 | 0 | 5,000 | — |
| 25 | Destroy Intruders | 29–31 | 12,000 | 30,000 | — |
| 26 | Destroy Plane Computer | 22–25 | 15,000 | 25,000 | — |
| 27 | AC Battle | 26–28 | 0 | 42,000 | — |
| 28 | Attack Urban Center | 11–14 | 0 | 0 | — |
| 29 | "29 Omitted" (no window, no files) | — | | | |
| 30 | Eliminate Squatters | 1–3 | 0 | 14,000 | 21 |
| 31 | Eliminate Squatters | 6–9 | 0 | 19,000 | — |
| 32 | Destroy Unknown MTs | 2–4 | 0 | 12,000 | — |
| 33 | Rescue Transport Truck | 4–6 | 0 | 20,000 | — |
| 34 | Eliminate Strikers | 1–3 | 0 | 13,000 | 22 |
| 35 | Stop Security MTs | 3–6 | 0 | 17,000 | 1 |
| 36 | Stop Gang, "Dark Soul" | 17–20 | 0 | 0 | 6 |
| 37 | Reclaim Oil Facility | 2–8 | 0 | 18,000 | 6 |
| 38 | Recover Capsules | 20–24 | 0 | 30,000 | 20 |
| 39 | "39 Omitted" (no window, no files) | — | | | |
| 40 | Guard Factory Entrance | 16 | 0 | 32,000 | — |
| 41 | Capture Space Station | 32 | 0 | 45,000 | — |
| 42 | Release Organisms | 26–28 | 0 | 32,000 | — |
| 43 | Retake Air Cleaner | 32 | 0 | 46,000 | — |
| 44 | Kill "Struggle" Leader | 26–29 | 18,000 | 20,000 | 0 |
| 45 | Stop Security MT | 17–21 | 0 | 38,000 | — |
| 46 | Destroy Base Computer | 23–25 | 13,000 | 25,000 | 5 |
| 47 | Mop Up Chrome Remnants | 35 | 0 | 48,000 | — |
| 48 | Destroy Floating Mines | 36 | 0 | 50,000 | 16 |
| 49 | Destruction of Ravens (no window: reached another way `guess`) | — | 0 | 0 | — |
| 50 | AC Battle (runs **engine mission 60**) | 26–28 | 0 | 42,000 | 2 |
| 51–63 | Japanese "dummy 51…63" placeholders, no window | | | | |

Engine mission number = slot for 0–49 (`MIS.T` 0 +0xB00). Two "AC Battle" slots (27 and 50) share window and
reward but run different maps (27 / 60) `data`. Advance pay is non-zero on 11 AC1 missions (0 on every PP one).

**Files per mission** (`foundations.md` §4): code FDAT 2*n*, map FDAT 2*n* + 1, pictures RTIM *n*. Code 6–43 KB,
maps 188–897 KB; slots 0–49 + 60 + 70 = **23.4 MB**. Closest PP stage per map (tool `stages`, share of 256-byte
pieces): most below 25% (own maps). Higher: slot 0 → PP 19 (73%), slot 32 → PP 21 (45%) `data`.

**Engine slot 70 = AC1's garage TEST** (code): the garage test routine (FDAT 201 `0x8007145C`, PP twin
`0x80070BC8`, pair `low` but same shape) writes **70** to the mission number `0x800411F8` where PP writes 19 (its
test map), then music 255 and the compile `0x8009C964`. Its map is 72% PP stage 19.

**Briefings**: 152 picture pages + 328 text pages (`MIS.T` 1 … 816; text with `@`/`%` colour codes, `>` end).
PP keeps the same 16-per-slot layout but fills only slots 0–16.

**Ending checks** (`records.md` §6): engine missions 21/22, 47/48 and 49 are the story's last missions; slot 49
"Destruction of Ravens" has no window, so it is started some other way `guess` (topic 4).

## 2. Mail (data)

`MIS.T` 818 in PP's format (`../mapping/scenario.md` §5b); pages = `MIS.T` 819 + n (819 … 882). **23 mails, 17
of them with pictures** (PP: 14 mails, no pictures). Kinds as on PP §5c: 0 = chapter ≥ value, 1 = slot completed,
2 = slot completed or failed (PP's exception for value 6 would hit AC1's slot 6, "Factory Assault Report" is
kind 1 there, so harmless), 3 = score below 0.

| # | Kind, value | Subject | # | Kind, value | Subject |
|---|---|---|---|---|---|
| 0 | 0, 11 | New Parts Added | 12 | 1, 8 | The Red AC |
| 1 | 0, 21 | New Parts Added | 13 | 1, 6 | Factory Assault Report |
| 2 | 0, 14 | Human Plus | 14 | 1, 40 | Factory Affair Report |
| 3 | 0, 0 | To All New Ravens | 15 | 1, 7 | Giant Organisms |
| 4 | 1, 37 | Reclaiming the Facility | 16 | 2, 24 | Plus |
| 5 | 2, 37 | Struggle | 17 | 1, 38 | Relics of the Past |
| 6 | 1, 31 | Terrorists in the Shadows | 18 | **3**, 44 | Above Ground |
| 7 | 2, 31 | Chrome and Murakumo | 19 | 1, 44 | Struggle's Demise |
| 8 | 1, 2 | Thanks | 20 | 1, 18 | ??? |
| 9 | 1, 28 | Ranking Raven | 21 | 1, 42 | Biological Weapons |
| 10 | 2, 3 | EERC | 22 | 1, 43 | Imminent Storm's Demise |
| 11 | 2, 9 | Chemical-Dyne Co. | | | |

Mail 18 is the only stock mail of kind 3 on any disc: PP's delivery routine delivers mail 18 for kind 3 (§5c,
"dormant") — on AC1 that is exactly this mail, so the "dormant" path is AC1's, kept in PP `guess`. AC1's score
(T+0x28) has to move for it: AC1's score writer not read yet (topic 4).

## 3. Parts (data)

Tables in the menu, same record sizes and fields as PP (`../mapping/parts-tables.md`), back to back (tool
`parts`). Located from the paired globals (`../xdisc/ac1-pp/globals.tsv`) and checked by name.

| Category | AC1 address (FDAT 201 +x) | Stride | AC1 slots (real) | PP slots (real) |
|---|---|---|---|---|
| head | `0x800B91F4` (+0x6E454) | 60 | 10 (10) | 18 (11) |
| core | `0x800B944C` (+0x6E6AC) | 56 | 3 (3) | 11 (4) |
| legs | `0x800B94F4` (+0x6E754) | 72 | 32 (24) | 40 (25) |
| arms | `0x800B9DF4` (+0x6F054) | 52 | 16 (16; weapon arms 8–15) | 24 (17; 12–20) |
| hand weapon | `0x800BA134` (+0x6F394) | 44 | 16 (16, 0 = NO WEAPON) | 21 |
| back weapon | `0x800BA3F4` (+0x6F654) | 52 | 34 (34) | 59 (41) |
| blade | `0x800BAADC` (+0x6FD3C) | 52 | 4 | 9 (4) |
| booster | `0x800BABAC` (+0x6FE0C) | 48 | 6 | 14 (7) |
| FCS | `0x800BACCC` (+0x6FF2C) | 96 | 8 | 16 (10) |
| generator | `0x800BAFCC` (+0x7022C) | 48 | 7 | 15 (9) |
| option | `0x800BB11C` (+0x7037C) | 44 | 11 | 19 (12) |
| weapon detail: weapon arms (arms id − 8), hand, back | `0x800BB300` (+0x70560) | 24 | 8 + 16 + 34 | 16 + 21 + 59 |
| weapon type names (28 × 16, same text as PP) | `0x800BD338` (+0x72598) | 16 | 28 | 28 (PP `0x800BCB18`) |

**AC1 parts = a subset of PP's: 139 parts, all on PP, every stat byte the same.** Only differences: the type
text and the weapon type index (detail +7) of six weapons:

| Part | AC1 label (detail +7) | PP label |
|---|---|---|
| AW-XC5500 (weapon arms) | PLASMA CANNON (5) | LASER CANNON (4) |
| AW-XC65 | LASER CANNON (4) | PLASMA CANNON (5) |
| WC-XC8000 (back) | LASER CANNON (4) | PLASMA CANNON (5) |
| WC-01QL | PLASMA CANNON (5) | LASER CANNON (4) |
| WG-XC4 (hand) | LASER RIFLE (27) | PLASMA RIFLE (12) |
| WG-1-KARASAWA | LASER RIFLE (27) | PLASMA RIFLE (12) |

(MoA later went back to AC1's KARASAWA label, `../mapping/content-data.md` §1c.) So AC1 parts are PP's numbers,
not MoA's: e.g. AC1 generators have PP's lower outputs and HD-G780 doesn't exist on AC1.

**PP-only parts**: HD-G780; XXA_S0; LN-2KZ-SP; AW-DC/2; WG-RFM118, WG-XFwPPk, WG-HG1, WG-PB26, WA-Finger; WM-AT,
WM-TO100, WM-SMSS24, M118-TD, WC-SPGUN, WC-IR24, RZ-Fw2; B-HP25; FBMB-18X, RATOR; GBX-TL, GBX-XL; SP-DEhf.
That is **22 parts** (PP 161 real parts, AC1 139). Blades: none new; PP's 9 slots hold the same 4.

**Part ids AC1 → PP** (data; needed for AC1 saves, builds and mission enemy loadouts):
- head, core, legs, hand, blade, booster, FCS, generator, option: **same id**.
- arms: 0–7 same; **weapon arms 8–15 → PP 12–19**.
- back: 0–11 same; **12–16 → +5 (17–21)**; **17–23 → +10 (27–33)**; **24–30 → +15 (39–45)**; **31–33 → +20 (51–53)**.

**Owned-part index** (save counts, shop stock): bases **`0x8004CC2C`**, 10 × u16 = head 0, core 10, arms 13,
legs 29, generator 61, FCS 68, option 76, booster 87, back 93, hand 127; blade follows at 143 (no 11th entry:
the table is followed by text) `data`. Same category order as PP (PP `0x800B5094`: 0, 18, 29, 53, 93, 108, 124,
143, 157, 216, 237) but other bases, so **every AC1 index past the heads differs from PP's**. 147 of the 160
entries used (139 parts + 8 `DUMMY` legs), 13 spare.

**New game** (code: copied by FDAT 201 `0x8005AD34` … into the owned counts `0x80031A94` and the shop stock
`0x80031B34`; tool `stock`):
- owned at start (`0x800B70D4`, 160 B): 11 parts, the starting AC — HD-GRY-NX, XCA-00, AN-201, LN-1001-PX-0,
  GPS-VVA, COMDEX-C7, B-P320, WM-S40/1, RXA-01WE, WG-RF35, LS-2001.
- shop stock at start (`0x800B7174`, 160 B): 85 parts at 1, 23 back weapons at 2, 31 at 0.
- 21 parts (plus the NO WEAPON entry) are neither owned nor in the shop at the start (HD-X1487, LN-SSVT, LN-3001, LN-D-8000R, LFH-X5X,
  LC-MOS4545, GBG-XR, QX-AF, SP-SAP, SP-AXL, B-T001, WM-S60/4, WM-L201, WR-L24, WC-LN350, WC-XC8000, RZT-333,
  WX-S800-GF, WG-FG99, WG-1-KARASAWA, LS-99-MOONLIGHT): the two "New Parts Added" mails
  (chapters 11 and 21) and hidden finds in missions `guess` (which part comes from where: topic 4).

**Part descriptions = `MIS.T` 817** (same as PP; 161 lines AC1, 247 PP; 153 AC1 lines also on PP) `data`.

**Part textures and garage models** (data, tool `archives`): every entry of HEAD_T, CORE_T, LEG_T, BWR_T, WEL_T,
WER_T, SPEC_T, GENE_T, COMP_T, BST_T is on PP; ARMS_T has one AC1-only entry (151), BWL_T one (11). MENU_TMD (garage
models): all but entry 0 are on PP.

## 4. Stages (data)

| AC1 slot | Engine | Files | Closest PP stage (share) | RTIM |
|---|---|---|---|---|
| 80 | VS | FDAT 160 / 161 (98 KB) | **20** (84%) | own |
| 81 | VS | 162 / 163 (281 KB) | **21** (66%) | own |
| 82 | VS | 164 / 165 (98 KB) | **22** (72%) | own |
| 83 | VS | 166 / 167 (193 KB) | **23** (95%) | own |
| 84 | VS | 168 / 169 (104 KB) | **24** (84%) | own |
| 90 | link | 180 / 181 (203 KB) | **30** (36%) | = PP RTIM 30 |
| 91 | link | 182 / 183 (279 KB) | **31** (67%) | = PP RTIM 31 |
| 92 | link | 184 / 185 (199 KB) | **32** (74%) | = PP RTIM 32 |
| 93 | link | 186 / 187 (168 KB) | **33** (48%) | = PP RTIM 33 |
| 94 | link | 188 / 189 (242 KB) | **34** (68%) | = PP RTIM 34 |

So AC1's 10 fight stages are the first versions of PP stages 20–24 and 30–34 (PP is a bit bigger each time:
PP added to them). **No AC1 stage is missing from PP.** Names: the VS STAGE SELECT shows pictures, not text, so
names wait for test A2 (ROUND1 #14). Stage data and code files are AC1's own builds (call AC1 engine addresses).

## 5. Body kits, `GG/P0`–`P3/PA00`–`PA71.T` (data)

Same 72 files, same body table idea; **176 entries** (PP 268), entry = base + part id (found by matching bytes
to PP entries, tool `pa`; the AC1 loader code not read yet):

| Entries (AC1) | What | PP entries |
|---|---|---|
| 0, 1 | body, body record | 0, 1 |
| 2 + core | core | 2 + core |
| 10 + legs | legs (32 ids) | 15 + legs |
| 44 + head | head | 57 + head |
| 60 + booster | booster | 81 + booster |
| 70 + hand | hand weapon | 99 + hand |
| 88 + arms | arms (weapon arms 8–15 = 96–103) | 122 + PP arms id |
| 108 + back | back weapon, left | 150 + PP back id |
| 142 + back | back weapon, right | 209 + PP back id |

**AC1 entries not on PP** (all small edits of a PP piece):
- body (entry 0) of sets 1, 5, 9, 26, 30, 33, 69, 70, 71 (3–156 bytes differ; sets 9 and 30 ~23 KB moved by 4 bytes:
  an animation length changed `guess`).
- WC-XP4000 (back 21) left and right in all 72 sets (10 bytes).
- legs LB-4401 (16) and LF-205-SF (21) in their 12 sets (4 bytes each).
- **leg id 12**: AC1's table says `DUMMY`, yet 12 sets carry a 20 KB model for it; PP put LN-2KZ-SP (24 KB, a
  different model) in that id. So AC1 has a cut light leg model `guess`.
Everything else (≈ 98% of entries) is byte-identical to PP's same-number file.

## 6. Sound (data)

| What | AC1 vs PP | |
|---|---|---|
| `GG/BGM/BGM00`, `BGM01.XA` (fight music) | same file sizes (46.4 / 42.9 MB); not compared (packs on the owner's PC). Missions use ids 0–6, 16, 18–22 | open |
| `GG/BGM/TALK00–05.XA` (mission radio) | **all AC1's own** (no sector in common); TALK05 is almost empty (10 distinct sectors) | |
| `GG/BGM/CPU0x`, `CPU2x` (computer voice types 0 and 2) | **identical** to PP | |
| `GG/BGM/CPU1x` (voice type 1) | 67–88% of distinct sectors also on PP: partly re-recorded on PP | |
| `MENU_VAB.T` (menu sounds, menu song) | identical (5 of 5) | `foundations.md` §5 |
| FDAT 200 fight bank | AC1 3 programs / 48 tones / 37 samples (PP 4 / 51 / 38); **no sample bytes in common** (re-encoded or new) | |
| FDAT 200 sound-id table | 119 of 256 id records the same as PP | |
| FDAT 200 message blocks 4–6 | AC1's own (smaller) | |

So an AC1 mission on PP's engine needs AC1's TALK files, and its sound ids checked against PP's bank (topic 4).

## 7. Movies (data, from `disc-toc.tsv`; packs not here)

| AC1 file | Size | PP |
|---|---|---|
| ACOPA.STR, ACOPB.STR (openings) | 20.0 / 17.8 MB | PP has one ACOPA (14.6 MB, different) |
| ACED1–ACED5.STR (endings) | 12.9 / 13.5 / 14.5 / 10.2 / 4.0 MB | PP has one ACED5 (14.6 MB, different) |
| DEMOPLAY.STR, STAFF.STR | 33.2 / 33.8 MB | PP's differ in size |
| — | | PP's ASLOGO.STR |

All seven AC1 movies are AC1's own `data` (sizes). Which ending plays when: the boot loop checks (`records.md` §6),
topic 4.

## 8. Menus, emblems, pictures (data)

`MENU_TIM.T` (122 entries; tool `archives`):

| AC1 entries | What | PP |
|---|---|---|
| 0–8 | title, menu art, full-screen pictures (0, 2, 3, 8 differ on the rerelease) | own |
| 9 | emblem sample sheet (4 × 4 emblems) | = PP 9 |
| 10 | 256 × 256 sheet (PP's 8 is the other emblem sheet `guess`) | own |
| **11–26** | **16 portraits, 120 × 128 (one 128 × 144), the Raven ranking** `guess` | none |
| **27–51** | **25 Raven profile texts** ("The No. 1 Raven…"; some repeat) | none |
| 52–61 | 100 × 100 pictures | = PP 62–66, 52–56 |
| 62–121 | 60 colour sets (512 × 3 palettes; paint schemes `guess`) | = PP 72–131 |

`RTIM.T`: 0–49 mission pictures (all own), 60 (AC Battle), 70 (garage test), 80–84 VS (own), **90–94 = PP 30–34**,
100 fight-common, 101 boot picture, **102** a 320 × 240 16-bit picture, **103** an 88 × 16 strip (neither loaded
by a constant-entry call; the missions engine loads RTIM with a register entry, not read) `open`.

## 9. RAM rows

Added to `../ram-map.toml` (`disc = "AC1"`, src `ac1/content.md`): the 11 part tables, the weapon detail tables,
weapon type names, owned-part bases, new-game owned counts and shop stock.

## 10. Tests waiting (ROUND1.md #23–25)

- **A7** Garage test: in AC1, Garage → TEST → yes; during the fight `0x800411F8` = 70 (and `0x8019F518` = 70).
- **A8** New game: right after NEW GAME, the 160 bytes at `0x80031B34` (shop stock) equal those at `0x800B7174`,
  and `0x80031A94` (owned counts) has 1 at index 0 (HD-GRY-NX) and 10 (XCA-00).
- **A9** Mail with pictures: after "Exterminate Organisms" (slot 7), the mail "Giant Organisms" opens with 2 pictures.

## 11. Open

- Fight music: whether AC1's BGM00/01 songs are PP's (needs `ac1-music-bgm00/01` + `pp-music-bgm00/01`; only
  worth it for topic 4 if AC1 missions should keep their own music).
- VS / link stage names (test A2); RTIM 102 / 103; MENU_TIM 0–10 roles in detail; MIS.T 818 / ranking screen code.
- Which parts the two "New Parts Added" mails and hidden mission finds give (topic 4).
- How slot 49 "Destruction of Ravens" is reached; what moves AC1's score for mail 18.
- The PA loader bases above are from matching bytes, not from AC1's loader code.

## Handed over
- To the round 2 mapping chat (Lane 5 / content): PP's six laser/plasma labels are swapped relative to AC1
  (§3); PP's kind-3 mail path delivers mail 18, which is AC1's "Above Ground" (§2).

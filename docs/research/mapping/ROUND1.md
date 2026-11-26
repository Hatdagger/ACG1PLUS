# Mapping round 1 — results (2026-11-26)

All 8 lanes of `PLAN.md` finished the same day and were merged into `main` (PRs #1–#8). Everything is a
**static read** of the owner's own discs (Project Phantasma USA = PP, Master of Arena USA = MoA). Nothing was
seen running on PP yet. Lane write-ups: project docs `claude/mapping-<lane>.md`; full notes in this folder.

## Where we stand, in plain words

- **We can find our way around Phantasma.** The disc layout, every code module, where each one loads, and a
  table pairing 4,199 MoA routines with their PP twins at high confidence (521 medium) are in
  `docs/research/xdisc/`. Old MoA research can now be carried over by lookup instead of by hand.
- **The main records are laid out on PP**: the two player blocks, the AC objects, the object pool (bullets,
  missiles), the compiled stat block, the save block, the mission table, the mail block and the hub.
  The rule for moved fields is known (AC object −8/−9 bytes, player block +8, one missing byte in the stat block).
- **How every mode starts is written down** (title, load, new game, hub, VS, arena, missions, test fight, link),
  with the list of what a launcher direct start must replay (`boot-loads.md` PP-12).
- **Why Phantasma feels different is explained from code**: hit stun (almost every hit is a 1-frame flinch on PP
  because PP's stability is ~6× MoA's), weaker generators, smaller lock boxes on rifles, a stronger interceptor,
  locks kept after a blade swing. Kaze lives in animation data (PA11/13/27); MoA replaced those files.
- **Why Phantasma's AI feels stronger is explained from code**: it fires the instant you're lined up. MoA's AI
  rolls dice to shoot, holds shots back 10–20 frames, but aims ahead and dodges. PP's AI records are 49 × 60
  bytes inside the arena module.
- **Room for MoA content**: the save has room (47 free mission slots, 50 free mail slots, 4 KB unused); the
  tight spots are the disc archives and the menu's memory, so MoA content will come from our own pack.
- **Performance**: every fight engine caps itself at about 22 fps by design, and PP never reads VRAM back during
  a fight (good for the render thread). Top suspect for slowness: disc code that isn't pre-compiled.

## Decisions made during the round (owner)
- Kaze as a setting: Original / Both sides / Off (default Original). `feel-diff.md` §8b.
- When MoA opponents come over: a settings toggle between Phantasma-style and MoA-style AI. `ai.md`.
- Import rule: keep PP's animation files, PP's numbers for shared parts, PP's hit stun and stability; add MoA's
  new parts beside them. `feel-diff.md`, `content-data.md` §5.

## Merged at the end of the round
- Lane 3's 43 RAM rows went into the shared `ram-map.toml`. 40 of those addresses were already there, carried over
  from MoA by tool. Lane 3 read them directly on PP, so its size and meaning were taken. 13 rows were renamed
  where the carried-over meaning was wrong (for example 0x800424E8 is player block 1, not a sound flag; Lane 4
  agrees). The old meaning stays in the row's text. 3 rows were new.

## Live tests waiting (none need writing code; most run in DuckStation on the real PP disc)

| # | From | Test | Needs |
|---|---|---|---|
| 1 | Lane 4 | 5 checks in VS: AP, state, energy, clock, stagger (`battle-system.md` PP test list); includes Lane 1's "AC +0xCC changes while boosting" | DuckStation memory view |
| 2 | Lane 8 | Watch the mission table T and mail block S across one mission win and one loss; load a save and check S+0x40 before and after opening Mail | DuckStation |
| 3 | Lane 3 | First boot/load check, `boot-loads.md` PP-14 | DuckStation |
| 4 | Lane 6 | Cheats `801DBA7C 0000` (no lost track) and `801DBA96 1000` (always blade lunge); count one opponent's shots in 30 s, PP vs MoA | DuckStation |
| 5 | Lane 2 | Kaze check (XCA-00 + AN-K1 + LN-1001, standing, Down + R1 without boost); hit stun with WG-RF/5 and WG-HG1; energy refill timing (`feel-diff.md` §7) | real PP, DuckStation |
| 6 | Lane 7 | Test A: 2-minute VS fight on the existing MoA build (Deck + Windows), send `psx_last_run_report.json` and FPS. Test B: Deck FPS with `PSX_RENDER_THREAD=1 %command%` vs without | old MoA build |
| 7 | Lane 7 | Test C: `host_profile.py` in VS and arena | **first PP build** |
| 8 | R2 PA | Hit size from the body file: plain VS; default AC, then XCA-00 + AN-K1 + LN-1001, then any caterpillar. Halfwords at `0x801D8030` / `0x801D8032` should read 100 / 212, 100 / 225, 112 / 150 (`pa-archives.md` §4) | DuckStation memory view |
| 9 | R2 PA | Body number: in VS, byte `0x80042769` (player 1 block + 0x281) = 0 with the default AC, 11 with XCA-00 + AN-K1 + LN-1001 (`pa-archives.md` §1) | DuckStation memory view |
| 10 | R2 stages | AI markers: arena fight on stage 23 or 29, then on stage 20. On 23 / 29 the opponent should now and then run to a ledge and boost up onto it; never on 20 (`stages.md` §3) | any build, eyes only |
| 11 | R2 screens | Garage TEST map: Garage → TEST → yes; the fight should be on mission map 19 (the test map), and byte `0x80042745` = 19 during it (`menus-screens.md` PP-2) | DuckStation memory view |
| 12 | R2 screens | Card names: LOAD DATA with an Armored Core 1 (USA) save on the card → listed; LOAD EMBLEM with an AC1 emblem file → expected *not* listed (PP's emblem list says BASCPS-94182, `menus-screens.md` PP-4) | real PP or DuckStation, an AC1 card |
| 13 | AC1 foundations | AC1 mode byte: `0x80039C5E` = 2 in a VS fight, 0 in a mission, 1 in a link fight (`../ac1/foundations.md` §3) | AC1 disc in DuckStation, memory view |
| 14 | AC1 foundations | AC1 slots: in a VS fight `0x801AB678` is 80–84, in a mission `0x8019F518` is the mission's slot; write down which stage / mission name goes with which number (`../ac1/foundations.md` §4) | AC1 disc in DuckStation, memory view |
| 15 | AC1 foundations | AC1 body number: `0x8004121B` = 0 with the starting AC and changes with core + arms + legs like PP's `0x80042769` (`../ac1/foundations.md` §4) | AC1 disc in DuckStation, memory view |
| 16 | AC1 records | AC1 AC object: in a VS fight, P1 velocity at `0x801AB748` + 0xCC changes while boosting (same place as PP; `../ac1/records.md` §1) | AC1 disc in DuckStation, memory view |
| 17 | AC1 records | AC1 stat block: in a mission `0x800411F8` = the engine mission number (1 for "Stop Terrorist Threat"); in VS P1 energy `0x80040FBE` starts at the capacity `0x80041204` (`../ac1/records.md` §3) | AC1 disc in DuckStation, memory view |
| 18 | AC1 records | AC1 save: in a real AC1 save file, the byte sum of +0x230 … +0xE13 equals the word at +0x200 (`../ac1/records.md` §4) | an AC1 save file (card image) |

## Open questions left by the lanes
- **Foundations:** menu module 201 header/entry; disc files 0 and 3; what the 72 PA archives hold (motion,
  skeleton?); seeds for the PP modules (only the boot exe has seeds); a −4 struct family in `fields.tsv`.
- **Battle:** AC +0x3A, +0x60, +0x7C; match +0x16 / +0x1A; the wreck sequence and stagger installers; lock slots,
  missile steering and energy rules were carried over from MoA, not re-read on PP.
- **Feel:** the arena engine's movement (PP 205 / MoA 210) not compared.
- **Content:** PP weapon type-name table; part model archive grouping; shop stock address (likely 0x800343F0,
  Lane 3 agrees); paint and emblem field layout.
- **AI:** stage-object kinds 0x0A / 0x14 and which stages have them; which AI record each arena rank gets.
- **Boot:** peak menu heap use (live); which side the link engine treats as "me".
- **Renderer:** stage geometry format; arena HUD and screen-edge sites.
- **Scenario:** ~~the screens behind each hub entry in depth~~ (round 2 topic 3: `menus-screens.md` §PP) (garage pages, shop, card screen, arena list, options,
  VS menu); which draw kind each menu draw routine serves.

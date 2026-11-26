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
| 8 | Build chat (0.0.x) | **Deck build from an exFAT USB drive**: the portable folder on a USB stick (exFAT, no execute permission), started on the Steam Deck. Does the game start (through `bash`, or does it need a copy on the Deck's own disk)? Do saves, settings and caches stay on the stick? (`docs/building.md`) | first Deck build (0.1) + an exFAT USB drive |
| 9 | Build chat (0.0.x) | **Load times from USB**: time boot to title, title to hub, and a VS and an arena fight load, with the folder on the USB drive vs on the internal disk (Windows and Deck) | first PP build + USB drive |

## Open questions left by the lanes
- **Build chat (0.0.x), for the stages topic:** the arena stage code pieces FDAT 107, 112, 132, 137, 157 and 187 don't fit the
  arena slot 0x800956F0: their handler tables and internal calls point near 0x801C9DD8 (static guess), and FDAT 192 holds
  almost no code. Left out of the pre-compile list (`aot/overlays.json`) for now; where do they load, and why?
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
- **Scenario:** the screens behind each hub entry in depth (garage pages, shop, card screen, arena list, options,
  VS menu); which draw kind each menu draw routine serves.

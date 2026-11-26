# Mapping plan — restart (2026-11-26)

ACG1PLUS starts over. This plan splits the mapping into **8 lanes**, one chat each, running at the same time.
A chat works only in its own lane, writes up what it found, pushes it, shares it in the Project, and ends.

**Base game (decided by the owner 2026-11-26):** Project Phantasma USA (SLUS-00670) is the base; Master of
Arena (SLUS-01030) and later Armored Core 1 are content sources to import from. The MoA Japanese disc work is
shelved. Why: competitive players prefer Phantasma's feel (movement tricks MoA lost, a stronger arena AI).
Background and the first code comparison: project doc `claude/phantasma-base.md`.

Everything in `docs/research/` (MoA, ~5,000 lines) is **reference**: the addresses are MoA's. Each lane's first
job is to find the same routines on Phantasma, then go further.

## Rules for every lane chat

1. **No game code or game data on GitHub** (`docs/no-game-code.md`). Generate C from the disc inside the chat
   sandbox to read it; never commit it. Write findings in our own words: addresses, sizes, meanings, short
   instruction snippets only when needed to identify a routine.
2. **Every address is tagged with its disc**: `PP` (Phantasma US), `MoA-US`, `MoA-JP`, `AC1`. One address
   without a disc tag is a bug in the notes.
3. **Disc patch track** (`docs/disc-patch-track.md`): beside each RAM address give the disc file + offset
   (or overlay number + offset), keep **data** findings and **code** findings in separate sections, note free space.
4. **Status per finding**: `tested` (seen in a running game), `code` (read from code), `guess`.
5. **RAM map**: add rows to `docs/research/ram-map.toml` (lives on branch `ram-map`; Lane 1 brings it to main
   with a `disc` field). Any lane may add rows; rerun `tools/ram_map.py`.
6. **Stay in your lane.** Found something for another lane? Add one line to that lane's "Handed over" list at the
   bottom of its doc. Don't edit another lane's sections.
7. **Git**: branch `mapping/<lane>`, commits `[skip ci]`, rebase on `main` before pushing, never force-push,
   open a PR at the end. Docs only: no builds started.
8. **Old MoA code** in `reference/ac1recomp/` and old hooks are reference only (CLAUDE.md working rule).
9. **Plain language** in summaries for the owner; detail belongs in the lane doc.
10. **Ending a chat**: push + PR, update the lane row in `README.md`, write a short project doc
    `claude/mapping-<lane>.md` (what was found, what's open, where it lives), then stop.

## Work packs (already in the Project, nothing to upload)

- **`pp-work.tar.xz`** (25.8 MB): everything on the Phantasma disc except fight music and movies (boot EXE
  `SLUS_006.70`, FDAT.T, RTIM.T, GG/MS, stages, voices), its tools, a disc table of contents, and
  **`maps/moa-to-pp-functions.tsv`**: 5,024 MoA routines paired with their PP twin. Extract into one folder,
  run `python3 -I pp-disc/tools/setup.py`. Extracted files stay in the sandbox, never in the repo.
- **`ma-work.tar.xz`**: MoA `FDAT.T` + `SLUS_010.30`.
- Music / movie packs exist (`pp-music-bgm00/01`, `pp-movies`, `pp-movie-demoplay`) but are only on the owner's
  PC; ask for one only if the lane really needs it.
- Lanes that need a running game (tested values) ask the owner for a short test with exact steps.

Known from the first comparison (`claude/phantasma-base.md`), so no lane redoes it:
- PP modules FDAT 201–206 = MoA FDAT 206–211 (menu, missions, ?, VS, arena, VS replay). **PP modules load at
  `0x800490D0`** (MoA `0x80050DD8`). Boot EXE loads at `0x80011000`, entry `0x80011E94`.
- **Struct fields moved**: AC object fields sit 8–9 bytes lower in PP (MoA +0xD4 = PP +0xCC), player block
  fields 8 higher (MoA +0x20C = PP +0x214). Never reuse a MoA offset without checking.
- The arena AI is different code, not different numbers.

## The 8 lanes

| # | Lane | Doc | Needs | Can start |
|---|---|---|---|---|
| 1 | Foundations: disc layouts, function catalogue, cross-disc tools | `foundations.md` | PP + MoA packs | now (others lean on it, but don't wait) |
| 2 | Phantasma vs MoA: game feel (movement, physics, timing) | `feel-diff.md` | PP + MoA | now |
| 3 | Boot, loads and mode launch | `boot-loads.md` (new PP sections) | PP | now |
| 4 | Battle system core | `battle-system.md` (new PP sections) | PP | now |
| 5 | Data and content formats (parts, stat block, builds, arenas) | `content-data.md` | PP + MoA | now |
| 6 | AI: arena opponents and AI records | `ai.md` | PP + MoA | now |
| 7 | Renderer and performance (Steam Deck) | `renderer.md` (new PP sections) + `performance.md` | PP | now |
| 8 | Scenario side: hub, menus, missions, mail, saves, sound | `scenario.md` | PP + MoA | now |

---

### Lane 1 — Foundations
**Goal:** the map of the maps. Everyone else uses this.
- Phantasma disc layout: every file, archive format (entry counts, sector starts), which archive holds the
  menu, fight engines, missions, stages, textures, sound. Side-by-side with MoA's `GG/` layout.
- Overlay table: which overlays exist on PP, where each loads, what it is (menu, VS, arena, missions, link, replay).
- Function catalogue for PP: seeds for psxrecomp (like `seeds/` for MoA), named by matching MoA routines.
- Start from `maps/moa-to-pp-functions.tsv` (routines only, static, cut at each `jr ra`). Extend it to a
  **cross-disc address table**: globals and data tables too, the struct field shifts as a list, fix the
  data-table miscounts, and give each pair a confidence. Commit the tool and the address-only table (no code).
- RAM map: bring `ram-map.toml` from branch `ram-map` to main, add a `disc` field, add PP rows.
- **Done when:** PP layout + overlay table written, catalogue/seeds for the PP boot EXE, cross-disc table for at
  least the boot EXE and the VS/arena engines, tool committed and documented.

### Lane 2 — Phantasma vs MoA: game feel
**Goal:** say in plain words what MoA changed in how an AC moves and fights, and where in the code. This is the
evidence for the base-game choice and the list of what must survive when MoA content is imported.
- Compare the movement code side by side: ground boost, jump, air boost, landing, turning, friction, energy
  drain/recovery, stagger, overheat, blade lunge. The changed routines are already listed in
  `claude/phantasma-base.md` (air/rise, energy, stat scale-in, stagger, blade swing, stepper installer, recover turn).
- Tie each trick from the owner's clips to code: **Kaze** (down + R1 + boost on one frame = full boost from
  standing; lead: MoA only ground-boosts once already moving), **strafe dancing** / lock cancelling,
  **heavy-biped high jump** (said to work in MoA too), **wall/floor clip** (lead: MoA's extra checks in the
  stepper installer). Then a test on real PP the owner can run, with exact steps.
- Compare constants and tables that shape feel: speeds, gravity, turn rates, damage factors, lock-on timing,
  missile tracking, frame/step timing.
- Output: a difference table (what, PP value/behaviour, MoA value/behaviour, address on both, status), and a
  short plain summary for the owner.
- Not in scope: AI (Lane 6), drawing (Lane 7). Field meanings belong to Lane 4; this lane compares.
- **Done when:** every movement routine is paired PP↔MoA, every difference found is listed, and the owner has a
  one-page "what MoA changed" summary.

### Lane 3 — Boot, loads and mode launch
**Goal:** know exactly what Phantasma loads and sets at each step, so the launcher can start any mode directly
and MoA content can be loaded into PP.
- Power-on → logos → title → NEW/LOAD → hub; VS setup → VS START → fight; arena entry → fight; mission → fight;
  back to the hub. For each: files read (and where they land), VRAM loads, state set, the mode byte/resume flags.
- Disc read layer: file lookup, archive reader, async loads (feeds single disc + fast loading, ladder 0.3).
- Where MoA-only data would have to be loaded (extra arenas, parts, textures) and whether PP has room.
- Old MoA notes: `boot-loads.md`, `mode-launch.md`.
- **Done when:** a step-by-step load table for every mode on PP, plus the list of what a direct start must replay.

### Lane 4 — Battle system core
**Goal:** everything that runs during a fight, on Phantasma: the meaning of each field.
- Player block and AC object (layout, size, per-player copies), match start/end, win/lose, timers, stage area
  boxes, damage routine, hit stagger, weapons in flight and the object pool, lock-on, the fire routine, energy
  and heat, collision.
- Start in VS (both units under the owner's control; plain VS with unlimited time for tests), then arena.
- Old MoA notes: `battle-system.md`, the "Handed over" lists in `README.md`.
- Border with Lane 2: Lane 4 says what fields *mean*; Lane 2 says how PP and MoA *differ*.
- Border with Lane 5: the compiled stat block's contents belong to Lane 5; Lane 4 records who reads it.
- **Done when:** AC object and player block fully laid out for PP VS, match flow and damage routine named, object
  pool kinds listed; arena engine addresses given beside them.

### Lane 5 — Data and content formats
**Goal:** the formats needed to **import MoA content into PP**: parts, builds, arenas.
- Part tables on both discs (heads, cores, arms, legs, boosters, FCS, generators, weapons, options): record
  layout, where they live (file + offset), which parts exist on each disc, and how IDs differ. Phantasma
  vs MoA part list differences.
- The compiled stat block (MoA: 324 bytes built at VS START): PP's version, every field, the compile routine.
- AC build records (64-byte builds), paint, emblems; arena entries (names, builds, prize money, AI record
  pointer); shop tables; prize tables.
- Output: an **import sheet** — for each MoA content type, the PP format it must become and any field that has
  no PP equivalent.
- Old MoA notes: `parts-tables.md`, `stat-block.md`, `arena-prizes.md`, `weapon-map.md`.
- **Done when:** every part/build/arena format is laid out for PP and MoA side by side with the conversion rules.

### Lane 6 — AI
**Goal:** understand Phantasma's arena AI (said to be stronger) well enough to keep it, feed it MoA opponents,
and later drive AI Battle and Rogue Arena opponents.
- Arena opponent AI: per-frame decision routine, state record, how it reads the AI record (the 36-byte MoA AI
  record and PP's equivalent), how it presses "virtual buttons" vs moves directly, difficulty knobs.
- PP vs MoA AI difference: same routine with different numbers, or different logic? Plain summary.
- Mission enemy AI only as far as it shares code (the rest is Lane 8).
- Old MoA notes: arena opponent AI rows in the RAM map (`0x801E2750` state, `0x801E2730` globals, MoA arena),
  `vs-ai-plan.md` and `ai-battle-handover.md` in the Project.
- **Done when:** the AI loop and its record are laid out for PP, the PP↔MoA difference is stated, and the
  owner has a plain summary of "why Phantasma's AI feels better" backed by code.

### Lane 7 — Renderer and performance (Steam Deck)
**Goal:** know how a fight frame is drawn and where the CPU time goes, so the game runs at full speed on the
Steam Deck and can later get in-between frames / widescreen without losing playability (no PGXP-style slowdown).
- Frame anatomy on PP: scene render, buffers, ordering table, flip, VSync wait, steps per frame; camera → GTE;
  draw list (stage, ACs, objects, effects, HUD).
- Hot spots: which routines take the most time in a fight (owner runs the framework's profiler on Windows and
  the Deck with exact steps); which of them could move off the main core or to the GPU
  (`cpu-offload-plan.md`).
- Old MoA notes: `renderer.md` (steps 1–7 done for MoA VS; reuse the method, re-find the addresses).
- **Done when:** PP frame anatomy + camera + draw list written for VS, and a ranked hot-spot list with an
  offload idea for each of the top few.

### Lane 8 — Scenario side: hub, menus, missions, mail, saves, sound
**Goal:** the non-fight side of Phantasma, so MoA's arenas/missions/mail can be added and the launcher can
take over menus.
- Hub ring and its entries; garage, shop, arena list, system/card screen; menu building blocks (windows, text,
  cursors, menu sounds).
- Missions: picker, mission table, per-mission code/maps, objectives, rewards, mail triggers (and how MoA's
  mission/mail lists differ in size and format — feeds the mail-folder and mission-source-picker plans).
- Save format: PP's scenario save on the memory card vs MoA's (8,192 bytes, checksum), what must change to hold
  MoA content.
- Sound: menu music (SEQ), fight music (CD audio), sound effects banks.
- Old MoA notes: `menus-hub.md`, `menus-screens.md`, `missions.md`, `phantasma/missions-import.md`, `sound-map.md`.
- **Done when:** PP's hub, missions table, mail and save format are laid out with the space available for MoA
  content, and the sound sources are listed.

---

## Order and overlap

All 8 can start now. Lane 1's cross-disc table speeds everyone up, so lanes 2–8 should pull `main` when they
begin and again before pushing. If a lane needs a Lane 1 result that isn't there yet, it finds that one
address itself and hands it to Lane 1. Lanes 2 and 6 are the ones that settle the base-game question; their
plain summaries go to the owner first.

## Starter prompt for a lane chat (copy, fill in the lane number)

> ACG1PLUS mapping, **Lane N** (see `docs/research/mapping/PLAN.md` in Hatdagger/ACG1PLUS; I've granted repo
> access and push). Read PLAN.md, CLAUDE.md and the old notes your lane lists. Work only in your lane, follow
> the rules in PLAN.md, ask me for the work pack files you need and for any live test with exact steps. When
> done: push branch `mapping/<lane>`, open a PR, update the lane row in README.md, write the project doc
> `claude/mapping-<lane>.md`, give me a short plain summary, and end.

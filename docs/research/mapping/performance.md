# Performance — Project Phantasma on the Steam Deck (Lane 7)

Started 2026-11-26 (ACG1PLUS mapping restart, Lane 7). Base game: Project Phantasma US (SLUS-00670, `PP`).
Drawing is mapped in `renderer.md` (PP sections at the top); this file is about **where the time goes and
what could move off the one busy CPU core**. Plan it builds on: project doc `cpu-offload-plan.md`.

**Status: nothing measured on PP yet** (no PP build boots yet). Everything below is read from the disc
and the framework docs (psxrecomp pin `44a401b7`). Status tags: `code` (read from code), `doc` (from a
framework doc), `guess` (reasoned, not measured). The ranking in §3 is a **ranked list of suspects to
measure**, not a measured profile; §4 says how to turn it into one.

## 1. Plain summary (for the owner)

- **The game itself only ever wants about 20 frames a second in a fight.** Every fight engine waits until
  715 TV lines have passed since the last frame (about 2.7 screen refreshes), so it can never go faster
  than ~22 fps on a real PlayStation either. "Full speed" on the Deck therefore means **holding ~20 fps
  without slowing down**, not 60. Smooth 60 fps on top is a separate step (in-between frames, §5).
- **Phantasma draws a fight with nearly the same code as Master of Arena**, so what made MoA slow on the
  Deck (15–17 fps in the old AppImage) will most likely make PP slow too, and the same fixes apply.
- **Most likely the biggest cost is not the drawing at all but how the code runs:** all of the fight code
  is loaded from the disc while playing and, in our builds so far, is not translated ahead of time, so it
  runs in a slow fallback (an interpreter, or a quick compiler that doesn't optimise). Translating it ahead
  of time, using all of the PC's cores at install, is the first fix to try (`cpu-offload-plan.md` step 1).
- **Good news for spreading work across cores:** PP never reads pictures back from the video memory
  during a fight (MoA did, every time a scrolling texture moved). Those read-backs are what force the
  framework's "render thread" (drawing on a second core) to stop and wait, so on PP it should be able to
  stay on for a whole fight.
- **Split-screen VS is the heaviest mode to draw**: the whole scene is built twice per frame (once per
  player). The arena draws once.
- To know for sure, I need **two short tests from you** (§4): one now with the build you already have,
  one later on the first PP build.

## 2. The frame budget (code)

| Fact | Where (PP) | How we know |
|---|---|---|
| One logic step per frame in every fight engine | VS loop `0x8004A42C`…`0x8004A98C` (FDAT 204) | read: camera ×2, scene render, eye copy, step counter `0x8008F5B0` +0x12, loop |
| Frame end waits for **715 scanlines** since the last frame start (`slti 0x2CB`), then flips; no `VSync` | VS `0x80054564`, arena `0x800566C4`, missions `0x80055644`, link `0x80055310`, replay `0x80054AA8` | read in all five engines; same as MoA |
| 715 lines ≈ 2.72 NTSC fields ≈ **45.4 ms** of game time → at most ~22 fps, normally ~20 (most frames shown for 3 refreshes) | | arithmetic (262.5 lines per field, 63.56 µs per line) |
| Primitive buffer per frame: VS 135,168 bytes (both views share it), arena 114,688 | `renderer.md` PP 2 / PP 3 | read from the buffer setup |
| VS draws everything twice: stage walk, 512-slot stage-object scan, 256-slot pool scan, both ACs and shadows, HUD — once per view | VS scene render `0x8005F838` → per-view draw `0x8005F4FC` ×2 | read |
| Both views go out in **one** `DrawOTag` over two chained 8,192-entry OTs (16,384 links walked a frame, most empty) | frame end `0x80054534` | read |
| **No VRAM read-back during a fight**: the one `StoreImage` in each fight engine runs at match start (scrolling-texture copy); per frame only `LoadImage` from RAM | VS `0x80053FA4` (once) / `0x8005411C` (per frame) | read; engines 202–206 each have one `StoreImage` call, none has `MoveImage` |
| A built-in measuring stick: the largest primitive use so far | VS `0x800888BC`, arena `0x8008DCCC` (RAM, bytes) | read (`renderer.md` PP 2 step n) |

What "slow" means in a recomp (doc): the game counts its own time from its own work, so when the PC
cannot finish one frame's work (game code + turning its drawing into GPU commands) in about 45 ms of real
time, the game simply runs slower (the 15–17 fps seen on the Deck with MoA). The frame cap itself costs
nothing; the time is in the work before it.

## 3. Ranked suspects (guess until §4 is done)

Ranked by how much of a fight frame each is likely to take on the Deck, and what could move it off the
main core. The order is reasoning, not measurement.

| # | Suspect | Why it is suspected | Offload / fix idea | What it needs | Confirmed by |
|---|---|---|---|---|---|
| 1 | **Disc code not pre-compiled** (fight engines, menu, stage code run in the interpreter or as un-optimised TinyCC code) | All fight and draw code lives in FDAT 201–206 (PP sizes: 548,864 / 290,816 / 274,432 / 270,336 / 294,912 / 270,336 bytes) and is loaded while playing; the old repo had `[aot] mode = "none"` (`cpu-offload-plan.md`). A framework example (Tomba 2) went from ~14 to ~65 fps from this alone (`AOT_OVERLAY_PLAN.md`, doc) | **Pre-compile on the player's PC at install, on all cores** (fits the build-on-your-PC route). Facts it needs from mapping: PP modules load at `0x800490D0`, which FDAT entries are code (201–206 + per-stage code: Lane 1 / 3) | Lane 1's PP overlay table; the build route | Test A: `interp_detail` / `overlay_compile` in the run report |
| 2 | **GPU command translation on the same thread** (each frame's primitives, up to 135 KB in VS, turned into OpenGL calls) | Release builds run "emu + GL" on one thread (`RENDER_THREAD.md`, doc); split screen means two clip rectangles and two passes of state changes | **Render thread** (`render_thread = true`): frame cost becomes max(emu, GL). PP has no per-frame VRAM read in fights (§2), so it should not be forced back to one thread mid-fight. Not with the frame-rate mod (render passes sync it) | nothing: a setting / env var | Test B |
| 3 | **Drawing the scene twice in VS** (game code) | §2: both views rebuild stage, objects and HUD; stage near-level meshes go vertex by vertex through the GTE in game code (`0x80064ECC`, `0x80065CCC`), AC meshes per joint (`0x80055E70` / `0x80062988`) | (a) after #1, native C stand-ins for the few hottest leaf routines (same results, faster; one core still); (b) later: stage geometry kept on the GPU (retained terrain, `cpu-offload-plan.md` step 4), needs the stage format mapped | #1 first, then a profile (Test C) to pick the routines | Test C (host profile in VS vs arena on the same stage) |
| 4 | **AC posing** | `0x80051B44` (1,078 instructions) + the recursive joint walk `0x800517F4` per AC per frame, with GTE work per joint (joint count per model: `renderer.md` MoA "ACs (VS)", model header +5) | native stand-in (it is pure maths on known records, `renderer.md` "ACs (VS)"); or a worker thread for P2's pose (both poses are independent until the views draw) | #1 first; Lane 4 for field meanings | Test C |
| 5 | **Game logic per step** (battle, AI in arena) | not the renderer, but on the same core: the arena AI runs here too | stays on one core (step-by-step work); only #1 speeds it up | Lanes 4 / 6 | Test C |
| 6 | Smaller per-frame costs: OT clear (2 × 8,192 entries in VS) and the 16,384-link `DrawOTag` walk, the scrolling-texture uploads, sound upkeep | cheap per item; listed so the profile can rule them out | none needed unless the profile says so | | Test C |

Not a suspect: PGXP (compiled out of release builds in the old repo, `cpu-offload-plan.md`).

## 4. Tests (exact steps)

Each test is short. Send me the files it names; I read them.

### Test A — where the code runs (now, with the build you already have)

Uses the newest Master of Arena build you have (Armored-Core-Gen-1 or AC1Recomp), because no PP build
boots yet. Its fight drawing is almost the same code as PP's, so the answer carries over. Do it on the
**Steam Deck**, and if you can, the same on **Windows**.

1. Start the game the normal way. Go to **VS** (split screen), default ACs, any stage, unlimited time.
2. Play (or just move both ACs around) for **2 minutes**.
3. Quit the game completely (back out to the launcher and close it).
4. Next to the game's exe there is now a file **`psx_last_run_report.json`**. Send it (one per machine;
   rename them `report-deck.json` / `report-windows.json`).
5. Tell me roughly what the frame rate felt like (on the Deck: press the `…` button → battery icon →
   Performance Overlay Level 1 shows FPS; write down the number during the fight).

What I read from it: `interp_detail` (how much code ran in the slow interpreter, and which addresses were
hottest there), `overlay_compile` (units compiled / failed), `autocompile_degraded`.

### Test B — drawing on a second core (Deck, now, same build)

1. In Steam, game → Properties → Launch Options, enter: `PSX_RENDER_THREAD=1 %command%`
2. Repeat Test A steps 1–2 and write down the FPS.
3. Clear the launch options (back to how they were) and repeat once more for the "off" number.
4. Send the two numbers. If the game misbehaves with it on (flicker, freeze, crash), tell me what you saw.

(The setting exists in our framework pin; if your build is too old it is simply ignored — the two numbers
will then be the same, and that is also an answer.)

### Test C — real hot-spot list (later, first PP build, Windows then Deck)

Needs a build with the debug tools on (`-DPSX_DEBUG_TOOLS=ON`; a one-line change on the build command,
I will prepare it when the PP build route works) and Python on the PC. Same VS fight as Test A, then, while
the fight is running:

```
python tools/host_profile.py --port <debug port> --exe <game exe> --frames 600 --top 40
```

(`psxrecomp/tools/host_profile.py`: 1 kHz samples of the emulation thread, named by function; game code
shows up as its recompiled routines, so the rows map straight onto the addresses in `renderer.md`.) Then the
same in an arena fight on the same stage, to split "drawing twice" from the rest. I turn the output into
the measured version of §3.

## 5. In-between frames on the Deck (60 fps look)

- **CPU render passes** (re-running the game's draw code between steps; MoA's camera-only pass worked,
  `renderer.md` MoA "First milestone") cost a whole extra draw build per in-between frame, on the busy
  core. On the Deck that only makes sense once stock fights hold 20 fps with room to spare.
- **GPU in-between frames** (`frame_generation`, made by the render thread from the recorded command
  lists, `FRAME_GENERATION.md`, doc) do not re-run game code. On the Deck this is the cheaper road; it
  needs #2 (render thread) working first.
- PP render-pass facts are ready in `renderer.md` PP 2 ("Render-pass facts"); note the PP-only
  `0x80017A3C` that must be skipped inside a pass.

## 6. Open

- Everything in §3 needs Tests A–C.
- ~~Which FDAT entries below 201 are code on PP and where they load~~: §7 (round 3 topic 1).
- Whether `DrawSync` in the frame end costs anything in the recomp (it waits on emulated GPU work; `?`).

## 7. Code files to pre-compile (round 3 topic 1a, 2026-11-26)

Static read of FDAT.T on PP (disc file `GG/COM/FDAT.T`). Tool: `tools/mapping/code_files.py` (rerun, don't commit
its output). Load addresses are the stage loaders' (`stages.md` §1); each file was checked by its 8 header
handlers (they must point inside the file at that address) and by where its calls go.

**In plain words:** besides the boot program and the six big modules (FDAT 201–206), the game's code sits in
**78 small files**: one per mission map and one per fight stage and engine. **71 of them can ever be loaded;
61 are different** (some stages share identical files), **about 0.94 MB** in all (0.70 MB of it mission code).
These are what the first-run build should translate ahead of time, next to the six modules. Nothing else on the
disc holds code (every other archive was scanned for code patterns; none found). (code)

| FDAT | What | Loaded by | At | Size | Count |
|---|---|---|---|---|---|
| 5 × m, m = 0–16 and 19 | **mission code**, one per mission map (map 19 = the garage test fight; 17 and 18 are empty) | missions 202 | `0x80097A38` | 18–54 KB | 18 |
| 5 × s + 0, s = 20–39 | stage code for the **link** engine | link 203 | `0x800904B0` | 4–6 KB | 20 |
| 5 × s + 1, s = 20–29 | stage code for **VS** | VS 204 | `0x8008F5D0` | 4–6 KB | 10 |
| 5 × s + 2, the 12 arena stages | stage code for the **arena** | arena 205 | `0x800956F0` | 4–6 KB | 12 |
| 5 × s + 3, s = 20–29 | stage code for the **VS replay** | replay 206 | `0x8008F828` | 4–6 KB | 10 |
| 5 × s + 2, s = 21, 22, 26, 27, 31, 37, 38 | **never loaded**: arena files for the 7 stages the arena doesn't use (stage table `0x800BD6D4`: 20, 23, 24, 25, 28, 29, 30, 32, 33, 34, 35, 36). Built for another address (about `0x801C9DD8`): their handlers point outside the file at `0x800956F0` | — | — | 4–6 KB | 7 |

- **Identical files** (same bytes): FDAT 120–123 = 115–118 (stage 24 = stage 23), 175 / 177 and 180 / 182 =
  165 / 167 (stages 35, 36 = 33), 195 / 197 = 170 / 172 (stage 39 = 34). (data)
- **All mission files share one address**, and the link and VS-replay files each share one too: the recomp has to
  tell them apart by content (which file is in memory), not by address. (code)
- **Every call leaves the file only for the boot program or its own engine** (checked per file: no stage file
  calls into another engine). So each file is translated together with the engine named in its row. A handful of
  call-shaped words in mission files point elsewhere (0–6 per file): data, not calls `guess`. (code)
- **For a MoA stage import / a new arena stage:** an arena stage needs a working arena file. The 7 stale ones show
  that the disc's arena files for unused stages can't simply be switched on; they need a fresh build (as
  `stages.md` already says for MoA stages). (code)

## Handed over (to this lane)

(none yet)

## Handed over to the build chat (round 3 topic 1)

- **Pre-compile list** = the 71 loadable files of §7 (61 distinct), with their engine and load address, plus the
  six modules 201–206 at `0x800490D0`. Skip the 7 never-loaded arena files. Mission files need the missions
  engine's addresses, stage files their engine's.

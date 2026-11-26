# Reference: our work from the old AC1Recomp fork

Copied 2026-11-26 from `Hatdagger/AC1Recomp` before that repo is retired. **Reference only: nothing here is
built, and the rule in `CLAUDE.md` stands (do not copy this code; learn from it and write it fresh).**

## What is here

- `master/`: every file of ours on the fork's `master` (commit `7884f4c`): the Master of Arena hooks
  (`games/master-of-arena/*.c`: Rogue Arena, netplay, VS AI, Phantasma import, launcher glue), the model decoder
  (`acr/`), our hub tile art, tools, scripts, mapping docs, mod manifests.
- `branches/<branch>/`: for each of the 46 other branches, only the files that differ from `master` or do not
  exist there. A `/` in a branch name is written `__`. Look here for newer versions of a file and for work that
  never reached `master`.

## What was left out on purpose

- **All game code and game files**: `games/*/generated/` (translated code of three games) and `games/*/disc/`
  (original boot executables). Not copied anywhere.
- **The frameworks** `psxrecomp/` and `recomp-ui/` (copied into the old fork; here they are linked submodules).
- **Files that are not ours**: 38 files identical to the original AC1Recomp by NeonLightning (no license stated),
  e.g. `games/armored-core/*`, the stock build scripts, icons.
- **Held back for the owner to decide** (kept off GitHub, given to the owner as a zip):
  - 6 files we changed but that started as NeonLightning's: `.github/workflows/release.yml`, `CMakeLists.txt`, `README.md`, `build.sh`, `games/master-of-arena/game.toml`, `launcher/main.cpp`
  - `games/master-of-arena/moa_phantasma_missions.inc`: the Project Phantasma mission "recipe" (copy instructions
    plus XOR differences; it says it holds no game bytes, but it is the closest thing to game data we made)
  - `moa_phantasma_tables.inc` and `moa_phantasma_patches.inc`: small tables of values taken from Project
    Phantasma (opponent AI settings, byte patches with the original bytes beside the new ones)
  - pictures that show the game: `docs/readme/*.png` and `docs/mapping/weapon-map/*.png` (texture atlases)

## How it was checked

Automatic scans (file names and types, translated-code and executable signatures, large number tables) plus spot
checks by eye. It was **not** read line by line: a hook file can still hold a short table of values copied
from the game. If you find one, move it out and tell the owner.

## Branch folders (files kept)

- `appimage-new-hooks`: 25
- `arena-run-demo`: 18
- `arena-run-step1`: 18
- `fast-load-boot-only`: 25
- `hub-background-direct`: 24
- `launcher-keyboard`: 16
- `launcher-start-menu`: 27
- `load-game-fixes`: 27
- `mapping-debug`: 20
- `mapping-weapons-sound`: 6
- `merge-2026-11-17`: 25
- `moa-assembly`: 21
- `moa-debug`: 11
- `moa-default-settings`: 27
- `moa-garage-rework`: 16
- `moa-instant-loading`: 16
- `moa-launcher`: 16
- `moa-link-cable`: 6
- `moa-mission-picker`: 17
- `moa-netplay-autosetup`: 18
- `moa-next`: 16
- `moa-pp-missions`: 11
- `moa-save-config`: 16
- `moa-save-defaults`: 11
- `moa-xbox-memory`: 16
- `netplay-direct`: 25
- `netplay-room`: 20
- `op-wlink`: 30
- `perf-engines`: 2
- `phantasma-mission-fixes`: 25
- `phantasma-mission-fixes-2`: 26
- `renderer-vs-ac-pass`: 12
- `renderer-vs-camera-pass`: 24
- `renderer-vs-pool-pass`: 12
- `rogue-arena-49`: 15
- `rogue-arena-shared-start`: 25
- `rogue-direct`: 25
- `rogue-direct-hub-gfx`: 25
- `shared-direct-start`: 28
- `vs-ai`: 24
- `vs-ai-debug`: 25
- `vs-direct`: 24
- `weapon-map`: 4

Useful starting points: `branches/perf-engines/games/master-of-arena/aot-draft-overlays.json` (a draft profile for
pre-compiling the fight code), `branches/mapping-weapons-sound/docs/mapping/` (sound and weapon maps),
`branches/netplay-room/`, `branches/moa-link-cable/`, `branches/rogue-direct/games/master-of-arena/ROGUE-ARENA.md`.

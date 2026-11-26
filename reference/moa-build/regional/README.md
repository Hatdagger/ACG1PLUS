# Two disc versions, two programs

Master of Arena disc 1 exists as the US disc (SLUS-01030) and the Japanese disc
(SLPS-01855). Their game code differs (146,189 of the boot program's 190,464 bytes),
so each is recompiled on its own and built as its own program:

| Disc | Config | Game C | Build | Windows release |
|---|---|---|---|---|
| US | `game.toml` | `generated/` | `-DACG_REGION=US` (default) | `RecompData/` |
| Japan | `game-jp.toml` | `generated-jp/` | `-DACG_REGION=JP` | `RecompData/Japan/` |

The JP build stages `game-jp.toml` as `game.toml` beside its exe: every program reads the
`game.toml` next to it.

## Switching between them
The framework's regional switch (psxrecomp `runtime/include/regional_launch.h`) does it:
`RecompData/regional-launch.toml` (made from `regional-launch.toml.in` by
`scripts/package_prebuilt_release.sh`) lists each disc and its program. When a program
starts with, or the launcher picks, the other version's disc, it closes and starts that
program with the same command line (memory card folder included). The last disc used is
remembered in `RecompData/europe-selection.toml` (the framework's name), so the next
start goes straight to the right one.

- Windows only: the framework starts the other program on Windows only.
- A disc is recognised by the SHA-256 and size of its data track. Only these dumps work
  while the list is shipped (a .chd or another dump of the same disc doesn't match):
  - US: 700,507,920 B, SHA-256 `9e18bb32…a97b921e` (MD5 `4d5774c4…68e945`, same image the
    old fork used)
  - Japan: 701,578,080 B, SHA-256 `bd15f6c9…f9de8574` (MD5 `5c071998…6b922`, the owner's D2D
    dump of 2026-11-20)
- Each program has its own settings and mods; memory cards are shared (`Memory Card/`).

## Japanese program, step 0.2
- Seeds: `seeds/ghidra_funcs_jp.txt`, first-pass jump targets from the boot program, made
  by psxrecomp's `probe_disc.py` the same way as the US list (374 seeds; 1,083 functions).
- Generated with psxrecomp-game at the pinned framework; the US code regenerates
  byte-for-byte with the same procedure.
- START GAME: LOAD GAME and NEW GAME need the US disc's addresses found on the Japanese
  disc; until then the JP program boots every start normally (`game/acg_start_none.c`).

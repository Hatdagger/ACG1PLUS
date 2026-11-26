# Research

Everything learned about Master of Arena while working on the old AC1Recomp fork, carried over as notes only.
No code from the old fork is here. All addresses are for the **US disc 1 (SLUS-01030)**, the disc this project is
generated from, so they still apply.

Where a note names a source file (`moa_card.c`, `moa_arena_run.c`, `mod_builtin_moa_*.c` …) it means the old fork,
https://github.com/Hatdagger/AC1Recomp, at `7884f4c` (0.8.4.4.2). Those files are not in this repo.

| Note | What it covers |
|---|---|
| `disc-versions.md` | The discs on hand (US, Japan, Phantasma) and how they differ |
| `mapping/README.md` | Index of the mapping lanes, plus arena rosters and save format pointers |
| `mapping/battle-system.md` | Fights: player block, AC object, damage, missiles, lock-on |
| `mapping/stat-block.md` | The 324-byte compiled stat block, every byte |
| `mapping/parts-tables.md` | Part tables, weapon detail records, options |
| `mapping/boot-loads.md` | What loads at each step: boot, menus, hub, VS, arena |
| `mapping/mode-launch.md` | Mode byte, resume flags, starting Scenario / VS directly |
| `mapping/menus-hub.md` | The hub ring and how to change its entries |
| `mapping/menus-screens.md` | Garage, Shop, System, Arena, Options, VS menu |
| `mapping/missions.md` | Mission launch, MIS.T, per-mission code, Phantasma mission tests |
| `mapping/renderer.md` | How a fight frame is drawn; plan for in-between frames and widescreen |
| `mapping/arena-prizes.md` | Arena prize parts |
| `mapping/ram-map-gameshark.md`, `ram-map-codes.md` | RAM addresses from cheat codes |
| `weapon-map.md` | Every weapon: shot, look, sound, knockback, special effects |
| `sound-map.md` | Sound banks, sound ids, music, voices |
| `hook-addresses.md` | Function entries and instruction sites the old mods hooked |
| `custom-parts/` | OP-WLINK custom option part (on hold) |
| `phantasma/missions-import.md`, `netplay-notes.md` | How the old fork did the Phantasma import and link netplay |

Not carried over yet (still in the old fork or the Claude project): the mod code itself, the launcher, Rogue Arena,
AI Battle, Raven cards, and their design docs.

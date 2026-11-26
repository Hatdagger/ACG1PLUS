# ACG1PLUS

**Armored Core: Project Phantasma running as a native PC program** — not an emulator. Built from your own disc
with [PSXRecomp](https://github.com/RetroPortingToolKit/psxrecomp). Master of Arena (and later Armored Core 1)
content comes in later.

> **You need your own game disc.** This repo holds **no game code and no game data**: not the original files,
> and not a translated copy either. The game is translated and built on your own PC from your own disc.

## Where it's at

| | |
|---|---|
| 🧪 | 0.0.x: build Project Phantasma USA (SLUS-00670) on Windows from your disc ([how](docs/building.md)), not yet tested on a real PC |
| ✅ | One portable folder: nothing goes to AppData, can live on a USB drive |
| ✅ | Research notes ([`docs/research`](docs/research/README.md)), mapping round 1 |
| ⏳ | Steam Deck (0.1), launcher "preparing game" screen (0.2), fast starts (0.3) … ([ladder](docs/versioning.md)) |

## Building it yourself

1. Make an empty folder, put [`Build ACG1PLUS.bat`](Build%20ACG1PLUS.bat) in it.
2. Put your Project Phantasma USA `.cue` + `.bin` in a `Disc` folder inside it.
3. Double-click `Build ACG1PLUS.bat`, then start `ACG1PLUS.exe`.

Details, folder layout and what stays where: [`docs/building.md`](docs/building.md).
`generated/` is ignored by git on purpose. Do not commit it.

## What's in here

| Folder | What |
|---|---|
| `reference/ac1recomp/` | Our own work from the old AC1Recomp fork, kept for reference (see its README) |
| `docs/history/` | Pull request write-ups from the old Armored-Core-Gen-1 repo |
| `game.toml` | Which disc this is (Project Phantasma USA) and how to run it |
| `aot/overlays.json` | Where the code the game loads from the disc sits (offsets and checksums only) |
| `scripts/windows/` | The build-on-your-PC script |
| `reference/moa-build/` | The retired Master of Arena build setup |
| `seeds/`, `symbols.toml` | Function starts (addresses) and names the translator uses |
| `docs/research/` | Everything we know about how the game works |
| `mods/preloaded/` | Mods that ship with the game (empty for now) |
| `psxrecomp/`, `recomp-ui/` | The framework and its launcher (linked, not copied) |

## License

PolyForm Noncommercial 1.0.0 (see `LICENSE`), same as the PSXRecomp framework. Armored Core is © FromSoftware;
this project contains none of the game's data.

Earlier repos (Armored-Core-Gen-1, AC1Recomp) are being retired; what was ours is carried over here.

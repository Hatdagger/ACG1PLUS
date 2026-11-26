# ACG1PLUS (Armored Core Gen 1)

**Armored Core: Master of Arena running as a native PC program** — not an emulator. Built fresh from our own
disc with [PSXRecomp](https://github.com/RetroPortingToolKit/psxrecomp).

> **You need your own game disc.** This repo holds **no game code and no game data**: not the original files,
> and not a translated copy either. The game is translated and built on your own PC from your own disc.

## Where it's at

| | |
|---|---|
| ⏳ | Build-on-your-PC route: first launch translates and builds the game from your disc (portable result) |
| ✅ | Research notes carried over from the old project ([`docs/research`](docs/research/README.md)) |
| ⏳ | First test build (not booted yet) |
| ⏳ | Japanese disc (SLPS-01855) support |
| ⏳ | Project Phantasma arena + missions, Rogue Arena, AI Battle, netplay room, launcher |

Nothing to download yet.

## What's in here

| Folder | What |
|---|---|
| `reference/ac1recomp/` | Our own work from the old AC1Recomp fork, kept for reference (see its README) |
| `docs/history/` | Pull request write-ups from the old Armored-Core-Gen-1 repo |
| `game.toml` | Which disc this is and how to run it |
| `seeds/`, `symbols.toml` | Function starts and names the translator uses |
| `docs/research/` | Everything we know about how the game works |
| `mods/preloaded/` | Mods that ship with the game (empty for now) |
| `psxrecomp/`, `recomp-ui/` | The framework and its launcher (linked, not copied) |

## Building it yourself

Being reworked. The old way kept the translated game code in the repo; it is now made on your PC and never
uploaded. Until the new first-launch setup is ready:

```sh
git clone --recurse-submodules https://github.com/Hatdagger/ACG1PLUS.git
cd ACG1PLUS
# put your disc image in disc/  (Armored Core - Master of Arena (USA) (Disc 1).cue + .bin)
./psxrecomp/tools/ci/build_emitters.sh
python3 psxrecomp/psxrecomp_cli.py generate --config game.toml --project-root . --disc disc/<your>.cue
python3 psxrecomp/psxrecomp_cli.py rebuild --config game.toml --project-root . --build-dir build-release --no-pgo
```

`generated/` is ignored by git on purpose. Do not commit it.

## License

PolyForm Noncommercial 1.0.0 (see `LICENSE`), same as the PSXRecomp framework. Armored Core is © FromSoftware;
this project contains none of the game's data.

Earlier repos (Armored-Core-Gen-1, AC1Recomp) are being retired; what was ours is carried over here.

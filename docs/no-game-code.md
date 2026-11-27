# No game code in this repo

Decided 2026-11-26. This repo holds our own work only: research notes, address lists, the launcher, our hooks,
build recipes. It holds nothing that comes from the game itself.

Never commit:
- original game files: boot executables (`SLUS_010.30`, `SLPS_018.55`, …), `SYSTEM.CNF`, disc archives (`*.T`),
  disc images, BIOS dumps
- translated game code: `generated/`, `generated-jp/`, overlay shards (`overlays_static*.c`), anything a tool
  wrote from the disc
- textures, models, sounds, music or text pulled off the disc, including upscaled or converted copies
- screenshots of the game, unless the owner has said yes to that picture

Fine to commit: addresses and what they mean, file names and offsets on the disc, formats, our own code that
reads those things from the player's disc at run time, patches written as instructions (not as game bytes).
Also checksums (SHA-256 / MD5) of the disc and of pieces of it: a checksum can't be turned back into the bytes.
`aot/overlays.json` is made of exactly these (offsets, sizes, load addresses, entry points, checksums) and
`seeds/` of addresses only.

How players get the game: the download contains no game code. The build reads the player's own disc,
translates it and builds the game on their PC (`docs/building.md`).

Local work files that hold game bytes and stay on the PC (all ignored by git): `disc/` (the boot program
Generate extracts), `generated/`, `.cache/aot-static/` (the disc evidence the AOT pipeline keeps),
`build-*/`.

What moved here and what was left out: `reference/ac1recomp/README.md`.

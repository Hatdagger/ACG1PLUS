# Parked: the old bundled-release packaging (not used)

From the Master of Arena days, when the translated game C was committed and GitHub Actions built and
zipped the game. None of it runs now (no game code on GitHub, `docs/no-game-code.md`):

- `release.yml`: the old Actions workflow
- `package_release.sh`, `package_prebuilt_release.sh`, `release.toml.txt`, `release/`: the old zip packaging
- `linux/`: the old AppImage entry. It keeps the player's files under `~/.local/share`, which the portable
  folder decision (2026-11-26) rules out; the Steam Deck route (ladder 0.1) will not use an AppImage.

The build now happens on the player's PC: `docs/building.md`. Kept for reference only.

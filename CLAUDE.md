# ACG1PLUS — notes for Claude

An Armored Core recomp on psxrecomp, based on **Project Phantasma** (USA, SLUS-00670) since 2026-11-26, with
Master of Arena (and later Armored Core 1) content brought in. Successor of Armored-Core-Gen-1 (same work, moved 2026-11-26
so that no game code sits on GitHub).
The owner is not a programmer: explain decisions in plain language, short, no jargon without a one-line meaning.

## Rules
- **No game code or game data in this repo, ever.** Not the original files (boot executables, disc archives,
  textures, sounds) and not translated code (`generated/`, `generated-jp/`, overlay shards, anything a tool
  wrote from the disc). The game is translated and built on the player's own PC from the player's own disc.
  `.gitignore` blocks the usual names; check `git status` before every commit anyway. The owner wants this
  handled as cautiously as possible: when unsure whether something is game-derived, leave it out and ask.
  See `docs/no-game-code.md`.
- **The old fork (AC1Recomp) is reference only.** Our own files from it are in `reference/ac1recomp/`
  (not built, not maintained). Don't copy its code. Use its notes and addresses (`docs/research/`) to learn what it did and what went wrong, then write it fresh, and say briefly what you
  reconsidered. Exception: the launcher was ported on purpose (`launcher/`, `patches/recomp-ui/`).
- Order of work is the version ladder in `docs/versioning.md`. Mapping runs alongside every step and is the
  biggest payoff. The disc patch track (`docs/disc-patch-track.md`) shapes how mapping is written up:
  record disc file + offset beside each RAM address, keep data and code findings apart, say which disc.
- Versions: pass the version by hand to the "Release builds" workflow (auto-bump only understands 3 numbers).
  Builds raise the last number; fixes add a number at the end.
- The owner wants changes reviewed before a build is pushed. Don't start workflow runs unless asked.
- Release notes cover only that release's changes.

## Building
GitHub cannot build a playable game any more (no game code here). The old Actions workflow is parked in
`packaging/old-ci/release.yml` for reference and does not run. Playable builds are made on the owner's PC:
generate from the disc, then rebuild (see README). The target is a first-launch setup that does this for every
player, with the compiler packed in so the result is portable. Plan: project docs `code-off-github-plan.md`
and `cpu-offload-plan.md`. The sandbox can't link the runtime (no OpenGL/X11); launcher code can be
syntax-checked against SDL3 headers only.

## Overnight runs
Scheduled runs work only on the `nightly` branch, from the queue in `docs/nightly/QUEUE.md`, and log what they
did in `docs/nightly/LOG.md`. See the top of `QUEUE.md` for the rules. Never push to `main` from a nightly run.

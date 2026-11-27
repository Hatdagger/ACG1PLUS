# The ACG1PLUS launcher (ladder 0.2.x): plan

Owner decisions, 2026-11-26:
- **No scripts or batch files for the player** (or the owner). One launcher program does everything:
  finding the disc, building the game from it, updating, settings, mods, starting the game.
- **Controller first**, with mouse and keyboard as a fallback. Must work on the Steam Deck in Game Mode.
- **Updates built in: releases + a test channel.** Players get approved releases. The owner also gets a
  hidden TEST channel to try a change (a pull request) before it is merged.
- ~~Claude builds the launcher in the sandbox and uploads it to a GitHub Release~~ **Changed 2026-11-26:**
  Claude's sessions can't create releases, so the workflow `.github/workflows/launcher-test-release.yml`
  builds the launcher on GitHub (it has no game code) and makes the release. Started by hand, one tap in the
  GitHub app, each time the owner approves. The game itself is still never built on GitHub.
- **One launcher for everything**: the game's current in-game menu (START GAME, SETTINGS, MODS, UPDATES)
  moves into the new launcher, including every settings page. The game window only plays.

## Why a separate program
The current launcher (`launcher/moa_menu.inc`) is drawn inside the game program, so it can't exist until
the game has been built, which is why the build needed `Build ACG1PLUS.bat` / `.sh`. The new launcher is a
small program of its own with **no game code in it**: it can be downloaded ready-made, and it builds the
game from the player's disc.

## What a player does
1. Download one file from the Release page into an empty folder (or a USB drive): `ACG1PLUS.exe`
   (Windows) or `ACG1PLUS` (Steam Deck / Linux). On the Deck, add it to Steam as a non-Steam game once.
2. Start it. It finds the disc (or lets them pick it with the controller), checks it, and shows a
   **"Preparing game"** screen while it downloads the tools and builds the game (first time: up to an hour
   or more; it can be left running, and picks up where it stopped if interrupted).
3. Main menu: START GAME, VERSUS, ROGUE ARENA, SETTINGS, MODS, UPDATES, EXIT. When the game closes (Esc),
   the launcher comes back.

## How it is built (technical, short)
- C++ with **SDL3** (window, controller, sound) and **Dear ImGui** (drawing), both already in the framework
  (`recomp-ui/src/third_party/imgui`, the compiler pack's SDL3). Linked statically: one file, nothing to
  install. Lives in `launcher2/` (new code; the AC look and menu layout are ported from `launcher/`, the
  owner's own design; that is the allowed exception to the "don't copy old code" rule).
- Windows exe cross-compiled in the sandbox (llvm-mingw); Linux file built with the Linux compiler pack and
  its SteamOS sysroot (same as PR #17), so it runs on stock SteamOS.
- The build steps of `scripts/windows/build.ps1` / `scripts/linux/build.sh` (PRs #11, #17) move into the
  launcher: pack download + checksum, disc check, `psxrecomp_cli.py generate` / `rebuild` run as child
  processes with `--json-progress` for the progress bar. The scripts stay in the repo as developer tools.
- Same portable folder as now (`docs/building.md`): everything inside it, nothing in AppData / ~/.local /
  ~/.config / ~/.cache; Windows and Deck builds side by side.

## What a Release holds (uploaded by Claude after the owner approves)
| File | What |
|---|---|
| `ACG1PLUS.exe` | the Windows launcher |
| `ACG1PLUS` | the Steam Deck / Linux launcher |
| `acg1plus-source-<version>.zip` | our code + the framework at its pinned versions + the library sources the build would otherwise download (libwebp etc.). **No game code**: checked with the no-game-code scan before upload |
| `manifest.json` | version, checksums of every file above and of the compiler packs |

- **Decided 2026-11-26 (0.2.2):** the source reaches the player as this zip, made first by the release
  workflow (`launcher2/release.sh` → `tools/release/make_source_zip.py`); its address, SHA-256 and size
  are built into both launchers, so each launcher builds exactly that source. The framework patches are
  applied inside the zip (`patches/APPLIED.txt`, so CMake needs no git) and the library sources are in
  `psxrecomp/third_party/` (the framework's own vendoring), so the build runs with `PSX_DEPS_OFFLINE=1`.
- Disc outside the folder (owner decision 2026-11-26): the launcher **offers to copy it into `Disc/`**
  (recommended) or uses it where it is.
- **No git needed** on the player's PC, and no downloads during the build itself (fixes the open problem of
  libwebp being fetched from codeload.github.com at build time). Downloads: the launcher, the compiler pack
  (from the toolchain project, pinned checksum) and the source zip.
- **Test channel** = GitHub pre-releases tagged `test-<name>` (for example `test-pr17`) with the same files.
  Shown only after turning on TEST in UPDATES (a hidden option).
- The repo is public, so no GitHub sign-in or token is needed (the old token feature goes away).
- Licences: the framework (PolyForm Noncommercial 1.0.0) and the libraries allow passing on their source for
  noncommercial use with their licence files; the zip carries them.

## Updates
- On start (and from UPDATES), the launcher checks the newest Release (or test pre-release) and says what's new
  in one line.
- Updating = download the new source zip, rebuild the game. **The old game stays playable until the new one
  has built**; if the build fails, nothing changes and the log is kept.
- The launcher updates itself the same way (download, check, swap, restart).
- Release notes cover only that release's changes.
- **How 0.2.4 does it (2026-11-26):** a release's launcher and its source zip belong together (the zip's
  checksum is built into the launcher), so one update = the new launcher replaces this one and restarts,
  then UPDATE THE GAME rebuilds the game from the new launcher's source. Before the swap, the new launcher
  must match the release's `manifest.json` checksum and start here (it is asked its version). The replaced
  launcher is kept in `RecompData/launcher/updates/previous/`. The rebuilt game is put together beside the
  old one and swapped in with renames at the very end, so a failed or stopped build leaves the old game.
  TEST channel: press Y five times on the UPDATES page (again to hide it); lists the newest test-* builds.

## Steps (each one a pull request, reviewed before anything is uploaded)
| Step | Done when |
|---|---|
| 0.2.1 | Launcher shell on Windows and Deck: AC-style main menu with every entry (VERSUS and ROGUE ARENA shown as "coming"), controller + mouse/keyboard, 1280x800 and full screen, EXIT. First upload as a test pre-release so the owner can see it. **Built** (`launcher2/`, see its README); not yet run on a real PC or Deck |
| 0.2.2 | First run: disc finder (Disc/, beside the launcher, USB drives, a controller-friendly file picker), disc check, "Preparing game" screen with plain stage names, progress, time estimate, cancel/resume, log, retry. **Built** (`launcher2/src/firstrun.cpp`, `disc.cpp`, `setup.cpp`; the source zip and `launcher2/release.sh`); whole first run checked on Linux in the sandbox with a test disc; not yet run on Windows or a real Deck |
| 0.2.3 | START GAME: launches the built game straight into the game (its own menu skipped, `--no-launcher`), memory cards in `Memory Card/`, the launcher returns when the game closes; FULL BOOT toggle. **Built** (`launcher2/src/play.cpp`, START GAME page in `menu.cpp`; `game/acg_escape.c`: started by the launcher, Esc or holding SELECT + START for 2 s closes the game). FULL BOOT = the PlayStation start-up screen only (the game's own logos need 0.3 hooks). First start writes `settings.toml` with full screen + full volume. Checked on Linux in the sandbox with a stand-in game; not yet with the real game, on Windows or a real Deck |
| 0.2.4 | UPDATES: releases + hidden TEST channel, rebuild-on-update with the old game kept, launcher self-update. **Built** (`launcher2/src/update.cpp`, UPDATES page in `menu.cpp`, swap-in at the end in `setup.cpp`); checked on Linux in the sandbox (TEST list from GitHub, install + restart from a fake release, damaged download refused); not yet on Windows or a real Deck |
| 0.2.5 | SETTINGS: every page from the current menu (DISPLAY, PICTURE, SOUND / SYSTEM, ADVANCED, controller mapping), saved into the game's `settings.toml`; menu sounds read from the disc |
| 0.2.6 | MODS page |
| 0.2.7 | The game's in-game launcher menu removed; first stable Release (0.2.x) |

Start GAME's LOAD GAME / NEW GAME (quick starts) stay ladder 0.3.

## Open, to settle while building
- Exact settings list and how the launcher reads the game's setting definitions (from
  `psxrecomp/recompiler/src/player_settings.h`), so settings aren't typed in by hand twice.
- Windows SmartScreen warns about any new unsigned program ("Windows protected your PC" → More info → Run
  anyway); signing costs money. Some antivirus tools distrust small downloader programs.
- Text entry with a controller (avoid it where possible; an on-screen keyboard where needed).
- Deck: launcher on an exFAT USB drive (execute permission, ROUND1 test #32).

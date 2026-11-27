# launcher2: the ACG1PLUS launcher (ladder 0.2.x)

One program that will do everything a player needs: find the disc, build the game from it, update,
settings, mods, start the game (plan: `docs/launcher-plan.md`). **No game code in it**: it can be
downloaded ready-made from a Release.

Step **0.2.1** was the shell: the Armored Core system menu with every entry, controller first with mouse
and keyboard as a fallback, a 1280x800 window or full screen, EXIT.

Step **0.2.2** (this folder today) adds the **first run**: it finds the player's disc, checks it, and
builds the game from it on a "Preparing game" screen, all inside the launcher (no scripts, no git).
Starting the game comes in 0.2.3; SETTINGS, MODS and UPDATES say which step brings them; VERSUS and
ROGUE ARENA are marked COMING.

## The first run

Opens by itself while no game is built (and from START GAME).

1. **Welcome**: what will happen, the game folder and its free space.
2. **Your disc**: looks in `Disc/`, beside the launcher, at the disc chosen last time
   (`launcher.ini` `disc=`), in Downloads / Desktop / Documents / Games, then on other drives (Windows:
   the other drive letters; Linux / Deck: `/run/media/<user>/*`, `/media/*`, `/mnt/*`, the home folder),
   two folders deep. Each `.cue`'s data track is checked here, before any download: its size against
   `game.toml` `known_sizes`, then its MD5 against `known_md5` (both read from `game.toml` when the
   launcher is built, so they are never typed in twice). BROWSE FOR THE DISC opens a file browser for
   the controller (drives, folders, `.cue` files). A disc outside the folder: copy it into `Disc/`
   (recommended) or use it where it is.
3. **Preparing game**: the steps of `scripts/linux/build.sh` / `scripts/windows/build.ps1`, run by the
   launcher (`src/setup.cpp`), each child program started by it:

   | Step | What |
   |---|---|
   | Checking the game folder | folders; free space; Linux: can programs run from this drive (noexec) |
   | Downloading the tools | compiler pack `cmake-clang-v1` v1.0.14 with `curl` (resumes), SHA-256 checked |
   | Unpacking the tools | into `RecompData/build/tools-<os>/toolchain` (miniz); the pack's compiler is tried once |
   | Getting the game's source | `acg1plus-source-<version>.zip` from the same release (SHA-256 built into the launcher), unpacked into `RecompData/build/source` |
   | Checking your disc | `psxrecomp_cli.py verify-disc` |
   | Translating the game from your disc | `psxrecomp_cli.py generate` (`--json-progress`) |
   | Building mission and arena code | the same `generate`, its `aot_static` part (no counter in its output: the bar follows the usual time) |
   | Compiling the game | `psxrecomp_cli.py rebuild`; the bar comes from Ninja's `.ninja_log` (the framework only passes the compiler's output on at the end) |
   | Putting the game in place | into `RecompData/<os>/`, compiler path made relative, `build-info.txt` |
   | Final check | files there; Linux: nothing written to `~/.local`, `~/.cache`, `~/.config`; Windows: user PATH unchanged |

   Every build program gets the same environment as in the scripts (temp, ccache, XDG and toolchain
   folders inside the game folder; on Linux the pack's `env.sh` settings and the `gcc` stand-in).
   `PSX_DEPS_OFFLINE=1` when the source zip carries every library source (its `SOURCE.txt` says
   `offline=1`), so the build downloads nothing.

   Progress bar per step and for all steps, time left (first-time guesses, then this PC's own times from
   `RecompData/launcher/build-times.ini`), B = stop (asks first), Y = log. Markers in
   `RecompData/build/state/` let a stopped build carry on from its step; downloads resume. On a failure:
   the reason in plain words, RETRY / SHOW LOG / BACK, and the log file in `RecompData/build/logs/`.

## The source zip

`acg1plus-source-<version>.zip` (`tools/release/make_source_zip.py`, about 70 MB) is this repo at the
release's commit plus the framework (`psxrecomp`, `recomp-ui` and their submodules) at their pins, with our
`patches/` already applied (listed in `patches/APPLIED.txt`, so CMake needs no git) and the library
sources in `psxrecomp/third_party/` (from its `deps.manifest`, checksums checked). Before writing it, the
script refuses game code and disc files (our own files strictly; the framework's for disc images and boot
programs). `.github/` is left out.

## Controls

| | Controller | Keyboard | Mouse |
|---|---|---|---|
| move | D-pad / left stick | arrows, W / S | point at a row |
| choose | A | Enter, Space | left click |
| back | B | Esc, Backspace | right click |
| START GAME | START | | |
| window / full screen | VIEW (SELECT) | F11, Alt+Enter | |

The mouse pointer hides while a controller is used and comes back when the mouse moves.

## Window and full screen

- Starts as a **1280x800 window** (the Deck's screen size; smaller screens get 90% of the screen).
- Starts **full screen** in Steam's Game Mode / Big Picture (it sees Steam's `SteamDeck` /
  `SteamGamepadUI` setting).
- The choice made with F11 / Alt+Enter / VIEW is remembered in `RecompData/launcher/launcher.ini` beside
  the launcher (portable: nothing goes to AppData or `~/.config`). Nothing is written until it changes.
- `--fullscreen` / `--windowed` on the command line win for that start.

## What it is made of

| Part | From |
|---|---|
| `src/main.cpp` | window, controllers, fonts, main loop, command line |
| `src/menu.cpp` | the menu; look and layout ported from `launcher/moa_menu.inc` (the owner's design) |
| `src/ui.cpp` | the shared look (panels, rows, header, footer, bars) and input, moved out of menu.cpp |
| `src/firstrun.cpp` | the first-run pages |
| `src/disc.cpp` | finding and checking the disc, copying it into `Disc/` |
| `src/setup.cpp` | the "Preparing game" steps |
| `src/platform.cpp` | child programs (process group / job object), drives, free space; Windows + Linux |
| `src/hash.cpp` | SHA-256 and MD5 |
| `src/unzip.cpp` + `third_party/miniz` | unpacking; miniz 3.0.2 (MIT licence, `third_party/miniz/LICENSE`) |
| SDL3 3.4.10 (static) | the compiler packs' `deps/` (`cmake-clang-v1` v1.0.14), the same SDL the game uses |
| Dear ImGui 1.91.9b + its SDL3 / OpenGL 3 backends | `recomp-ui/src/third_party/imgui` |
| LatoLatin Regular + Bold | `recomp-ui/assets/common/fonts`, built into the program (SIL Open Font License 1.1; see that folder's `NOTICE.md`) |

Drawn with OpenGL (3.2, or whatever the driver offers). Linked into one file:

- **Linux / Steam Deck `ACG1PLUS`**: built with the Linux compiler pack and its SteamOS sysroot, the C++
  runtime inside. Needs only libc, libm and libgcc_s (glibc 2.34 at most), which every SteamOS has. SDL
  opens X11 (the Deck's Game Mode runs X programs through Xwayland), OpenGL and sound by itself.
- **Windows `ACG1PLUS.exe`**: cross-compiled with llvm-mingw (ucrt), the C++ runtime inside. Needs only
  Windows' own DLLs (Windows 10 or newer). Carries a manifest (UTF-8 file names, sharp on high-DPI
  screens) and version information; no icon yet.

## Building

On a Linux machine (the sandbox does this before an upload):

    launcher2/build.sh WORK_DIR            # both; or: linux / windows

The source zip is built in when `ACG_SOURCE_URL`, `ACG_SOURCE_SHA256`, `ACG_SOURCE_SIZE` and
`ACG_SOURCE_VERSION` are set (`release.sh` does that); without them it is a developer build that needs
`--source-zip FILE`. `release.sh WORK_DIR TAG` makes every file of a release: the source zip first, then
both launchers with it built in, `manifest.json` and `SHA256SUMS.txt`.

It downloads the Linux compiler pack (CMake, Ninja, Clang, SteamOS sysroot, SDL3), the Windows pack's
`deps/` (SDL3 for Windows) and llvm-mingw 20261117, each checked against a pinned SHA-256, into
`WORK_DIR/downloads/`, and leaves `WORK_DIR/out/linux/ACG1PLUS` and `WORK_DIR/out/windows/ACG1PLUS.exe`.
`recomp-ui` must be checked out (`git submodule update --init recomp-ui`).

## Options

| Option | |
|---|---|
| `--fullscreen` / `--windowed` | start that way (not saved) |
| `--no-first-run` | don't open the first run by itself |
| `--source-zip FILE` | developers / tests: build from this source zip (any checksum) |
| `--force-translate` | translate the game again even if nothing changed |
| `--version` | print the version and the source it builds |

## Test options

`--test-keys up,down,confirm,back,x,y` presses those (one every 10 frames; `waitN` pauses N seconds) and
`--test-shot FILE.bmp` saves a picture of the window afterwards and closes. `--test-shots DIR` saves a
picture of every new page and step (and one every 2 minutes while preparing). `--test-auto` goes through
the first run by itself, `--test-exit-when-done` closes when it ends, `--test-disc-md5 HEX` also accepts a
test image from `tools/build/packs_to_test_image.py` (its source zip needs the same MD5 in `game.toml`
and `aot/overlays.json`, made from a throwaway copy). Used for the screenshots in pull requests, for
example under a virtual screen:

    xvfb-run -s "-screen 0 1280x800x24" env SteamDeck=1 out/linux/ACG1PLUS --test-keys down,confirm --test-shot versus.bmp

## Test pre-releases

GitHub builds them: Actions → **Launcher test release** → Run workflow, branch to build (for example
`launcher/0.2`). It runs this branch's `launcher2/release.sh` and makes `test-launcher-<VERSION>` with
both launchers, the source zip, `manifest.json` and `SHA256SUMS.txt`, notes from
`launcher2/test-notes.md` (only that build's changes; update it with each test build). Running it again
for the same tag replaces every file, the source zip included: a launcher downloaded before then no longer
matches it and must be downloaded again.

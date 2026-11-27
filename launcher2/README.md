# launcher2: the ACG1PLUS launcher (ladder 0.2.x)

One program that will do everything a player needs: find the disc, build the game from it, update,
settings, mods, start the game (plan: `docs/launcher-plan.md`). **No game code in it**: it can be
downloaded ready-made from a Release.

Step **0.2.1** was the shell: the Armored Core system menu with every entry, controller first with mouse
and keyboard as a fallback, a 1280x800 window or full screen, EXIT.

Step **0.2.2** added the **first run**: it finds the player's disc, checks it, and builds the game
from it on a "Preparing game" screen, all inside the launcher (no scripts, no git).

Step **0.2.3** added **START GAME**: the launcher starts the game it built and comes back when the
game closes.

Step **0.2.5** (this folder today; it comes before 0.2.4 UPDATES) adds **SETTINGS**: the game's settings
pages and its button layout, in the launcher. MODS and UPDATES say which step brings them; VERSUS and ROGUE
ARENA are marked COMING.

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

## START GAME

START GAME opens the first run until the game is built; after that, its page:

| Row | |
|---|---|
| LAUNCH GAME | starts the game at its title screen |
| FULL BOOT | ON: the PlayStation start-up screen plays first; OFF (default): skipped. Saved in `launcher.ini` (`full_boot=`) |
| LOAD GAME, NEW GAME | shown as COMING IN 0.3 (they need game-side hooks at Phantasma's addresses) |

START on the controller starts the game straight from the main menu. The game (`src/play.cpp`) is
`RecompData/<os>/ACG1PLUS_Game` started with:

- `--no-launcher`: straight into the game, the game's own in-game launcher skipped;
- `--disc <the .cue chosen last time>` (`launcher.ini` `disc=`); if it isn't there any more, the disc
  page opens first (choosing one there carries on starting the game);
- `--memcard-dir "<folder>/Memory Card"`: memory cards shared by the Windows and Deck builds;
- `ACG1PLUS_LAUNCHER=1`: the game's Esc handler (`game/acg_escape.c`) then **closes the game** on Esc,
  or when SELECT and START are held together for two seconds (for the Deck, which has no Esc key),
  instead of opening the old in-game launcher;
- `PSX_BIOS_HLE_KEEP_INTRO=1` / `0` for FULL BOOT on / off.

Before the first start (or the first visit to SETTINGS), when the game has no `settings.toml` yet, the
launcher writes one with borderless full screen and full volume (what the game's own launcher used to do on its first run;
`game.toml` can't carry those two).

The launcher's window is hidden while the game runs and comes back when it closes; what was pressed
to close the game is ignored for a moment so it doesn't choose anything. A normal close goes back to
the START GAME page. If the game ends with an error (or can't be started), a page says so with the
exit code, the game's last lines and the log file: START AGAIN / BACK. Everything the game prints
goes to `RecompData/launcher/logs/game-last.log` (the run before: `game-previous.log`).

## SETTINGS

The pages of the game's old in-game settings menu (`launcher/moa_menu.inc`, the owner's design), in the
launcher (`src/settings.cpp`). Four tabs, switched with L1 / R1 (Q / E on the keyboard) or a click:

| Tab | Rows |
|---|---|
| DISPLAY | ASPECT (fixed at 4:3 until the game has widescreen support), FULLSCREEN, WINDOW SIZE, RENDERER, INTERNAL RESOLUTION, AUTOMATIC RESOLUTION, VSYNC, SMOOTH MOTION, SMOOTH MOTION RATE |
| PICTURE | TEXTURE FILTER, DITHERING, STEADY POLYGONS, STEADY TEXTURES, MOVIE FILTER, SCREEN MODEL, SCANLINES, SCANLINE STRENGTH |
| SOUND / SYSTEM | VOLUME, HIGH-QUALITY SOUND, BUTTON LAYOUT, REWIND, RESTORE DEFAULTS |
| ADVANCED | anisotropic filter, mipmaps, FXAA, smooth scaling, bloom, accurate blending, movie colour smoothing, supersampling, lowest auto resolution, smooth motion method, texture batching, the PGXP options, audio buffer, anti-deadzone, SELECT shortcuts, low latency input, rewind buffer and interval |

LEFT / RIGHT (or A) changes a row. A row that only matters with another one on (WINDOW SIZE, SCANLINE
STRENGTH, the PGXP rows...) shows only then. A value the player chose is amber; the game's default is
white, and INFO says "(Game default.)". RESTORE DEFAULTS (press A twice) takes every row's line out of the
file again and puts back the first start's full screen and full volume.

**Where it is saved**: beside the game, in `RecompData/<os>/` (Windows and the Deck each keep their own,
for their own screens). Changes apply the next time the game starts.

| File | What | Read by |
|---|---|---|
| `settings.toml` | the four tabs | the game at start (psxrecomp `config_loader.cpp`, `load_user_settings`) |
| `input.ini` | controller buttons, `[mapping]` | the game (`runtime/src/main.cpp`, `load_input_config`) |
| `keybinds.ini` | keyboard keys, `[player1]` | the game (`runtime/src/psx_keybinds.c`) |

Only the lines a row owns are changed (`src/kvfile.cpp`): comments, other keys and other sections stay as
they were. `settings_format = 2` is written at the top (the framework's current file format).

**Nothing typed in twice**: rows that are framework player settings take their allowed values from
`psxrecomp/recompiler/src/player_settings.h`, built in, and are checked with its `player_setting_valid`, so
the launcher never offers a value the game would refuse; if a framework update drops one of those settings,
its row disappears instead of writing a line nobody reads. The game's own defaults come from `game.toml`,
built in (or the copy beside the built game, once there is one).

**BUTTON LAYOUT** (on SOUND / SYSTEM): one page for player 1, each PlayStation button with its controller
button and its keyboard key side by side. LEFT / RIGHT picks the column, A waits for the new button or key
(6 seconds; Esc or a right click leaves it as it was), Y puts that one back to the game's default, ALL BACK
TO DEFAULT (A twice) does every one. The controller column takes buttons, triggers and stick directions
under the names the game reads (`a`, `lefttrigger`, `leftx-`...); buttons the game can't read (back
paddles) are refused with a note. Esc can't be a key: it closes the game. A second key already in
`keybinds.ini` (`cross = X, Mouse1`) is kept when the first is changed.

Left out on purpose, with why: SKIP BIOS INTRO (START GAME's FULL BOOT), SKIP THIS MENU (the game's own menu
is always skipped), the framework's stock settings page (hotkeys, post-effect chains, raw tuning numbers:
there is no in-game menu to open it from any more), MSAA (the PlayStation build never offered it), MENU
SOUNDS (they need the disc's sound files read; later).

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
| START GAME (starts the game at once) | START | | |
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
| `src/play.cpp` | START GAME: starting the built game, waiting for it, its log |
| `src/settings.cpp` | SETTINGS: the tabs and BUTTON LAYOUT |
| `src/kvfile.cpp` | changing single lines of `settings.toml`, `input.ini`, `keybinds.ini`, keeping the rest |
| `player_settings.h` (psxrecomp, built in) | the framework's player settings: allowed values |
| `game.toml` (built in) | the game's own defaults, for the SETTINGS pages |
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
`recomp-ui` and `psxrecomp` must be checked out (`git submodule update --init recomp-ui psxrecomp`).

## Options

| Option | |
|---|---|
| `--fullscreen` / `--windowed` | start that way (not saved) |
| `--no-first-run` | don't open the first run by itself |
| `--source-zip FILE` | developers / tests: build from this source zip (any checksum) |
| `--force-translate` | translate the game again even if nothing changed |
| `--test-game-args a,b` | tests: extra words for the game's command line (for example `--headless`) |
| `--test-no-hide` | tests: keep the launcher's window up while the game runs |
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

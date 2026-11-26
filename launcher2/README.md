# launcher2: the ACG1PLUS launcher (ladder 0.2.x)

One program that will do everything a player needs: find the disc, build the game from it, update,
settings, mods, start the game (plan: `docs/launcher-plan.md`). **No game code in it**: it can be
downloaded ready-made from a Release.

Step **0.2.1** (this folder today) is the shell: the Armored Core system menu with every entry, controller
first with mouse and keyboard as a fallback, a 1280x800 window or full screen, EXIT. START GAME,
SETTINGS, MODS and UPDATES say which step brings them; VERSUS and ROGUE ARENA are marked COMING.

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
| `src/main.cpp` | window, controllers, fonts, main loop (new) |
| `src/menu.cpp` | the menu; look and layout ported from `launcher/moa_menu.inc` (the owner's design) |
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

It downloads the Linux compiler pack (CMake, Ninja, Clang, SteamOS sysroot, SDL3), the Windows pack's
`deps/` (SDL3 for Windows) and llvm-mingw 20261117, each checked against a pinned SHA-256, into
`WORK_DIR/downloads/`, and leaves `WORK_DIR/out/linux/ACG1PLUS` and `WORK_DIR/out/windows/ACG1PLUS.exe`.
`recomp-ui` must be checked out (`git submodule update --init recomp-ui`).

## Test options

`--test-keys up,down,confirm,back` presses those (one every 10 frames) and `--test-shot FILE.bmp` saves a
picture of the window afterwards and closes. Used for the screenshots in pull requests, for example under
a virtual screen:

    xvfb-run -s "-screen 0 1280x800x24" env SteamDeck=1 out/linux/ACG1PLUS --test-keys down,confirm --test-shot versus.bmp

## Test pre-releases

GitHub builds them: Actions → **Launcher test release** → Run workflow, branch to build (for example
`launcher/0.2`). It makes `test-launcher-<VERSION>` with both files and `SHA256SUMS.txt`, notes from
`launcher2/test-notes.md` (only that build's changes; update it with each test build).

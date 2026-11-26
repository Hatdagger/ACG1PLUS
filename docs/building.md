# Building the game on your PC (ladder 0.0.x)

No game code is on GitHub (`docs/no-game-code.md`). The game is translated from **your own
Project Phantasma USA disc (SLUS-00670)** and compiled on your PC. Today that is a script you start
by hand; the launcher's own "preparing game" screen comes at ladder step 0.2.

## Windows

1. Make an empty folder for the game, for example `E:\Games\PC\Recomps\ACG1PLUS`. It can be on a
   USB drive.
2. Put `Build ACG1PLUS.bat` in it (from this repo's top folder).
3. Put your disc image (`.cue` + `.bin`, Redump-style, one data track, 454,072,416 bytes) in a
   `Disc` folder inside it. Any file names.
4. Double-click `Build ACG1PLUS.bat`. The first build downloads a few hundred MB of tools and
   source and can take an hour or more (not timed on a real PC yet); later builds are faster.
5. Start `ACG1PLUS.exe`.

To update: double-click `Build ACG1PLUS.bat` again. It fetches the newest source and rebuilds.

What the build does (`scripts/windows/build.ps1`):

| Step | What |
|---|---|
| git | uses the PC's git, or fetches PortableGit (pinned version and checksum) into the folder |
| source | this repo and the framework (`psxrecomp`, `recomp-ui`) at their pinned versions |
| compiler pack | `cmake-clang-v1` **v1.0.14** (pinned checksum): CMake, Ninja, Clang, and Python 3.12. Nothing to install |
| disc | finds your `.cue` in `Disc\` (or beside the .bat) and checks it against `game.toml` |
| translate | `psxrecomp_cli.py generate`: the main program, then the code the game loads from the disc while playing (`aot/overlays.json`) |
| compile | `psxrecomp_cli.py rebuild` |
| place | copies the game into `RecompData\windows\` and `ACG1PLUS.exe` to the top |

## The portable folder

Owner decision 2026-11-26: the whole game lives in one folder that can sit on a USB drive (exFAT)
and be copied to another device. Nothing is written to AppData, `~/.local`, `~/.config` or
`~/.cache`.

    ACG1PLUS\
      ACG1PLUS.exe              starts the Windows game (launcher/acg_starter.c)
      Build ACG1PLUS.bat        build / update (Windows)
      Start ACG1PLUS.sh         starts the Steam Deck game (scripts/linux/start.sh)
      Build ACG1PLUS.sh         build / update (Steam Deck / Linux)
      Disc\                     your disc image
      Memory Card\              card1.mcd, card2.mcd: shared by the Windows and the Deck game
      Mods\                     drop folder for mod packages
      RecompData\
        windows\                the Windows game: exe, DLLs, assets, bios, mods, game.toml,
                                settings.toml, overlay_toolchain\, cache\, keyboard-profiles\
        linux\                  the Steam Deck game, side by side
        launcher\               menu-sounds.txt, full-boot.txt, github-token.txt
        build\
          source\               the source, translated game C (generated\) and build files
          tools-windows\        PortableGit (if used) and the compiler pack
          tools-linux\          the Deck's compiler pack
          temp\  logs\          temporary files, one log per build

How each thing that used to leave the folder is kept in it:

| Thing | Default place | Kept in the folder by |
|---|---|---|
| game.toml, BIOS, settings, saves, memory cards, .cfg files | beside the exe already | the framework (exe-relative paths) |
| memory cards | `Memory Card\` | `ACG1PLUS.exe` passes `--memcard-dir` |
| compiler pack | `%LOCALAPPDATA%\retcomm\toolchains` | `build.ps1` unpacks it in `RecompData\build\tools-windows\toolchain` and points `RETCOMM_TOOLCHAIN_DIR` at it; the framework's installer (which also edits the user PATH) is never called. The user PATH is checked before and after, and put back if anything changed it |
| temporary files, compiler cache | `%TEMP%`, `%LOCALAPPDATA%\ccache` | `TEMP`/`TMP`/`CCACHE_DIR` set by the build, and `TEMP`/`TMP`/`TMPDIR` set by the game for itself (`game/acg_portable.c`) |
| Vulkan pipeline cache | SDL pref path `retcomm/psxrecomp` | `PSX_VK_PIPELINE_CACHE_DIR` (game/acg_portable.c) |
| media import cache | `%LOCALAPPDATA%\psxrecomp\imports` or `~/.cache` | `PSX_MEDIA_CACHE_DIR`, a 4-line framework patch (`patches/psxrecomp/0001-media-cache-dir.patch`, applied at configure) |
| mod resident cache | `%LOCALAPPDATA%` or `~/.cache` | `PSX_RESIDENT_CACHE` (game/acg_portable.c) |
| keyboard profiles | `%APPDATA%\recomp-ui` or `~/.config` | `RECOMP_UI_KEYBOARD_PROFILES_DIR` (game/acg_portable.c) |
| launcher files (menu sounds, full boot, updater token) | SDL pref path / `%APPDATA%\Armored-Core-Gen-1` | `RecompData\launcher\` (game/acg_portable.h) |
| updater downloads | `%TEMP%\acg-update` | `RecompData\launcher\update-temp\` |
| background overlay compiler | absolute path in `overlay_toolchain\compiler.txt` | rewritten as a relative path by `build.ps1` |

`game/acg_portable.c` runs before the game's `main()` and only fills a setting that isn't already
set (except the temp folder), so a player or a test can still point any of them elsewhere.

Known limits:
- **Graphics drivers** keep their own shader caches (for example `%LOCALAPPDATA%\NVIDIA\GLCache`,
  `~/.cache/mesa_shader_cache`). That is the driver, not the game, and it can't be redirected
  from inside the game.
- The framework's own first-run wizard (Generate & rebuild inside the game) is switched off in this
  build (`CMakeLists.txt`): its toolchain step would install into AppData and edit the PATH.
- `RecompData\build\` is needed for rebuilding and for the background compiler (code the game
  meets that wasn't pre-compiled). Keep it with the folder.
- The updater token in `RecompData\launcher\` travels with the folder: forget it
  (UPDATES > FORGET GITHUB TOKEN) before giving the folder to anyone.

## Steam Deck (ladder 0.1)

The same folder works on the Deck: the Deck game goes in `RecompData/linux/`, beside the Windows one,
and both use the same `Disc/`, `Memory Card/` and `Mods/`.

1. Copy `Build ACG1PLUS.sh` into the game folder (an empty folder, or the one Windows already built,
   for example on a USB drive). Put the disc in `Disc/` as for Windows.
2. On the Deck, switch to **Desktop Mode**, open the folder in Dolphin, right-click an empty spot and
   pick **Open Terminal Here** (or open Konsole and `cd` there), then type:

       bash "Build ACG1PLUS.sh"

   Starting it through `bash` works even on drives where files can't be marked as programs.
   The first build downloads about 830 MB (the compiler pack) plus the source.
3. Start the game with `bash "Start ACG1PLUS.sh"`.
4. **For Game Mode:** in Desktop Mode, open Steam → *Games* → *Add a Non-Steam Game to My Library* →
   *Browse*, pick `Start ACG1PLUS.sh` (set the file type filter to *All Files*), then *Add Selected
   Programs*. It then shows up in Game Mode under *Non-Steam*.

To update: run `bash "Build ACG1PLUS.sh"` again.

What the build does (`scripts/linux/build.sh`), the same steps as on Windows:

| Step | What |
|---|---|
| 0 | checks that programs can run from the folder's drive (some drives are mounted so they can't), before any download |
| git | SteamOS comes with git; nothing is installed |
| source | this repo and the framework at their pinned versions |
| compiler pack | `cmake-clang-v1` **v1.0.14 linux-x64** (pinned checksum): CMake, Ninja, Clang, Python 3.12 and a **build sysroot** (the system headers and libraries a compiler needs, which stock SteamOS leaves out). Unpacked into `RecompData/build/tools-linux/toolchain/` (about 2.6 GB) |
| disc | finds your `.cue` in `Disc/` (or beside the script) and checks it against `game.toml` |
| translate | `psxrecomp_cli.py generate`, as on Windows |
| compile | `psxrecomp_cli.py rebuild`, as on Windows |
| place | copies the game into `RecompData/linux/` and writes `Start ACG1PLUS.sh` at the top |

Kept in the folder on Linux: the same list as for Windows (the game's own caches through
`game/acg_portable.c`), and for the build `TMPDIR`, `CCACHE_DIR` and the four `XDG_*` folders (where
Linux programs keep caches and settings) point inside `RecompData/build/temp/`. The build checks
afterwards that it made nothing new in `~/.local`, `~/.cache` or `~/.config`.

Not checked yet (ROUND1 tests #8, #9): a real Deck, an exFAT USB drive (file permissions, speed), how
the game runs and loads from USB. The Linux compiler pack is big: with the Windows pack too, a folder
holding both builds needs roughly 6-7 GB, most of it compilers (to be measured; trimming the packs is
a later job).

## What the sandbox checked (2026-11-26)

**Steam Deck route** (`scripts/linux/build.sh`, run start to finish in a fresh folder, with a test disc
image rebuilt from the disc packs, `tools/build/packs_to_test_image.py`): the drive check, the Linux
compiler pack (downloaded, checksum matched, unpacked), the disc check, translation (main program and all
67 disc code pieces, 93,165 audited variants, every guard matching), compile and link with the pack's
Clang and SteamOS build files, the folder layout and `Start ACG1PLUS.sh`. The game needs only libraries
every SteamOS has (OpenGL, the C and C++ runtime; glibc 2.35 at most). Started through `Start ACG1PLUS.sh`
it booted headless (game entry at frame 141) and ran 200 seconds; no disc code ran in the slow interpreter;
memory cards landed in `Memory Card/`; nothing was written to the home folder. Folder size after the
build: 4.2 GB (3.6 GB of it in `RecompData/build/`, mostly the compiler pack). Two optional libraries
are switched off so every Linux machine builds what a stock Deck builds: OpenSSL and FreeType
(`build.sh` explains why). The disc code's compiler is always the pack's Clang (a stand-in named `gcc`).

**Windows route**, earlier the same day:

On Linux, from the owner's disc: Generate translated the main program (970 routines) and all 67 pieces
of disc code in `aot/overlays.json` (93,165 audited variants, every guard matching the disc), every
generated C file compiled with gcc, the whole game configured with CMake. The Windows script itself
could only be read through, not run.

## Making the code lists again

| File | Made by | From |
|---|---|---|
| `seeds/ghidra_funcs.txt` | `tools/xdisc/catalogue.py` | the main program (mapping round 1) |
| `seeds/modules/fdat201.txt` … `fdat206.txt` | `tools/build/pp_module_seeds.py` | `docs/research/xdisc/pp-catalogue.tsv` |
| `aot/overlays.json` | `tools/build/make_aot_profile.py --disc <cue>` | the disc (checksums) + mapping round 1 facts |

The module lists are a cross-check: the framework finds 4,267 of their 4,269 starts by itself
(2 missed in the missions engine, FDAT 202: `0x8007E468`, `0x8007FEB4`).

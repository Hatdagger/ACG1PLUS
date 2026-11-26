# Launcher

The launcher is the framework's own launcher window (recomp-ui) with this
project's Armored Core system-menu style menu drawn over it. Since 2026-11-26 it runs
Project Phantasma USA (SLUS-00670); the menu's look still comes from the Master of Arena days.

- `moa_menu.inc`: the menu (START GAME, SETTINGS, MODS, UPDATES, EXIT), the
  START GAME page, the automatic disc finder and the first-run defaults. Included by recomp-ui's
  `launcher_imgui.cpp` through the hook in `patches/recomp-ui/`.
- `acg_starter.c`: "ACG1PLUS.exe" (Windows), see Folder layout.
- `moa_update.h/.cpp`: the updater. **Switched off since 2026-11-26**: no download holds the game any
  more (it is built from the player's disc, `docs/building.md`), so UPDATES only says to run
  "Build ACG1PLUS.bat". Rebuild-on-update in the launcher comes with ladder step 0.2. The code that
  unpacked release zips stays for now, unreachable.

Brought over from the AC1Recomp fork's launcher (the user's own design) on
2026-11-20, trimmed to this ladder step. Rogue Arena, Extra Garage, Extras and
the custom network pages come back with their own steps.

## The recomp-ui patch

recomp-ui has no hook for a title's own menu. `CMakeLists.txt` applies
`patches/recomp-ui/*.patch` to the submodule's working tree at configure time
(skipped when already applied), so a local checkout shows recomp-ui as
modified after configuring. After bumping recomp-ui, if the configure stops
with "recomp-ui patch does not apply", redo the four small edits the patch
makes and regenerate it with `git -C recomp-ui diff > patches/recomp-ui/0001-title-menu-hook.patch`.

## Settings

Every player setting is on the menu's own SETTINGS page, on four tabs
(DISPLAY, PICTURE, SOUND / SYSTEM, ADVANCED; L1 / R1 or Q / E to switch).
Each row in `kSettings` is either a launcher-model call (as on the stock page)
or one of the framework's player settings (`psx.runtime.settings`, from
`psxrecomp/recompiler/src/player_settings.h`), read and saved through the mod
provider. Rows whose setting this build doesn't have are hidden. Where the
model and the framework both own a setting, only the model row is listed.
Raw tuning values (post-effect chain, PGXP tolerance, line limits), JogCon,
mouse and hotkeys stay on the stock page, reached from ADVANCED > STOCK
SETTINGS PAGE. Framework settings can only change once the disc is verified.

## Menu sounds

`moa_ui_sound.*` plays the game's own menu sounds (cursor, OK, back, window
opens, refused), read from the player's disc when the launcher has one:
`GG/MS/MENU_VAB.T` entries 0 + 1, program 0, note 60, as the game plays them
(`docs/research/sound-map.md` section 5). `moa_vab.h` decodes the PS1 sound
bank. Nothing is shipped: no disc, no sounds. The disc is read in the
background through the framework's own reader (`PS1::ISOReader`). They follow
VOLUME and can be turned off with SETTINGS > SOUND / SYSTEM > MENU SOUNDS
(saved as `menu-sounds.txt` in `RecompData/launcher/`, inside the game folder: the install is portable).
Phantasma's disc has the same `GG/MS/MENU_VAB.T`; that its entries 0 + 1 hold the same sounds is not
checked yet.

## Disc finding

On the first menu draw with no usable disc, the launcher looks in the player's
folder (Windows layout, below), then beside the game, and in `disc`, `discs`,
`iso`, `roms` and `games` under each, for an image whose SYSTEM.CNF names
SLUS-00670, Project Phantasma USA (any file name), using the framework's own disc
check (`disc_identity.h`). In the portable layout the player's folder is two levels above the game
(`<folder>/RecompData/windows/`), and `Disc/` is among the subfolders searched.

## START GAME

START GAME opens a page with three ways in. On Project Phantasma only **LAUNCH GAME** works for now
(the normal boot: logos, intro, title); LOAD GAME and NEW GAME are dimmed with "LATER" until
Phantasma's quick-start hooks exist (ladder 0.3). `ACG_QUICK_START` in `moa_menu.inc` switches them
back on; `game/acg_start_none.c` is the stand-in that boots every start normally.

The Master of Arena version (save list `BASLUS-01030A` … `O`, a function override on `0x800AAE94`,
FULL BOOT saved as `full-boot.txt`) is kept for reference in `reference/moa-build/`.

## VERSUS and ROGUE ARENA

Listed ahead of their ladder steps so the menu has its final shape: VERSUS
opens a page with LOCAL LINK (0.4), NETWORK (0.6), PLAYER VS AI and AI VS AI
(0.7); ROGUE ARENA (0.8) sits on the main menu. Each says what it will be and
which version brings it; choosing one only shows "not ready yet". When a mode
lands, its row gets a real start (`kVersusModes` / `MAIN_ROGUE` in
`moa_menu.inc`).

## Folder layout (portable)

The game lives in one folder that can be copied, also to a USB drive (owner decision 2026-11-26;
the full layout and what is kept where: `docs/building.md`):

    ACG1PLUS.exe             starts the Windows game (acg_starter.c)
    Build ACG1PLUS.bat       build / update from the player's disc
    Disc/                    the player's disc image
    Memory Card/             card1.mcd, card2.mcd, shared by the Windows and Deck games
    Mods/                    drop folder for new .psxmod packages
    RecompData/windows/      the Windows game: exe, DLLs, assets, bios, mods/, settings
    RecompData/linux/        the Steam Deck game (ladder 0.1)
    RecompData/launcher/     menu-sounds.txt, full-boot.txt, github-token.txt
    RecompData/build/        source, compiler pack, logs

- `acg_starter.c` creates `Memory Card/` and `Mods/` if missing and starts
  `RecompData/windows/<game>.exe --memcard-dir "<folder>\Memory Card"` (the framework's
  own option), passing any other arguments through. A `--memcard-dir` already on
  the command line wins (for running two copies later, Link Versus).
- The launcher notices it runs from `RecompData/windows` (or `linux`) and treats the
  folder two levels up as the player's: discs are looked for there first, and the MODS
  page's drop folder is `Mods/` (installing copies a package into the game's
  own `RecompData/windows/mods/installed/`; the framework keeps its mods there).
- `game/acg_portable.c` keeps the framework's caches (Vulkan, media, mod resident data, keyboard
  profiles, temp files) in `RecompData/<os>/cache/`, and the launcher's files in
  `RecompData/launcher/`.
- Starting `RecompData/windows/<game>.exe` directly still works, but memory cards then
  live in `RecompData/windows/`.

**Test build, not a release.** Launcher 0.2.5: SETTINGS, now on top of 0.2.4 (UPDATES, merged), built on GitHub by the "Launcher test release" workflow. It has everything 0.2.4 has.

New in 0.2.5:
- **SETTINGS works.** The game's settings pages are in the launcher now: **DISPLAY, PICTURE, SOUND / SYSTEM and ADVANCED**. Switch tabs with **L1 / R1** (Q / E on the keyboard), change a row with **LEFT / RIGHT** (or A).
- **Changes apply the next time the game starts.** A value you chose shows in amber; the game's own default in white.
- **BUTTON LAYOUT** (on SOUND / SYSTEM): each PlayStation button with its controller button and its keyboard key. LEFT / RIGHT picks controller or keyboard, A then press the new button or key, Y puts one back to default. Esc can't be used as a key (it closes the game).
- **RESTORE DEFAULTS** (press A twice) puts every setting back: full screen, full volume, the game's own look.
- Rows that only matter with another one on (WINDOW SIZE, SCANLINE STRENGTH, the PGXP rows...) show only then.
- Not here on purpose: SKIP BIOS INTRO (that's FULL BOOT on START GAME), aspect choices (4:3 until widescreen support), menu sounds (later).

Where it's saved: `RecompData/windows/` or `RecompData/linux/` beside the launcher (`settings.toml`, `input.ini`, `keybinds.ini`), so the PC and the Deck each keep their own.

To test: the easy way is from a 0.2.4 launcher: UPDATES, TEST channel, install test-launcher-0.2.5 (it then offers UPDATE THE GAME, since this build has its own source). Or use this in a new, empty folder. Try a few settings, start the game, and check they took (for example FULLSCREEN → WINDOW, TEXTURE FILTER, a changed button).

Files:
- `ACG1PLUS.exe`: Windows 10/11. "Windows protected your PC": More info → Run anyway.
- `ACG1PLUS`: Steam Deck / Linux. Mark it as a program (Properties → Permissions → "Is executable").
- `acg1plus-source-0.2.5.zip`: the source the launcher builds the game from (no game code).
- `manifest.json`, `SHA256SUMS.txt`: checksums.

Tested in the sandbox on Linux with the keyboard: every row changed, the file the game reads checked, keys set, put back to default and reset. Not yet with the real game reading them, on Windows, on a real Deck, or with a real controller.

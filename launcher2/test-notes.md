**Test build, not a release.** Launcher 0.2.1: the shell only (PR #19), built on GitHub by the "Launcher test release" workflow.

- The Armored Core system menu: START GAME, VERSUS, ROGUE ARENA, SETTINGS, MODS, UPDATES, EXIT. VERSUS and ROGUE ARENA are marked COMING. START GAME, SETTINGS, MODS and UPDATES show which step brings them. It can't find the disc, build or start the game yet.
- Controller (D-pad/stick, A, B, START), keyboard (arrows, Enter, Esc) or mouse.
- 1280x800 window or full screen: F11, Alt+Enter or the controller's VIEW button. In Steam's Game Mode it starts full screen.

Files:
- `ACG1PLUS.exe`: Windows 10/11. Put it in an empty folder and double-click it. Windows may say "Windows protected your PC" because it isn't signed: More info → Run anyway.
- `ACG1PLUS`: Steam Deck / Linux. Mark it as a program (Properties → Permissions → "Is executable"), then start it, or add it to Steam as a non-Steam game.
- `SHA256SUMS.txt`: checksums.

It writes only `RecompData/launcher/launcher.ini` beside itself, and only after you switch between window and full screen.

**Test build, not a release.** Launcher 0.2.3: START GAME (PR on top of #19), built on GitHub by the "Launcher test release" workflow.

New in 0.2.3:
- **START GAME starts the game.** Once the game is built, START GAME opens a page with LAUNCH GAME and FULL BOOT. START on the controller starts the game straight from the main menu.
- **The launcher comes back when the game closes.** In the game, press **Esc**, or **hold SELECT and START together for two seconds** (for the Steam Deck). The launcher hides while you play.
- **Memory cards** go in the `Memory Card` folder beside the launcher (shared by Windows and the Deck). The disc used is the one chosen when the game was built; if it has moved, the launcher asks for it again.
- **FULL BOOT**: ON plays the PlayStation start-up screen before the game; OFF (default) skips it. Skipping the game's own logos and intro comes with 0.3.
- **First start**: the game opens in full screen at full volume.
- **If the game closes with an error**, a page says so, with the game's last lines and the log file (`RecompData/launcher/logs/game-last.log`).
- **Use a new, empty folder for this test** (you can skip testing 0.2.2: this one does everything it did). The Esc / SELECT + START change is inside the game, so a game built by 0.2.2 doesn't have it, and this launcher doesn't rebuild an older game yet (that's UPDATES, 0.2.4).

Files:
- `ACG1PLUS.exe`: Windows 10/11. Put it in an empty folder and double-click it. "Windows protected your PC": More info → Run anyway.
- `ACG1PLUS`: Steam Deck / Linux. Mark it as a program (Properties → Permissions → "Is executable"), then start it, or add it to Steam as a non-Steam game.
- `acg1plus-source-0.2.3.zip`: the source the launcher builds the game from (no game code). The launcher downloads it by itself.
- `manifest.json`, `SHA256SUMS.txt`: checksums.

Tested in the sandbox on Linux only, with a stand-in for the game (how it is started, the launcher coming back, the error page, a moved disc). Not yet run with the real game, on Windows, or on a real Deck.

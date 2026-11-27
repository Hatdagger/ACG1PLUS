**Test build, not a release.** Launcher 0.2.5.1: a small addition on top of 0.2.5 (SETTINGS) and 0.2.4 (UPDATES), built on GitHub by the "Launcher test release" workflow.

New in 0.2.5.1:
- **Play the game you have while the new one builds.** When the game is being built again (UPDATE THE GAME after an update), press **B** on "Preparing game" and choose **BACK TO THE MENU (KEEPS BUILDING)**. START GAME then plays your current game; the main menu says BUILDING THE NEW ONE...
- When the new game is ready, it **waits for you to close the game**, then swaps it in (a few seconds). Your settings, button layout, saves and installed mods stay.
- A note on the menu says when the build has finished (or stopped).

To test: from a 0.2.4 or 0.2.5 launcher, UPDATES → TEST channel → install test-launcher-0.2.5.1. Choose UPDATE THE GAME, then B → BACK TO THE MENU, and play while it builds. Close the game and check UPDATES says the game is up to date.

Files:
- `ACG1PLUS.exe`: Windows 10/11. "Windows protected your PC": More info → Run anyway.
- `ACG1PLUS`: Steam Deck / Linux. Mark it as a program (Properties → Permissions → "Is executable").
- `acg1plus-source-0.2.5.1.zip`: the source the launcher builds the game from (no game code).
- `manifest.json`, `SHA256SUMS.txt`: checksums.

Tested in the sandbox on Linux with a test disc: the old game ran through the whole rebuild, the swap waited for it to close, then the new game went in and started. Not yet on Windows or a real Deck.

**Test build, not a release.** Launcher 0.2.4: UPDATES (built from `main`), made on GitHub by the "Launcher test release" workflow.

New in 0.2.4:
- **UPDATES works.** It looks for a newer launcher on this repo's Releases page when the launcher starts (CHECK AT START, can be turned off) and when you choose CHECK FOR UPDATES. If there is one, the main menu marks UPDATES with NEW and says what's new in one line.
- **Installing** asks first, downloads the new launcher, checks it against the release's checksums and that it starts on this PC, then puts it in place of the old one and starts it. The old launcher is kept in `RecompData/launcher/updates/previous/`.
- **UPDATE THE GAME**: after an update (or with a game built by 0.2.2 / 0.2.3), this builds the game again from your disc with the new source. **The game you have stays playable until the new one is ready**; if the build stops or fails, nothing changes.
- **TEST channel** (for the owner): on the UPDATES page press **Y five times** (keyboard: L) to show the CHANNEL row, then switch it to TEST. It lists the newest `test-*` builds; choose one to install it. Press Y five times again to hide it.
- There are no real releases yet, only test builds, so on RELEASES it says "There is no release on GitHub yet". Use the TEST channel to try updating.

How to test updating: start this launcher in a folder where 0.2.3 built the game. UPDATES should offer UPDATE THE GAME (that game is older). To test the launcher replacing itself, a newer test build is needed: after this one, the next test build (0.2.5) will show on the TEST channel.

Files:
- `ACG1PLUS.exe`: Windows 10/11. Put it in an empty folder (or the 0.2.3 folder) and double-click it. "Windows protected your PC": More info → Run anyway.
- `ACG1PLUS`: Steam Deck / Linux. Mark it as a program (Properties → Permissions → "Is executable"), then start it, or add it to Steam as a non-Steam game.
- `acg1plus-source-0.2.4.zip`: the source the launcher builds the game from (no game code). The launcher downloads it by itself.
- `manifest.json`, `SHA256SUMS.txt`: checksums (the launcher's UPDATES read `manifest.json`).

Tested in the sandbox on Linux only: the TEST list read from GitHub, a full install and restart from a stand-in release, a damaged download refused (launcher unchanged), and the game built again over an existing game folder with the player's files kept. Not yet run on Windows or a real Deck.

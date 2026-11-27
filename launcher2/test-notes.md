**Test build, not a release.** Launcher 0.2.2: the first run (PR #19), built on GitHub by the "Launcher test release" workflow.

New in 0.2.2:
- **Finds your disc**: looks in `Disc/`, beside the launcher, in Downloads / Desktop / Documents, and on other drives (USB drives too). Checks it is Project Phantasma USA (size and checksum) before downloading anything. If it isn't found, BROWSE FOR THE DISC opens a file browser you can use with the controller.
- **Disc outside the folder**: offers to copy it into `Disc/` (recommended; the folder then holds everything) or to use it where it is.
- **Preparing game**: builds the game from your disc inside the launcher (no scripts, no git): downloads the compiler pack and the source zip from this release, checks both, then translates and compiles. It shows each step, a progress bar and the time left. B stops (it carries on from that step next time, and downloads resume), Y shows the log. If a step fails: what happened in plain words, RETRY / SHOW LOG. The log file is in `RecompData/build/logs/`.
- START GAME opens this the first time. **Starting the built game comes in 0.2.3**. Until then, start it by hand: Windows, double-click `RecompData\windows\ACG1PLUS_Game.exe`; Deck, run `RecompData/linux/ACG1PLUS_Game`. It asks for the disc image: pick the `.cue` in `Disc`. (Memory cards then go beside the game program, not in `Memory Card`; 0.2.3 starts it with both set.)

Files:
- `ACG1PLUS.exe`: Windows 10/11. Put it in an empty folder (with your disc in a `Disc` folder beside it, or anywhere it can find it) and double-click it. Windows may say "Windows protected your PC" because it isn't signed: More info → Run anyway.
- `ACG1PLUS`: Steam Deck / Linux. Mark it as a program (Properties → Permissions → "Is executable"), then start it, or add it to Steam as a non-Steam game.
- `acg1plus-source-0.2.2.zip`: the source the launcher builds the game from (our code + the framework + library sources, no game code). The launcher downloads it by itself; you don't need to.
- `manifest.json`, `SHA256SUMS.txt`: checksums.

Space needed: about 3 GB on Windows, about 5.5 GB on the Deck (most of it the compiler pack). Downloads: 210 MB (Windows) or 830 MB (Deck) plus 70 MB.

Tested in the sandbox on Linux only (whole first run, with a test disc image). Not yet run on Windows or a real Deck.

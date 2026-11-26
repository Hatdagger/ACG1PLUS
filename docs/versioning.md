# Version numbers

Goal: **0.8.8 is the first public release, and it must be further along than the old AC1Recomp ever got**
(AC1Recomp stopped at 0.8.4.4.2). Getting to 0.8.8 is meant to take a long time.

Rules
1. Start at 0.0.1.
2. Each step below is reached only when its work is done. **0.8.4 = level with AC1Recomp 0.8.4.4.2**: everything
   it has, rebuilt on the fresh recomp. The steps before it follow this project's own order, not AC1Recomp's.
3. Small steps. A build normally raises only the last number. Fixes add another number at the end
   (0.3.2 → 0.3.2.1 → 0.3.2.1.1) instead of moving up.
4. 0.8.5 to 0.8.7 are for things AC1Recomp never had. 0.8.8 comes after those.
5. Mapping the game's code runs alongside every step; it doesn't get a number of its own. Neither does the
   disc patch track (`docs/disc-patch-track.md`): using the mapping to patch the original game's disc.

Ladder (rewritten 2026-11-26 for the Phantasma base; numbering restarted at 0.0.1)

Base game: Project Phantasma USA (SLUS-00670). Master of Arena and later Armored Core 1 are content sources.
Builds are made on the player's PC from their own disc (no game code on GitHub). The Steam Deck is the
benchmark: if it runs well there, it runs well everywhere else.

| Version | Reached when |
|---|---|
| 0.0.x | Stock Phantasma builds on the owner's Windows PC from the owner's disc and plays: a mission, VS and the arena |
| 0.1.x | Steam Deck: the same build route works there with everything it needs packed in, the install is portable, and a performance baseline is measured |
| 0.2.x | Launcher on Phantasma: finds the Phantasma disc, shows a "preparing game" screen during the first-run build, the updater rebuilds when our changes change, works with a controller only |
| 0.3.x | Fast loading, plus direct starts from the launcher (load a save, new game, VS) |
| 0.4.x | Optimizations: full speed on the Deck, work spread across cores and the GPU |
| 0.5.x | Link Versus (two sessions on one PC) |
| 0.6.x | Netplay (LAN / online versus) |
| 0.7.x | Master of Arena content into Phantasma: arenas, opponents and parts first, then missions and mail; mission source picker, Extra Garage, AI Battle |
| 0.8.0 – 0.8.4 | Rogue Arena; level with AC1Recomp 0.8.4.4.2 |
| 0.8.5 – 0.8.7 | New ground: mail folders, Armored Core 1 content, visual upgrades that keep the game playable |
| 0.8.8 | Public release |

Why this order (owner, 2026-11-26)
- The build route and the Deck come first: nothing else can be tested until Phantasma builds from the disc.
- Optimizations come before Link Versus and netplay: a game that runs solid first benefits the networking later.
- Dropped from the old ladder: the Japanese disc step (it was for Japanese Master of Arena; a Japanese
  Phantasma disc is an open question) and single disc (Phantasma is already one disc).
- 0.7 now runs the other way: Master of Arena content goes into Phantasma, data first, missions last.

Earlier ladder (Master of Arena base, 2026-11-20, reached 0.0.4 in Armored-Core-Gen-1): 0.0 stock US,
0.1 launcher, 0.2 Japanese disc, 0.3 single disc + fast loading, 0.4 Link Versus, 0.5 optimizations, 0.6 netplay,
0.7 Phantasma content, 0.8.0–0.8.4 Rogue Arena, 0.8.5–0.8.7 new ground, 0.8.8 public release.

Building: always give the version by hand when starting a build (the old "Release builds" workflow, now parked,
only understood three-number versions); numbers typed in by hand can have as many parts as we like.

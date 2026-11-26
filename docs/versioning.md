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

Ladder (order set 2026-11-20)

| Version | Reached when |
|---|---|
| 0.0.x | Boots and plays the stock US game |
| 0.1.x | Custom launcher with built-in updater (dev builds also download Actions builds) |
| 0.2.x | Japanese disc plays stock too |
| 0.3.x | Single disc and fast loading |
| 0.4.x | Local link cable versus (Link Versus: two sessions on one PC) |
| 0.5.x | Optimizations |
| 0.6.x | Netplay (LAN / online versus) |
| 0.7.x | Additional content: Project Phantasma arena and missions, mission picker, save fix, Extra Garage, AI Battle |
| 0.8.0 – 0.8.4 | Rogue Arena; level with AC1Recomp 0.8.4.4.2 |
| 0.8.5 – 0.8.7 | New ground: things AC1Recomp never had (mail folders, …) |
| 0.8.8 | Public release |

Building: always give the version by hand when starting the "Release builds" workflow. Its auto-bump only
understands three-number versions; numbers typed in by hand can have as many parts as we like.

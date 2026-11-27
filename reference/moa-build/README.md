# The Master of Arena build setup (retired 2026-11-26, reference only)

Since 2026-11-26 this project builds **Project Phantasma USA (SLUS-00670)** only (owner decision, PR for
ladder step 0.0.x). Master of Arena becomes a content source (ladder 0.7), brought in as data, not as a
second program. What the repo used to build it from was moved here unchanged, so it can be looked up or
moved back:

| File | Was |
|---|---|
| `game-moa-us.toml` | `game.toml`: MoA USA disc 1 (SLUS-01030) |
| `game-moa-jp.toml` | `game-jp.toml`: MoA Japan disc 1 (SLPS-01855) |
| `seeds/ghidra_funcs_moa_us.txt`, `seeds/ghidra_funcs_moa_jp.txt` | `seeds/ghidra_funcs.txt`, `seeds/ghidra_funcs_jp.txt` |
| `regional/` | `regional/`: the US/JP program switch |
| `acg_quick_start_moa_us.c` | `game/acg_quick_start.c`: LOAD / NEW / FULL BOOT at MoA US addresses |
| `catalog_identity_moa_us.json`, `disc_probe_moa_us.json` | the root files of the same names |

Addresses only, no game code (same rule as the rest of the repo).

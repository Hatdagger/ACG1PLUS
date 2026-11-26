# Hook list check against this repo's generated code

Date: 2026-11-21 (nightly run). Source list: `docs/research/hook-addresses.md` (old fork's `game.toml`, US disc 1, SLUS-01030).
Checked against: `generated/SLUS_010.30_full_*.c` (function bodies), `generated/SLUS_010.30_dispatch.c` (dispatch table)
and `generated/SLUS_010.30_full.ranges` (function list, 1095 entries, highest `0x80036AB4`).
Boot EXE range from `game.toml`: `load_address 0x80011000` + `text_size 0x2E000` = `0x80011000`–`0x8003F000`.

## Result in short
- **106 addresses** checked (77 function entries, 29 instruction sites). No address appears twice.
- **3 are in the boot EXE** and all 3 are real function entries in `generated/` and in the dispatch table:
  `0x80012348`, `0x800167A0`, `0x800227D4`. They are also in `seeds/ghidra_funcs.txt`. **Confirmed in code.**
- **The other 103 are above `0x8003F000`**, so they sit in disc-loaded overlay code (the menu, VS, arena and mission
  engines the game loads from the disc). This repo's `generated/` has no code there at all, which is expected: overlay
  code is compiled when the game loads it, not ahead of time. Whether each one is a real function start or
  instruction inside an overlay **can't be checked without the disc** — they stay **unverified** (old notes only).
- Plain-language meaning: the boot-EXE hooks (instant loading, loading screen, a sound call) can be ported straight
  away; every other old hook first needs its overlay code looked at from the disc.

## Things to keep in mind (not checked, from the old notes' own comments)
- The old notes say all engines load at the **same address**, so one overlay address means different code depending on
  which engine is loaded (menu vs VS vs arena). A hook must check which engine is in memory before acting. This is
  one of the things to design fresh, not copy.
- For overlay hooks to fire once the overlay is compiled, the address must be listed in this project's `game.toml`
  (`mod_function_entry_funcs` / `mod_instruction_sites`). None are listed yet; that's a later, non-nightly change.
- Name `SsUtKeyOn` for `0x800227D4` comes from the old notes only; this run didn't confirm the name.

## Table
"Old use" is the comment group from the old fork. "In generated?" means a function body `func_XXXXXXXX` exists.


### Function entries (`mod_function_entry_funcs`)

| Address | Old use | In generated? (file) | EXE or overlay | Confidence |
|---|---|---|---|---|
| `0x800A69C8` | menu | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800B0CB4` | menu | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800B2D7C` | menu | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800639A4` | menu | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800AF590` | menu | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800AB500` | menu | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80054B54` | menu | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800AAE94` | menu | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80051F3C` | link versus engine (netplay) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80055428` | link versus engine (netplay) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80055578` | link versus engine (netplay) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80067FE4` | link versus engine (netplay) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80053FE4` | link versus engine (netplay) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80053808` | link versus engine (netplay) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80053498` | link versus engine (netplay) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80052C80` | link versus engine (netplay) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80052EE4` | link versus engine (netplay) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800532FC` | link versus engine (netplay) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80055768` | arena engine (AI Battle) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x8008DC98` | arena engine (AI Battle) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x8008D4AC` | arena engine (AI Battle) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x8007F69C` | arena engine (AI Battle) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x8007DA4C` | split-screen VS engine: controller sampler (VS AI, moa_vs_ai.c) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80067404` | split-screen VS engine: render passes (mod_builtin_moa_frame_rate.c): scene render (the pass point), queued sounds, frame-begin sound upkeep, start a sound | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800535F4` | split-screen VS engine: render passes (mod_builtin_moa_frame_rate.c): scene render (the pass point), queued sounds, frame-begin sound upkeep, start a sound | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80053DD0` | split-screen VS engine: render passes (mod_builtin_moa_frame_rate.c): scene render (the pass point), queued sounds, frame-begin sound upkeep, start a sound | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80053284` | split-screen VS engine: render passes (mod_builtin_moa_frame_rate.c): scene render (the pass point), queued sounds, frame-begin sound upkeep, start a sound | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80070F1C` | menu: save checksum (AI Battle keeps the settings on DATA LOAD) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800167A0` | boot exe: archive sector read (instant loading, mod_builtin_moa_loading.c) | yes — `SLUS_010.30_full_05.c` | EXE | confirmed in code |
| `0x80012348` | boot exe: loading screen before an engine, and SsUtKeyOn (Rogue Arena: the overlay is released at the loading screen; menu sounds muted while the run drives the menus behind a picture) | yes — `SLUS_010.30_full_00.c` | EXE | confirmed in code |
| `0x800227D4` | boot exe: loading screen before an engine, and SsUtKeyOn (Rogue Arena: the overlay is released at the loading screen; menu sounds muted while the run drives the menus behind a picture) | yes — `SLUS_010.30_full_21.c` | EXE | confirmed in code |
| `0x80095058` | menu: mission source picker (mod_builtin_moa_phantasma.c) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80056528` | menu: mission source picker (mod_builtin_moa_phantasma.c) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80093CD8` | menu: mission source picker (mod_builtin_moa_phantasma.c) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800AC4B4` | Arena Run (moa_arena_run_boot.c): LOAD GAME menu, arena result, hub pick | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BBEA8` | Arena Run (moa_arena_run_boot.c): LOAD GAME menu, arena result, hub pick | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80056414` | Arena Run (moa_arena_run_boot.c): LOAD GAME menu, arena result, hub pick | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BC420` | Arena Run (moa_arena_run_boot.c): LOAD GAME menu, arena result, hub pick | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BDE88` | Arena Run: the run's opponents (per-opponent loaders, CD queue check) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800586A0` | Arena Run: the run's opponents (per-opponent loaders, CD queue check) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800589DC` | Arena Run: the run's opponents (per-opponent loaders, CD queue check) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80055848` | Rogue Arena overlay hub: hub input state, opponent panel, opponent list | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BED64` | Rogue Arena overlay hub: hub input state, opponent panel, opponent list | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BD044` | Rogue Arena overlay hub: hub input state, opponent panel, opponent list | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800544A0` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80057CB0` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80057ED4` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80057B28` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80057554` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80056DBC` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80056564` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800552C0` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800AC8E8` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80059554` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800AC93C` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80072E54` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800B6A80` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80099098` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800941F4` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80093BC4` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800A2318` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800A2020` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800631FC` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BA854` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800C566C` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800B69F8` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x8007C6B0` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80090FD8` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80074DC0` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800633A4` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BA64C` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800C0E7C` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800822B0` | Rogue Arena boot trace (moa_boot_trace.c, diagnostic): menu routines | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80057F74` | boot trace, from the boot chat's boot-loads.md: model, sound bank, card screen's finish | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80058700` | boot trace, from the boot chat's boot-loads.md: model, sound bank, card screen's finish | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x8005CE68` | boot trace, from the boot chat's boot-loads.md: model, sound bank, card screen's finish | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80064F24` | Rogue Arena music (moa_music.cpp): the menu song's start | no (overlay, not pre-generated) | overlay | unverified (needs disc) |

### Instruction sites (`mod_instruction_sites`)

| Address | Old use | In generated? (file) | EXE or overlay | Confidence |
|---|---|---|---|---|
| `0x800C4D50` | single disc | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BB93C` | Project Phantasma arena (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800C01E8` | Project Phantasma arena (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800C0460` | Project Phantasma arena (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BD5B8` | Project Phantasma arena (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BB9F4` | Project Phantasma arena (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BBEE8` | Project Phantasma arena (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BA8AC` | Project Phantasma arena (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x8009417C` | Project Phantasma missions: mission screen (menu), mission loads (engine) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800943AC` | Project Phantasma missions: mission screen (menu), mission loads (engine) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800954E4` | mission source picker (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800955BC` | mission source picker (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800955D0` | mission source picker (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x8009510C` | mission source picker (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800556B8` | mission source picker (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800556D4` | mission source picker (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80055874` | mission source picker (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x80055894` | mission source picker (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BBA98` | Arena Run: arena 0 as one 50-rank ladder (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BBFB4` | Arena Run: arena 0 as one 50-rank ladder (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BBFE0` | Arena Run: arena 0 as one 50-rank ladder (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BC110` | Arena Run: arena 0 as one 50-rank ladder (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BC274` | Arena Run: arena 0 as one 50-rank ladder (menu) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BBB3C` | Arena Run: arena 0's whole-arena reads (names, rewards, builds, AI) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800BBCFC` | Arena Run: arena 0's whole-arena reads (names, rewards, builds, AI) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800C0250` | Arena Run: arena 0's whole-arena reads (names, rewards, builds, AI) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800C04FC` | Arena Run: arena 0's whole-arena reads (names, rewards, builds, AI) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800C01FC` | Arena Run: arena 0's whole-arena reads (names, rewards, builds, AI) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |
| `0x800C04A8` | Arena Run: arena 0's whole-arena reads (names, rewards, builds, AI) | no (overlay, not pre-generated) | overlay | unverified (needs disc) |

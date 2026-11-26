# Disc patch track (runs alongside the ladder)

Added 2026-11-21. Like mapping, this has no version number of its own.

**Goal:** use what the mapping finds to improve the *original* game, not just the
recomp: patches applied to the player's own disc image (and cheat codes), so the
changes also play on a real PS1, emulators and FPGA consoles (MiSTer and similar).
A recomp build itself can never go on a disc: it is a PC program.

## What can go back to the original game
- Changes to game data: arena line-ups and ranks, prize and shop tables, mail
  triggers, parts lists, AI values.
- Changes to game code, rewritten as PS1 (MIPS) code that fits on the disc:
  Rogue Arena-style rules, fixes, mail folders, maybe single-disc work.
- Cheat codes (GameShark style) from the RAM map (`docs/research/ram-map-codes.md`).

## What can't (needs the PC)
Launcher overlays and screens drawn by the PC, netplay, Link Versus across two
monitors, HD resolution, widescreen, fast loading, custom music from files.

## How mapping should be written with this in mind
- For every finding, record the **disc location** too (file on the disc and offset,
  or the overlay it lives in), not only the RAM address.
- Note how much free space a routine or table has, and whether a change could fit
  in place.
- Keep data findings (tables, values) separate from code findings: data changes are
  the easy first patches.
- Mark which disc each address is for (US SLUS-01030 / Japan SLPS-01855).

## Possible later steps
- Pick a patch format players can apply easily (xdelta or PPF).
- First candidate: a pure data patch (for example an arena line-up change) to prove
  the route on an emulator, then on MiSTer.

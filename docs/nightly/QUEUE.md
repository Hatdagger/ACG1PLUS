# Nightly queue

Work list for the scheduled overnight runs. The owner adds or reorders items by asking Claude in a chat.

## Rules for a nightly run (read first)
1. Work on the `nightly` branch only. At the start, merge `origin/main` into it. Never push to `main`.
2. Take the **first unchecked item** below and do only that one (one item per night keeps usage down).
   If it's too big for one run, finish a clear chunk and write what's left as a new sub-item under it.
3. **Docs and research only** unless an item says otherwise: no changes to `generated/`, build files,
   `game.toml`, the launcher or the workflows. No workflow runs.
4. Follow `CLAUDE.md` (old fork is reference only; disc patch track write-up rules).
5. Mark anything not checked against real code or a disc as **unverified**. Don't guess addresses.
6. When done: tick the item (`- [x]`, with the date), add a LOG.md entry in plain language (what was done,
   what was learned, anything the owner should decide), commit, push `nightly`, and make sure a single
   **draft** PR "Nightly work" from `nightly` into `main` exists (open it if missing, otherwise leave it).
7. If stuck (missing file, needs the disc, needs a decision), write that in LOG.md, leave the item unchecked
   with a `BLOCKED:` note, and move to the next item in the same run.

## Queue
- [x] (2026-11-21) **Check the hook list against the new code.** For every address in `docs/research/hook-addresses.md`,
  check whether it is a function entry in this repo's `generated/` (dispatch table / function list) or sits in
  disc-loaded overlay code (outside the boot EXE range `0x80011000`–`0x8003F000`, see `game.toml`). Write the
  result as a table in `docs/research/mapping/hook-check.md`: address, old use, found in generated? (file),
  EXE or overlay.
- [ ] **Disc offsets for EXE-resident findings.** For research addresses inside the boot EXE range, add the
  file offset in `SLUS_010.30` (offset = address − `0x80011000` + `0x800` header; confirm the header size from
  the framework or psxrecomp docs first). Put them beside the RAM addresses in the mapping notes, per
  `docs/disc-patch-track.md`. Overlay addresses: mark "needs disc".
- [ ] **Function index.** Start `docs/research/mapping/function-index.md`: one row per function that the
  research notes name or the old hooks used — address, generated file, plain-language job, confidence
  (confirmed in code / from old notes only). Batch by area (menus first, then VS, then arena).
- [ ] **Stutter fix research.** Test builds stutter every 15–20 s; the suspect is disc-loaded mission code being
  compiled in the background (`release.toml [aot] mode = "none"`, no AOT overlay profile). Read the psxrecomp
  docs/tools in `psxrecomp/` on AOT overlay profiles and write `docs/research/stutter-aot.md`: how a profile is
  made, whether it needs the disc, what the owner would have to run, and what changes in the build. No changes.
- [ ] **Japanese disc prep (step 0.2).** From `docs/research/disc-versions.md` and the psxrecomp setup docs,
  write `docs/research/jp-disc-plan.md`: steps to add SLPS-01855 (seeds, generate pass, address re-finding),
  what needs the disc on the owner's PC, and which research notes will need JP addresses.

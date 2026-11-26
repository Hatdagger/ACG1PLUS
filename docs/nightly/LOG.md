# Nightly log

Newest entry at the top. Each entry: date, queue item, what was done, what was learned, anything for the owner
to decide.

## 2026-11-21 — Check the hook list against the new code
**Done:** checked all 106 addresses from `docs/research/hook-addresses.md` (77 function hooks, 29 instruction
hooks from the old fork) against this repo's generated code. Table in `docs/research/mapping/hook-check.md`;
lane row added to `docs/research/mapping/README.md`.

**Learned:**
- Only 3 are in the boot program that's already turned into C here (`0x80012348` loading screen, `0x800167A0`
  disc sector read used by instant loading, `0x800227D4` a sound call). All 3 are real function starts in
  `generated/` — confirmed.
- The other 103 live in code the game loads from the disc later (menus, VS, arena, missions). That code isn't
  pre-generated in this repo, so they can't be checked without the disc. Marked **unverified**.
- The old notes say all engines load at the same address, so the same number means different code per engine.
  New hooks should check which engine is loaded — something to design fresh, not copy.

**For the owner to decide:** nothing urgent. When a mod is ported, its overlay addresses will need a check
against the disc (or a test run) and must be added to `game.toml`; that's a normal change for review, not a
nightly one.

## 2026-11-21 — setup
Nightly runs set up. Queue in `QUEUE.md`; work lands on the `nightly` branch with one draft PR into `main` for
review.

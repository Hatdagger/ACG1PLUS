# Cross-disc tables: Master of Arena US (MoA-US) ↔ Project Phantasma US (PP)

Made by `tools/xdisc/` from the owner's own discs (work packs); addresses and counts only, no game code.
How to read and rebuild them: `../mapping/foundations.md` §1, §4 and §7.

| File | What |
|---|---|
| `functions.tsv` | every routine on both discs (boot exe + the six modules), its twin, kind, likeness, call check, confidence |
| `pp-catalogue.tsv` | the same, one row per PP routine, sorted by PP address, with the research notes that mention the MoA twin |
| `globals.tsv` | RAM addresses the paired code uses, MoA-US → PP, with votes and confidence |
| `runs.tsv` | stretches of globals that all moved by one amount (for addresses not in globals.tsv) |
| `struct-ac-object.tsv` | AC object fields, MoA-US → PP |
| `struct-player-block.tsv` | player block fields (incl. the compiled stat block), MoA-US → PP |
| `fields.tsv` | all other differing field offsets (structs mixed) |

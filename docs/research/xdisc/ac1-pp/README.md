# Cross-disc tables: Armored Core 1 USA (AC1) ↔ Project Phantasma US (PP)

Made by `tools/xdisc/ac1_xdisc.py` from the owner's own discs (work packs); addresses and counts only, no game
code. How to read and rebuild them: `../../ac1/foundations.md` §5 and §8. Same columns as the MoA↔PP tables in
`../` (with `ac1_` in place of `moa_`), except the two struct tables, which are keyed by the **PP** offset.

| File | What |
|---|---|
| `functions.tsv` | every routine of the AC1 boot exe and its four modules, its PP twin, kind, likeness, call check, confidence |
| `globals.tsv` | RAM addresses the paired code uses, AC1 → PP, with votes and confidence |
| `runs.tsv` | stretches of globals that all moved by one amount (for addresses not in globals.tsv) |
| `struct-ac-object.tsv` | AC object fields, PP offset → AC1 offset |
| `struct-player-block.tsv` | player block fields (incl. the compiled stat block), PP offset → AC1 offset |
| `fields.tsv` | all other differing field offsets (structs mixed) |
| `summary.tsv` | counts from the last run |

Pairs: AC1 exe ↔ PP exe, FDAT 201 ↔ 201 (menu), 202 ↔ 202 (missions), 203 ↔ 203 (link), 204 ↔ 204 (VS).
PP's arena engine (205) and VS replay (206) have no AC1 twin.

**Reading the confidence:** AC1 was compiled with a different compiler setup, so a twin routine often has its
instructions in a different order. `low` here is very often the right routine, just reordered: check it by eye
before relying on it. `high`/`medium` can be taken as is for reading.

# Lane 5 scripts (content data, PP vs MoA)

Read the player's own extracted disc packs (`pp-disc/`, `ma-disc/` after `setup.py`); set `ACG_WORK` to the folder
holding them. They store nothing from the disc in the repo; outputs (`*.pkl`) stay local and are git-ignored by
name below. Needs `pip install unicorn capstone`.

- `compare.py` — part tables of both discs side by side (writes `tables.pkl`, used by the rest).
- `fielddiff.py` — field-by-field differences of same-named parts and weapon details.
- `refs.py FILE BASE LO HI` — addresses built by lui/addiu in a code blob.
- `emu.py` — runs a game's own compile routine in an emulator on a loadout.
- `match.py N`, `swap.py N OUT` — random builds through both compile routines (swap.py writes PP's part records over
  MoA's first); `analyze.py OUT` pairs the stat block bytes.
See `docs/research/mapping/content-data.md` ("How we know").

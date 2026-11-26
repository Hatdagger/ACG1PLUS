# feeldiff — comparing Phantasma and Master of Arena code and data

Small read-only tools from mapping Lane 2 (`docs/research/mapping/feel-diff.md`). They read the
extracted work packs in a chat sandbox and print addresses, short disassembly lines and numbers.
They contain no game code or data, and their output must not be committed either (short snippets
to identify a routine are fine in notes).

Setup: extract `pp-work.tar.xz` and `ma-work.tar.xz`, run each pack's `tools/setup.py`, and put (or
link) `pp-disc/` and `ma-disc/` side by side in one folder. Point `FEELDIFF_WORK` at that folder.
Needs `pip install capstone`.

| Tool | What it does |
|---|---|
| `mdis.py pp\|ma ADDR [N]` | Disassemble from a RAM address (boot EXE or VS module; `setmod()` switches module) |
| `ld.py MA PP [ctx] [nMA] [nPP]` | Side-by-side diff of a MoA/PP routine pair that ignores registers, field offsets, immediates and load width, so only real logic changes show (`!` lines) |
| `scan.py 'FDAT 209'` | Runs that diff over every pair of a module in `maps/moa-to-pp-functions.tsv`; prints pairs with real differences |
| `deep.py` | Older pass: lists changed constants inside matched routines |
| `xref.py pp\|ma ADDR…` | Callers (`jal`) and stored pointers of an address |
| `grepop.py pp\|ma REGEX [lo hi]` | Find instructions by text, e.g. every access to a struct field |
| `dataref.py MA_ADDR…` | Finds the PP address of a MoA data table through the code that uses it |
| `anim.py N` | Root-motion tracks of body set N (`GG/P*/PA<N>.T` entry 1) that differ between the discs |
| `parts.py` | Every part on both discs, field by field (PP → MoA), plus PP table addresses |

The two discs were compiled with different settings, so instruction order and registers differ even
where the C was the same. Trust `ld.py`/`scan.py` more than the pair table's "changed" label.

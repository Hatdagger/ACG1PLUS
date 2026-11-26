# Mapping Armored Core 1 — plan (2026-11-26)

The owner wants all three games combined: Project Phantasma (PP) is the base, Master of Arena (MoA) and Armored
Core 1 (AC1) content come in. AC1 content is ladder 0.8.5–0.8.7, but the owner wants the mapping started now, so
it gets **one AC1 mapping chat** that works through the topics below **in order**, alongside the round 2 mapping
chat and the build chat. If the chat gets long, it finishes the topic it's on, writes a handover in its project
doc, and the next chat picks up the next topic.

Same rules as `PLAN.md` (aim, rules 1–10). Static reads are fine; new live tests go on the list in `ROUND1.md`.

## The disc
**AC1 = Armored Core USA, SCUS-94182** (1997, image sha256 `0c7f6350…f4d4d59`). The owner also has the 2000
Agetec rerelease (SLUS-01323): same boot program byte for byte, same data except 4 menu pictures in
`GG/MS/MENU_TIM.T`, so AC1 work applies to both.
Work pack: **`ac1-work.tar.xz`** in the Project (plus `ac1-rerelease.tar.xz`); music and movie packs are on the
owner's PC, ask by name. Extract, run `python3 -I -B ac1-disc/tools/setup.py`. The pack's `README.md` and project
doc `claude/ac1-disc-packs.md` say what's known: **same disc layout as PP** (same folders, archive names, 72 PA
files), boot program `SCUS_941.82` (169,984 B), FDAT 205 entries (PP 207), PA files 176 entries each (PP 268),
and **AC1's `MIS.T` writes its entry count one higher** (the pack's `tarc.py` accepts that; the repo's
`tools/` copies don't yet).

## Border with the other two chats
- **AC1 chat owns:** `docs/research/ac1/**` (new), `docs/research/xdisc/ac1-pp/**` (new), new files
  `tools/xdisc/ac1_*.py` (it may import from the existing `tools/xdisc/*.py` but doesn't edit them), and an
  `AC1` entry in the `DISCS` table of `tools/ram_map.py` (only that). Branch `mapping/ac1-<topic>`.
- **Round 2 mapping chat** keeps `docs/research/mapping/**` (except this plan's row in `README.md`), the
  existing `tools/xdisc/` files, `tools/mapping/`, `tools/feeldiff/`. **Build chat** keeps what `PLAN-ROUND2.md`
  gives it. No AC1 seeds or build files: AC1 is never built on its own.
- **Shared files, append only:** `docs/research/ram-map.toml` (rows with `disc = "AC1"`, added as one block at the
  end), `ROUND1.md`'s test table (new rows at the end), this plan's row in `README.md`. Rebase on `main` right
  before each push; if a shared file clashes, keep both sides' rows. Exception (2026-11-26): PP rows copied
  from MoA may be corrected in place, one mapping chat at a time (rule at the top of `ram-map.toml`).
- Old notes worth a look: old AC1Recomp had an `armored-core` game folder; our own leftovers from it are in
  `reference/ac1recomp/` (reference only, CLAUDE.md).

## Topics, in order

| # | Topic | Doc | What it answers |
|---|---|---|---|
| 1 | **Foundations**: boot program (load address, entry, gp, archive registration, loader routine), FDAT 205 list (which entries are code modules, where they load, which PP module is each one's twin), the other archives side by side with PP, and **the AC1 ↔ PP cross-disc tables** (routines, globals, runs, struct fields) made with `tools/xdisc/` | `docs/research/ac1/foundations.md`, tables in `docs/research/xdisc/ac1-pp/` | Can old knowledge be carried over by lookup? How close is AC1's code to PP's? |
| 2 | **Main records on AC1**: AC object, player block, compiled stat block, save block, mission table, mail (if any), via the struct tables from topic 1 | `docs/research/ac1/records.md` | What moved between AC1 and PP |
| 3 | **Content inventory**: what AC1 has that PP doesn't (and the reverse, briefly): missions (count, MIS.T entries, briefings), parts (list and stats, AC1 vs PP), stages, body kits (PA), music, voices, movies, emblems/menus | `docs/research/ac1/content.md` | The AC1 import list for 0.8.5–0.8.7 |
| 4 | **Import outlook**: what it takes to run AC1 missions and stages on PP's engines, and to bring AC1-only parts across. Reads round 2's `missions-import-pp.md` (MoA) if it exists by then and stays consistent with it | `docs/research/ac1/import-outlook.md` | How hard each kind of AC1 content is to bring in, in plain words |

## Done when
Each topic: its doc written (status per finding, disc tag `AC1` on every address, disc file + offset beside RAM
addresses), RAM rows added (`disc = "AC1"`, then `tools/ram_map.py --disc AC1`), new tests added to `ROUND1.md`,
PR opened and merged by the chat (docs and new tools only, normal merge commit), project doc
`claude/mapping-ac1.md` updated with one short section per topic, and this plan's row in `README.md` updated.

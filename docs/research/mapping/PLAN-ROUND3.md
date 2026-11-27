# Mapping round 3 — plan (2026-11-26)

Round 2 (`PLAN-ROUND2.md`) is done and merged, and so is its review (PRs #22, #24, #25; project doc
`claude/mapping-round2.md`, "Review"). Round 3 works the same way: **one mapping chat** goes through the topics below
**in order**, next to **one build chat** and the **AC1 mapping chat** (`PLAN-AC1.md`). If the mapping chat gets long,
it finishes the topic it's on, writes a handover, and the next chat picks up the next topic.

Same rules as round 1 (`PLAN.md`: aim, rules 1–10). Base game: Project Phantasma USA (PP). Static reads are fine;
the owner tests later from the list in `ROUND1.md`, so new tests go on that list (same table, next free number at
the end; the AC1 chat adds there too) instead of being waited for.

Why this order (owner, 2026-11-26): the two small topics that help the next build steps (0.4 speed, 0.5 Link
Versus) first, then the clean-up that every later topic leans on, then ladder 0.7 in its own order (parts,
opponents and arenas first, missions last). Paint / emblem sits between them because the Extra Garage and netplay
both need it.

## Border with the other chats
- **Mapping chat (this plan):** `docs/research/**` except the AC1 chat's files, `tools/xdisc/`, `tools/mapping/`,
  `tools/feeldiff/`, branch `mapping/round3-<topic>`.
- **Build chat:** `game*.toml`, `seeds/`, `symbols.toml`, `CMakeLists.txt`, `launcher/`, `packaging/`, `scripts/`.
  Seeds for the PP modules belong to the build chat: the mapping chat lists any routine starts it finds in a
  "Handed over to the build chat" list at the end of its topic doc. Topic 1's list of code files to pre-compile is
  handed over the same way (the mapping chat writes the list, the build chat uses it).
- **AC1 mapping chat:** `docs/research/ac1/**`, `docs/research/xdisc/ac1-pp/**`, `tools/xdisc/ac1_*.py`, the `AC1`
  entry in `tools/ram_map.py` (`PLAN-AC1.md`, "Border").
- **Shared files** (`docs/research/ram-map.toml`, `ROUND1.md`'s test table, `README.md`'s plan rows): append only
  for both mapping chats. Rebase on `main` right before each push; on a clash keep both sides' rows.
- **Copied rows may be fixed** (rule since 2026-11-26, top of `ram-map.toml`): a PP row copied from MoA (it has a
  `moa` field) may be corrected in place — `what`, `size`, `type`, `status`, `src` only, never `addr` or `name` —
  by **one mapping chat at a time**. Topic 2 is that kind of work, so before starting it the mapping chat claims it
  in `README.md` (one commit), and the AC1 chat leaves copied PP rows alone until topic 2's PR is merged.

## Topics, in order

| # | Topic | Doc | Unlocks on the ladder | Size | From |
|---|---|---|---|---|---|
| 1 | **Build helpers.** (a) Every code file on the disc that isn't in the boot exe or FDAT 201–206: the small per-stage and per-mission code files below FDAT 201, where each loads, which engine calls it, so the first-run build can pre-compile them. (b) Which side the link engine treats as "me" (player 1 vs 2 on each console) | `performance.md`, `boot-loads.md` (link part) | 0.4 speed (top slowness suspect is code that isn't pre-compiled), 0.5 Link Versus | S | `performance.md` §6, `boot-loads.md` PP-14 |
| 2 | **The 60 "MoA 0x…" rows.** Find the PP address behind each MoA address still in the text of copied rows in `ram-map.toml` (by hand where `tools/xdisc/` found no match); also look at the 17 "(likely)" twins. Fix in place under the copied-rows rule; regenerate `ram-map-pp.md` | `ram-map.toml` | Clean map for every later topic and for the build chat | M | Round 2 review point 2 |
| 3 | **Parts batch.** (a) Part model archives (`*_T.T`): how a body-part archive groups its 16 entries per part, what PP's filled `DUMMY` slots are, so a MoA part's garage model can be served from our own pack. (b) Shop stock: confirm the address (guess `0x800343F0`), what stock / owned 127 mean, new-game values. (c) The bytes after the weapon-name table (`0x800BCCD8` …): what they are and who reads them, so MoA's 5 extra type names can be placed | `content-data.md`, `menus-screens.md` (shop) | 0.7 parts (garage, shop), Rogue Arena prizes | M | `content-data.md` §7, `menus-screens.md` §4 |
| 4 | **Opponents.** Which of PP's 49 AI records each arena rank gets, and where that choice is made | `ai.md` | 0.7 arenas and opponents, Rogue Arena line-ups, the Phantasma / MoA AI-style toggle | S–M | `ROUND1.md` open list (AI) |
| 5 | **Stages batch.** (a) MoA's extra 80-byte front chunk. (b) Chunks 4, 10, 12 and classes 250 / 241 / 247. (c) Whether the engines accept stage numbers 40 and up (table sizes, RTIM index = stage), since MoA's 11 own fight stages need new numbers. (d) What flag 1 is and what MoA's flag-2 surfaces are in the level (`stages.md` §5a) | `stages.md` | 0.7 MoA arena stages, Rogue Arena stages, the stage converter | M | `stages.md` §6 |
| 6 | **Paint / emblem layout.** Every byte of the 48-byte paint record and the emblem format; read the CHANGE COLOR page and the emblem editor on PP (round 2 named the starts) | `content-data.md`, `menus-screens.md` (§2l, §2m on PP) | 0.6 showing the other player's colours and emblem, 0.7 MoA AC import and Extra Garage | M | `content-data.md` §7 |
| 7 | **Missions batch.** (a) PP's message parser: which text codes PP understands, so MoA's `F0 81` / header codes can be converted. (b) The per-mission u16 table (`0x8008F874` PP / `0x80098C9C` MoA, values 24 / 56) and what its reader `0x8008DE76` uses it for | `missions-import-pp.md` | 0.7 MoA missions and mail texts | M | `missions-import-pp.md` §10 |
| 8 | **The 57 animation names** (if there is room): which move each of the 57 animations is and which AC state starts it | `pa-archives.md` | Safer Kaze "Both sides", later visual work | M | `pa-archives.md` §8 |

**Parked for a later round:** re-reading lock slots, missile steering and energy rules on PP (now carried over
from MoA); the arena engine's widescreen edge sites and HUD (`renderer.md` PP 7).

## Done when
Each topic: its doc written (disc tag and disc file + offset beside each RAM address, data and code findings kept
apart), RAM rows added to `ram-map.toml` (`disc = "PP"`, then `tools/ram_map.py --disc PP`), new tests added to the
end of `ROUND1.md`'s table, `README.md`'s Round 3 row updated, PR opened and merged by the chat (docs and mapping
tools only; normal merge commit), project doc `claude/mapping-round3.md` updated with one short section per topic.

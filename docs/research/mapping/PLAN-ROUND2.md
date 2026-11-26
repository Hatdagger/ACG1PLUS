# Mapping round 2 — plan (2026-11-26)

Round 1 (8 parallel lanes) is done: `ROUND1.md`. The owner found 8 chats a lot to manage, so round 2 is **one
mapping chat** working through the topics below **in order**, next to **one build chat** (ladder 0.0.x prep).
If the mapping chat gets long, it finishes the topic it's on, writes a handover, and the next chat picks up the
next topic.

Same rules as round 1 (`PLAN.md`: aim, rules 1–10). Base game: Project Phantasma USA (PP). Static reads are fine;
the owner tests later from the list in `ROUND1.md`, so new tests go on that list (same table) instead of being
waited for.

## Border with the build chat
- Mapping chat: `docs/research/**`, `tools/xdisc/`, `tools/mapping/`, `tools/feeldiff/`, branch `mapping/round2-<topic>`.
- Build chat: `game*.toml`, `seeds/`, `symbols.toml`, `CMakeLists.txt`, `launcher/`, `packaging/`, `scripts/`.
- **Seeds for the PP modules (FDAT 201–206) belong to the build chat** (it needs them first). The mapping chat
  hands over any routine starts it finds in a "Handed over to the build chat" list at the end of its topic doc.

## Topics, in order (why this order: what the ladder needs first)

| # | Topic | Doc | Why now | From round 1 |
|---|---|---|---|---|
| 1 | **PA archives**: what the 72 body files GG/P0–P3/PA##.T hold (models, skeletons, animations), format, how body combination 0–71 is picked | `pa-archives.md` | Kaze setting (Both sides) and part imports both need it | Lane 1 open, Lane 2 §8b |
| 2 | **Stages**: FDAT 100–199 stage code + data, geometry format, stage-object list (kinds 0x0A / 0x14 and which stages have them), area boxes | `stages.md` | MoA stage import, AI behaviours 2 and 12, renderer draw distance | Lanes 4, 6, 7 open |
| 3 | **Hub screens in depth**: garage pages, shop, system/card screen, arena list, options, VS menu (MoA `menus-screens.md` is the model) | `menus-screens.md` (PP sections) | launcher menus, Rogue Arena overlays, adding MoA's Ex Arena / Arena Maker | Lane 8 open |
| 4 | **Missions import plan**: what it takes to run MoA mission code + maps on PP's missions engine (relocation 0x800A0DD0 → 0x80097A38, engine calls, object fields, MIS.T texts, our own pack) | `missions-import-pp.md` | ladder 0.7, the hardest import; the old PP→MoA import crashed (project doc `phantasma-missions-crashes.md`) | Lane 8 room notes |
| 5 | **Small leftovers**: arena engine movement compare (PP 205 / MoA 210), sound (FDAT 200 bank + sound-id table), weapon type-name table, paint/emblem field layout, menu 201 header, disc files 0 and 3, battle unknowns (AC +0x3A/+0x60/+0x7C, match +0x16/+0x1A) | the owning lane docs | close round 1's open list | all lanes |

## Done when
Each topic: its doc written, RAM rows added to `ram-map.toml` (`disc = "PP"`, then `tools/ram_map.py --disc PP`),
new tests added to `ROUND1.md`'s table, PR opened and merged by the chat (docs only; merge with a normal merge
commit), project doc `claude/mapping-round2.md` updated with one short section per topic.

# The "MoA 0x…" rows on Project Phantasma (round 3 topic 2, 2026-11-26)

Disc: **PP** (Project Phantasma US, SLUS-00670) unless marked **MoA** (Master of Arena US disc 1, SLUS-01030).
Static reads of the owner's discs. Code findings only (no new data rows). Modules: PP menu FDAT 201, missions 202,
link 203, VS 204, arena 205 (load at 0x800490D0, file offset = address − 0x800490D0); boot exe SLUS_006.70
(file offset = address − 0x80011000 + 0x800).

## What was done
`ram-map.toml` had 50 PP rows copied from MoA whose text still named **59 MoA addresses** (72 mentions; the
plan's "60") that `tools/xdisc/ram_text_xlate.py` couldn't put into PP terms, plus **17 "(likely)"** twins.
Each was checked by hand: find the PP code that reads or writes the row's own PP address (lui + offset, or the
base-plus-offset of `exe_sys` / the VS frame buffers), and line it up with the MoA routine. The rows were fixed in
place under the copied-rows rule (what / size / type / status / src only). In the fixed rows, "MoA's 0x…" now
means a MoA address named on purpose.

Why the tool missed them: most rows don't say which engine the routine is in, so a module address had up to six
possible PP answers (one per engine) and the tool gave none. Most turned out to be exact routine starts in one
engine (VS for the sound / scratchpad / pose rows). The rest were routines the tools call "PP-only", or code PP
simply doesn't have.

## Result
- **46 of the 59 MoA addresses now have their PP address** (in the row's text).
- **13 have no PP counterpart**, and the row now says why:
  - `0x80058AB4`, `0x80058B04` (put the drive to rest, the disc check): PP is one disc. Its menu never calls
    CdControl, so it has neither.
  - `0x800C566C`, `0x8007DE4C`, `0x800C5FB8`, `0x800C3E3C`, `0x8005F224`: MoA-only menu code (Arena Maker
    reset, Human PLUS level step, arena writer of the 36-byte AI record, emblem-screen card scan, one card list).
  - `0x800AB7B8`, `0x800AB92C`, `0x800ABEEC`: MoA's three window routines that copy the 4-byte zero constant.
  - `0x800A69C8`: MoA's hub name draw. The low-confidence PP twin `0x80098640` doesn't touch the names.
  - `0x800896AC`: first of MoA's arena AI decision routines. PP's decision code is mostly its own
    (`0x80081180` … `0x80084528`).
  - `0x801FC898`: MoA's third main-loop constant. PP has only two (see `module_bounds`).
- **17 "(likely)" twins:** 16 confirmed (each one touches the row's address at the same spot as its MoA twin)
  and the mark is dropped. `0x800B08E0` is a fair twin of MoA's arena writer `0x800BF85C`, but on PP it doesn't
  write `opponent_ai`. The row says so.

## Rows whose PP address turned out to be wrong or different
The row's address and name stay (rule), so the text now carries the PP truth:

| Row (PP addr) | Finding |
|---|---|
| `build_check_names` (0x8003BD2C) | PP's two name records are at **0x8003BDB0** (P1) and **0x8003BDD4** (P2): the VS menu 0x800A6360 / 0x800A6458 pass them where MoA's twins pass MoA's records. 0x8003BD2C is `vs_paint_pattern` |
| `scenario_flags_hi` (0x8003BD27) | PP's flag block after Human PLUS is **7 bytes** (0x8003BD24 … 0x8003BD2A, copied from 0x80042C50 by 0x800A6360; MoA copies 8), so the "high" part is 3 bytes at 0x8003BD28 |
| `rect_lower_screen` (0x8003BC88) | PP's picture screen 0x80012B04 passes **0x8003BC8C** to ClearImage |
| `const_zero4` (0x8003BCA4) | These 8 bytes are the twin of **MoA 0x8003EA48** (8 zero bytes the fight engines copy as a block), not of MoA's 4-byte menu constant 0x8003EA44 |
| `cd_paused` (0x80041450) | Not a pause flag on PP: a count (exe_sys +0x508), cleared at boot by 0x800173A0, taken down by the menu's 0x8004F8B8, entries at 0x80041454 |
| `disc_in_drive` (0x80041451) | Nothing on PP uses it: no disc check |
| `module_bounds` (0x800114E4) | Two constants on PP (0x800490D0, 0x801F45C8). PP's main loop 0x80012C74 uses a fixed 0x801F8000 as the menu clear's end. Size 12 → 8 |
| `opponent_ai` (0x80043138) | Nothing on PP's menu or arena uses it. PP's arena AI setup **0x8004DDBC** reads the opponent straight from **60-byte AI records at 0x8008E498** in the arena engine (FDAT 205 file +0x453C8), entry = byte 0x80042AEC. Lead for topic 4 |
| `arena_ai_vars` (0x801DB838) | PP has no separate AI-globals block. 0x801DB838 is where PP's AI state starts (`arena_ai`) |

## Routines matched by hand (the tools had no twin)
| MoA | PP | How |
|---|---|---|
| 0x80012348 picture screen | 0x80012B04 | same calls in the same order, ClearImage on the lower-screen RECT |
| 0x80015994 disc queue filler | 0x800163D8 | takes the queue table and the tail like MoA's |
| 0x80055768 arena AI setup | 0x8004DDBC | same job (personality bytes × 4096 ÷ 100), different source (see above) |
| 0x8008DC98 arena AI init | 0x80083DF4 | clears the state (586 bytes, MoA 584), then the same steps (+0x22E … +0x232, +0x228) |
| 0x8008D4AC per-frame AI | 0x80083A40 | the routine that reads the position-track pointer +0x228 |
| 0x8007CE84 VS per-AC update | 0x80075050 | stores the object and argument in vs_fight_sys +4 / +8 |
| 0x8008E720, 0x8008E9F0 (replay cameras 2 / 3) | 0x800846EC, 0x800849BC | same code inside 0x80084528 |
| 0x80061AD0 (vertex count store) | 0x80059F88 | same code (area entry +0x66) |

Data: PP's arena stage table is at **0x800BD6D4** (menu FDAT 201 file +0x74604; MoA 0x800D59F0, file +0x84C18).
Its pairs differ from MoA's. PP's replay camera-order table is at **0x8008DCE8** (arena, picked by the match
byte 0x800956D0). MoA's own step reads its table at 0x800980BC, so the MoA row's "0x80099FBC" looks like a slip.
The MoA row was left alone (append-only).

Exe block layout: PP's `exe_sys` fields from the disc queue on (+0x74) sit 4 lower than MoA's up to the queue
head / tail, and 0x58 higher from the event handles on (`sys_handles` +0x3E8, `link_sio` +0x3FC,
`disc_buffer_ptr` +0x504). The rows quote MoA offsets where PP's weren't checked one by one.

## Handed over to the build chat
No new seeds: every routine above is already a routine start in `xdisc/pp-catalogue.tsv`.

## Test
Test 34 in `ROUND1.md`: watch 0x8003BDB0 / 0x8003BDD4 while renaming in VS.

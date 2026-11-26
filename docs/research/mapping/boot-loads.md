# Boot, loads and mode launch — Project Phantasma (Lane 3)

What Project Phantasma loads from the disc, what goes into video memory (VRAM) and what state it sets at
each step, so the launcher can start any mode directly and MoA content can be loaded into PP.

- **Disc: `PP` = Project Phantasma USA, SLUS-00670 "Version 1.0".** Every address in these PP sections is PP
  unless tagged otherwise. MoA addresses (tagged `MoA-US`) are only given to point at the older notes below.
- **How we know:** static read, 2026-11-26, of the boot exe `SLUS_006.70` and FDAT 201–206 from the pp-work
  pack, with the MoA→PP routine map (`maps/moa-to-pp-functions.tsv`) used only to find candidates. Every
  routine named here was read in PP's own code; a name is only carried over from MoA after the PP routine was
  read and does the same thing. **Nothing here has been run yet**, so every finding is status `code` unless
  marked `guess` / `?`. There is no PP build to test on.
- **Disc locations:** boot exe file offset = RAM − 0x8000F800 (`SLUS_006.70`, image at 0x80011000 from file
  +0x800). Modules: FDAT entry *n* offset = RAM − 0x800490D0 (every module loads at 0x800490D0). FDAT.T
  starts at disc sector 71,793; entry 201 is FDAT sector 8,545 (268 sectors), 204 = 9,089, 205 = 9,221.
- RAM rows for the shared RAM map: `boot-loads-pp-ram.toml` (same format as `ram-map.toml`, with a `disc`
  field; for Lane 1 to merge).
- The Master of Arena notes (both this file's old part, further down, and `mode-launch.md`) are reference.

## PP-0. Plain summary

- PP starts the same way MoA does: one small boot program owns a loop; it loads the **menu** (FDAT 201) to
  run everything outside a fight, and when a fight should start, the menu sets the **mode byte** and returns.
  The boot program then loads the matching **fight engine** into the same memory and runs it. After the
  fight, the menu is loaded again and uses a few **resume flags** to decide where to reopen (title, hub,
  VS menu, link menu).
- Almost every routine MoA uses for this exists in PP, but **the flag and variable addresses are all
  different** and a few flags swapped roles (table in PP-3). MoA values cannot be reused.
- Differences that matter for direct starts: PP has **one disc**, so there is no "which disc is in the drive"
  check; logos and intro are **movies** (ASLOGO, ACOPA) played by one routine with a table; PP's **LOAD** is its
  own routine with its own copy table (PP-5b); the compile of the stat block is `0x8009A238`; there is a
  **garage test fight** (`0x80070BC8`) that runs in the missions engine on stage 19.
- The fight engines never read the part-texture archives (same as MoA): the menu must have put the AC's
  textures in VRAM before any fight.

## PP-1. Memory and the module area

| What | PP | MoA-US (for comparison) |
|---|---|---|
| Boot exe image | 0x80011000 … 0x8003C000 (file 178,176 bytes) | 0x80011000 … |
| Exe data that survives every module load | 0x80011000 … 0x800490D0 | … 0x80050DD8 |
| crt0 clears | 0x8003BCE0 … 0x800490D0 (`0x80011E94`) | 0x8003EA88 … 0x80050DD8 |
| main clears at power-on | 0x8003BCF0 … 0x801F8000 (`0x80012598`) | 0x8003EA90 … 0x801FE000 |
| Module base (all of FDAT 201–206) | **0x800490D0** | 0x80050DD8 |
| Area cleared before each module load | 0x800490D0 … **0x801F8000**; VS replay / link with replay flag: … **0x801F2000** | … 0x801FE000 / 0x801FC898 |
| gp | 0x8003BC7C | 0x8003EA28 |
| Menu (FDAT 201) size / end | 548,864 bytes, ends 0x800CF0D0 | 618,496 bytes, ends 0x800E9DD8 |
| VS 204 / arena 205 / missions 202 end | 0x8008B0D0 / 0x800910D0 / 0x800900D0 | |

So **only 0x80011000 … 0x800490D0 survives a module load** (exe image + exe data): garage, owned parts,
credits, flags, player blocks, VS staging, settings. Everything from 0x800490D0 up is new every time (menu
tasks 0x8019CEC0, heap, card list …). (code: the clears in `0x80012C74`, see PP-2.)

The replay flag (0x8003BCF2 ≠ 0) keeps 0x801F2000 … 0x801F8000 (24 KB) through the load, which is where the
VS replay data sits. Values: **0** normal fight, **1** replay (the VS engine writes it at `0x8004A560`; every
engine reads the flag to know it is playing back), **2** = the recorded input ran out: `0x800754E8` (VS, MoA
`0x8007D5E8` identical) reads 2 bytes of recorded input per step and, once it has read 0x3000 = 12,288 bytes,
writes 2 at `0x8007587C`, starts a 30-frame timer and ends the round. (code)

**The menu's memory** (code: heap init `0x8004F194`, called at every menu start):

| Range | What |
|---|---|
| 0x800490D0 … 0x800CEA40 | menu file (code and data) |
| **0x800CEA40 … 0x80185A40** | **menu heap**, 749,568 bytes (`InitHeap(0x800CEA40, 0xB7000)`); it starts inside the last 1,680 bytes of the file, which are zeros |
| 0x80185A40 … 0x801F8000 | fixed work memory (first use 0x80185BC8; tasks 0x8019CEC0 … 0x801B72D0, mail/mission table 0x8018D5D0, …) |

Engines have no such gap: the first fixed variable above each engine's file is 0–9,240 bytes past its end
(missions 202 +9,240, link 203 +0, VS 204 +288, arena 205 +576, replay 206 +888).

## PP-2. Power-on and the boot exe loop

1. crt0 `0x80011E94`: clear 0x8003BCE0 … 0x800490D0, gp = 0x8003BC7C, call main `0x80012598`.
2. main: clear 0x8003BCF0 … 0x801F8000, library setup, then the loop `0x80012C74` (never returns).
3. `0x80012C74`, once: graphics, pads, sound; **register archive 1 = `COM\RTIM.T`**, **loading picture
   RTIM 61** (`0x80012B04(61)`), register archives 2, 4, 6, 5, 7 … 19 (table below).
4. Loop:
   - clear 0x800490D0 … 0x801F8000, **load FDAT 201 → 0x800490D0**, call the **menu `0x80098EF0`** (raw code, no
     module header, unlike 202–206). It returns when a fight should start.
   - **Loading picture**: RTIM 61, except mode 0 (missions): RTIM *stage* + 40 (stage = player block 1 +0x25D),
     stage 19 → RTIM 61.
   - clear 0x8003BCF4, 0x8003BCF1, 0x8003BCF6, 0x8003BCF2 (replay flag), player block 1 +0x39C and player
     block 2 +0x39C (both blocks are 0x3A8 bytes apart).
   - **dispatch on the mode byte 0x8003BC86**, clear the module area, load the engine, call the entry word at
     **0x800490D4** (module header +4):

     | Mode 0x8003BC86 | Engine | Entry (header +4) | Cleared up to |
     |---|---|---|---|
     | 0 | **FDAT 202 missions** (also the garage test fight) | 0x8004A68C | 0x801F8000 |
     | 1 | **FDAT 203 link-cable versus** | 0x8004A2B8 | 0x801F8000, or 0x801F2000 with the replay flag |
     | 2 | **FDAT 204 split-screen VS** | 0x8004A228 | 0x801F8000 |
     | 2 + replay flag | **FDAT 206 VS replay** | 0x8004A280 | 0x801F2000 |
     | 3 | **FDAT 205 arena** | 0x8004A654 | 0x801F8000 |
     | other | nothing loaded, the old entry word is called again | | |

   - engine returns **1** → call the same engine again (no reload, no picture); **2** → back to the dispatch
     (no menu, flags not cleared; this is how VS → replay works); anything else → `0x800305F8(2,0,0)` (reset
     the serial port, see below), loading picture RTIM 61, back to the menu.

**`0x800305F8(mode, sub, value)`** is the library's **serial-port control** (`_sio_control`; MoA `0x8002F7CC`
identical). It works on the port registers through the pointer at 0x8003BB44: mode 0 reads (sub 0 = status),
mode 1 writes (control bits, baud rate from 0x1FA400 / value), mode 2 resets. The serial port is where the
link cable plugs in; status bit 0x80 is the "other side present" line (DSR). (code)

(code: `0x80012C74` read in full.)

### Loading picture `0x80012B04(n)`
Reads RTIM *n* synchronously into the module area, sends its TIMs to VRAM (`0x80012624`), shows it, and
installs a per-frame callback at 0x80040F50 with its frame counter at 0x80040F54 (reset to 0): `0x80012710`
for n = 61 draws the moving NOW LOADING sprite (MoA `0x80012030`); `0x800129D4` for the mission pictures steps
two boxes over the picture as the counter runs (PP-only). (code)

## PP-3. Resume flags and other launch state (exe data, survive loads)

PP keeps these together at 0x8003BCxx. **Same jobs as MoA, different addresses, and two roles moved.** Initial
values are from the exe image (0x8003BCD0 … 0x8003BCDF is in the image, so it is not zeroed by crt0).

| PP | Start | Meaning | Set / cleared by (PP) | MoA-US twin |
|---|---|---|---|---|
| 0x8003BC86 | 0 | **mode byte** (table PP-2) | title rows, VS / link menu entry, `0x800AE330` (3, arena list opening `guess`), `0x800AF418` (0) | 0x8003EA2A |
| 0x8003BCD0 | 100,000 | credits (s32; the menu clamps to ±99,999,999 after a mission) | | 0x8003EA74 |
| 0x8003BCD5 | 1 | PRESS START (`0x8009E5B8`) writes **0 if L1 + R1 are held**, else 1. **Nothing reads it** (no load of any width found in the exe or FDAT 201–206): a leftover | | 0x8003EA79 |
| 0x8003BCD6 | **1** | **go to the title** (first boot; set again when a link session ends, below) | cleared when the title opens | 0x8003EA7A |
| 0x8003BCD7 | 0 | **back from the garage test fight** | set `0x80070C40`; the hub reads and clears it (`0x8004D1E8`) | (MoA used 0x8003EA7D = 5) |
| 0x8003BCD8 | 0 | **back from an arena fight** (hub opens on entry 5) | set by arena launch `0x800B141C`, cleared `0x800AE638` | 0x8003EA7C |
| 0x8003BCD9 | 0 | **return to the VS menu** | set by the VS menu `0x800A5410`, cleared on leaving VS `0x800A9670` | 0x8003EA7E |
| 0x8003BCDA | 0 | **title without logos/intro** — set only by the **link engine** (FDAT 203 `0x8004ACA0`) | cleared when the title opens | 0x8003EA7F |
| 0x8003BCDB | 0 | **return to the link-cable menu** | set `0x800A08A0` | 0x8003EA80 |
| 0x8003BCDC | 0 | **VS stage** (20 … 29) chosen in the VS menu; VS START copies it into both player blocks +0x25D | VS menu | (MoA kept a map number 1–10 at 0x8003EA81 and a table) |
| 0x8003BCDD | 0 | read after a mission (PP-9), but **never set**: no store of any width, direct or through a base register, in the exe or FDAT 201–206. The PP-9 branch it guards is dead in this version | cleared `0x80050F00`, `0x80099450`, `0x800994B4` | |
| 0x8003BCDE | 0 | **"new-mail notice already shown"**: hub state `0x8004C41C` shows "You have new mail." when `0x80093C04` reports unread mail and this is 0, then sets it. Cleared at a menu start after a mission or VS (PP-4), set by LOAD | | |
| 0x8003BCF2 | 0 | **replay flag** (PP-1) | VS / link / arena engines | 0x8003EA92 |
| 0x8003BCF4 | 0 | **match number** in the current engine session: the exe clears it before each engine load, VS START / link launch set 0, the VS, link and arena engines add 1 at each match end (and set bit *n* of player block +0x39C of the winner). **VS: even matches use player 1's stage, odd ones player 2's** (`0x8004DF20`). SCENARIO sets 0xFF (menu only) | | 0x8003EA94 |
| 0x8003BC85 | | **fight music** number, set by every launch (arena: picked by thresholds on 0x8003BD1C) | | 0x8003EA29 |
| 0x8003BD1C | | **random number** (u32, 15 bits used): the pad-reading routine `0x80059104` steps it every frame, `r = ((r + 0x3FD + pad bits) × 109) & 0x7FFF`. Launches pick the music from its low byte (≥217 → 1, ≥181 → 5, ≥145 → 16, ≥109 → 17, ≥73 → 19, else 23) | `0x80059234` | 0x8003EABC |
| 0x8003BD10 … 0x8003BD17 | 0 | **win / loss counters** (4 × u16, capped at 999): on a fight's end the engines add 1 to +0 and +6 when player 1 wins, to +2 and +4 when player 2 wins (on time-up the AC with more AP, AC +0x160, wins; equal = draw, no count). So +0 = P1 wins, +2 = P2 wins, +4 = P1 losses, +6 = P2 losses `guess` on which pair is shown as which. The arena result reads +0 ("player won") | cleared at menu start (PP-4), VS first visit, link menu, arena result | |
| 0x8003BD24 … 0x8003BD2A | 0 | 7 per-AC extras (Human PLUS level and flags); cleared at SCENARIO / VS rows; LOAD fills +4 … +6 | | 0x8003EAC8 … (8 bytes) |
| **0x8019CEA0** (u16) | | **"start the fight now"**: the menu's frame loop `0x80098640` runs until it is non-zero | written by `0x8005C788`, `0x800B151C` (arena) and VS START (through base 0x8019CE90 +0x10) | 0x801B1CC8 |

Note for hooks: the VS START write uses `0x8019CE90 + 0x10`, so a search for "0x8019CEA0" alone misses it.

## PP-4. Every menu start (`0x80098EF0`, before any screen)

Runs at power-on and after every fight. (code, read in full.)
1. Clear 0x801B72D4; library resets (`0x8004F194`, `0x8004F148`); draw/display environments.
2. `0x800654B0`, `0x80097C94`, `0x80097CE4(0x801B7C90)`; reset 10 records of 0x38 bytes at 0x8019CC60;
   **task list init `0x80098604(0x8019CEC0)`** (tasks: 0x8019CEC0 … 0x801B72D0, 0xD0 bytes each).
3. `0x8004ED60`, `0x8004EDBC`, `0x8004EE58`, `0x8004EBF8(0, 0)`.
4. **PICTURE MENU_TIM 0** (synchronous, straight to VRAM; MoA queued it instead). `0x80012C1C`, clear
   screen, `0x800590A4`.
5. No disc check (PP has one disc; MoA's `0x80058B04` has no PP twin).
6. If 0x8003BCDB (link) and 0x8003BCDA: 0x8003BCDB = 0, **0x8003BCD6 = 1** (a finished link session goes back
   to the title, without logos).
7. If not VS-return, not link-return and 0x8003BCD6: **settings defaults `0x800AA428`**. Always **apply
   settings `0x800AA4B8`**.
8. Resume, in this order:
   1. If not back from a test fight (0x8003BCD7 = 0) and not back from the arena: 0x8003BCDE = 0. If none of
      VS / link / arena is set: clear the win / loss counters 0x8003BD10 … 17 (PP-3).
   2. **0x8003BCD9 (VS)** → PICTURE MENU_TIM 0, mode = 2, **VS menu `0x800A42DC`**.
   3. **0x8003BCDB (link)** → PICTURE MENU_TIM 0, mode = 1, VRAM moves (x 0x2C0 / 0x300, CLUT rows 0x1E6 …,
      back-weapon blocks), **link menu `0x800A04A8`**.
   4. **0x8003BCD6 (title)** → title `0x8009E308(0x8003BCDA ? 1 : 0)`, clear 0x8003BCDA and 0x8003BCD6, then
      **StoreImage (368,225,16,1) → 0x80043B80 and (448,192,16,64) → 0x80043180** (part of MENU_TIM 0 the hub
      puts back, PP-6).
   5. mode 0 → **back from a mission / test fight** (PP-9).
   6. mode 3 → **back from the arena** (PP-9).
   7. anything else → the title (as 4).
9. Run frames (`0x80098640`) until 0x8019CEA0 ≠ 0. Then: if VS-return or mode 3, put back the two windows
   (`0x8005CFF0`, x 0x3C0); otherwise if link-return the same with another buffer; `0x8004F690`; wait for the
   disc queue; `0x800681D4`, `0x8004EE9C`; return to the exe.

## PP-5. Title → Scenario → hub

### PP-5a. Title (`0x8009E308(skip)` and its states)

| State | Does |
|---|---|
| open `0x8009E308(skip)` | new task; skip = 1 → straight to `0x8009E4DC`; else `0x8009E390`. Clears the movie counter 0x800BD39C |
| `0x8009E390` | wait for the disc queue → `0x8009E408` |
| `0x8009E408` | **movies** by the counter 0x800BD39C: 0 → movie 0 (ASLOGO), then 1 → movie 1 (ACOPA) with "can skip"; then the title picture. After idling on the title the counter is 2 → movie 2 (DEMOPLAY), then back to 0 |
| `0x8009E4DC` | `0x800185C4(14)`, `0x8002B220(14)` `?`, **TIM queue MENU_TIM 3** (title picture) → `0x8009E53C` |
| `0x8009E53C` | wait for the disc, title widgets `0x8009F69C` (76, 82), `0x8009F75C` → `0x8009E5B8` |
| `0x8009E5B8` | PRESS START (clears 0x8003BCD5) → SCENARIO / VS rows → `0x8009E9D0` |

**Movie player `0x8009F7E4(n, can_skip)`** reads the table at **0x800BD448** (FDAT 201 +0x74378), 8 bytes per
movie: name pointer, frame count. 0 `\GG\STR\ASLOGO.STR;1` 110, 1 `ACOPA.STR` 1,415, 2 `DEMOPLAY.STR` 2,160,
3–7 `ACED5.STR` 10, 8 `STAFF.STR` 3,890. It allocates its own buffers from the menu heap and frees them.
(data + code) Who plays what (the only three calls): the title plays 0–2; **`0x80099D74` plays 6** (ACED5; the
same routine raises the Human PLUS level 0x8003BD24, so this is the Human PLUS scene `guess`); **`0x8004E1F0`
plays 8** (staff roll), then opens the title. Entries 3, 4, 5 and 7 are never played.

**So "skip logos and intro" = open the title with skip = 1** (what the link engine does through 0x8003BCDA).
Setting 0x8003BCDA = 1 before the first menu start should do the same for a normal boot `guess` (it is only
read at the title branch of PP-4, but also clears 0x8003BCDB first, which is 0 at boot).

### PP-5b. SCENARIO / VS rows (`0x8009E9D0`)

The highlighted row is read as a Y position (task +0x14 → +0x4C): **0xB0 = 176 = SCENARIO, 0xC0 = 192 = VS**
(same as MoA).

**SCENARIO (176):** 0x8003BCF4 = 0xFF, **garage = the scenario starting AC `0x8005895C`**, mode = 0,
**`0x800AE088` = arena reset** (code): arena rank 0x80043A78 = 50, the 15 words after it from the table at
0x800BD6F0 (FDAT 201 +0x74620: 9,000, 7,500, 7,000, 6,500, 4,500, 4,300, 3,500, 3,400, 2,700, 2,500, then 2,400 ×
5; meaning Lane 8's), and the 50 "beaten" bytes at 0x80042E60 cleared, settings defaults + apply, clear 0x8003BD24 … 0x8003BD2A, kill task group 2, **PICTURE MENU_TIM 0, 1
and 4**, window `0x80059658`, → **NEW GAME / LOAD GAME menu `0x8009ECF8`**. (MoA also reset its VS staging
here; PP doesn't.)

**VS (192):** if the second pad is connected (0x8019CEA8 bit 1) or the serial-port status (`0x800305F8(0,0,0)`,
PP-2) has bit 0x80 set → split-screen:
`0x8006844C`, **garage = VS default AC `0x80058B04`**, clear 0x8003BD24 … 2A, kill group 2, PICTURE MENU_TIM 0,
**mode = 2, VS menu `0x800A42DC`**. Otherwise → the same but **mode = 1, link menu `0x800A04A8`**.

### PP-5c. NEW GAME / LOAD GAME (`0x8009ECF8`)

- Opens: wait for the disc, card-window VRAM moves (`0x8005B6DC` / `0x8005B7B0`, x 0x3C0), texts.
- **NEW GAME:** **queue the AC's part textures `0x800730A4`** + **TIM queue MENU_TIM 7**, kill group 4 →
  `0x8009F158` → `0x8009F344`: **credits = 100,000**, `0x8005DD30`, `0x8005A79C`, `0x8005E01C`, **name entry
  `0x80080210`** → `0x8009F3C0` → `0x8009F43C` (wait for the disc, message `0x8005B100(1, 0x800BD43C)`) →
  `0x8009F4C8` (wait for a press, kill group 4) → `0x8009F560`: **open the hub `0x8004BFA0`**, then
  `0x80085B50`, `0x80093B28`, `0x8008A9B0`, `0x800938E8`.
- **LOAD GAME:** kill group 4 → `0x8009F220` → **card screen `0x800501DC(task, callback 0x8009F274)`**. The
  callback either opens the hub (`0x8004BFA0`) or, for the same result as MoA's "78", goes on to the NEW GAME
  setup `0x8009F344`.
- Back: PICTURE MENU_TIM 0, back to the title picture state `0x8009E4DC`.

### PP-5d. LOAD = `0x80066C44` (PP's own; MoA's `0x80073424` has no twin)

Reads the save into a 3,968-byte heap buffer (`0x8004F0C4(0xF80)`), up to 3 tries, **checksum `0x80065E7C(buf +
560)` must equal the word at buf + 512** (3,288 bytes from +560, the range the save builder zeroes). Then
copies (code, every loop bound read):

| Save | Bytes | → RAM (PP) | What (from where the game uses it) |
|---|---|---|---|
| +560 | 12 | **0x800342B4** | garage part ids (MoA 0x80036FB0) |
| +572 / +573 | 1 + 1 | 0x8003BCCE / 0x8003BCCF | paint pattern / variant (MoA 0x8003EA72/73) |
| +574 | 2 | 0x8003BCCC | option parts (MoA 0x8003EA70) |
| +576 | 4 | 0x8003BCD0 | credits |
| +580 | 32 | 0x800344F0 | AC name and pilot name, 16 bytes each (`0x80058F08` resets them to two ">" placeholders from 0x800B4604; the VS menu copies them) |
| +616 | 96 | 0x800439E0 | owned-part bitmasks (the card finish rebuilds them) |
| +712 | 48 | 0x800342C0 | paint |
| +760 | 256 | 0x800342F0 | owned parts |
| +1016 | 256 | 0x800343F0 | shop stock: NEW GAME fills it from a 256-byte table at 0x800B42D4 (right after the starting owned parts at 0x800B41D4) with 121 non-zero entries, values 1 and 2; same place and shape as MoA's shop stock. What 2 means: Lane 8 |
| +1272 | 2,048 | VRAM **(544,384) 16×64** | emblem pixels (LoadImage, rect at 0x800B48A4) |
| +3320 | 32 | VRAM **(256,510) 16×1** | emblem colours (rect 0x800B48AC) |
| +3352 | 120 | 0x80043108 | scenario / mission progress (the mission return reads +0, +4, +0x38, +0x42) |
| +3472 | 56 | 0x80043A40 | settings record (56 bytes, as MoA) |
| +3528 | 64 | 0x80043A78 | **arena**: u32 player rank (new game 50, −1 for each new opponent beaten, `0x800AE59C`), then 15 u32 from 0x800BD6F0 (PP-5b) |
| +3596 … +3598 | 3 | 0x8003BD28 … 0x8003BD2A | extras (Human PLUS …) |
| +3599 | 192 | 0x80043AB8 | saved copy of the menu table at 0x8018D5D0: `0x80093830` (LOAD, NEW GAME) copies it in, `0x80093634` copies it back. The table is used by the routines around the mail and mission screens (`0x80095F9C`, `0x80096DEC`), so **mail state** `guess`; Lane 8 |
| +3795 | 52 | 0x80042E60 | **arena "beaten" flags**, one byte per opponent number (50 used; cleared by the arena reset). After a won arena fight, if the opponent's byte (opponent 0x8003BD2E) is not yet 1 it becomes 1 and the rank goes up by one |

Then: DrawSync, **apply settings `0x800AA4B8`**, `0x8008A9B0`, `0x80085B50`, **`0x80085750`** (mission
progress copy-back, MoA `0x80093BC4`), `0x80093B28`, `0x80093830` (the +3599 table into 0x8018D5D0), **0x8003BCDE
= 1** (no mail notice straight after a load).
The save **builder** (DATA SAVE) is `0x80065ECC`: header "SC", 0x11, 1, title text from 0x800B48B4, icon
palette from 0x800B48F4, then the same table the other way. Lane 8 owns the save format; this table is here
because a direct Load Save must replay it.

### PP-5e. The card screen's finish (`0x800531B0`, after a good LOAD)

Same steps as MoA (code):
1. owned parts 0x800342F0 → **0x8003BDF8** (256 bytes)
2. **`0x800829A4`** owned-part bitmasks, **`0x80073F08`** fill empty slots
3. opened from the title (the opener task is `0x8009E308`): **`0x8005C828(1)`** stand-ins for empty slots,
   **`0x800730A4` queue the AC's part textures**. Opened from the hub: VS/link flags hidden for the moment,
   `0x8005C828`, **garage rebuild `0x80072C14`**, `0x800730A4`, flags put back.
4. **wait for the disc queue**
5. back weapon L or R ≥ 51 (and ≠ 255): MoveImage (384,256,64,64) → (768,448), (320,482,256,1) → CLUT rows
   493 / 494 (rects 0x800B4080 / 0x800B4088).

### PP-5f. The hub opening (`0x8004BFA0`, every way in)

- New task, first state `0x8004C1AC`. Hub entry +0xAE = 5 if back from the arena (0x8003BCD8), else 0.
- **LoadImage 0x80043B80 → (368,225,16,1) and 0x80043180 → (448,192,16,64)**: the two blocks the title saved
  (rects at FDAT 201 +0x4 / +0xC). They are the third picture of **MENU_TIM 0** (image (448,192) 16×64 4-bit,
  colours (368,225)), which every menu start loads (PP-4 step 4), and which almost every stage texture load
  overwrites (PP-10). That is why the title saves them, `0x8005C5E4` saves them again before each fight, and the
  hub puts them back. A direct start that skips the title must fill the two buffers itself: StoreImage after
  the menu start (as the title does), or write them from MENU_TIM entry 0. MoA's recomp found StoreImage read
  back black there, so writing them from the disc entry is the safer road (MoA `boot-loads.md` §10).
- `0x800592B8` full-screen layer, `0x8004D33C`; back weapon ≥ 51: MoveImage (768,448,64,64) → (384,256) and
  (0,493,256,1) → (320,482); `0x8005DD30`; **hub ring task `0x8004DB68`**:
  - **sound bank `0x8005ADA8(2)`**, **TIM queue MENU_TIM 1 and 4**, **MODEL slot 15 ← MENU_TMD 1**
  - wait for the disc, **AC part models `0x80072C3C`**, **MODEL slot 16 ← MENU_TMD 2**, wait again.
- **The hub never loads the AC's textures** (same rule as MoA): LOAD's finish, NEW GAME or the arena return
  must have queued `0x800730A4`.

## PP-6. VS: menu and VS START

### PP-6a. VS menu (`0x800A42DC`, first state `0x800A460C`)
- ~20 MoveImage calls build the screen from blocks already in VRAM (x 0x380 / 0x2C0, CLUT rows …), **TIM queue
  MENU_TIM 6**.
- **First visit (0x8003BCD9 = 0):** garage = VS default AC (`0x80058B04`), settings defaults, option parts
  0x8003BCCC = 0, `0x80058F08`, settings 0x80043A40 → 0x800439B0 (48), names 0x800344F0 → 0x80042FD0 /
  0x80042FF4 (32 each), extras 0x80042C50 / 0x80042C57 cleared; then 0x8003BCD9 = 1 (`0x800A5410`).
- Stage: 0x8003BCDC (20 … 29). Random / "next" picks come from a 10-entry list at `0x800A469C` (20 … 29, code).
- Later states: **sound bank 2**, **TIM queue MENU_TIM 1 and 4**, **MODEL slots 15 / 16 ← MENU_TMD 1 / 2**,
  **TIM queue MENU_TIM (stage + 32)** = the stage preview (MENU_TIM 52 … 61), AC part models `0x80072C3C`.

**VS staging area** (survives loads):

| PP | Per player | What | MoA-US |
|---|---|---|---|
| 0x80042C38 | +12 | 12 part ids | 0x80045940 |
| 0x80042C50 | +7 | extras → 0x8003BD24 at VS START | 0x8004A7D0 (+8) |
| 0x8003BD20 | +2 (u16) | option parts → 0x8003BCCC | 0x8003EAC0 |
| 0x8003BD2C / 0x8003BD18 | P1 | paint pattern / variant (restored into the garage after VS START) | 0x8003EAD2 / 0x8003EAB8 |
| 0x80042C60 | 48 | paint `guess` (copied with the garage paint 0x800342C0) | 0x8004B730 |
| 0x80042FD0 / 0x80042FF4 | 36 | build-check / name buffer | 0x8003EAD8 / 0x8003EAFC |
| 0x8003BDB0 / 0x8003BDD4 | 36 | names | 0x8004AB18 / 0x8004AB3C |

### PP-6b. VS START (`0x800A6360` → `0x800A6458` → `0x800A654C` → `0x800A69F8`)
**No disc reads.** (code)
1. `0x800A6360` / `0x800A6458`: for P1 then P2, parts into the garage, extras into 0x8003BD24, **build check
   `0x80070C5C`**; a failed check shows a message (`0x8005B884`) and stops.
2. `0x800A654C`, per player: parts → garage, extras → 0x8003BD24, options → 0x8003BCCC, **compile
   `0x8009A238`** (the stat block: writes player block +0x258 on; Lane 5), **`0x800A6DEC`** (only when task
   +0x81 is set: the same player 1 ↔ player 2 texture-set swap as the arena's `0x800B16A0`, PP-10a, plus a
   redraw of two VS windows), back-weapon MoveImages (rects 0x8004B83C / 0x8004B844). Then P1's paint/options back into the
   garage, kill groups, windows.
3. `0x800A69F8`: **stage 0x8003BCDC → player blocks +0x25D (0x80042745) and +0x605 (0x80042AED)**, music
   0x8003BC85 (from 0x8003BD1C), 0x8003BCF4 = 0, **0x8019CEA0 = 1** → the exe loads FDAT 204.

After the fight 0x8003BCD9 is still 1, so the menu reopens on the VS menu.

## PP-7. Arena fight launch (menu)

- Mode 3 is set in `0x800AE330` (the arena list opening `guess`), back to 0 at `0x800AF418`.
- `0x800B0EFC`: wait for the disc, **texture-set swap `0x800B16A0`**, **`0x800B1538(records, n)`** queues the
  opponent's textures (MoA `0x800C0D14`, same shape), → `0x800B0F7C`.
- `0x800B0F7C`: wait for the disc, swap again, **compile the player `0x8009A238`**, back-weapon moves; **park
  the player's build in the VS staging area** (0x80042C38, extras 0x80042C50), put the **opponent's build in the
  garage** (parts, options, extras), swap, back-weapon moves, **compile the opponent**, put the player's build
  back, free a heap buffer, close windows → `0x800B13C0`.
- **Launch `0x800B13C0`:** mission slot 0x80043108 = −1 and copy-back (`0x800857A4`, `0x800938E8`,
  `0x80085750`), **0x8003BCD8 = 1**, opponent 0x8003BD2E = list entry +8, three heap buffers freed, **stage =
  table 0x800BD6D4[0x8003BCD4 × 2]** into both player blocks +0x25D, music 0x8003BC85, **0x8019CEA0 = 1** →
  FDAT 205.
- Arena stage table **0x800BD6D4** (FDAT 201 +0x74604), 12 pairs (stage, stage − 20): 20, 23, 24, 25, 28, 29,
  30, 32, 33, 34, 35, 36. (data)

## PP-8. Missions, the garage test fight, link

- **Mission launch `0x800870F0`** (MoA `0x8009580C`, identical): once the mission screen task (`0x800895BC`)
  is gone, **compile `0x8009A238(0)`** and **`0x8005C5E4`** (below). Mode stays 0. Which routine picks the
  mission and sets its stage is Lane 8's.
- **Garage test fight `0x80070BC8`** (MoA `0x8007DB44`; PP-only behaviour): mission slot −1 and copy-back,
  **stage 19** into player block 1 +0x25D, music 0xFF, compile, **0x8003BCD7 = 1**, `0x8005C5E4`. Mode stays 0,
  so it runs in the **missions engine** (MoA used the arena engine for this). Stage 19 also explains the
  loading-picture exception in PP-2.
- **Link-cable versus launch** (state `0x800A3504`, installed at `0x800A319C` in the link menu; code):
  1. two 2,800-byte heap buffers: this console's packet (pointer 0x801E1688) and the other one's (0x801B72D0);
  2. **compile the local AC** (`0x8009A238(0)`) and pack it: garage parts, options, paint pattern / variant,
     paint, stage (player block 1 +0x25D), and the emblem read back from VRAM (StoreImage (544,384) 16×64 and
     (256,510) 16×1);
  3. **swap packets over the cable**: `0x800A402C` / `0x800A40D8` (MoA `0x800B0A04` / `0x800B0AB0`, identical);
  4. **move the local AC's textures from the player 1 slots to the player 2 slots** (MoveImage (704,256) and
     (768,256) 64×256 → y 0, colour rows 486–494 → 495, emblem (544,384) → (560,384), its colours row 510 → 511);
  5. **load the other AC into the player 1 slots**: its emblem by LoadImage, its stage → player block 2 +0x25D,
     its build into the garage, compile it (`0x8009A238(1)`), its colours by the paint ramp `0x80073324`, its part
     textures by **PICTURE (synchronous)** from HEAD_T, CORE_T, ARMS_T, LEG_T, …, BST_T; then the local build
     back into the garage;
  6. free both buffers, 0x8003BCF4 = 0, music 17, `0x8005C5E4`.
  So on each console the other player's AC sits in the player 1 texture slots. Which side the link engine then
  treats as "me" is the link engine's business (not read here).
- **`0x8005C5E4` = "leave the menu for a fight"** (shared by the three above): kill task groups 2 … 20, a task
  whose first state `0x8005C750` waits for the disc queue and then `0x8005C788` sets **0x8019CEA0 = 1**;
  back-weapon VRAM moves; **StoreImage the hub's two blocks back into 0x80043B80 / 0x80043180** (rects
  0x800B46E8 / 0x800B46E0).

## PP-9. Back from a fight (menu entry, PP-4 step 8)

- **Mission / test fight (mode 0):** `0x80085854`, `0x800939A0`, `0x800857A4`, `0x800938E8`, credits clamped to
  ±99,999,999. (A branch on 0x8003BCDD, title or texture reload, never runs:
  PP-3.) If progress +0 = 10 with progress +0x42 bit 1, +4 ≠ 0 and 0x8003BD25 = 0:
  **PICTURE MENU_TIM 0, 1, 4 and `0x80093D84`**, then `0x8004E010` (PP-only; leads to the ending: the staff
roll `0x8004E1F0`, PP-5a, is the state installed at `0x8004E1AC` / `0x8004E1D8` `guess` on the exact chain). Otherwise **PICTURE MENU_TIM 0,
  wait for the disc, open the hub**. This road does **not** queue the AC's textures, and doesn't need to: no
  stage texture load touches player 1's texture slots (PP-10a).
- **Arena (mode 3):** the same result calls, ~40 MoveImage calls that rebuild the hub windows from VRAM,
  back-weapon moves, **queue the AC's textures `0x800730A4`**, **PICTURE MENU_TIM 0**, wait, **open the hub**
  (entry 5).
- After the missions engine's results the engine itself reads **RTIM 62** (results picture,
  `0x80088E30` in FDAT 202) and clamps credits at −999,999 there.

## PP-10. The fight engines' loads (FDAT 202–206)

All five load the same way (addresses: VS 204 first; code):

| Load | What | VS 204 |
|---|---|---|
| **Body archive**: register **archive 0 = `P{n/20}\PA{nn}.T`** (n = player block +0x281, the body combination; the name is patched into the string at 0x800874C4) and read entry 1, then one model per part with `0x80050C08` (entries = part id + 2, + 15, + 57, + 122 …, from player block +0x283 …) | per player | `0x8004D820` |
| **RTIM 60** → VRAM (shared fight textures `guess`: 210 KB, 8-bit) | | `0x8004DECC` |
| **RTIM [stage]** → VRAM (the stage's textures; RTIM 0 … 39 are 140–360 KB each) | | `0x8004DECC` |
| **FDAT 200** (shared battle data, 278 KB) → 0x800A2C88 | | |
| **Stage FDAT 5s + k** (engine code for that stage) → 0x8008F5D0, then **5s + 4** (stage model/map data) | k: missions 0, VS 1, link 1, arena 2, replay 3 | |

The archive-0 registration saves and restores the table cursor (0x8004144C), so it doesn't eat space each fight.
**No engine reads a part-texture archive** (only archives 0, 1, 2), so the AC textures come from the menu (rule
below). Stage number s comes from player block +0x25D (VS: 0x8008F5B0; arena 0x800956D0; missions
0x80097A18; link 0x80090490; replay 0x8008F808).

### PP FDAT layout (from the entry sizes and the reads above)
| Entries | What |
|---|---|
| 0 … 99 | mission stages 0 … 19: 5s + 0 = missions code (18–54 KB), 5s + 4 = map data (360–876 KB); +1 … +3 empty |
| 100 … 149 | stages 20 … 29 (VS / arena): +0 … +3 = code for each engine (4–6 KB), +4 = map data |
| 150 … 199 | stages 30 … 39 (arena and missions only: +1 and +3 empty) |
| 200 | shared battle data |
| 201 | menu (no header; called at 0x80098EF0) |
| 202 / 203 / 204 / 205 / 206 | missions / link / VS / arena / VS replay (header: 4, entry) |

### RTIM (63 entries)
0 … 39 stage textures (17 and 18 empty), 40 … 56 small 4-bit mission loading pictures, 57 … 59 bigger ones,
60 shared fight textures, 61 NOW LOADING (8 KB), 62 mission results.

### PP-10a. VRAM: the AC slots and what fight loads overwrite
Read from the TIM headers on the disc (every part archive entry and every RTIM entry) and from the swap
routine `0x800B16A0`, run step by step in a small simulator (data + code):

| Slot | Player 1 | Player 2 |
|---|---|---|
| part textures (8-bit) | **(704…831, 256…511)**: core (704,256) 64×128, booster (704,384) 64×32, legs (704,416) 64×96, arms (768,256) 64×96, head (768,352) 64×32, blade (768,384), hand weapon (768,416), back L (768,448), back R (768,480), 64×32 each | (704…831, 0…255) |
| part colours | rows 486 … 494 at x 0 … 255 | rows 495 … 503 |
| emblem / its colours | (544,384) 16×64 / (256,510) | (560,384) / (256,511) |
| menu-only pictures (generator, FCS, blade extra) | (320…447, 256…479) and colour rows 480 … 494 at x 320 … 575 | (448,256), (832,0), rows 496 … 510 |

The part archives always load into the player 1 slot; `0x800B16A0` (and `0x800A6DEC`) swap the whole set with
player 2's through a scratch block at (960,0).

What the fight engines' texture loads cover (every RTIM entry tested against the slots):
- **No RTIM entry touches player 1's part textures, colours or emblem.**
- **Mission stages** (RTIM 0 … 16 and 19) overwrite **player 2's** part textures and colour rows (nothing needs
  them after a mission: any later VS or arena fight loads player 2 again).
- **Every stage except RTIM 8**, and RTIM 62, overwrites at least one of the hub's two blocks (448,192) and
  (368,225) (PP-5f).
- RTIM 60 (shared fight textures) covers x 320 … 575 / y 256 … 511 and (832…959, 256…511): only menu pictures
  live there.
- RTIM 40 … 61 (loading pictures) touch none of these.

## PP-11. Disc read layer (exe)

| Routine | What it does |
|---|---|
| `0x80017244(n, name)` | register archive n: CdSearchFile `\GG\` + name + `;1` (prints "CdSearchFile error" on failure), read sector 0, copy its halfword table into a pool. Archive record at **0x8004135C + 12n**: +0 table pointer, +4 disc position |
| `0x800171B4(arc, entry, dest)` | **synchronous read** of one entry: sectors table[entry] … table[entry+1], retried until good |
| `0x80012B04(n)` | loading picture (PP-2) |
| `0x80012624(buf)` | send every TIM in a buffer to VRAM |
| `0x80050A1C(dest, arc, entry)` (in each engine) | read helper with size lookup `0x80016974` |

Archive numbers (names at exe 0x800111FC, file +0x9FC; same numbers as MoA): 0 body (per fight), 1 RTIM, 2 FDAT,
4 MENU_TIM, 5 MENU_TMD, 6 MENU_VAB, 7 CORE_T, 8 LEG_T, 9 ARMS_T, 10 HEAD_T, 11 BST_T, 12 BWL_T, 13 BWR_T,
14 WEL_T, 15 WER_T, 16 COMP_T, 17 SPEC_T, 18 GENE_T, 19 MIS.T. Music / voice XA names follow at 0x80011354.

**Table pool ("mxt_head_work"): 0x8003BF48 … 0x80040F48 = 20,480 bytes**, cursor 0x8004144C. The 19 menu-time
archives use 7,198 bytes; a fight's body archive adds 538 and gives it back. **About 13 KB free**, room for
the sector tables of ~6,600 more archive entries. (code + counted from the disc)

**Menu loaders** (FDAT 201; same as MoA, matched 1.00 and read): PICTURE `0x8004EA2C(arc, entry)` (synchronous,
TIMs straight to VRAM), TIM queue `0x8004F4D4(arc, entry)` (asynchronous), MODEL `0x8004EC98(slot, entry)`,
SOUND `0x8004F52C(n)`, read into heap `0x8004F2D8`, read with callback `0x8004F478`, **disc queue busy
`0x8004F860`**, heap alloc / free `0x8004F0C4` / `0x8004F114`, task new / find / kill group `0x8004E3A8` /
`0x8004E260` / `0x8004E91C`.

## PP-12. What a direct start must replay

| Direct start | Must not skip |
|---|---|
| **Skip logos only** | title with skip = 1 (0x8003BCDA = 1 before the first menu start `guess`) |
| **Scenario via Load Save** | PP-5b SCENARIO steps; LOAD's copy table + emblem LoadImages + the five calls + 0x8003BCDE = 1 (PP-5d); **the card finish, above all `0x800730A4`** and the wait (PP-5e); **fill 0x80043B80 / 0x80043180** (the title normally does); open the hub `0x8004BFA0`; let the menu loop drain the disc queue |
| **Scenario via New Game** | PP-5b; `0x800730A4` + TIM queue MENU_TIM 7; PP-5c NEW GAME steps (credits, the three calls, name entry or a name written in, then the hub open and its four calls); fill the two hub buffers |
| **VS without menus** | the VS menu must have opened once (MENU_TIM 6, 1, 4, sound bank 2, model slots 15 / 16); each player's AC models and textures (garage rebuild `0x80072C14` with that player's build, the way MoA's AI Battle did it); staging area (PP-6a); PP-6b steps 2 and 3; mode 2, 0x8003BCD9 = 1 |
| **Arena fight** | player textures already in VRAM; PP-7 (swap, `0x800B1538`, wait, compile both, swap back); launch values; mode 3 |
| **Test fight** | `0x80070BC8` as is (from the hub) |

## PP-13. Room for MoA content (first look)

- **Module area:** PP's area (0x800490D0 … 0x801F8000, 1,765,168 bytes) is about the same as MoA's
  (0x80050DD8 … 0x801FE000, 1,757,736 bytes). MoA's menu is 69,632 bytes bigger than PP's.
- **Menu:** the file is followed directly by a fixed-size heap (PP-1). Growing the menu file means moving the
  heap start and shrinking its size by the same amount: both are constants in `0x8004F194` (lui / addiu at
  0x8004F1A4 … 0x8004F1B4: FDAT 201 +0x60D4 … +0x60E4). How much of the 749,568-byte heap the menu really uses
  at its busiest needs a live measurement.
- **Engines:** no free room above their files (fixed variables start 0 … 9,240 bytes after them).
- **Archive tables:** ~13 KB free in the pool (PP-11), enough for many extra entries.
- **Part archives** have almost the same entry counts on both discs (PP / MoA): CORE_T 176 / 176, LEG_T 640 /
  640, ARMS_T 384 / 384, HEAD_T 288 / 288, BST_T 14 / 14, BWL_T 59 / 60, BWR_T 59 / 60, WEL_T 9 / 6, WER_T 21 /
  26, COMP_T 16 / 16, SPEC_T 19 / 19, GENE_T 15 / 15; MENU_TMD 249 / 252. Different: **MENU_TIM 524 / 885**
  (MoA keeps arena records and more pictures there), **MIS.T 833 / 409**, RTIM 63 / 69, FDAT 207 / 212. Both
  discs have 72 body archives (PA00 … PA71). So new MoA part textures and models would need new entries in
  PP's `*_T` and MENU_TMD archives (the recomp can serve extra entries from files; a disc patch would have to
  rebuild the archives).
- **Free space in the code files:** none for disc patches. Each module file ends in a block of zeros and one
  final word, but code points into every one of those zero blocks (they are the modules' zero-started variables,
  and in the menu's case the heap's start); the exe's long zero runs are library data that code uses too. So a
  disc patch has to make room (for example by shrinking the menu heap, above) rather than find it.

## PP-14. Open

Cleared 2026-11-26 (second pass): every unknown from the first pass is answered above (0x8003BCD5, 0x8003BCDD,
0x8003BCDE corrected, 0x8003BCF4, 0x8003BD10 … 17, 0x8003BD1C, the replay flag's 2, `0x800305F8`, the loading
callbacks, the movie entries, the save blocks at +580, +1016, +3528, +3599, +3795, the swap `0x800A6DEC`, the
link launch, VRAM overlap, free space). What is left needs a running game or belongs to another lane:
- **Nothing is tested.** First live check once there is a PP build or an emulator with a memory viewer: boot to
  the title and confirm 0x8003BCD6 = 0 and 0x8003BC86 = 0; pick VS and confirm 0x8003BC86 = 2 and 0x8003BCD9 = 1;
  start a VS fight and confirm 0x8019CEA0 = 1 just before the loading screen; win one VS round and confirm
  0x8003BD10 = 1 and 0x8003BD16 = 1.
- How much of the menu heap is used at its busiest (PP-13), live only.
- For other lanes: what the table at 0x800BD6F0 and the value 2 in the shop stock mean, and the mail table
  0x8018D5D0 (Lane 8); which side the link engine treats as "me" (link work).

## Handed over (from Lane 3)

- **To Lane 1:** RAM rows in `boot-loads-pp-ram.toml` (disc = "PP"); FDAT layout and RTIM layout (PP-10);
  module ends and cleared ranges (PP-1); archive table pool (PP-11).
- **To Lane 4:** player block 2 = player block 1 + 0x3A8 (0x800424E8 / 0x80042890); stage byte at player block
  +0x25D and +0x605 (= block 2 +0x25D); the main loop clears block +0x39C before each fight.
- **To Lane 5:** compile = `0x8009A238` (MoA `0x800A7BB4` has no twin; writes player block +0x258 on); the
  arena pre-fight compiles the opponent by parking the player's build in the VS staging area (PP-7); VS
  staging layout (PP-6a); garage 0x800342B4, paint 0x800342C0, owned parts 0x800342F0.
- **To Lane 7:** RTIM 60 and RTIM [stage] go to VRAM at every fight load; engines never load part textures.
- **To Lane 8:** LOAD `0x80066C44` copy table and SAVE builder `0x80065ECC` (PP-5d); checksum `0x80065E7C`;
  card list builder `0x80065948`; mission launch `0x800870F0`; test fight `0x80070BC8`; scenario progress
  block 0x80043108 (120 bytes, save +3352); movie table 0x800BD448 (6 = Human PLUS scene `guess`, 8 = staff
  roll); arena rank 0x80043A78, arena table 0x800BD6F0, beaten flags 0x80042E60, arena reset `0x800AE088`;
  mail: 0x8003BCDE "new-mail notice shown", `0x80093C04` "unread mail?", table 0x8018D5D0 saved at +3599;
  shop stock 0x800343F0 (values 1 and 2).
- **To Lane 4:** win / loss counters 0x8003BD10 … 17 and match number 0x8003BCF4 (who wins on time-up: more AP,
  AC +0x160); winner's bit in player block +0x39C.
- **To Lane 7:** the VRAM slot table (PP-10a).

---

# Master of Arena (reference, MoA-US addresses) — what the game loads, step by step

What is read from the disc, what goes into video memory (VRAM) and what state is set at each
step: power-on, title, Scenario (Load Save / New Game) → hub, VS setup, VS START, arena launch,
and the fight engines. Goal: anything that starts a mode directly (launcher, Rogue Arena, AI
Battle, netplay) can do exactly what the game does, and no step is skipped by accident.

Disc: USA disc 1 (SLUS-01030 v1.0) only. Disc 2 is not used (it has no NEW GAME). Read
statically 2026-11-17 from the boot exe and FDAT 206 (menus), 209 (VS), 210 (arena). Measured
from code unless marked `?`. Related: `mode-launch.md` (how each mode is started),
`menus-hub.md` (the hub ring).

## The rules that explain most bugs

1. **Each module load wipes the module area.** The menu clears 0x80050DD8 … 0x801FC898 before
   it loads (arena the same, VS / missions / link to 0x801FE000). Menu work memory (tasks
   0x801B1CE8, model slot table 0x801FBB00, card list 0x801F9DB8, 0x801B1CC8 …) is new every
   time. Only the exe image and its bss survive, 0x80011000 … 0x80050DD8: garage 0x80036FB0,
   owned parts 0x80036FEC, flags 0x8003EAxx, player blocks 0x80045198 / 0x80045538, VS staging,
   settings.
2. **VRAM is never cleared**, and the fight engines **do not load part textures**. The AC's
   textures must already be in VRAM when a fight starts. The menu loads them; the fight uses
   whatever is there. A skipped texture load is not a crash. The AC just draws wrong or not at
   all until some later path loads the textures.
3. **Loads are queued.** Most menu loads go into a disc queue and finish over the next frames.
   The game waits with `0x800589DC` (non-zero = still busy) before it relies on them. A direct
   start that runs the same routines from a hook must let the menu loop drain the queue (or
   wait) before it opens the next screen. **Fast Loading** (the 16-sectors-a-frame drive,
   `mod_builtin_moa_loading.c`) only covers the initial load since 2026-11-18: it goes back to
   the drive's real speed once the first usable screen (hub open `0x800552C0` or title open
   `0x800AAE94`) has opened and the disc queue has stayed empty for 30 frames. The switch back
   must use `psx_mod_set_disc_speed_live` (from the Rogue Arena merge): the plain
   `psx_mod_set_disc_speed` is only read when a session starts, so the first version left the
   drive fast for the whole session.
4. **Anything sent to VRAM must sit in main RAM.** LoadImage / StoreImage go by DMA, which
   only reaches main RAM (0x80000000 … 0x801FFFFF). Mod memory from `psx_mod_alloc_guest_memory`
   (0x81000000) folds onto low RAM there, so an upload from it sends whatever code sits at the
   folded address (seen as a garbled emblem, which the game's next SAVE then wrote back to the
   card). Copy the data into a menu heap buffer first (`0x800583A0(bytes)` → buffer,
   `0x800583F8(buffer)` frees it), as LOAD's own card buffer does. In code: `moa_quick_start.c`.
5. **Hooks on menu code run only if listed.** A mod hook on a routine in a disc-loaded module
   (menu, engines) is only called if its address is in `game.toml` `[recompiler]
   mod_function_entry_funcs` (and the code is regenerated). Unlisted, the hook silently never
   fires. Rogue Arena's old X-at-hub hook `0x80055848` was missing for this reason.
6. **A save made outside the game needs its list summary.** The DATA SAVE / LOAD list reads a
   summary before the checksummed range (menu save writer `0x80071ED8` … `0x80071FAC`): +0x184
   AC and pilot names (36 bytes, as at +0x1CC), +0x1A8 u16 sorties, +0x1AC u32 credits. Left
   zero, the list shows the save with no name until the game saves over it. In code:
   `save_summary()` in `moa_card.c`. A new game's state is needed too (§9a).

## Load routines (to read the tables below)

| Routine | What it does |
|---|---|
| `0x80016A58(archive, entry, dest)` | exe: synchronous read of an archive entry |
| `0x800585BC(archive, entry)` | menu: synchronous read into a new heap buffer |
| `0x80057CB0(archive, entry)` | **PICTURE**: sync read, every TIM in the entry → VRAM (`0x80057DD0`), DrawSync, free |
| `0x800586A0(archive, entry)` | **TIM queue**: async read, then the TIMs → VRAM (callback `0x80058A34`) |
| `0x80057F74(slot, entry)` | **MODEL**: async read of MENU_TMD entry into slot (table 0x801FBB00, sizes 0x801D0B28) |
| `0x80058700(n)` | **SOUND**: MENU_VAB entries n and n+1 (bank header + data), callback `0x80058828` |
| `0x80058644(archive, entry, cb)` | async read with your own callback |
| `0x800589DC()` | disc queue busy (exe queue at 0x80043FE8 / 0x80043FEC) |
| `0x8002C75C` / `0x8002C7BC` / `0x8002C81C` | exe LoadImage(rect, src) / StoreImage(rect, dst) / MoveImage(rect, x, y) |
| `0x800575B8(group)` / `0x80057B28(group)` | new menu task / kill every task of a group |
| `0x80057470(open_fn)` | find the live task opened by `open_fn` (0x801D0BA0 = none). "Wait until X is gone" |

Archives (registered at boot, numbers used everywhere): 1 RTIM (loading pictures), 2 FDAT
(code), 4 MENU_TIM, 5 MENU_TMD, 6 MENU_VAB, 7 CORE_T, 8 LEG_T, 9 ARMS_T, 10 HEAD_T, 11 BST_T,
12 BWL_T, 13 BWR_T, 14 WEL_T, 15 WER_T, 16 COMP_T, 17 SPEC_T, 18 GENE_T, 19 MIS.T.

### Where things sit in VRAM (from the TIM headers on the disc)

Sizes are in VRAM units (16-bit words): an 8-bit picture is twice as wide in pixels, a 4-bit one four times.

| What | VRAM |
|---|---|
| **Player's AC textures** (`*_T` archives) | core (704,256) 64×128, legs (704,416) 64×96, head (768,352) 64×32, arms (768,256) 64×96, booster (704,384), back L (768,448), back R (768,480), arm weapon L (768,384) + (320,384), R (768,416); CLUTs on rows 487–494 at x 0 |
| MENU_TIM 0 | (448,0) 64×216, (416,0) 16×64, (368,96), (960,0) 64×255 + CLUTs (0,481), (368,224/225), (960,255) |
| MENU_TIM 1 | hub banner icons (96×48 px) at x 320/368 (y 0–192), (512,0) 64×256, (672,0), (368,192) + CLUTs row 480 / 229–232 |
| MENU_TIM 2 | (640,0) 128×96 16-bit |
| MENU_TIM 3, 875 | full screen 320×240 16-bit at (320,0) (title pictures) |
| MENU_TIM 4 | four 64×256 pages at (576,256), (640,256), (896,256), (960,256), CLUT (320,240) |
| MENU_TIM 6 | (960,0) 64×48 |
| MENU_TIM 7 | (768,0) 160×144 8-bit, (704,0) 64×256, (960,0) 64×240 |

`0x800C0E7C` swaps the player 1 and player 2 texture sets (pages in the 704–831 / 256–511 area,
CLUT rows 486–495) through scratch space at (960,0). VS START and the arena use it.

## 1. Power-on (boot exe)

1. crt0 `0x80011D84`: zero 0x8003EA88 … 0x80050DD8, gp = 0x8003EA28.
2. main `0x80011E40`: **zero all RAM 0x8003EA90 … 0x801FE000**, library setup.
3. `0x800124B0` once: graphics, pads `0x80016C10`, sound `0x80016DB8`, register RTIM, **loading
   picture RTIM 62**, register archives 2–19.
4. Load FDAT 206 → 0x80050DD8 and call the menu `0x800A69C8`.

Initial values from the exe image (not zeroed): credits 0x8003EA74 = 100,000;
**0x8003EA7A = 1** ("go to the title"); 0x8003EA79 = 1; mode 0x8003EA2A = 0.

## 2. Every menu start (`0x800A69C8`, before any screen)

Runs on power-on **and after every fight**:
- Display/draw environments, task list init (`0x800A698C(0x801B1CE8)`), `0x8005803C`,
  `0x80058098`, `0x80058134`, `0x80057ED4`.
- **TIM queue MENU_TIM 0.**
- **0x80044121 = `0x80058B04()`** = which disc is in the drive (reads `\GG\COM\CD.DAT`, magic
  0xBA; 0 = disc 1, 1 = disc 2, 250+ = drive errors). Disc 1 → SCENARIO opens NEW/LOAD;
  disc 2 → card load at once. We only use disc 1.
- `0x800122F0`, clear screen, `0x80063944`.
- If 0x8003EA7A (first boot / back to title): `0x800B69F8` = **settings defaults**
  (0x8004B568 ← 0x800D5860, 48 bytes; sound flags 0x80044200/02). Always `0x800B6A80` =
  **apply settings** (button layout into player block +0x250…, sound).
- Then the resume checks in `mode-launch.md` §3 (VS menu, link menu, back from mission, back
  from arena, or title).
- Title branch only, after the title opens: 0x8003EA7A = 0, **StoreImage (416,0,16,64) →
  0x8004ACA8 and (368,225,16,1) → 0x8004B6F0**. These hold part of MENU_TIM 0, and the hub open
  and the VS menu put them back.

## 3. Title (`0x800AAE94` and its states)

| State | Loads / does |
|---|---|
| open `0x800AAE94(skip)` | new task. skip = 1 (0x8003EA7F): **TIM queue MENU_TIM 875**, go straight to `0x800AB014` |
| `0x800AAF30` | wait for the disc queue; the first time → logos `0x800AAA8C` |
| `0x800AAA8C` | **TIM queue MENU_TIM 875**, draws the logo screens with their own loop |
| `0x800AAFB4` | intro movie `0x800544A0(n, 1)` (n alternates 0/1 on each attract loop). Inside: TIM queue MENU_TIM 2, `0x80054B54` movie, then `0x800B6A80` again |
| `0x800AB014` | **TIM queue MENU_TIM 3** (title picture) |
| `0x800AB04C` | wait for the disc, title widgets `0x800ACC34` ×2, `0x800ACCF4` |
| `0x800AB0C8` | PRESS START: random seed `0x800A7B80` (0x8003EABC), 0x8003EA79 = 0; idle → TIM queue MENU_TIM 2, back to the movie |
| `0x800AB2FC` → `0x800AB4F0` → `0x800AB500` | SCENARIO / VS rows, then the choice |

**SCENARIO picked (`0x800AB500`, row 176):**
1. 0x8003EA94 = 255, mode = 0.
2. `0x800631FC`: **garage = the scenario starting AC** (parts 0x800CBA0C, options 0x800CBA18,
   owned parts 256 bytes from 0x800CBA1C).
3. `0x800BA854`: arena tables reset (0x8004B5A0 from 0x800D5A08, arena lock 0x8004B686).
4. `0x800C566C`: VS staging area at 0x80045958 / 0x8004A7C4 reset.
5. `0x800B69F8` settings defaults, `0x800B6A80` apply.
6. 0x8003EAC8 … 0x8003EACF = 0 (Human PLUS, Ex Arena, overweight flags).
7. Kill task group 2; **PICTURE MENU_TIM 0, 1, 4** (synchronous, straight to VRAM).
8. Disc 1 → NEW GAME / LOAD GAME menu `0x800AC3AC`.

## 4. Scenario → hub

### 4a. LOAD GAME

1. NEW/LOAD menu, cursor 0 + ✕ (`0x800AC4B4`): kill group 4, dialog, then `0x800AC8E8` →
   card screen `0x80059554(task, callback 0x800AC93C)`. **The card screen loads nothing from the
   disc.** It draws its windows by moving VRAM blocks (`0x80065D00` / `0x80065DD4` at x 960).
2. LOAD `0x80073424` ('N' save): checksum, inline unpack into the exe area, **LoadImage emblem
   (544,384) and its CLUT (256,510)**, then `0x800B6A80`, `0x80099098`, `0x800941F4`,
   `0x80093BC4`, `0x800A2318`, `0x800A2020`, 0x8003EA82 = 1.
3. **The card screen's finish `0x8005CE68`** (runs only when LOAD succeeds):
   - owned parts 0x80036FEC → **0x8003EB20** (256 bytes)
   - `0x80090FD8` part lists, `0x80083130` defaults for empty required slots (both in §4d)
   - opened from the title: `0x80067244(1)` + **`0x800822B0` = queue the AC's part textures**
     (from the hub's DATA screen: `0x80081E20` garage rebuild as well)
   - **wait for the disc queue**
   - back weapon id ≥ 52: MoveImage (384,256,64,64) → (768,448), (320,482,256,1) → (0,493) /
     (0,494)
4. Callback `0x800AC9E8`: **open the hub** `0x800552C0`, delete the card-screen task.

### 4b. NEW GAME

1. NEW/LOAD menu, cursor 1 + ✕: **`0x800822B0` (AC textures)** + **TIM queue MENU_TIM 7**,
   kill group 4 → `0x800AC820` → `0x800ACA0C`.
2. `0x800ACA0C`: credits = 100,000, `0x80068A58`, `0x80064918`, `0x80068D7C`, name entry
   `0x8008E8D8` (callback `0x800ACA88`).
3. `0x800ACB10`: **open the hub**, then `0x800941F4`, `0x800A2318`, `0x80099098`, `0x800A20D8`.

### 4c. The hub opening (`0x800552C0`, every way in)

- New task (open `0x800552C0`, first state `0x800554F4`). Hub entry +0xBE: 5 or 6 after an
  arena fight (Ex Arena when 0x8003EAD4 ≠ 0), 7 when 0x8003EA7D = 5, else 0.
- **LoadImage 0x8004ACA8 → (416,0,16,64), 0x8004B6F0 → (368,225)**: the blocks the menu saved
  after the title.
- `0x80063B54` full-screen layer, `0x80056564` (stored at +0x80).
- Back weapon id ≥ 52: MoveImage (768,448,64,64) → (384,256), (0,493,256,1) → (320,482).
- `0x80068A58`, then the hub ring task `0x80056DBC`:
  - **SOUND MENU_VAB 2 + 3** (`0x80064F24(2)`)
  - **TIM queue MENU_TIM 1 and 4**, **MODEL slot 15 ← MENU_TMD 1**
  - waits for the disc, then `0x80081E48` = **AC part models** into slots 3–14 (from the
    garage, 0x80036FB0), **MODEL slot 16 ← MENU_TMD 2**, waits again.
- **The hub never loads the AC's textures.** Whatever ran before it must have queued
  `0x800822B0` (LOAD finish, NEW GAME, or the arena return path).

### 4d. The AC helpers in the finish (2026-11-18)

None of them changes a full, legal AC, so a real save (or a launcher save with a legal build)
comes through untouched.
- **`0x80090FD8` = owned-part bitmasks.** For each part kind (mask pointers `0x800CDE44`, part
  counts `0x800CDE6C`), bit i is set when owned copy 0x8003EB20 holds part i of that kind
  (id from `0x800996AC(kind, i)`). Masks at 0x8004B508 … 0x8004B560 (8 bytes each). The garage
  and shop lists read them.
- **`0x8008859C(kind)`** = the first owned part of a kind (lowest set bit), 255 if none.
- **`0x80083130` = fill empty slots.** An empty core, head, arms, legs, generator, FCS or
  booster gets `0x8008859C` of its kind. Weapon arms filled in this way also clear the hand
  weapon and blade. Then the fit rules run on every AC (read 2026-11-18, the same as the ASSEMBLY
  page, `menus-screens.md` §2h):
  - caterpillar legs (id ≥ 27): no booster, and no radar 40, 41, 42, 44 or 46 in either back slot
  - WG-1-KARASAWA (hand 15) comes off legs that can't hold it: 5, 21–24, 26 and the
    caterpillars (table 0x80052358)
  - ANKS-1A46J arms (5): no back weapons below 40, no WX-S800-GF (53)

  A build the game itself made never breaks these, so nothing changes.
- **`0x80067244(flag)` = stand-ins for empty slots.** Only for slots that are 255: MODEL
  queue of the first owned part of that kind (MENU_TMD entry + per-kind offset 3, 14, 54, 78,
  96, 202, 237), TIM queue of its textures (same entries as §7, texture set from
  **0x8003EA72**, the player's own byte 14), then `0x80083130` when flag ≠ 0. A full AC loads
  nothing here; its models and textures come from the hub open and `0x800822B0`.
- **`0x800822B0` = queue the AC's part textures** (2026-11-18). It is the player's version of
  §7's table, with v = texture set 0x8003EA72:

  | Archive | Entry |
  |---|---|
  | HEAD_T (10) / CORE_T (7) / ARMS_T (9) / LEG_T (8) | head + 18v / core + 11v / arms + 24v / legs + 40v |
  | WEL_T (14) / WER_T (15) / BWL_T (12) / BWR_T (13) / BST_T (11) | blade / hand weapon / back L / back R / booster, skipped if 255 |
  | colours | v = 0: `0x80082530` builds them from the garage paint (LoadImage); else TIM queue MENU_TIM 26 + 4v + 0x8003EA73 |

  The garage rebuild `0x80081E20` is the same list plus GENE_T (18) generator and COMP_T (16)
  FCS (`0x8008201C`), then the part models (`0x80081E48`; which part goes in which MODEL slot:
  `menus-screens.md` §2h).

### 4e. LOAD's copy table: save → RAM (2026-11-18)

LOAD's 'N' path (`0x80073610` … `0x80073B90`) copies these blocks of the save into the exe's data,
unchanged. Offsets are in the 3,712-byte block LOAD reads (checksum at +384 over +432 … +3671). This
is `k_unpack` in `moa_quick_start.c`, which the launcher's LOAD GAME applies (user-tested 2026-11-18).

| Save | Bytes | RAM | What |
|---|---|---|---|
| +440 | 12 | 0x80036FB0 | garage part ids |
| +452 | 2 | 0x8003EA72 | paint pattern / variant (0x8003EA72 = texture set, §4d) |
| +454 | 2 | 0x8003EA70 | option parts |
| +456 | 4 | 0x8003EA74 | credits |
| +460 | 36 | 0x800371EC | AC name, pilot name |
| +496 | 96 | 0x8004B508 | owned-part bitmasks (the finish rebuilds them anyway, §4d) |
| +592 | 560 | 0x80036FBC | paint, owned parts (+640 → 0x80036FEC), shop stock (+896 → 0x800370EC) … |
| +3232 | 44 | 0x8004AC50 | mission progress (§9a) |
| +3276 | 288 | 0x8004B568 | settings, then the arena tables from 0x8004B5A0 (+3332): opponent counts +3553, arena lock +3562 |
| +3564 | 8 | 0x8003EAC8 | Human PLUS and the flags after it |
| +3572 | 100 | 0x8004B688 | rest of the arena state (+3607 → 0x8004B6AB) |

Besides these, only the emblem (+1152 → VRAM (544,384)) and its colours (+3200 → (256,510)) leave
the save, by LoadImage (§4a step 2). +384 is the checksum and +388 … +431 the list summary (rule 6).

## 5. VS setup (`0x800AB500` row 192 → `0x800B0CB4`)

- Row: `0x80074DC0` (display env 0x800CD3B4), `0x800633A4` = **garage = the VS default AC**
  (0x800CBC1C tables, all parts owned), 0x8003EAC8…CF = 0, kill group 2, PICTURE MENU_TIM (entry `?`, likely 0 as on the SCENARIO
  row), mode = 2.
- `0x800B0CB4`: about 20 MoveImage calls build the VS screen from blocks already in VRAM
  (x 0x380 / 0x2C0 and the CLUT rows 0x1E7 / 0x1F0), **TIM queue MENU_TIM 6**, first visit
  copies the garage to both players (see `mode-launch.md` §5).
- First state `0x800B0FE4`: StoreImage (`0x800637A8`) → 0x8004B6F0 / 0x8004ACA8, **SOUND
  MENU_VAB 2 + 3**, **TIM queue MENU_TIM 1 and 4**, then (`0x800B15D8`) **MODEL slots 15 / 16
  ← MENU_TMD 1 / 2**, then **TIM queue MENU_TIM (stage byte − 10)** = the **stage preview
  picture** of the chosen map (stage byte = `0x800D5800[map − 1]`, map = 0x8003EA81; entries
  10–13, 20, 23–27; each 104×100 px 8-bit at (896,0), CLUT row 506).
- Each player's AC (AC SETUP / DATA LOAD) loads its own models and textures. AI Battle's
  `moa_load_unit` already replays this with `0x80081E20` and the per-player file tables
  0x8004AB60 / 0x8004A9E0.

## 6. VS START (`0x800B2D7C` → `0x800B2E90` → `0x800B2F9C` → `0x800B348C`)

**No disc reads.** Per player: extras → 0x8003EAC8, build check `0x8007E164`, options →
0x8003EA70, **compile `0x800A7BB4(p)`**, **`0x800C0E7C` texture-set swap**, window moves,
weapon blocks MoveImage → (768,448), (0,493), (0,494). Then stage (player block +0x255), music
0x8003EA29, 0x8003EA94 = 0, **0x801B1CC8 = 1** → the exe loads FDAT 209.

## 7. Arena launch (menu)

- **Opponent's textures (2026-11-18):** state `0x800C028C` waits for the disc, swaps the
  texture sets `0x800C0E7C` (the player's set moves to the player 2 area), then
  **`0x800C0D14(records, n)`** queues the opponent's textures from its 64-byte arena record
  (records from MENU_TIM, n = arena list row +8), then sets state `0x800C030C`. TIM queue per part,
  v = record byte 14:
  | Archive | Entry |
  |---|---|
  | CORE_T (7) | +0 + 11v |
  | HEAD_T (10) | +1 + 18v |
  | ARMS_T (9) | +2 + 24v |
  | LEG_T (8) | +3 + 40v |
  | WEL_T (14) / WER_T (15) / BWL_T (12) / BWR_T (13) / BST_T (11) | +5 / +4 / +6 / +7 / +12, skipped if 255 |

  Each `*_T` archive holds 16 complete texture sets (CORE_T 176 = 11 × 16, HEAD_T 288 = 18 × 16,
  ARMS_T 24 × 16, LEG_T 40 × 16). **v = 0 (all but 12 opponent records)**: set 0, colours built from
  the record's paint at +16 (`0x800C14AC`, LoadImage into the AC CLUT rows). **v ≠ 0 (12
  opponents, v 1–15)**: a pre-painted set, colours = **MENU_TIM 26 + 4v + record byte 15**
  (CLUT row 486 + rows 488–490).
- Pre-fight `0x800C030C` / `0x800C0AD8`: wait for the disc, compile `0x800A7BB4`, swap
  `0x800C0E7C` again (the opponent's set goes to the player 2 area, the player's comes back).
- Launch `0x800C0878`: kill group 2, `0x80093C20`, `0x800A20D8`, `0x80093BC4`, 0x8004AC50,
  **0x8003EA7C = 1**, arena 0x8003EAD4 / opponent 0x8003EAD5, frees three heap buffers, stage
  from 0x800D59F0, music, **0x801B1CC8 = 1** → FDAT 210.

## 8. The fight engines (FDAT 209 VS, FDAT 210 arena)

Both load the same way (VS addresses first, arena second):

| Load | VS (209) | Arena (210) |
|---|---|---|
| Body archive: registers **archive 0 = `P0\PAnn.T`** (nn from player block +0x279, the body combination) and reads entry 1 | `0x800552B8` | `0x80055AF8` |
| AC part models from that archive, one per part (arena: entry = part + a per-slot offset 2, 15, 57, 127, 81, 99, 155, 215; VS loader not read in detail) | `0x80058548` | `0x8005A76C`, from `0x80055B54` … `0x80055D9C` |
| Loading pictures RTIM 61 and RTIM [stage picture byte] | `0x80055990` / `0x800559A8` | `0x80056218` / `0x80056230` |
| Shared battle data **FDAT 205** | `0x800559C4` | `0x8005624C` |
| Stage: FDAT 5s+1 and 5s+4 (VS) / 5s+2 and 5s+4 (arena), s = stage | `0x80055AA8` / `0x80055AC8` | `0x80056330` / `0x80056350` |

**No `*_T` texture archive is read by either engine.** The AC (and the opponent) are drawn
with the textures the menu left in VRAM, which is rule 2.

## 9. Direct starts: what each must replay

| Direct start | Must not skip |
|---|---|
| Scenario via Load Save (Rogue Arena) | §3 SCENARIO steps 1–7; LOAD unpack + emblem + six calls; **§4a step 3 (card screen's finish), above all `0x800822B0`**; then the hub open, letting the menu loop drain the queue |
| Scenario via New Game | §3; §4b step 1 (`0x800822B0` + MENU_TIM 7); §4b steps 2–3 |
| VS without menus | §5 (the VS menu must have opened once: the StoreImage blocks, sound bank, MENU_TIM 1/4/6, model slots); each player's AC models + textures (`moa_load_unit`); §6 |
| Arena fight | the player's textures in VRAM (rule 2); the opponent's set: swap, `0x800C0D14`, wait, swap (§7) |

In code: `games/master-of-arena/moa_quick_start.c` replays the Load Save and New Game rows for
the launcher's START GAME menu (LOAD GAME / NEW GAME / LAUNCH GAME with FULL BOOT).

## 9a. What a save must hold besides the AC (2026-11-17)

LOAD copies the save into RAM unchanged, so a save made outside the game must carry a new
game's state, not zeros:
- **+3232, 44 bytes → 0x8004AC50** (mission progress). After the unpack, `0x800941F4` resets
  the mission screen's table T = 0x8019F7A8 and `0x80093BC4` copies 0x8004AC50 back over
  T+0…T+43. **T+1 = the slot whose briefing reopens at once (-1 = none).** Zero there means
  "slot 0", so MISSIONS skips the list start (and the mod's mission source picker). New game:
  FF FF 00 00 01 (T+0 = -1, T+1 = -1, chapter 0, T+4 = 1).
- **+3553 → 0x8004B67D = 0x8004B5A0 + 221**: opponent counts of the 9 arenas (from
  0x800D5A08: 50 10 10 10 10 16 31 10 20), **+3562 → 0x8004B686 = arena lock (2)**. Zero =
  empty arenas. +3607 → 0x8004B6AB: the rest of the arena state.
- +896: shop stock (1 = on sale, cleared when bought).
The full list is `games/master-of-arena/moa_arena_run_save.inc` (read from a fresh NEW GAME
save). `moa_card_to_save` starts from it since branch `load-game-fixes`.

## 10. Open

- ~~Hub background black after the direct LOAD GAME~~ fixed 2026-11-18 (merge of
  `hub-background-direct`, user-tested): the StoreImage from VRAM left the hub's RAM copies
  0x8004ACA8 / 0x8004B6F0 black in the recomp, so `moa_quick_start.c` now writes MENU_TIM 0's
  tile (VRAM (416,0) 16×64, 4-bit) and its 16 colours ((368,225)) from the disc, as Rogue
  Arena does with its belt tiles. Mission picker fix (§9a) user-tested OK 2026-11-18.

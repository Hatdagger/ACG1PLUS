# Lane 8 — Scenario side (Project Phantasma): hub, menus, missions, mail, saves, sound

Lane 8 of the 2026-11-26 restart (`PLAN.md`). First pass, 2026-11-26, **static read** of the Project Phantasma
USA disc (SLUS-00670) with the MoA USA disc 1 (SLUS-01030) beside it for comparison. Nothing here has been seen
running yet.

**How to read this doc**
- Every address carries its disc: **PP** = Phantasma USA, **MoA-US** = Master of Arena USA disc 1.
- PP menu module = **FDAT 201**, loaded at `0x800490D0`: **file offset = address − 0x800490D0**. PP boot exe
  `SLUS_006.70`: file offset = address − 0x80011000 + 0x800. Where a table is loaded from the disc, the archive
  entry is given (e.g. `MIS.T` 0 → RAM).
- Status: **code** = read from the code, **data** = read from the disc file, **guess** = not proven, `?` = unknown.
- MoA twins come from `maps/moa-to-pp-functions.tsv`, then checked by reading both routines unless marked.
- Old MoA notes this builds on: `menus-hub.md`, `menus-screens.md`, `missions.md`, `phantasma/missions-import.md`,
  `../sound-map.md`. Their addresses are MoA-US.

**Plain summary for the owner** is in §8.

---

## 1. Rules of thumb found on the way (code)

- **Globals moved by a fixed amount.** The scenario globals MoA-US keeps around `0x8003EA70…0x8003EAFF` sit
  **0x2DA4 lower** on PP: credits MoA-US `0x8003EA74` = PP `0x8003BCD0`; "back from AC TEST" MoA-US `0x8003EA7B` =
  PP `0x8003BCD7`; "back from an arena fight" MoA-US `0x8003EA7C` = PP `0x8003BCD8`; fight music byte MoA-US
  `0x8003EA29` = PP `0x8003BC85`. Checked on these four only; check any other before using it.
- **Menu tasks are 4 bytes shorter at the head.** PP task: +0x6C parent, **+0x70 opener** (the routine that made
  it; what "find task" searches on), **+0x74 state** (called every frame), +0x78 destructor, own data from +0x7C.
  MoA-US has these at +0x70 / +0x74 / +0x78 / +0x7C / +0x80 (`menus-screens.md` §1a). Seen in the hub open
  `0x8004BFA0`, the arena open `0x800AE330` and "back to hub" `0x8004D28C` (PP, code).
- **The function map needs the module.** MoA-US modules all load at one address, so a MoA address alone is
  ambiguous in `moa-to-pp-functions.tsv`; always look a routine up together with its module (handed to Lane 1).

## 2. Menu building blocks (PP FDAT 201)

Twins of `menus-screens.md` §1. "same" = the map says only numbers or field offsets differ.

| PP | In FDAT 201 | MoA-US twin | What | Status |
|---|---|---|---|---|
| `0x8004E3A8(group)` | +0x52D8 | `0x800575B8` | new task | code, same |
| `0x8004E260(opener)` | +0x5190 | `0x80057470` | live task made by `opener` (screens wait on it) | code, same |
| `0x8004E344(group)` | +0x5274 | `0x80057554` | first task of a group | same |
| `0x8004E91C(group)` | +0x584C | `0x80057B28` | kill groups | changed (0.96) |
| `0x8004E790()` | +0x56C0 | `0x800579A0` | delete every task | same |
| `0x8004EA24` / `0x8004EA1C` | +0x5954 / +0x594C | `0x80057C2C` / `0x80057C24` | idle state / no destructor | identical |
| `0x80059104` | +0x10034 | `0x800639A4` | pad sampler | changed (0.98) |
| **`0x8019CE90`** (RAM) | — | `0x801B1CB8` | **pad words**: +0x18 pressed this frame, +0x1C held last frame (hub reads both) | code |
| `0x80059658` | +0x10588 | `0x80063EF4` | window open (the yes/no prompt calls it) | code, changed (0.84) |
| `0x80059E94` | +0x10DC4 | `0x80064348` | window close | changed (0.95) |
| `0x8005B1B4` | +0x120E4 | `0x80065330` | one line of text | same |
| `0x8005B100` / `0x8005A63C` | +0x12030 / +0x1156C | `0x8006527C` / `0x800647B8` | sprites | same |
| **`0x8005B278`** | +0x121A8 | `0x80065858` | **yes / no prompt** (same window 88, 96, 144, 48) | code |
| `0x8005B884(task, next state, text, group)` | +0x127B4 | `0x80065EA8` | message used by the hub ("no mail", no core) | code |
| `0x800A9FAC` | +0x60EDC | `0x800B6550` | shop / garage message by number | same |
| `0x800592B8` | +0x101E8 | `0x80063B54` | full-screen layer behind a screen | same |
| `0x8004F860` / `0x8004F478` | +0x6790 / +0x63A8 | `0x800589DC` / `0x80058644` | disc queue busy? / queue an archive load (file, entry, callback) | identical |
| `0x8004F0C4(size)` / `0x8004F114(ptr)` | +0x5FF4 / +0x6044 | `0x800583A0` / `0x800583F8` | menu heap alloc / free (the save code uses both) | code |
| `0x8004EC98` / `0x8004EA2C` | +0x5BC8 / +0x595C | `0x80057F74` / `0x80057CB0` | model load / TIM to VRAM | identical |
| `0x8004F52C` | +0x645C | `0x80058700` | sound bank load | identical |
| `0x8005ADA8` | +0x11CD8 | `0x80064F24` | menu song task (SEQ) | same |
| exe `0x80022F98` | SLUS +0x12798 | exe `0x800227D4` | `SsUtKeyOn` (menu sounds) | same |
| **`0x8019CC58`** (RAM, s16) | — | `0x801B1A80` | menu sound bank handle (every menu sound reads it) | code |

Not found yet (`?`): the twin of the MoA-US message box `0x80066C08` (the map's nearest guess is wrong), the menu
entry / frame loop (map pairs MoA-US `0x800A69C8` with PP `0x80098640` at 0.60 only), the draw dispatcher.

## 3. The hub (PP)

### 3a. Six entries, not eight (code)

PP's ring has **6 icons**. Every bound is 6 (`sltiu 6`), MoA-US has 8.

| # | Entry | Opens (jump table **`0x80049174`**, FDAT 201 +0xA4) | MoA-US twin |
|---|---|---|---|
| 0 | Garage | `0x80070038(page)`: page 6 (AC TEST) if `0x8003BCD7` is set (cleared on use), else 0 | `0x8007C6B0` |
| 1 | Shop | `0x8008A9C4()`, then `0x800B64B0 = 1` | `0x800990AC` |
| 2 | Missions | `0x80085D74()` | `0x800943B4` area |
| 3 | System | `0x800501DC(hub, 0)` | `0x80059554` |
| 4 | Mail | `0x80094098()` | `0x800A273C` |
| 5 | Arena | `0x800AE330()` (no arena number: PP has one arena) | `0x800BA64C(0)` |

No Ex Arena, no Arena Maker. The pick routine is **`0x8004D198`** (FDAT 201 +0x40C8, twin of MoA-US
`0x80056414`); hooking it is the way to replace or block an entry, as on MoA.

### 3b. Hub task (code)

Opened by **`0x8004BFA0`** (+0x2ED0): new task in group 0, opener = itself, first state `0x8004C1AC`.

| Offset | Meaning | MoA-US |
|---|---|---|
| +0x80 … +0x94 | the 6 icon holders, entries 0 … 5 (built by 6 calls of `0x8004D51C(2, n, selected)`, n = 1 … 6) | +0x84 … +0xA0 (8) |
| +0x98 / +0x9C / +0xA0 / +0xA4 | helper tasks (banner, sprites; the pick sets +0x9C byte 1) `?` | +0xA4 … |
| **+0xAE** | **selected entry** (s8, 0–5) | +0xBE |
| +0xAF / +0xB0 | turn direction / turn frame | +0xBF / +0xC0 |

- **Cursor start:** 5 (Arena) when `0x8003BCD8` is set (back from an arena fight), else 0.
- The build state **`0x8004C200`** (+0x3130) makes the icons in one of two orders depending on `0x8003BCD8`
  (draw order only, `?` why).
- **Position switch** `0x8004D51C` has 11 cases (offset −5 … +5 from the selected icon), MoA-US 15.
- **"Back to hub"** = **`0x8004D28C`** (+0x41BC): finds the task made by `0x8004BFA0` and puts it back on the build
  state `0x8004C200`. Every hub screen calls it when it closes.
- **Input** state `0x8004C600` (+0x3530): ✕ opens (also opens by itself when coming back from AC TEST or an arena
  fight). Checks before opening (second jump table **`0x80049144`**, FDAT 201 +0x74):
  - Mail with no mail received in any of the **64** mail flags at `0x8018D5D0` and none due (`0x80093C04`) →
    message, the ring stays.
  - Garage: copies 256 bytes `0x800342F0 → 0x8003BDF8` (owned parts, MoA-US `0x80036FEC → 0x8003EB20`), calls
    `0x800829A4` (MoA-US `0x80090FD8`), refuses with a message when both words `0x800439E0` / `0x800439E4` are 0
    (`?` what that block is: also saved, §6).
  - Shop runs `0x80058CAC` then `0x8008A960` (MoA-US `0x8006354C`, `0x80099048`); Mail starts the mail table load
    `0x80093AD8`; Missions waits on `0x80085B00` (the mission table load).
- **Icons:** `MENU_TMD.T` entry 0 has **8 objects** on PP (MoA-US 9: floor + 8 icons) (data). Pictures not
  traced on PP yet; `MENU_TIM` entries 0, 1 and 4 differ from MoA-US (data, hashes), so MoA-US's picture offsets
  in `menus-hub.md` don't carry over.

### 3c. What this means for MoA content

To add Ex Arena, Arena Maker or a mission source picker, the **folder-entry** idea from `menus-hub.md` §5 is the
easy way on PP too: keep 6 icons and have one icon open a small pick list. A 7th or 8th icon means rewriting the
ring (6 holders, the 11-case position switch, the turn table, both jump tables).

## 4. Missions (PP)

### 4a. Mission table `T = 0x80186A40` (code + data)

The PP twin of MoA-US `T = 0x8019F7A8`. PP keeps **64 mission slots**; MoA-US keeps 24. On PP the copy of
`MIS.T` entry 0 starts at **T+0xC4** (MoA-US T+0x50): loaded by `0x80085B00` (file 19 = MIS.T, entry 0), copied by
the callback `0x80085AAC` (0x620 words = 6,272 bytes).

| Offset | Size | Meaning | Status |
|---|---|---|---|
| +0x00 | s8 | slot being played (−1 none) | code |
| +0x01 | s8 | slot just accepted (−1 none) | code |
| +0x02 | s8 | **chapter**; wraps to 0–63 (`andi 0x3F` at launch) | code |
| +0x04 | s8 | mode: 1 = story (mail and chapters only work when 1) | code |
| +0x05 | s8 | score change for this mission (added on success, taken off on failure) `?` who sets it | code |
| +0x06 / +0x08 | u16 | copied at launch from entry tables +0x13C0 / +0x1440 (below) `?` | code |
| +0x0A … +0x27 | 15 × u16 | cleared at new game `?` | code |
| **+0x28** | s32 | **score**: + T+5 on each success, − T+5 on each failure. Mail kind 3 tests its sign | code |
| +0x2C | u32 | missions launched (capped 999) | code |
| +0x30 / +0x34 | u32 | successes / failures (capped 999) | code |
| **+0x38** | 64 × u8 | **per-slot flags**: bit 0 accepted, **bit 1 completed, bit 2 failed** | code |
| +0x78 / +0x79 / +0x7A | u8 | cleared at new game; +0x79 = cursor row | code |
| +0x84 | 64 × s8 | the list shown (−1 empty) | code |
| **+0xC4** | 6,272 B | **copy of `MIS.T` entry 0** (below) | code |

**Saved with the game: T+0x00 … +0x77 only** (header + the 64 flags), via the RAM copy `0x80043108`
(`0x800856FC` stores, `0x80085750` restores). Everything from +0x78 on is rebuilt from the disc.

### 4b. `MIS.T` entry 0 (8,192 bytes; offsets inside the entry, T+0xC4 + offset in RAM) (data + code)

| Entry offset | RAM | What |
|---|---|---|
| 0x000 | T+0x0C4 | 64 × 4: window (lowest chapter, highest chapter, 0, 0xFF); the list builder `0x80085C54` offers a slot when lo ≤ chapter ≤ hi |
| 0x100 | T+0x1C4 | 64 × 32: names (ASCII, `>` ends, `{4` = narrow space) |
| 0x900 | T+0x9C4 | 64 × u32: advance pay (added to credits on accept; 0 for every stock mission) |
| 0xA00 | T+0xAC4 | 64 × u32: **reward** → `0x8003BF24` at launch |
| 0xB00 | T+0xBC4 | 64 × u8: **engine mission number** → `0x80042745` at launch (the missions engine loads FDAT number × 5) |
| 0xB40 | T+0xC04 | 64 × u8: **fight music** → `0x8003BC85` (XA id, 255 none; §7) |
| 0xB80 | T+0xC44 | 64 × 32: page records, u16[8] pictures + u16[8] text pages, each `MIS.T` entry − 1 (−1 none) |
| 0x1380 | — | 64 × s8 `?` (values −1 … 3) |
| 0x13C0 / 0x1440 | T+0x1484 / T+0x1504 | 64 × u16 each → T+6 / T+8 at launch `?` (first table 0, 310, 420, …; second all 0) |
| 0x14C0 … 0x1FFF | | zero except a few bytes `?` |

The launch (state code around `0x80088E84`, FDAT 201 +0x3FDB4, twin of MoA-US `0x80097594`) is the same shape as
MoA-US: accept → flag bit 0, advance pay; launch → T+0 = slot, T+0x2C += 1, engine number / music / reward copied
out, then a sum through `0x8008CE2C` (MoA-US `0x8009B488`, not read).

**Stock PP missions** (data, `MIS.T` 0; slots 17–63 are "dummy" rows):

| Slot | Name | Window | Reward | Engine no. | Music |
|---|---|---|---|---|---|
| 0 | Infiltrate Amber Base | 1–1 | 32,000 | 0 | 6 |
| 1 | Search and Destroy | 0–0 | 33,000 | 1 | 5 |
| 2 | Rescue the Allies | 2–2 | 35,000 | 2 | — |
| 3 | Raid the Laboratory | 3–3 | 34,000 | 3 | — |
| 4 | Disrupt Shipping Lanes | 5–5 | 36,000 | 4 | — |
| 5 | Capture the VIP | 6–6 | 40,000 | 5 | 4 |
| 6 | Protect the VIP | 7–7 | 41,000 | 6 | 1 |
| 7 | Destroy Main Facility | 9–9 | 35,000 | 7 | 18 |
| 8 | Confrontation | 10–10 | 50,000 | 8 | — |
| 9 | Search the Facility | 12–12 | 45,000 | 9 | 19 |
| 10 | The Final Battle | 13–13 | 0 | 10 | 23 |
| 11 | The Underground Maze | 4–4 | 32,000 | 11 | 22 |
| 12 | Disrupt Military Units | 4–4 | 33,000 | 12 | 0 |
| 13 | Destroy Receiving Base | 8–8 | 31,000 | 13 | 5 |
| 14 | Destroy the Detachment | 8–8 | 32,000 | 14 | 19 |
| 15 | Intercept Enemy Units | 11–11 | 33,000 | 15 | 3 |
| 16 | Destroy Secret Base | 11–11 | 34,000 | 16 | — |

### 4c. Coming back from a mission: `0x80085854` (FDAT 201 +0x3C784) (code)

Restores T+0 … +0x77 from `0x80043108`, then reads the result word **`0x8003BF28`** (0 = success):
- **Success:** flag bit 1, successes += 1, T+0x28 += T+5; in story mode chapter += 1 (mission 6 "Protect the VIP"
  only moves the chapter on when `0x8003BCF5` = 0 and `0x8003BCDF` ≠ 0 `?`).
- **Failure:** flag bit 2 (for slot 6 only under the same two bytes), failures += 1, T+0x28 −= T+5.
- T+0 = −1, `0x80043108` = −1.

Unlike MoA-US (`missions.md`, "Mission results and chapters") there are **no chapter jumps, no arena-rank gates,
no credit/part gifts** in this routine. PP's chapter is simply "story missions won so far" (with the mission 6
exception).

### 4d. Mission files on the disc (data)

- **FDAT 5n = mission code, 5n + 4 = mission map**, n = 0 … 16 (17 missions). PP mission code is linked to run
  at `0x80097A38` (first words point into `0x8009….`; `phantasma/missions-import.md`).
- FDAT 85–94 (n = 17, 18) are **empty**; n = 19 (FDAT 95 / 99) holds a mission-style code block (28 KB) and map `?`
  what it is (AC TEST? training?).
- FDAT 100 … 199 (n = 20 … 39) are stages: four code blocks + map each (other engines; Lanes 3/4/7).
- FDAT 200 = common block (twin of MoA-US FDAT 205; §7). FDAT 201–206 = modules.
- **MIS.T (833 entries):** 0 = mission table; **1 … 816 = mission pages, 16 per slot** (slot s uses entries
  16s + 1 … 16s + 16: text pages on even page numbers, pictures on odd), so only slots 0 … 50 have entries at all,
  and only 1 … 272 hold data; **817 = part descriptions** (20 KB); **818 = mail table**; **819 … 832 = the 14 mail
  texts** (2 KB each).

### 4e. What this means for adding MoA missions

- **Room in the menu and the save: yes.** 64 table slots, 17 used → 47 free; the 64 flags are saved. MoA-US's 20
  missions (slots 0–19) fit.
- **Room in FDAT: no.** Only n = 17 and 18 are free before the stages start at n = 20. MoA missions need the
  missions engine's file load redirected (hook the load, read our own pack) — the trick the old MoA-side import
  used in reverse. The engine-number byte (T+0xBC4) lets a menu slot point at any engine number.
- **MIS.T pages:** the 16-per-slot numbering leaves slots 17–50 with (empty) entries; new pages still mean
  rebuilding MIS.T or redirecting its loads.
- **Code:** MoA-US mission code is linked to `0x800A0DD0` and calls MoA-US engine routines; it needs relocating to
  PP's `0x80097A38` and PP's engine (the old converter did PP → MoA; the map's 5,000 pairs are the starting point).
- **Chapters:** MoA-US windows assume MoA's chapter flow (jumps, arena gates). PP's chapter is a plain win count,
  so MoA windows must be re-set for PP, or MoA missions get their own list (mission source picker).

## 5. Mail (PP)

### 5a. Mail block `S = 0x8018D5D0` (code)

| Offset | Size | Meaning | MoA-US (`0x801A4F98`) |
|---|---|---|---|
| +0x00 | 64 × u8 | per-mail flags (bit 0 received …, as MoA) | 32 |
| +0x40 | 64 × 2 | trigger table (kind, value) | +0x20, 32 |
| +0xC0 | u8 | mails received so far | +0x60 |
| +0xC1 | u8 | `?` (with pad `0x8019CEA8` & 0x0C = L2+R2 held: a debug path in `0x80093D84` `?`) | +0x61 |
| +0xC4 / +0xC5 / +0xC6 | u8 | table load busy / selected row / screen state | +0x64 / +0x65 / +0x66 |
| +0xD0 | 64 × s8 | the mail list | +0x70 |
| +0x110 | 4,224 B | copy of `MIS.T` 818 (callback `0x80093A58` copies 0x420 words, then the 64 trigger pairs to S+0x40) | +0x90 |

**Saved:** S+0x00 … +0xC3 (196 bytes: flags, triggers, count) via the RAM copy **`0x80043AB8`** (`0x80093778`
stores, `0x80093830` restores). New game: `0x80093B28` clears it (twin of MoA-US `0x800A2318`). As on MoA-US the
trigger table travels in the save; whether the disc table's callback can land after a LOAD restore is `?`
(timing, needs a run).

### 5b. `MIS.T` entry 818 (6,144 bytes) (data)

| Offset | What |
|---|---|
| 0x000 | 64 × (kind, value); unused = (0xFF, …) |
| 0x080 | 64 × 32-byte subjects |
| 0x880 | 64 × 32-byte page records (u16[8] pictures, u16[8] text pages; text page n = `MIS.T` 819 + n) |
| 0x1080 | zero |

**Stock: 14 mails, all mission-triggered, no pictures** (data):

| Mail | Kind, value | Subject | Mail | Kind, value | Subject |
|---|---|---|---|---|---|
| 0 | 1, 0 | Confirmed Raids | 7 | 1, 7 | Destroy Organization |
| 1 | 1, 1 | Confirmed Destruction | 8 | 1, 8 | Test Run |
| 2 | 1, 2 | Forged Request | 9 | 1, 9 | Whereabouts |
| 3 | 1, 4 | Destroy Transports | 10 | 1, 13 | Rectenna |
| 4 | 1, 5 | Organization Discovery | 11 | 1, 14 | Destroy Detachment |
| 5 | 1, 6 | Stinger 1 | 12 | 1, 15 | Surprise Attack |
| 6 | 2, 6 | Stinger 2 | 13 | 1, 16 | Secret Base Discovery |

### 5c. When a mail is due: `0x80093C04` (FDAT 201 +0x4AB34) (code)

Story mode only (T+4 = 1; otherwise nothing is ever due — MoA-US delivers everything at once outside story mode).

| Kind | Due when | MoA-US |
|---|---|---|
| 0 | chapter T+2 ≥ value | same |
| 1 | slot *value* completed (T+0x38 bit 1) | same |
| 2 | slot *value* completed or failed — **except value 6: failed only** | same, no exception |
| 3 | score T+0x28 < 0 **and** chapter ≥ 26 (value unused) | never |
| 4 and up | never | never |

The mission 6 exception is how "Stinger 1" (won) and "Stinger 2" (lost) split. The delivery routine itself
(MoA-US `0x800A24FC`) is not pinned yet on PP `?`.

### 5d. What this means for MoA mail

- 64 slots, 14 used → **50 free**, all saved: MoA-US's 20 mails fit.
- Kinds 0/1/2 exist on PP, so MoA's mail can keep its triggers once its mission slots are re-numbered. A
  per-group mail folder (the mail-folders design) needs either one list with a group tag kept by us, or our own
  kind (≥ 4, never delivered by the game) delivered by a hook, as `missions.md` suggested for MoA.
- MoA's chapter mails (kind 0) would fire on PP's chapter count, which means something else on PP (§4c).
- **Text room:** PP mail texts end at `MIS.T` 832, the last entry. MoA mail texts (and its two mail pictures)
  need MIS.T rebuilt or the mail page loads redirected.

## 6. Save format (PP)

### 6a. How it is written and read (code)

- **SAVE = `0x80065ECC`** (FDAT 201 +0x1CDFC, called from `0x80052E74`): allocates **3,968 bytes** (0xF80),
  fills them, writes them to the card file and reads them back, comparing checksums, up to 3 tries.
- **LOAD = `0x80066C44`** (+0x1DB74, called from the card screen at `0x800532B0`): reads, checks, puts every field
  back (the same offsets as SAVE), then runs `0x800AA4B8` (settings), `0x8008A9B0`, `0x80085B50` (mission table
  reset), `0x80085750` (missions restore), `0x80093B28` + `0x80093830` (mail reset + restore). The same routine
  also holds the **Armored Core 1 import** path (its own checksum `0x80065EA4`, 3,044 bytes).
- **Checksum `0x80065E7C`**: a plain sum of the **bytes** over **+0x230 … +0xF07** (3,288 bytes), stored at
  **+0x200**. (MoA-US: sum of inverted bytes over +432 … +3671, stored at +384.) This is the same rule MoA-US uses
  when it imports a Phantasma save (type 'S', `missions.md` / "Hub chat → launch chat"), which fits.

### 6b. The 3,968 bytes (code; RAM sources from SAVE)

| Offset | Size | From RAM | What | Status |
|---|---|---|---|---|
| +0x000 | 4 | — | `S`, `C`, 0x11, 1 (card file header, 1 block) | code |
| +0x004 | ~56 | `0x800B48B4` + filled in | title (Shift-JIS); +0x1A … +0x2F a 3-digit number from u16 `0x80043134` as full-width digits `?` meaning; +0x32 … pilot name (from `0x80185C68`) | code |
| +0x060 | 32 | `0x800B48F4` | icon palette | code |
| +0x080 / +0x100 / +0x180 | 128 each | `0x800B2894` + 128 × k | icon frames; k is picked from a rank byte (`0x801E0548` + … + 0x460) `?` | code |
| **+0x200** | 4 | — | **checksum** | code |
| +0x204 | 36 | `0x800344F0` | names, list copy (outside the checksum) | code |
| +0x228 | 2 | `0x80043134` | list copy `?` | code |
| +0x22C | 4 | `0x8003BCD0` | credits, list copy | code |
| **+0x230** | 12 | `0x800342B4` | equipped parts (12 ids) — checksum starts here | code |
| +0x23C / +0x23D | 1 + 1 | `0x8003BCCE` / `0x8003BCCF` | `?` (twins of MoA-US `0x8003EA72` / `0x8003EA73`, which MoA-US also saves, at +0x1C4 / +0x1C5) | code |
| +0x23E | 2 | `0x8003BCCC` | **option parts mask** (twin of MoA-US `0x8003EA70`, saved there as +0x1C6) | guess |
| +0x240 | 4 | `0x8003BCD0` | **credits** | code |
| +0x244 | 36 | `0x800344F0` | names (AC, pilot) | code |
| +0x268 | 96 | `0x800439E0` | garage-side block, read by `0x800829A4` and the garage check `?` | code |
| +0x2C8 | 48 | `0x800342C0` | paint (right after the 12 part ids) | code |
| +0x2F8 | 256 | `0x800342F0` | **owned-part counts** | code |
| +0x3F8 | 256 | `0x800343F0` | **shop stock** | code |
| +0x4F8 | 2,048 | VRAM rect `0x800B48A4` | emblem pixels | code |
| +0xCF8 | 32 | VRAM rect `0x800B48AC` | emblem palette | code |
| +0xD18 | 120 | `0x80043108` | **missions**: T+0 … +0x77 (§4a) | code |
| +0xD90 | 56 | `0x80043A40` | **settings record** (used by the settings routines `0x800AA428` / `0x800AA4B8`, twins of MoA-US's 56-byte record) | code |
| +0xDC8 | 64 | `0x80043A78` | **arena state** (read by the arena screen `0x800AE088` …) `?` fields | code |
| +0xE08 | 7 | `0x8003BD24` | small flags (hub, missions, arena use them) `?` | code |
| +0xE0F | 196 | `0x80043AB8` | **mail** S+0 … +0xC3 (§5a) | code |
| +0xED3 | 52 | `0x80042E60` | **arena** (also read by the arena screen) `?` | code |
| +0xF07 | | | checksum ends | code |
| +0xF80 … +0x1FFF | 4,224 | — | **not written**: free space in the 8 KB card block | code |

The owned-part index uses the same category bases on both discs (head 0, core 18, arms 29, legs 53, generator
93, FCS 108, option 124, booster 143, back 157, hand 216, blade 237; table PP `0x800B5094`, MoA-US `0x800CD1A0`)
(data). So the 256 counts mean the same kind of part on both; whether every MoA part id exists on PP is Lane 5's.

### 6c. What this means for MoA content

- Missions (64 flags) and mail (64 slots) already have room for MoA's (§4e, §5d) inside today's save.
- Anything new (MoA arena data, extra flags) has **4,224 free bytes in the same card block**, but outside the
  checksum: either widen the checksum (old saves then fail it) or give the new part its own check.
- MoA-US saves can't be read as they are (other size, other checksum, other layout); a converter goes field by
  field using `arena-run-design.md` ("Scenario save block") and this table.

## 7. Sound sources (PP)

| Source | What | vs MoA-US | Status |
|---|---|---|---|
| `GG/MS/MENU_VAB.T` 0–4 | menu sounds (0+1), menu song instruments (2+3), **the menu song SEQ (4)** | **byte-identical** to MoA-US (all 5 entries) | data |
| Menu sounds | `SsUtKeyOn` (exe `0x80022F98`) on the bank at `0x8019CC58`, note 60, volume 127 | same call pattern; tone table in `../sound-map.md` §5 applies (same samples) | code |
| Menu song | task `0x8005ADA8` (twin of MoA-US `0x80064F24`) | same | code (twin) |
| `GG/BGM/BGM00–01.XA` | **fight music**, 2 files (MoA-US has 4) → XA ids 0–31 | half as many songs | data |
| `GG/BGM/TALK00–05.XA` | mission radio voices, 6 files (MoA-US 7) | | data |
| `GG/BGM/CPU00–03, 10–13, 20–23.XA` | AC computer voice, 3 types × 4 files | same files, some sizes differ | data |
| XA file names | exe `0x80011354` … (SLUS +0xB54): BGM00, BGM01, TALK00–05, CPU…; so **XA id = file × 16 + channel** with files 0–1 BGM, 2–7 TALK, 8–19 CPU `guess` (order of the names; the registration table itself not found yet) | MoA-US files 0–3 BGM, 4–10 TALK, 11–22 CPU | code (names) |
| Fight music byte | `0x8003BC85` (MoA-US `0x8003EA29`): missions set it from the table (§4b) | same idea | code |
| `FDAT.T` 200 | common block, **same layout as MoA-US FDAT 205**: a leading chunk, then VAB header (4,640 B) + VAB body (212 KB; MoA-US 204 KB), the 3,008-byte sound-id table, three message blocks (13.5 / 14.7 / 13.3 KB; MoA-US ~10 KB each), a 1,408-byte block | the fight bank and id table **differ** from MoA-US (hashes) | data |
| Stage / mission banks | inside each FDAT map (5n + 4), as MoA-US | not read | — |
| `GG/STR/*.STR` | movie sound (ASLOGO, ACOPA, ACED5, DEMOPLAY, STAFF) | | data |

So the menu side's sounds carry over as they are; MoA-US fight music (BGM02/03 and the songs it uses) and its
seventh TALK file don't exist on the PP disc and would have to be read from the MoA disc.

## 8. Plain summary for the owner

- Phantasma's hub has **6 icons** (Garage, Shop, Missions, System, Mail, Arena). MoA's Ex Arena and Arena Maker
  would fit best behind an existing icon as a small pick list, not as new icons.
- Phantasma's mission list and save already have **64 mission slots** (17 used) and **64 mail slots** (14 used).
  MoA's 20 missions and 20 mails fit in the numbers and in the save without changing its size.
- The tight spots are the **disc archives**, not the save: there is room for only 2 more missions in FDAT, and
  the mission pages and mail texts sit at the end of MIS.T. Adding MoA missions means loading them from our own
  pack (a hook on the game's file load), and moving MoA's mission code over to Phantasma's engine.
- Phantasma's story is simpler than MoA's: its chapter just counts story wins; there are no arena-rank gates.
  MoA's timed mails and mission windows need new values or their own list (the mission source picker).
- The save is 3,968 bytes with a simple checksum; the card block has **4 KB unused** for anything we add.
- Menu sounds and the menu music are **exactly the same files** on both discs. Fight music differs: Phantasma has
  2 music files, MoA 4.

## 9. Open (next steps for this lane)

- Run tests (none done): watch T (`0x80186A40`) and S (`0x8018D5D0`) across one mission win and one loss; check
  which trigger table a loaded save ends up with.
- Pin on PP: mail delivery (MoA-US `0x800A24FC`), the message box (MoA-US `0x80066C08`), the menu frame loop, who
  writes T+5, the meaning of MIS.T 0's tables at +0x1380 / +0x13C0 / +0x1440, `0x800439E0` (96 bytes), the arena
  blocks `0x80043A78` / `0x80042E60` and the 7 bytes at `0x8003BD24`, the title number `0x80043134`.
- The screens behind the hub entries in depth (garage pages, shop, system/card, arena list, options, VS menu) on PP:
  only their entry points are pinned here. `menus-screens.md` is the MoA-US reference for each.
- FDAT n = 19 (mission-style block, 28 KB code): what it is.
- Hub pictures (MENU_TIM) on PP.

## RAM rows for the RAM map (PP)

`docs/research/ram-map.toml` isn't on `main` yet (Lane 1 brings it with a `disc` field). Rows to add then:

| Address (PP) | Name | Size | Status |
|---|---|---|---|
| `0x8003BCD0` | credits | u32 | code |
| `0x8003BCD7` | back_from_ac_test | u8 | code |
| `0x8003BCD8` | back_from_arena_fight | u8 | code |
| `0x8003BC85` | fight_music_xa | u8 | code |
| `0x8003BF24` | mission_reward | u32 | code |
| `0x8003BF28` | mission_result (0 = success) | u32 | code |
| `0x80042745` | mission_engine_number | u8 | code |
| `0x80043108` | mission_table_save_copy | 120 B | code |
| `0x80043AB8` | mail_save_copy | 196 B | code |
| `0x80043A40` | settings_record | 56 B | code |
| `0x800342B4` | garage_parts | 12 B | code |
| `0x800342C0` | garage_paint | 48 B | code |
| `0x800342F0` | owned_part_counts | 256 B | code |
| `0x800343F0` | shop_stock | 256 B | code |
| `0x8003BDF8` | owned_parts_work_copy | 256 B | code |
| `0x80186A40` | mission_table | ~0x1944 B | code |
| `0x8018D5D0` | mail_block | ~0x1190 B | code |
| `0x8019CE90` | menu_pad | 0x20 B | code |
| `0x8019CC58` | menu_sound_bank | s16 | code |

## For other lanes (their docs aren't on `main` yet, so collected here)

- **Lane 1:** in `moa-to-pp-functions.tsv` the same MoA address appears for several modules (they all load at one
  address); a lookup by address alone picks the wrong module (it paired MoA-US `0x80055874` of FDAT 207 with a
  FDAT 206 routine). Key it by (module, address). The RAM rows above are for `ram-map.toml`.
- **Lane 1 / 3:** scenario globals MoA-US `0x8003EAxx` = PP − 0x2DA4 (checked on 4 addresses, §1).
- **Lane 3:** PP LOAD `0x80066C44` and what it runs after the copy-back (§6a); SAVE `0x80065ECC`; menu task head
  is 4 bytes shorter than MoA-US (§1). The hub opens with `0x8004BFA0`; "back to hub" `0x8004D28C`.
- **Lane 5:** owned-part bases are identical on both discs (§6b); PP part descriptions are `MIS.T` 817.
- **Lanes 3/4/7:** PP FDAT n = 20 … 39 (FDAT 100 … 199) are stages with four code blocks + map each; n = 19 is a
  mission-style block `?`.

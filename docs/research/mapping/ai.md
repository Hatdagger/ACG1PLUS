# Lane 6 — AI (Project Phantasma base)

First pass 2026-11-26, second pass same day (MoA's extra code, aim and fire, behaviour names, record number). Static read (nothing run yet). Disc tags: **PP** = Project Phantasma USA (SLUS-00670),
**MoA-US** = Master of Arena USA disc 1 (SLUS-01030). Status per finding: `code` = read from the code,
`guess` = inferred, not proven. No game code here: addresses, sizes and meanings only.

How it was read: the PP arena engine (FDAT 205, loads at `0x800490D0`) and the MoA-US arena engine (FDAT 210,
loads at `0x80050DD8`) were disassembled in the chat sandbox; the AI was found by taking MoA-US's known AI call
(`0x8008D4AC` from the arena step routine `0x8007F6F4`) and reading the same spot in its PP twin. Routines were
paired PP↔MoA-US with a loose instruction-shape comparison (the Lane 1 map missed most of them because their
tables and addresses moved).

## 1. Plain summary (for the owner)

- **Phantasma's arena AI and MoA's are the same design.** MoA revised it; it is not a rewrite. Most of the
  behaviour routines are identical or close on both discs. The earlier note "the arena AI was rewritten" came
  from an automatic match that couldn't see past the moved tables.
- **How it thinks.** Every 8 frames it scores each of its behaviours (close in, circle, keep distance, dodge a
  missile, wander, and so on) as *how well it fits the moment × how much the AI likes it × a bonus for sticking
  with the current one*, then does the best one. Every frame it turns, aims and fires. It drives its AC by
  "pressing buttons", like a player. It doesn't move the AC directly.
- **It learns during the fight.** When it gets hit, the behaviour it was using and the weapon in its hands lose
  favour, and the others gain. When it hits you, that behaviour and that weapon gain favour. Over a fight it
  drifts toward whatever works against *you*. Both games have this.
- **It reacts late on purpose.** It aims at where you were some frames ago (0 to 30 frames, set per
  opponent), so weaker opponents are slower.
- **Fast strafing really does throw it off.** When you sweep sideways across its view quickly, a "lost track"
  value rises and the AI starts turning the wrong way or at random until it settles. This is a code reason why
  strafe dancing works on the AI.
- **Per-opponent tuning is built into Phantasma's arena code.** There are 49 settings records of 60 bytes,
  one per arena opponent. MoA instead takes one 36-byte record from the menu, the one its AI editor and our old
  AI cards wrote.
- **Why Phantasma's AI feels stronger: it pulls the trigger.** Phantasma's AI fires every time you are lined
  up, in that same frame. MoA's AI rolls dice on every chance (its "shoot" setting, usually about half), fires a
  quarter as often in the first ~4 seconds, and in MoA's main arenas holds each shot back another 10 or 20
  frames. Same aim, far fewer and later shots. (`code`; not yet seen in a running game.)
- **MoA added smarter-looking tricks that cut the other way.** It aims ahead of you (where you'll be when the
  shot arrives), dodges ordinary bullets and rockets, and goes all-out when its AP gets low. Phantasma does
  none of these: it aims where you are (a few frames late) and only dodges missiles. Aiming ahead is easy to
  fool by changing direction, which is what strafe dancing does, while aiming at where you actually are keeps
  landing hits on a target that wiggles in place (`guess`).

## 2. Where things are (PP)

### Code

| What | PP address | MoA-US twin | Status |
|---|---|---|---|
| Arena step routine (pads, then AI, then both ACs) | `0x80077524` | `0x8007F6F4` | code |
| **AI, once per frame** | **`0x80083A40`** (237 instr) | `0x8008D4AC` (507 instr) | code |
| AI init at match start (called by the arena main loop `0x8004A654`) | `0x80083DF4` | `0x8008DC98` | code |
| **AI setup: record → working values** (called from `0x8004E688`) | **`0x8004DDBC`** | `0x80055768` | code |
| Setup helper: 6-byte behaviour tuning → behaviour entries | `0x8004DCB8` | `0x800556FC` | code |
| Situation and rules (current behaviour's action, blade lunge, boost trigger, "lost track") | `0x80083314` | partly `0x8008CBEC` / `0x8008BC60` | code |
| Aim and fire, every frame | `0x80081438` (270 instr) | `0x8008C4E4` (450 instr) | code (read in outline only) |
| **Behaviour chooser**, frame phase 2 of 8 | `0x80083158` | `0x8008BF78` | code |
| **Weapon chooser**, frame phase 5 of 8 | `0x80081180` | `0x8008C0E8` | code |
| Re-roll the 3 "desire" values | `0x800839D8` | `0x800898F4` | code |
| Learn: AI got hit (behaviour and weapon weights) | `0x80083810` | `0x80089744` | code |
| Learn: AI hit the player (current behaviour up) | `0x80083988` | `0x800898A4` | code |
| Learn: a weapon or blade of the AI hit (from the projectile hit routine `0x80085154`) | `0x8008376C` | `0x800896AC` | code |
| Damage routine that calls the two learn routines | `0x800734A0` | `0x8007B26C` | code |
| Turn toward target, with "lost track" errors | `0x800818E0` | `0x800899CC` | code |
| Line of sight to the player (shared with the rest of the engine) | `0x8006EDC8` | `0x80076C34` | code |
| Fuzzy maths helpers: a×b/4096 `0x80080DDC`, 4096−a `0x80080E1C`, raise (cap 3686) `0x80081014`, lower (floor 409) `0x80081030`, commitment curve `0x80080FB4` | `0x80080DDC`… | `0x80089308`… | code |

Not in the missions engine (FDAT 202), FDAT 203 or the VS engine (FDAT 204): none of the probed AI routines
are there (`code`). Mission enemies use other code (Lane 8).

### Data and RAM (PP)

| What | RAM (PP) | Disc (PP) | Status |
|---|---|---|---|
| **AI records**, 49 × 60 bytes (§3) | `0x8008E498` | FDAT 205 +0x453C8 = `GG/COM/FDAT.T` +0x1247BC8 (LBA 81152, byte 968) | code |
| **Behaviour table**, 20 × 28 bytes (§4) | `0x8008F014` | FDAT 205 +0x45F44 = `FDAT.T` +0x1248744 | code |
| **AI working state** (§5) | `0x801DB838` | (RAM only) | code |
| Player block 1 / 2 (936 bytes each; MoA-US 928) | `0x800424E8` / `0x80042890` | — | code |
| Current-player pointer (MoA-US `0x8003EAA0`) | `0x8003BD00` | — | code |
| AC object: player / AI (368 bytes each; MoA-US 376) | `0x801D1D20` / `0x801D1E90` | — | code |
| Frame counter (u16; the AI uses its low 3 bits as an 8-frame phase) | `0x801D400C` | — | code |
| Match flag word (bit 0x400: AI does nothing, pads cleared) | `0x800956DC` | — | code |
| **AI's button word** = player block 2 +0x08 (previous frame at +0x0C) | `0x80042898` | — | code |
| AI's wanted weapon slot = player block 2 +0x19 (selected slot is +0x18) | `0x800428A9` | — | code |
| **AI record number** = player block 2 +0x25C; written only by the arena pre-fight compile `0x800B0F7C` (menu FDAT 201) = the opponent number (§3a) | `0x80042AEC` | — | code |
| Opponent number of the next / last arena fight (MoA-US `0x8003EAD5`) | `0x8003BD2E` | — | code |
| Default key layout, 12 × u32 (§7) | `0x800BD568` | FDAT 201 +0x74498 = `FDAT.T` +0x1124C98 | code |

Free space: none looked for yet.

## 3. The AI record (PP, 60 bytes; MoA-US equivalent: 36 bytes)

Picked by player block 2 +0x25C (0–48) = compiled stat block +4, a byte MoA leaves unused (padding). **Record n belongs to opponent number n, the opponent who starts at rank n + 1** (§3a). The setup turns percents (0–100) into fractions of 4096.
"MoA name" = the slider of MoA's AI editor that writes the same state field (from the old card code's notes on
the editor, `reference/ac1recomp/.../moa_card.c`), so it is a lead to the meaning, not proof.

| PP off | Size | → state | MoA-US off | MoA name | Meaning | Status |
|---|---|---|---|---|---|---|
| +0x00 | u8 | player block 2 +0x25B = compiled stat block +3 | +0x00 | Human PLUS level | **The opponent's Human PLUS level** (stat block +3 is Human PLUS in MoA's layout, `stat-block.md`); also a gate for the boost/○ rule in `0x80083314` | code |
| +0x01 | % | +0x24E | +0x01 | 100 − fly | Base of desire A (ground), re-rolled | code / guess |
| +0x02 | % | +0x250 | +0x02 | 100 − fly | Base of desire B (ground) | code / guess |
| +0x03 | % | +0x252 | +0x03 | fly | Base of desire C (air); behaviour 5 scores with it | code / guess |
| +0x04 | u8 | +0x254 | +0x04 | — | How often desires are re-rolled: chance per frame = 1/(22·v); 0 = never | code |
| +0x05–07 | % | +0x256/+0x258/+0x25A | +0x05–07 | — | Random spread added to desires A, B, C on each re-roll | code |
| +0x08 | % | +0x25C | +0x08 | — | Unknown | — |
| +0x09 | % | +0x25E | +0x09 | blade | **Blade lunge chance**: inside 700 units and ±45°, a roll that also weighs a term from the AI AC's AP (+0x160, against 2000) is compared with 1 − this value; on success it presses forward + ○ (`0x80083314`); raised when the blade connects (`0x8008376C`) | code |
| +0x0A | u8 | +0x260 | +0x0A | 105 − blade | Frame count gate for the ○ rule at 1,000–5,000 units and the ✕ rule | code / guess |
| +0x0C | u16 | +0x262 | +0x10 | — | 1000 (1500 in record 0) | — |
| +0x0E | u16 | +0x264 | +0x12 | — | 6000 in records 0–29, 5000 in 30–48 | — |
| +0x10 | u8 | +0x266 = v·4096/600 + 512 | +0x0B | lock | **Tracking skill**: how much the target's sideways sweep feeds "lost track" (§5 +0x244); 0 = off | code / guess |
| +0x11 | u8 | +0x268 | +0x0C? (→ +0x26E) | (100 − lock)/4 | **Reaction delay** in frames: depth of the player-position history the AI aims from | code |
| +0x12 | 4 × 6 | behaviours 4–7 | +0x14… | range sliders | Tuning for behaviours 4–7 (layout below) | code |
| +0x2A | 3 × 6 | behaviours 12–14 | +0x1E… | — | Tuning for behaviours 12–14 | code |

6-byte behaviour tuning (`0x8004DCB8`): +0 base weight %, +1 learning blend %, +2 commitment time
(× 22 frames), +3 whim interval (chance per check 8/(22·v)), +4 u16 whim factor % → behaviour entry +0x0C,
+0x0E, +0x10, +0x12, +0x14.

MoA-US only: record +0x0D → state +0x268 **fire chance** (MoA "shoot"), +0x0E → +0x26A **shot-dodge chance**
("avoid"), +0x0F → +0x26C **desperation AP share** (§6). PP has no such fields. MoA's record comes from the menu at `0x8004AC80` (36 bytes); PP's are fixed in the arena module.

**The 49 records on the disc** (`code`): two families. Records 0–29 share most values (desires 40/75/75, no
spread, blade 0–100 %, delay 0–20 frames; 20–29 add a second-group tuning). Records 30–48 are a different
family (desires 70/25/25 with spread 30/30, re-roll v = 4, different behaviour tuning, delay 30 frames, +0x0E =
5000). Record n = the opponent who starts at rank n + 1 (`code`, §3a).

## 3a. Which opponent gets which record (PP arena screen, menu FDAT 201; round 3 topic 4)

Read in the menu module (`code`). Plain version first:

- **Each of the 49 arena opponents has a fixed number, 0–48, = its starting rank − 1.** That one number picks
  everything about the opponent: its name pair, its AC build, its prize, its "beaten" flag **and its AI record**.
  So record 0 is the rank 1 opponent's, record 48 the rank 49 opponent's. The AI follows the opponent, not the
  rank: an opponent that the player pushes down a place keeps its own AI.
- **The table matches strength:** ranks 1–20 mostly react at once and never lose track (delay 0, tracking 0;
  exceptions ranks 10, 11, 13 with delay 10–20), ranks 21–30 mostly react 10 frames late and lose track easily
  (tracking 60), ranks 31–49 are the second family (mostly delay 30; desires 70/25/25; rank 32 has delay 0). Records with Human PLUS byte 9
  are ranks 1–9, 12, 14–20.
- **Your rank = 50 − how many different opponents you have beaten.** Beating someone you already beat gives
  nothing. Beating a *new* opponent moves you up one place even if it was the one *below* you (`code`; in
  normal play the one below has always been beaten already, so this only shows with an edited rank; test 37).
- **Who you may challenge:** the opponent just above you, the one just below you, or anyone you have already
  beaten. Anyone else (and your own line) only plays the refuse tone.

The list (`code`):

| What | Where (PP) | Notes |
|---|---|---|
| Arena screen opener | `0x800AE330` | allocates the **list, 50 × 12 bytes** (task +0x7C+0x34; the 600-byte buffer `?` of `menus-screens.md` PP-5), loads names (`MENU_TIM` entry 482) and prizes (entry 483) |
| **List builder** | **`0x800AE59C`** | entry = +0 pilot-name pointer, +4 AC-name pointer, **+8 opponent number**. With rank R (`0x80043A78`): lines 0 … R−2 = opponents 0 … R−2; line R−1 = the player (names `0x80034502` / `0x800344F0`, number **50**); lines R … 49 = opponents R−1 … 48. Name pair of opponent n = entry 482 + 48 × n (pilot +0, AC +0x18) |
| Cursor | task +0x7C+0x46 (row on screen) + task +0x7C+0x47 (top line, 0–44; 6 rows shown) | list line = the two added; on open the top line = R − 3 (0 … 44) |
| Back from a fight | `0x800AE59C` | if `0x8003BCD8` (back from arena) and the player's win count `0x8003BD10` ≠ 0 and **`0x80042E60` + n** = 0: set it to 1 and **rank − 1**. Then both are cleared |
| Prize | `0x800AE84C` / `0x800AEA00` | money and part from entry 483 + 8 × n (n = `0x8003BD2E`): the prize belongs to the opponent, not the rank |
| **Challenge check** | `0x800B0360` (at `0x800B04C8`) | number 50 (own line) → tone 7. Line = R − 2 (just above) or R (just below) → allowed; otherwise allowed only if `0x80042E60` + n ≠ 0; else tone 7. Then the equipment check and "Do you want to challenge?" |
| Stage | `0x800B08E0` | the player picks it (Left / Right through the 12 stages); not tied to the opponent |
| Opponent build | `0x800B0DF0` | loads **`MENU_TIM` entry 382** (64 bytes per opponent) to task +0xB4 |
| **AI record + build** | **`0x800B0F7C`** (at `0x800B1104`) | n = list entry +8 → **`0x80042AEC` = n** (store at `0x800B1130`), then build = entry 382 + 64 × n into the garage (re-read from the list, not from `0x80042AEC`), compile `0x8009A238(1)` (does not write compiled +4), player's build back |
| Launch | `0x800B13C0` | `0x8003BD2E` = n (store at `0x800B1450`) |
| AI setup (arena FDAT 205) | `0x8004DDBC` | record = `0x8008E498` + 60 × `0x80042AEC`; byte +0 → `0x80042AEB` |

Nothing else on PP writes `0x80042AEC` (searched the exe and FDAT 201–206). The record table is full: 49 × 60 =
2,940 bytes end exactly at the behaviour table `0x8008F014`, so there is **no spare slot** for new records.

**MoA-US is numbered the same way** (`code`): its list builder `0x800BBEA8` gives each line the opponent's
position in that arena's own file list (top first, lines flagged bit 1 in the arena's byte list are skipped) and
the launch `0x800C0878` stores it in `0x8003EAD5`. So MoA's 10-frame fire hold (§6) goes to the **top 11
opponents of the main arena** (numbers 0–10) and the 20-frame hold to the rest; Ex Arenas get none.

What this gives the 0.7 work (`guess`, design notes, nothing built):
- **Any AI on any opponent:** the AI record and the build are chosen separately from the same list byte, so a
  hook right after `0x800B1130` can set `0x80042AEC` to a different record without touching the build (Rogue
  Arena line-ups, a "hard mode" giving low ranks top-rank AI).
- **MoA opponents need more records:** no room in the arena module, so `0x8004DDBC` has to be hooked to read
  records 49 and up from our own data. The same hook is the natural place for the Phantasma / MoA AI-style
  toggle (§6 open 5).
- **Second arena / Rogue Arena list:** entries 382, 482 and 483 are all indexed by the same number, so a new
  roster is three files (builds, names, prizes) plus AI records, swapped where the screen reads them.

## 4. Behaviours (PP: 20 slots of 28 bytes at `0x8008F014`, 10 in use; MoA-US: 20 × 24 at `0x80098890`, 13 in use)

Entry: +0x00 "fit" routine (returns 0–4096), +0x04 per-frame action, +0x08 on-start (optional),
+0x0C base weight, +0x0E learning blend, +0x10 commitment frames, +0x12 whim chance, +0x14 whim factor,
+0x16 learned weight (starts at +0x0C, moved by the learn routines between 409 and 3686), +0x18 enabled (= fit
routine, or 0 when base weight is 0). Init `0x80083DF4` fills +0x16/+0x18.

**Chooser** `0x80083158` (every 8 frames), for each enabled behaviour (`code`):
score = commitment × (base + (learned − base) × blend) × fit, where commitment = 0.5 for others and, for the
current behaviour, falls from 1.0 to 0.5 over its commitment time; with chance +0x12 the fit is multiplied by
the whim factor (skipped while state +0x238 bit 2 is set). Highest score wins; on a change it stores the
behaviour and the start frame and runs the on-start routine (which may refuse, and then it chooses again).

| # | Fit / action / start (PP) | MoA-US pair | What it seems to do | Status |
|---|---|---|---|---|
| 1 | `0x80081EC0` / `0x80082070` | `0x8008A078` (0.99) / `0x8008A228` | Fixed weight 4.0; fit 4.0 when player block 2 +0x10 & 7 = 5, otherwise (only while running) a distance from the AI to a line stored at block 2 +0x33E…+0x34A: **get around an obstacle** | code / guess |
| 2 | `0x800820F0` / `0x800821B4` | `0x8008A2CC` / `0x8008A384` (MoA much longer) | Fixed weight 4.0; fit 1.0 when an object of kind 0x14 / 0xF8 in the 512 × 52-byte list at `0x801CB258` touches either AC (`0x8008104C`): **react to a stage object** | code / guess |
| 3 | `0x800822C4` / `0x80082368` | none | Fixed weight 4.0; only on stage id 28 (`0x800956D0`), when the AI stands 500+ high and the player is 1,000+ lower; walks forward: **come down to the player on one stage** | code |
| 4 | `0x800823C4` / `0x80082428` | `0x8008A638` (1.00) / `0x8008A69C` | Fit = max(desire A, B) × how much the AI is in front of the player; presses forward with L1/R1: **close in / circle** | code / guess |
| 5 | `0x80082594` / `0x800825A4` | `0x8008A808` / `0x8008A850` | Fit = desire C (air); strafes: **air / strafe** | code / guess |
| 6 | `0x80082700` / `0x80082788` | `0x8008AD1C` / `0x8008ADE4` (1.00) | Fit 0 while the player can see it and it is inside the player's weapon range; otherwise (1 − record +8) × 0.5 (× 2 when block 2 +0x20 bit 0); action only turns toward the target: **hold position outside your weapon's reach** | code |
| 7 | `0x800827A8` / `0x80082850` / `0x80082838` | `0x8008AEAC` (1.00, the action) | Fit = 1.0 when a missile in the object pool targets the AI's AC: **dodge missile** | code |
| 12 | `0x80082B18` / `0x80082B6C` / `0x80082DA8` | none clear | Start: finds the **nearest stage object of kind 0x0A** (sub-value 0xF8) in the list at `0x801CB258` and keeps it in state +0x23C; if the stage has none, it clears its own enabled word (+0x18) and refuses, so the behaviour is **off for the whole match on such stages**. Action: no whims while running (sets +0x238 bits 1–2), turns and moves to the object (`0x80081B6C`), phase +0x240 = 1 once touching it; then compares its height with the object's top (+2 + byte +0x26 × 256), checks for clear space ahead (box test `0x800705A0`), stops moving when clear, and boosts through `0x80081BB0`. Fit 0.25, 0.5 while running, 0.75 once on it: **go to and ride a stage object (lift or platform to high ground?)** | code / guess (what kind 0x0A is) |
| 13 | `0x80082ED4` / `0x80082F4C` | `0x8008A98C` | Fit = a ramp of distance against the player's weapon range (state +0x236), capped at 0.5; 0.5 while running: **keep distance relative to your weapon** | code / guess |
| 14 | `0x80082FF0` / `0x80083030` | `0x8008AA14` | Fit 0.25, 0.5 once running; random forward/back/strafe: **wander** | code / guess |

MoA-US uses slots 1, 2, 4–10 and 14–17; its slot 3 (the stage-28 drop) is empty and slots 7, 8, 10, 15 have
routines with no PP twin.

**Weapon chooser** `0x80081180` (`code`): for each usable slot of player block 2 (count at +0x2DE, slots of 62
bytes from +0x2E0): skips empty ammo; scores range against distance (range default 4000), learned weight
(slot +0x3C, starts 2048), a 110-frame commitment bonus for the current pick, and for kind 2 slots needs the
player visible and a distance factor. The best goes to block 2 +0x19; its range to state +0x234.

**Learning** (`code`): AI hit for d damage → current behaviour −d/4, all others +d/8; weapon in hand −d (kind 2)
or −d/8, other weapons +d/4. Player hit → current behaviour +d/4. A shot from AI weapon slot n (tag 0xA0+n in
the projectile) lands → that slot +d/4; the blade (tag 90) lands → blade chance +d/4. Tags then become 0xB0.

## 5. AI working state (PP `0x801DB838`; MoA-US `0x801E2750`, same offsets where checked)

| Off | Meaning | Status |
|---|---|---|
| +0x000 | Distance to the (delayed) player, 3D | code |
| +0x004 | Distance, horizontal | code |
| +0x008/00C/010 | dx, dy, dz (delayed player − AI) | code |
| +0x018 / +0x01A | Pitch / yaw to the player, yaw relative to the AI's facing (±2048 = ±180°) | code |
| +0x020 / +0x022 | Reverse pitch / the AI's bearing from the player's facing | code |
| +0x028 … | History of the player AC's +0x08…+0x17 (position, facing), 16 bytes each, oldest first | code |
| +0x228 | Pointer to the newest history entry (depth = record +0x11) | code |
| +0x22C / +0x22E | Current behaviour / frame it started | code |
| +0x230 / +0x232 / +0x234 | Frame of the weapon pick / commitment 110 / chosen weapon's range | code |
| +0x236 | Player's current weapon range (4000 if none) | code |
| +0x238 | Flags: **bit 0 player in sight** (clear line, checked every 8 frames; guns and the ○ rule need it); bits 1–3 cleared each frame; bit 2 = no whims; bit 3 = "lost track" decays | code |
| +0x23C, +0x240, +0x241 | Scratch for behaviours 7 / 12 | code |
| +0x244 | **Lost track**, 0–3276: chance the turn routine turns wrong or at random; set from the target's sideways sweep and record +0x10, decays by 163 per frame | code |
| +0x246 | Frame counter for the ○ rule | code |
| +0x248/24A/24C | Desires A, B, C (base + random spread) | code |
| +0x24E … +0x268 | Copied record values (§3) | code |
| +0x24A bytes from +0x000 | Cleared by init | code |

Also `0x801DBAF4`: list scanned by behaviour 7 (object pool view, Lane 4).

## 5b. Aim and fire (PP `0x80081438`, every frame; read in full)

- **Up/down aim**: compares the pitch to the target (state +0x18) with the AC's look angle (block 2 +0x238)
  and presses L2 / R2 outside ±24; with probability "lost track" it keeps the last press and flips at ±767.
  With record +0x10 = 0 (no tracking skill) it writes the angle straight in instead (block 2 +0x220). `code`
- **Weapon in hand**: its learned weight drops by 1 per frame (floor 409), so the AI keeps rotating weapons. `code`
- **Kind 0 slot with no lock value (slot +0x14 = 0)**: fires (□, a fresh press every frame) when the player is
  in sight, under 4,000 units, and both yaw and pitch error are inside a cone: half-angle (4000 − distance) ×
  32 / 1000, clamped to 16–32 units (1.4°–2.8°), or 128 (11°) at distance 0. `code`
- **Kind 1, or kind 0 with a lock value**: fires once block 2 has a lock target (+0x42C) with lock bit 3 set
  (+0x442) and the player is in sight. `code`
- **Kind 2**: unless block 2 +0x280 is set, it presses □ until the AC reaches state 0x22 with that weapon, then
  releases movement (mask −0x504D) and fires on lock: looks like a back cannon that needs a firing stance. `code` / `guess`
- No dice and no delay: when the condition holds, it fires that frame.

## 6. PP vs MoA-US, and what to look at next

Same: the scheme (utility scores, 20 behaviour slots, learning, delayed aim, lost track), the state offsets,
the helpers, most behaviour actions (several identical).

Different (`code`):
- **Firing.** PP: fires whenever its condition holds (§5b). MoA-US `0x8008C4E4`: every chance is a roll against
  state +0x268 (record +0x0D, "shoot"), further divided by a weapon value (slot +0x23); during the first 110
  frames of a match (countdown `0x801E2734`) the chance is a quarter. And MoA-US init `0x8008DC98` sets a
  **fire-button delay line** (queue `0x801E2740`, length `0x801E2748`) that holds the AI's □ back by 10 frames
  (opponents 0–10) or 20 frames (11 and up) whenever `0x8003EAD4` (arena) is 0; Ex arenas get none.
- **Aiming ahead (MoA-US only).** `0x8008D4AC`, when the player is within twice the current weapon's range,
  looks up the shot speed by weapon number (tables `0x80099A10`, `0x80099950`, `0x80099D4C`, `0x80099AAC`,
  `0x800980E4`, `0x8009825C`) and stores the player's position moved forward by velocity × flight time into the
  history. PP stores the actual position.
- **Shot dodging (MoA-US only).** `0x8008CED0`: with chance state +0x26A ("avoid"), scans the object pool
  (`0x801E2A0C`, 256 × 228 bytes) for live shots of 6 kinds heading at the AI within about ±11°, then turns
  ±90° away (`0x80089B2C`) and boosts if energy allows. PP dodges missiles only (behaviour 7).
- **Desperation (MoA-US only).** When the AI's AP falls below max × state +0x26C (record +0x0F): blade chance,
  +0x268 and +0x26A set to 100 %, +0x260 = 5, tracking loss off, reaction delay emptied. Once per match.
- `0x8008D224` (MoA-US) is PP's lost-track and up/down-aim code moved into its own routine: not new.
- Record: PP 49 × 60 bytes inside the arena module, per opponent; MoA-US one 36-byte record from the menu.
- Behaviour entries 28 vs 24 bytes; PP 10 behaviours, MoA-US 13; PP's stage-28 drop behaviour is PP-only;
  PP's record tunes 4 + 3 behaviours.
- Per-frame AI: PP 237 instructions, MoA-US 507; aim-and-fire PP 270, MoA-US 450; MoA's rules routine
  `0x8008D224` and `0x8008CED0` have no PP twin.

Open:
1. What stage-object kinds 0x0A (behaviour 12) and 0x14 (behaviour 2) are, and which arena stages have them
   (who fills `0x801CB258`: Lane 4 / stages). Behaviours 1, 2, 4, 5, 13 are named from their fit routines only.
2. ~~Which record each arena rank uses~~ (round 3 topic 4, §3a: record n = opponent number n = starting rank − 1;
   MoA-US numbers the same way, so its 10-frame hold is for the main arena's top 11).
3. Weapon kinds 0 / 1 / 2 and slot +0x14, +0x23 meanings (Lane 5 / Lane 4).
4. Live test (owner, §8), plus a new one: the same opponent in PP and in MoA, count shots fired in 30 s.
5. **Decided (owner, 2026-11-26):** when MoA opponents are brought over, offer a **toggle in settings** between
   Phantasma-style AI and MoA-style AI, with a short note saying what differs (PP: fires whenever lined up,
   aims at you, dodges missiles only; MoA: fire chance, slow start and 10–20 frame hold, aims ahead, dodges
   shots, desperation mode). PP's records have no fields for the MoA parts, so MoA-style needs a hook that runs
   MoA's extra rules (or MoA's AI code) on top of PP. Not built yet.

## 7. Buttons the AI presses (PP button word)

PP's pad word has a different bit order from MoA-US's. Default layout from the PP menu table
(`0x800BD568`, in the menu's order GO FORWARD … FIRE): forward Up `0x1000`, back Down `0x4000`, turn left Left
`0x8000`, turn right Right `0x2000`, strafe L1 `0x0004` / R1 `0x0008`, look up L2 `0x0001` / down R2 `0x0002`,
blade ○ `0x0020`, boost ✕ `0x0040`, change weapon △ `0x0010`, fire □ `0x0080`. The AC code reads these
through the player block masks (PP +0x29E change weapon, +0x2A0 boost, +0x2A2 fire, +0x2A4 blade: MoA-US
+0x296…+0x29C plus 8). The AI writes fixed bits, so the opponent's block presumably holds this default layout
(`guess`, not checked). The AI writes block 2 +0x08 each frame after clearing it; the previous word is kept at
+0x0C.

## 8. Live test for the owner (optional, confirms §5)

Needs Phantasma in an emulator with cheat codes (for example DuckStation on Windows or the Steam Deck).
1. Arena, any low-rank opponent. Fight normally for 30 seconds, strafing left and right fast in front of it.
   Note whether it loses track of you.
2. Add the code `801DBA7C 0000` (keeps "lost track" at zero), restart the fight and do the same. Expected: it
   keeps turning onto you.
3. Replace it with `801DBA96 1000` (blade lunge chance 100 %). Expected: whenever you are close and in front, it
   lunges with the blade.
Tell me what you see for each. If step 2 shows no change, it may be timing (the game rewrites the value each
frame), not a wrong address.

## Handed over (to this lane)

- From stage enemies (2026-11-28): mission enemy ACs run a script-driven brain inside each mission module (PP mission 19 handler `0x800981B8`, script data `0x80099A80`, interpreter near `0x8009C3D4`), not the arena AI record. `stage-enemies.md` §6.

## Handed over to the build chat (round 3 topic 4)

No new routine starts (all were known from round 2). Hook points for 0.7: `0x800B1130` (AI record store, menu
FDAT 201), `0x8004DDBC` (record read, arena FDAT 205), `0x800AE59C` (list builder), `0x800B04C8` (challenge rule).

## Handed over from this lane

- Lane 1: PP player blocks `0x800424E8` + 936 (MoA-US `0x80045198` + 928), AC objects `0x801D1D20` + 368, current
  player `0x8003BD00`, frame counter `0x801D400C`; ~45 AI routine pairs above for the cross-disc table.
- Lane 4: PP damage routine `0x800734A0` (AP at AC +0x160), projectile hit `0x80085154`, AC object +0x4C/+0x54
  routine pointers, match flags `0x800956DC`.
- Lane 5: player block 2 +0x25C = AI record number; PP default key table `0x800BD568`; weapon slot +0x29 tag,
  +0x3C AI weight.
- Lane 2: "lost track" (+0x244) explains why strafe dancing beats the AI; keep it working. MoA's AI fires
  less and later (dice, 110-frame start, 10–20 frame delay): an AI difference, not a feel difference.
- Lane 4: stage-object list `0x801CB258` (512 × 52 bytes; kind byte at +0x2C, an index at +0x06 into 24-byte
  records at `0x801CAF58`): the AI looks for kinds 0x0A and 0x14.
- Lane 5: PP compiled stat block +4 = AI record number (MoA padding); +3 = Human PLUS, also set from the AI record.

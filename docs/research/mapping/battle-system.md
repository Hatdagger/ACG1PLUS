# Battle System Mapping

**Lane 4 of the 2026-11-26 mapping restart** (`PLAN.md`). The base game is **Project Phantasma USA
(SLUS-00670, "Version 1.0")**, so the first part of this file is Phantasma (tag `PP`). The second part,
"Master of Arena (reference)", is the earlier MoA work and is entirely **MoA-US** (SLUS-01030 v1.0 disc 1).

## Phantasma (PP) — battle system core (Lane 4, 2026-11-26)

Read from the disc's code only (static, nothing run yet). Status tags used below:

| Tag | Meaning |
|---|---|
| `code` | read in Phantasma's own code: the instruction that reads or writes it was looked at, and the meaning follows from what that code does |
| `moved` | Phantasma offset found by pairing a MoA routine with its Phantasma twin and seeing the same instruction with a different offset. The **meaning** is carried over from the MoA notes (second part of this file) and has not been re-proven on PP |
| `guess` | unproven |

Nothing here is `tested` yet; the live test list at the end says what to check first.

### How this was done

- Phantasma's modules were paired with MoA's using `pp-disc/maps/moa-to-pp-functions.tsv` (pp-work pack), then
  each paired routine was disassembled side by side: same instruction, different offset → the field moved.
  Doing that over all 1,000+ paired VS-engine routines, while tracking which register holds the player block
  (loaded from the "current player block" global) and which holds an AC object (loaded from player block
  +0x3A0, or the first argument of the AC routines), gives the field shift tables below with hit counts.
- Phantasma's VS engine and arena engine were then paired with each other (`mipscmp.py`, PP FDAT 204 vs 205):
  543 routines are byte-identical and 61 differ only in numbers, so almost all fight code is shared and
  the arena addresses below come from that pairing.
- The sandbox helper scripts (disassembler wrapper, pairing, field statistics) are not committed: they are
  small and Lane 1 owns the shared tools. They read the pp-work / ma-work packs only.

### Where things are on the disc

| What | Disc file | RAM | Disc position of an address |
|---|---|---|---|
| Boot program | `SLUS_006.70` | loads at `0x80011000` | file offset = addr − 0x80011000 + 0x800 |
| VS engine | `GG/COM/FDAT.T` entry **204** (sector 9,089, 132 sectors) | `0x800490D0` … | `FDAT 204 +(addr − 0x800490D0)` |
| Arena engine | FDAT.T entry **205** (sector 9,221, 144 sectors) | `0x800490D0` … | `FDAT 205 +(addr − 0x800490D0)` |
| Fight stage code | FDAT.T entries 100+ (e.g. **100**), one per stage | `0x8008F5D0` (match struct +0x20) | `FDAT n +(addr − 0x8008F5D0)` |

Every address below without a file is code or data in the module named in its section heading (VS = FDAT 204,
arena = FDAT 205). Addresses `0x8003xxxx`–`0x8004xxxx` below `0x800490D0` are boot-program RAM (no disc copy:
they are set at run time). Addresses `0x801Dxxxx` are run-time RAM only.

### Globals (PP, data)

| PP address | Size | Meaning | MoA-US twin | Status |
|---|---|---|---|---|
| `0x800424E8` | 936 (0x3A8) | **Player block P1** | `0x80045198` (928) | code (VS entry `0x8004A228`, hit routine) |
| `0x80042890` | 936 | **Player block P2** (= P1 + 0x3A8) | `0x80045538` | code |
| `0x8003BD00` | u32 | **Current player block** pointer (set before each per-player routine) | `0x8003EAA0` | code |
| `0x8003BD04` / `0x8003BD08` | u32 | Pointers to P1 / P2 player block (set by VS load) | `0x8003EAA4` / `A8` | code (`0x8004DECC`) |
| `0x8003BCF1` | u8 | Set to 1 just before the main loop starts `?` | `0x8003EA91` | code, meaning guess |
| `0x8003BCF2` | u8 | **Replay flag** (VS: 1 = START at CONTINUE?, replay next; arena: 2 = this fight is a replay, see Arena) | `0x8003EA92` | code |
| `0x8003BCF4` | u8 | **Round number** (+1 per decided round; its low bit picks whose stage is used) | `0x8003EA94` | code |
| `0x8003BD10` / `12` / `14` / `16` | u16 ×4 | **VS win/loss counters**: 1P wins, 1P losses, 2P wins, 2P losses (cap 999) | `0x8003EAB0` … `B6` | code |
| `0x8008F5B0` | 0x20 + | **VS match struct** (table below) | `0x80097EA0` | code |
| `0x8008AE40` / `0x8008B1F0` | 936 ×2 | VS engine's backup of P1 / P2 player block (copied in at entry, copied back at exit) | `0x80093750` / `0x80093AF0` | code |
| `0x801D80E0` / `0x801D8250` | 368 ×2 | **VS AC objects** P1 / P2 | `0x801D4BC0` / `0x801D4D38` (376) | code (VS load, hit routine) |
| `0x801D8088` / `0x801D80B4` | 44 ×2 | Object-type records passed to AC setup for P1 / P2 | MoA "44-byte object-type record" | code (address), moved (meaning) |
| `0x801DA408` / `0x801DA440` | 56 ×2 | VS camera records P1 / P2 (player block +0x3A4 points to them) | `0x801D6EF8` / `0x801D6F30` | code |
| `0x801DA3E0` | 0x10 ×2 | Two records set up by `0x800756C0` at match start, reset by `0x80075730` before a rematch / replay `?` | `0x801D6ED0` | code, meaning guess |
| `0x801DA47C` | 256 × **224** | **Object pool** (shots, missiles, effects) | `0x801D6F6C` (256 × 228) | code (`0x8007FCF8`) |

### VS match struct `0x8008F5B0` (PP)

| Off | Size | Meaning | Status |
|---|---|---|---|
| +0x00 | u8 | **Stage number** for this round: P1's choice (player block +0x25D) on even rounds, P2's on odd rounds | code (`0x8004DECC`) |
| +0x04 | u32 | Stage code table pointer = `0x8008F5D0` (+0 = per-step routine, +4 = init) | code |
| +0x08 | u32 | Load buffer pointer, starts `0x800A2C88`, moved along as the fight data is unpacked | code |
| +0x0C | u32 | Flags: 0x40 = time up, 0x80 = draw, 0x30 = result shown, **0x400 = inputs ignored and hits undo their damage** (pad reader writes 0; on-hit adds the damage back) | code |
| +0x10 | u16 | **End countdown** (0 = fighting; 30 at time up, +90 when the result is decided; waits at 30 for CONTINUE?) | code |
| +0x12 | u16 | Steps elapsed (capped 0x4000); the clock only runs once it is ≥ 16 | code |
| +0x14 | u16 | **Time left in steps** = the larger of the two players' time setting (player block +0x258) × 22 | code |
| +0x16 / +0x1A | u16 | Set to 0x80 / 0x40 at load `?` | code, meaning guess |
| +0x18 | u16 | Cleared at load `?` | code |
| +0x20 … | | Stage code loaded here (see "Stage area boxes") | code |

### Player block (PP, 936 bytes, `0x800424E8` / `0x80042890`)

**Rule:** every MoA player-block field from MoA +0x08 upward sits **exactly 8 bytes higher** in Phantasma.
Found 136 fields this way with no exception except one (below). The 8 bytes come from the pad words at the
start being 32-bit in Phantasma (16-bit in MoA).

Fields read on PP itself (`code`):

| PP off | Size | Meaning | MoA-US off |
|---|---|---|---|
| +0x000 | u32 | Pad this step, **byte-swapped bit layout** (✕ 0x40, ○ 0x20, △ 0x10, □ 0x80, L2 0x01, R2 0x02, L1 0x04, R1 0x08, SELECT 0x100, START 0x800, Up 0x1000, Right 0x2000, Down 0x4000, Left 0x8000). Read by `0x80075958` (called from `0x800759D4`) for both pads at once (P1 low half, P2 high half) | +0x000 (u16, raw layout) |
| +0x004 | u32 | +0x000 of the step before | +0x002 |
| +0x008 | u32 | **Pad the AC acts on** (= +0x000, or 0 while match flag 0x400 is set) | +0x004 |
| +0x00C | u32 | +0x008 of the step before | +0x006 |
| +0x010 | u16 | **Fight flags**: bits 0–2 = stage area bits (cleared each step, set by the stage's box test), 0x10 = won, 0x20 = lost | +0x008 |
| +0x018 | u8 | Selected weapon slot (cleared at match start) | +0x010 |
| +0x019 | u8 | Weapon change request (cleared at match start) | +0x011 |
| +0x01E | u16 | **Energy** (set to capacity +0x26A at match start) | +0x016 |
| +0x020 | u16 | Bit 0 = booster lock (cleared at match start) | +0x018 |
| +0x048 | u8 | Blade wave fired this swing (set to 1 on swing frame 0) | +0x040 |
| +0x04C | u32 | `0x80088B24` + body combination × 20 (table pointer, see Lane 5) | +0x044 |
| +0x084 | u32 | **Lock target** (lock slot 0's target object; also the blade lunge target) | +0x07C |
| +0x204 | u32 | Radar range² | +0x1FC |
| +0x208 … +0x21E | u16 | Movement values scaled in at match start (`0x8007AFD0`, same formulas as MoA: rise, boost, turn, friction) | +0x200 … +0x216 |
| +0x220 | u16 | Missile cursor rest pitch (copied to the AC on a non-locking shot) | +0x218 |
| +0x254 | u16 | "HIT" blink, 40 when this player lands a hit | +0x24C |
| +0x256 | u16 | Set to 40 when this player is hit (damage indicator `?`) | +0x24E |
| +0x258 … +0x39B | 324 | **Compiled stat block** — Lane 5's. Readers named in this file | +0x250 … +0x393 |
| +0x25D | u8 | VS stage choice (compiled +5) | +0x255 |
| +0x27A / +0x27B | u8 | Shell / energy **damage factor** (×/64) | +0x272 / +0x273 |
| +0x27C | u8 | Stability (AC +0x164 = this × 25 ÷ 4) | +0x274 |
| +0x28C | u16 | Stat flags (bits 0–1 dropped into AC +0x38; bit 3 cleared here at match start) | +0x284 |
| +0x2A2 | u16 | **Fire button** mask (tested against +0x008) | +0x29A |
| +0x2BE | | **Blade hit record** (flags; attack at +0x2C0) | +0x2B6 |
| +0x2B0 / +0x2B1 | u8 | Blade line joints (hand, tip) | +0x2A8 / +0x2A9 |
| +0x28B | u8 | Blade wave type | +0x283 |
| +0x2DE | u8 | Weapon slot count | **+0x2D7 (shift +7)** |
| +0x2E0 + slot × 62 | 62 | **Weapon slots** (+0x10 ammo, copied from +0x0E max at match start; +0x01 model ID; +0x09 shots / lock bits; +0x0C flags) | +0x2D8 + slot × 62 |
| +0x39C | u8 | Round history (bit n = won round n) | +0x394 |
| +0x3A0 | u32 | **Pointer to this player's AC object** | +0x398 |
| +0x3A4 | u32 | Pointer to this player's camera record | +0x39C |

The exception: MoA's stealth byte (MoA +0x2D6, read at match start into AC +0x16E) **does not exist in PP**:
PP's match start has no stealth read and its AC object has no stealth field. So PP's stat block is laid out
one byte tighter around there (+0x2DE = MoA +0x2D7), then back to +8 from the weapon slots on. (Lane 5 owns
the stat block; handed over.)

All other MoA player-block fields in the second part of this file (lock slots +0x7C…+0x1FB → PP +0x84…+0x203,
missile cursor +0x20/+0x22 → +0x28/+0x2A, boost mask +0x298 → +0x2A0, …) are `moved` by the +8 rule; the full
list with hit counts is in "Field shift list" below.

### AC object (PP, **368 bytes**; MoA 376)

**Rule:** MoA +0x00 … +0x38 unchanged; MoA +0x43 … +0x4A sit **9 lower**; MoA +0x50 up to +0x16C sit
**8 lower**. MoA's +0x3A … +0x42 (hit stagger timer, hit flash) and +0x16E (stealth) have no PP counterpart:
MoA added them.

| PP off | Size | Meaning | MoA-US off | Status |
|---|---|---|---|---|
| +0x08 / +0x0A / +0x0C | s16 | Position x / y / z (y negative = up) | same | code (box test, stage) |
| +0x12 | u16 | Facing (4,096 = full turn) | same | moved (unchanged offset, 29 hits) |
| +0x28 / +0x2A | u16 | Width / body height (step-size limit, hit cylinder centre) | same | moved |
| +0x36 | u16 | Flags; **0x10 = busy** (set on hit stagger, blade swing). VS load writes 1 for P1, 2 for P2 on top of match start's 1 `?` | same | code |
| +0x38 | u16 | Stat flags from match start (+0x28C minus bits 0–1) | same | code |
| +0x3A | u8 | 7 at match start `?` (MoA's +0x43 = 5) | +0x43 | code (value), meaning unknown |
| +0x3C | u8 | **State** (0x80 / 0x81 = hit stagger, 0x20 / 0x21 blade, 0x22 cannon stance …) | +0x45 | code |
| +0x3D | u8 | **Movement class** (0xC0 airborne) | +0x46 | code (on-hit) |
| +0x3E | u8 | Missile volley counter | +0x47 | moved |
| +0x3F | u8 | **Refire cooldown** (gun fires only at 0) | +0x48 | code (gun fire) |
| +0x40 | u8 | **Alive byte**: 255 fighting; drops when AP reaches 0; 0xFE = wrecked (0 also counts as out) | +0x49 | code (end rules, on-hit) |
| +0x41 | u8 | State when the animation started | +0x4A | moved (state setter `0x80077D94`) |
| +0x50 | u32 | Animation-finished callback (ACs: `0x8007B42C`) | +0x58 | moved |
| +0x54 / +0x58 | u32 | Two more callbacks | +0x5C / +0x60 | moved |
| +0x5C | u32 | **On-hit callback**, called after the AP write (ACs: `0x80079FE8`) | +0x64 | code |
| +0x60 | | Animation player argument / model data `?` | +0x68 | moved |
| +0x64 / +0x65 | u8 | Animation player (+0x64 = 255 while staggered; gun fire needs +0x65 = 255) | +0x6C / +0x6D | code |
| +0x68 | u8 | 2 from match start `?` | +0x70 | code (value) |
| +0x6A / +0x6C | u16 | Hit cylinder radius / height | +0x72 / +0x74 | moved |
| +0x6E | s16 | **Frame counter of the current state** (blade hit window 7–11) | +0x76 | code (blade) |
| +0x76 / +0x78 / +0x7A | s16 | Missile cursor pitch etc. reset on a non-locking shot | +0x7E / +0x80 / +0x82 | code (gun fire) |
| +0x7C | s16 | From weapon slot +0x0A on each shot `?` | +0x84 | code |
| +0xCC / +0xCE / +0xD0 | s16 | **Velocity x / y / z** (1/64 units per step) | +0xD4 … +0xD8 | code (on-hit knock-back) |
| +0x110 | | Missile interceptor burst counter (0 at match start) | +0x118 | code (value), moved |
| +0x148 | u16 | Cleared on stagger; gun fire clears bits 4–7 | +0x150 | code |
| +0x15C | u32 | Cleared on stagger; gun fire sets / clears one bit per weapon side (slot +0x03 / +0x05) — likely muzzle-flash or arm-pose bits `?` | +0x164 | code, meaning guess |
| +0x160 | u16 | **AP × 4** | +0x168 | code |
| +0x162 | u16 | Max AP × 4 | +0x16A | code |
| +0x164 | u16 | **Stability** = stat +0x27C × 25 ÷ 4 | +0x16C | code |

### Match flow (VS engine, PP, code)

**Engine entry `0x8004A228`** (FDAT 204 +0x1158; MoA `0x80051F4C`):
1. Backs up both player blocks (936 bytes each) to `0x8008AE40` / `0x8008B1F0`; copies them back on exit.
2. VS load **`0x8004DECC`**: match struct reset, stage and fight data loads (Lane 3), then for P1 and P2:
   current player block = that player, `0x8004D820`, AC setup **`0x80075144(ac, type record)`**,
   per-AC match start **`0x8007AFD0(ac)`**, then player block +0x3A0 = the AC object.
3. Object pool clear `0x8007FCF8`, cameras `0x8007F1CC` ×2, `0x800756C0` ×2, `0x8004AE38`.
4. **Main loop** (from `0x8004A42C`), one game step per pass:
   - clear player block +0x10 bits 0–2 on both;
   - `0x800873F0` (start / end texts `?`), `0x8006FD10`, `0x800702CC`, `0x80075AF8`, `0x80084118` (between
     them the pad reader, object and AC updates; not split up yet), then the stage's per-step routine
     (match +0x04 → +0);
   - end checks (below) while +0x10 = 0, else the CONTINUE? / countdown handling;
   - draw both views (`0x8007F704` with the current player block set to P1, then P2), `0x8005F838`, `0x8004B668`;
   - steps elapsed +0x12 += 1.
5. Exit: sound and vibration off, player blocks restored, returns 2 (rematch or replay) or 0 (back to the
   VS menu).

**End rules** (identical in effect to MoA, except the clock):

| Way | Check (PP) |
|---|---|
| Time up | time left (+0x14) reaches 1 → countdown 30, flag 0x40; the higher **AC +0x160** wins; equal = draw (flag 0x80) |
| Knock-out | a side's **AC +0x40** is 0 or 0xFE → wait 12 steps; one out → the other wins, both out → draw |
| Out of the area | player block **+0x10 & 7 = exactly 1** → that player loses (both → draw) |

**The clock:** −1 per step once 16 steps have passed and while it is ≥ 2. **Unlike MoA, it does not stop while
player 1's AC is being wrecked** (MoA tests P1's alive byte = 255 first, `0x800523A8`). Handed to Lane 2.

**Result writes:** winner's +0x10 |= 0x10, loser's |= 0x20; winner's +0x39C |= 1 << round; counters at
`0x8003BD10` (P1 wins + P2 losses, or P1 losses + P2 wins); round `0x8003BCF4` += 1.

**CONTINUE?** (P1's pad first, then P2's): ✕ (0x40) = rematch, return 2, replay flag 0; START (0x800) = replay,
return 2, replay flag `0x8003BCF2` = 1, the two `0x801DA3E0` records reset; ○ (0x20) = no, sound 0x36, the
countdown runs out, return 0.

### Damage and hits (VS engine, PP, code)

**Hit routine `0x800718D0(target, hit record, a2, a3)`** (MoA `0x800795DC`):
1. Target is P1's or P2's AC object → current player block = that player, damage = **`0x800717F4(hit)`**;
   other objects → `0x80071874(armour class, hit)`.
2. `0x800717F4` is the same formula as MoA: shell hits (flags & 0x30 = 0x10) attack × player block **+0x27A**
   >> 6, energy hits (0x20) × **+0x27B** >> 6, then × 2 ÷ 3.
3. AP (AC +0x160) −= damage × 4, floored at 0.
4. The AC's on-hit callback (+0x5C) is called with the damage.

**What Phantasma does not have:** MoA's hit routine also sets a stagger timer (AC +0x3A, from the hit's
attack, halved unless stat flag bit 0) and a hit flash (AC +0x3C / +0x3E). Neither exists in PP.

**On-hit callback `0x80079FE8`** (MoA `0x80082430`):
- Vibration on that player's pad (unless replaying).
- Match flag 0x400 → the damage is added back (no damage while inputs are frozen).
- Sets the hit player's +0x256 = 40.
- AP = 0 and not yet wrecked → the wreck sequence `0x800719DC`.
- **Any hit whose impact byte (hit record +4) is not 0 staggers the AC**: state 0x80 (ground) or 0x81
  (cannon stance 0x22 → a different recovery, or airborne / steep hits), +0x36 |= 0x10, knock-back added to
  the velocity, and the stagger strength from **`0x80071D0C(ac, impact)` = impact × 50 ÷ stability (AC
  +0x164), clamped 1 … 30**. Direction picks one of four hit animations (0x25–0x28 ground, 0x29–0x2C air).
- **MoA instead** first computes impact × 256 ÷ stability clamped 0 … 5 (`0x80079AA4`) and **does not
  stagger at all when that is 0** (light hits on a stable AC). Handed to Lane 2: in PP every hit that carries
  impact staggers.

**Hit dispatcher `0x8007FD44`** (MoA `0x8008821C`): sets the attacker's +0x254 "HIT" blink, then calls the hit
routine. The P1/P2 checks use the VS AC addresses above.

### Weapons in flight (VS engine, PP, code)

**Fire selection** in the AC update `0x8007A48C` (around `0x8007AD14`): weapon slot +0x00 = kind →
**gun `0x80078B8C`**, **missile `0x80079444`**, **cannon `0x800796D4`**.

**Gun fire `0x80078B8C`** (MoA `0x80080DF4`, rewritten): needs AC +0x65 = 255, refire **AC +0x3F = 0**, the
fire button (+0x2A2) in the pad +0x008, and ammo (slot +0x10) > 0. Slot +0x09 bits 0–1 = shots per trigger
(1 → 2, 2 → 4). Model ID (slot +0x01) families: **0–19, 20–39, 40–59, 60+** (MoA has five: 0–19, 20–39,
40–59, 60–89, 90+). Shot tables: `0x800889A0` (0–19, 28 bytes each), `0x8008A0F4` (20–39), `0x8008A394`
(40–59), `0x8008A004` (60+). Lock bit: slot +0x0C bit 2 → no lock-on use; otherwise `0x80077E84` lock logic.

**Object pool record** (224 bytes): MoA +0x00 … +0x0F unchanged; from MoA +0x16 on, **4 bytes lower**
(code: pool alloc `0x8008203C`, rifle spawner `0x80082740`, rifle update `0x800801A4`):

| PP off | Meaning | MoA-US off |
|---|---|---|
| +0x00 / +0x02 / +0x04 | Position | same |
| +0x06 | −1 = free, 0 = in use | same |
| +0x08 | Owner object | same |
| +0x0C / +0x0E | −1 / model or sprite id | same |
| +0x10 / +0x11 | 0 / 255 (bytes) | +0x10 / +0x12 |
| +0x12 | Life left | +0x16 |
| +0x14 | **Update routine** | +0x18 |
| +0x18 … | **Hit record** (+0x18 flags, +0x1A attack, +0x1C impact) | +0x1C … |
| +0x20 / +0x22 / +0x24 | Velocity | +0x24 … |
| +0xB8 / +0xBC / +0xC0 | Position one step earlier etc. | +0xBC … |

| Shot kind | Spawner | Update routine |
|---|---|---|
| Bullets, model 0–19 | `0x80082740` | `0x800801A4` |
| Missiles | (via `0x80079158`) | `0x80080BAC` |

**Missile tables** (data, same bytes in VS and arena):

| Table | VS (FDAT 204) | Arena (FDAT 205) | Notes |
|---|---|---|---|
| Launcher table, 8 bytes per model ID | `0x80088A9C` (FDAT 204 +0x3F9CC) | `0x8008DE0C` (FDAT 205 +0x44D3C) | models 0–10, **same values as MoA**; MoA adds model 11 |
| Flight table, 40 bytes per kind | `0x8008A888` (FDAT 204 +0x417B8) | `0x800909A4` (FDAT 205 +0x478D4) | kinds 0–10 same as MoA except +0x16 (PP 1–3, MoA 0x3002–0x3004: extra flag bits `?`); MoA adds kinds 11 and 12 |

### Blade (VS engine, PP, code where marked)

Swing routine `0x80076A54` (MoA `0x8007EB48`, only 62 % alike — the differences are Lane 2's):
the frame counter is AC +0x6E; frame 0 sets player block +0x48; **frames 7–11 are the hit window** with the
blade sweep `0x800768F8(ac, player block +0x2BE)`; the blade line uses joints +0x2B0 / +0x2B1; waves by
+0x28B through `0x800858EC`. Code.

### Stage area boxes (stage code, PP, code)

Fight stage code is loaded at `0x8008F5D0`. In **FDAT 100** (the first fight stage file):
- Box handler **`0x80090070`** (FDAT 100 +0xAA0): box = centre (record +0 / +2 / +4) plus signed bytes × 256
  (+0x22 … +0x27), bit number from the record — the same scheme as MoA.
- Box test **`0x8008FF2C(player block, box, bit)`** (FDAT 100 +0x95C): if the AC (player block +0x3A0) is
  inside min/max on all three axes, player block **+0x10 |= 1 << bit**. For bits 1 and 2 it also copies the
  box (16 bytes) to player block **+0x226** / **+0x236** (likely for the area display / radar `?`).
- Other stage files carry the same routine at other offsets (found by code shape: its final store is at
  +0x9F0 in FDAT 100–103, +0x774 in FDAT 110–111, …).

### Movement and energy (PP, moved)

The handlers pair one to one with MoA (table in `claude/phantasma-base.md`). PP addresses: state setter
`0x80077D94`, turning `0x800760E8`, ground boost `0x80077560`, rise / air `0x80077950` (changed logic),
recover turn `0x800762EC`, step-size cap `0x8004C730` (identical), animation stepper `0x8004D640` (identical),
AC update `0x8007A48C` (energy around `0x8007A960`, interceptor around `0x8007A5B4`, blade start around
`0x8007ADF4`), weapon scale-in `0x8007A404`, lock drop `0x80077E28`, lock build-up `0x800783D0`, sight sweep
`0x80070524`. Fields they use follow the +8 / −8 / −9 rules. What changed in their logic is Lane 2's.

### Arena engine (PP, FDAT 205)

Same player blocks and globals as VS. Arena-specific:

| What | PP arena | Status |
|---|---|---|
| Engine entry | `0x8004A654` | code |
| Player block backups | `0x80090F60` / `0x80091310` | code |
| Match struct | `0x800956D0` (same layout as VS) | code |
| AC objects | **`0x801D1D20`** (player) / **`0x801D1E90`** (opponent) | code (hit routine) |
| Object pool | `0x801DBAF4`, 256 × 224 | code (`0x80085068`) |
| Hit routine | `0x800734A0` (as VS, plus a call after each hit: `0x80083988(damage)` when the player is hit, `0x80083810(damage)` when the opponent is) | code |
| Shared fight code | damage formula `0x800733C4`, on-hit `0x8007BC08`, gun `0x8007A7BC`, missile `0x8007B074`, cannon `0x8007B304`, AC update `0x8007C09C`, match start `0x8007CBE0`, AC setup `0x80076C4C`, blade swing `0x80078684` | code (pairing: byte-identical or numbers only) |

In the arena the main loop runs a separate update set when the replay flag `0x8003BCF2` = 2 (`0x800774CC`
instead of the normal AI / AC / object updates) `?`. After a result the arena asks CONTINUE? only of player 1.

### Field shift list (PP vs MoA)

Player block (MoA → PP): +0x0A … +0x2D6: +8 · +0x2D7 → +0x2DE (+7) · +0x2D8 … +0x39C: +8.
AC object (MoA → PP): +0x00 … +0x38: +0 · +0x43 … +0x4A: −9 · +0x50 … +0x16C: −8 · +0x3A … +0x42, +0x16E: none.
Pool record (MoA → PP): +0x00 … +0x0F: +0 · +0x12 → +0x11 · +0x16 on: −4.
Counts per field (how many paired instructions showed the shift) were collected; the lowest-count fields
(1 hit) are the least sure and are marked `moved` anyway.

### Live tests to confirm (owner, plain VS, unlimited time)

Not run yet. Each is one value to watch in a debug build (or an emulator's memory viewer) on **Phantasma**:
1. AP: P2's AC +0x160 at `0x801D83B0` drops by 4 × damage when P1's rifle hits (default AC: expect 60 AP).
2. State: `0x801D811C` (P1 AC +0x3C) = 0 standing, 1 walking, 0x80 after a hit.
3. Energy: `0x80042506` (P1 player block +0x1E) falls while boosting.
4. Clock: `0x8008F5C4` counts down ~22 per second.
5. Stagger: a light weapon on a heavy AC still staggers in PP (it would not in MoA).

### Open

- The meaning of AC +0x3A (7), +0x60, +0x7C; match struct +0x16 / +0x1A; `0x8003BCF1`; the two
  `0x801DA3E0` records.
- The wreck sequence (`0x800719DC`) and the stagger installers (`0x80073E6C`, `0x80073EBC`) not read in full.
- The arena's extra hit calls (`0x80083988`, `0x80083810`) update a 20 × 28-byte table at `0x8008F014`
  (+0x16): handed to Lane 6.
- The lock slots, missile steering and energy rules are `moved` from MoA, not re-read on PP.

### RAM-map rows (PP) for `ram-map.toml`

Ready to paste once Lane 1 has the `disc` field on main:

```toml
[[var]]
addr = 0x800424E8
size = 936
type = "struct"
name = "pp_player_block_p1"
what = "Player block P1 (P2 at +0x3A8)"
disc = "PP"
status = "code"
src = "mapping/battle-system.md"

[[var]]
addr = 0x80042890
size = 936
type = "struct"
name = "pp_player_block_p2"
what = "Player block P2"
disc = "PP"
status = "code"
src = "mapping/battle-system.md"

[[var]]
addr = 0x8003BD00
size = 4
type = "ptr"
name = "pp_cur_player_block"
what = "Current player block pointer"
disc = "PP"
status = "code"
src = "mapping/battle-system.md"

[[var]]
addr = 0x8003BCF2
size = 1
type = "u8"
name = "pp_replay_flag"
what = "VS: 1 = replay requested at CONTINUE?; arena: 2 = replaying"
disc = "PP"
status = "code"
src = "mapping/battle-system.md"

[[var]]
addr = 0x8003BCF4
size = 1
type = "u8"
name = "pp_vs_round"
what = "VS round number"
disc = "PP"
status = "code"
src = "mapping/battle-system.md"

[[var]]
addr = 0x8003BD10
size = 8
type = "u16[4]"
name = "pp_vs_win_loss"
what = "1P wins, 1P losses, 2P wins, 2P losses (cap 999)"
disc = "PP"
status = "code"
src = "mapping/battle-system.md"

[[var]]
addr = 0x8008F5B0
size = 32
type = "struct"
name = "pp_vs_match"
what = "VS match struct (stage, flags, end countdown, steps, time left at +0x14); FDAT 204 loaded"
disc = "PP"
status = "code"
src = "mapping/battle-system.md"

[[var]]
addr = 0x801D80E0
size = 368
type = "struct"
name = "pp_vs_ac_p1"
what = "VS AC object P1 (AP x4 at +0x160)"
disc = "PP"
status = "code"
src = "mapping/battle-system.md"

[[var]]
addr = 0x801D8250
size = 368
type = "struct"
name = "pp_vs_ac_p2"
what = "VS AC object P2"
disc = "PP"
status = "code"
src = "mapping/battle-system.md"

[[var]]
addr = 0x801DA47C
size = 57344
type = "struct[256]"
name = "pp_vs_pool"
what = "VS object pool, 256 x 224 bytes"
disc = "PP"
status = "code"
src = "mapping/battle-system.md"

[[var]]
addr = 0x800956D0
size = 32
type = "struct"
name = "pp_arena_match"
what = "Arena match struct (same layout as VS); FDAT 205 loaded"
disc = "PP"
status = "code"
src = "mapping/battle-system.md"

[[var]]
addr = 0x801D1D20
size = 368
type = "struct"
name = "pp_arena_ac_player"
what = "Arena AC object, player"
disc = "PP"
status = "code"
src = "mapping/battle-system.md"

[[var]]
addr = 0x801D1E90
size = 368
type = "struct"
name = "pp_arena_ac_opponent"
what = "Arena AC object, opponent"
disc = "PP"
status = "code"
src = "mapping/battle-system.md"

[[var]]
addr = 0x801DBAF4
size = 57344
type = "struct[256]"
name = "pp_arena_pool"
what = "Arena object pool, 256 x 224 bytes"
disc = "PP"
status = "code"
src = "mapping/battle-system.md"
```

### Handed over (from Lane 4 to other lanes)

- **Lane 1:** PP globals and object addresses for the cross-disc table: player blocks `0x800424E8` / `0x80042890`
  (936 B), current player block `0x8003BD00`, VS AC objects `0x801D80E0` / `0x801D8250` (368 B), arena AC
  objects `0x801D1D20` / `0x801D1E90`, pools `0x801DA47C` (VS) / `0x801DBAF4` (arena), 224-byte records, match
  structs `0x8008F5B0` (VS) / `0x800956D0` (arena). Field shift rules above. The RAM-map rows for these are in
  this file (ram-map.toml is not on main yet).
- **Lane 2:** (a) PP has no per-hit stagger timer or hit flash (MoA AC +0x3A … +0x3E); (b) PP staggers on every
  hit with impact > 0, strength impact × 50 ÷ stability clamped 1–30, where MoA skips the stagger when
  impact × 256 ÷ stability < 1; (c) PP's clock keeps running while P1 is being wrecked, MoA's stops; (d) PP gun
  fire has 4 model families, MoA 5; (e) missile flight table +0x16 differs (PP 1–3, MoA 0x3002–0x3004).
- **Lane 3:** fight stage code loads from FDAT 100+ to `0x8008F5D0` through `0x800171B4(2, stage × 5 + 1, dest)`
  — which archive "2" is, and the stage-number → entry rule, are yours.
- **Lane 5:** PP's stat block has no stealth byte (MoA +0x2D6); weapon count is +0x2DE, slots from +0x2E0
  (62 bytes). The time setting is +0x258 (compiled +0x00), the stage choice +0x25D.
- **Lane 6:** arena hits feed a 20 × 28-byte table at `0x8008F014` (field +0x16) through `0x80083988` /
  `0x80083810`; the record index is the byte at `0x801DBA64`.

---

# Master of Arena (reference) — all addresses MoA-US (SLUS-01030 v1.0 disc 1)

Kept as it was written for the earlier MoA mapping. Do not use these addresses on Phantasma: translate them
with the rules in "Field shift list" above or the cross-disc table.

Goal: decode the battle engine (bucket 3 of the system map) well enough to read, change and
eventually replace how an AC moves, fights and takes damage. Status today: ~10%.

Disc: USA disc 1 (SLUS-01030, v1.0). Each mode has its own engine, loaded from
`GG/COM/FDAT.T` over the same address range (from 0x80050DD8), so every finding must say
which engine it was measured in:

| FDAT entry | Engine | Mode byte 0x8003EA2A |
|---|---|---|
| 206 | menus | — |
| 207 | missions | |
| 208 | link-cable versus (netplay) | |
| 209 | **split-screen VS** | 2 |
| 210 | **arena** (also AI Battle, which switches the mode byte from 2 to 3) | 3 |

**Earlier research lives in Armored-CoreR** (`docs/research/gen1-*.md` in
Hatdagger/Armored-CoreR): engine map, movement and energy, weapons/FCS/damage, timing,
models. That is the main source; this file only adds what the recomp side finds and
must not contradict it without a measurement.

## What is already anchored

| Thing | Where | Source |
|---|---|---|
| AC state byte: 0 standing, 1 walking, 64 jump take-off, 65 rising, 75 falling, 97 landing | AC object +0x45 | Armored-CoreR gen1-movement.md (VS engine) |
| Movement values scaled in at match start (rise, boost, turn speeds) | player block +0x200 on, by VS engine 0x8008342C | Armored-CoreR gen1-movement.md |
| Energy rules (drain, refill, red zone, booster lock) | VS engine 0x80082D90; lock = player block +0x18 bit 0 | Armored-CoreR gen1-movement.md |
| Weapon detail tables, FCS records, damage factors | menu tables + VS engine | Armored-CoreR gen1-weapons.md |
| Arena main loop, opponent AI scoring | arena engine 0x80052378 | Armored-CoreR gen1-engine-map.md |
| Player block, 928 bytes × 2 | 0x80045198 / +928 | repo (moa_ai_battle.c) |
| Pointer to the slot's AC object | player block +920 | repo |
| AC object, 376 bytes each, back to back | arena: player AP at 0x801D8D70, opponent at 0x801D8EE8 | codes + repo size |
| Weapon stats in play (ammo, range, lock speed, lock box, missile locks) | inside the compiled stat block, player block +0x250 … +0x393 | codes — **data chat's lane** (`stat-block.md`) |
| Booster / energy reserve | player block +0x016 | codes |
| Opponent AI per frame / init / setup | 0x8008D4AC / 0x8008DC98 / 0x80055768 | repo |
| Camera routine and state | 0x80088988, set 0x80053280 | repo (netplay) |
| Controller sampler, replay flag, match timer | 0x8007F69C, 0x8003EA92, 0x8009EFDC | repo |

See `ram-map-gameshark.md` for the full code-derived list.

## Test setup

Plain 2-player **VS** (split screen). The clock is kept alive by writing 2,600 back into
0x80097EB4 (unlimited time is also a real option in player 1's or 2's VS settings, but the
write is quicker to set up). Both units are under direct control (no AI), so one action at a time can be repeated and logged: one
shot, one boost, one hit. VS runs on its **own engine (FDAT 209)**, not the arena engine: the
arena-only addresses (the AP codes gated on 0x8007BBAE, the AI hooks) do not apply to plain
VS until re-found there. Player blocks (0x80045198) are shared by all engines.
Use AI Battle only later, to check that AI-driven units behave the same.

Before trusting any address, confirm the disc is NTSC-U **v1.0**: a v1.1 code set exists
with the same layout shifted (+0x64 save data, +0x60 battle data).

## Method

1. **Watch a value change, then name the code that changed it.** The runtime's debug tools
   (debug server, write trace, `PSX_READ_WATCH=lo,hi`) report which guest function touches
   an address. One AP drop → the damage routine. One ammo drop → the fire routine.
2. **Poke a value, watch the effect.** Change one byte of the AC object mid-fight in AI Battle
   and note what the AC does. This names fields fast without reading code.
3. **Record every finding** in the tables below: address/offset, meaning, how it was proven,
   which engine. Unproven guesses are marked `?`.

## Order of work

1. **AC object (376 bytes)** — first pass done 2026-11-17 (table below). Open: heat, the
   animation records, +0x118, the two extra callbacks. Everything else in combat reads these.
2. **Damage path** — what lowers AP: hit test → damage formula (weapon attack vs shell/energy
   defence) → AP write. Ties the part tables' defence numbers to combat.
3. **Weapons and projectiles** — fire routines, pool layout and update routines done
   2026-11-17 ("Weapons in flight"); missile lock slots, volleys and steering read 2026-11-17
   (static). Open: run-test the missile notes; the second missile spawn call `0x8007BED4`.
4. **Movement** — done for the default AC 2026-11-17 ("Movement and energy"): the 75 cap is ¾ of
   the AC's width, handlers, energy rules all measured. Open: landing recovery, other leg types.
5. **Match start / end** — done 2026-11-17 ("Match start and end"): entry chain, end rules,
   result counters, CONTINUE? choices. Open: the stage's +0x08 flag, START → replay test.

## AC object fields

Measured 2026-11-17 in VS (FDAT 209) by snapshotting both objects while doing one thing
at a time (stand, walk, turn, strafe, boost, fire, get hit), then naming fields from the code.
"snap" = seen changing in the snapshots; `?` = unproven.

| Offset | Size | Meaning | Proven by |
|---|---|---|---|
| +0x00 / +0x04 | u32 | List links (previous / next object; P1: 0x801D4B68 / 0x801D4B10) `?` | read |
| +0x08 / +0x0A / +0x0C | s16 | Position x / y / z (y = height, **negative = up**; start P1 (−3000, −1251, −3000), P2 (5000, −1251, −3000)) | pad + teleport |
| +0x12 | u16 | Facing, 4096 = full turn (0 = +z, 3072 ≈ +x) | pad test |
| +0x20 / +0x22 / +0x24 | s16 | Last step's x / y / z | snap |
| +0x28 / +0x2A | u16 | 100 / 212 (+0x2A = body height, same value as +0x74) `?` | read, `0x8007D2D8` |
| +0x2C | s16 | **Floor height below the AC** (−1250 on the ground; 32767 when nothing found below) | snap (high jump) |
| +0x30 | u16 | Floor polygon / material under the AC (87 on the VS floor, −1 = none) `?` | snap |
| +0x32 | u16 | Bit 2: can't be hit by the blade line test (set at creation from the model) | code `0x80078A38` |
| +0x36 | u16 | Flags. Starts 1. **Bit 0x10 = busy** (set during a blade swing and during hit stagger; blocks a new swing) | code + snap |
| +0x38 | u16 | Stat flags from match start (SP-JAM bit 3) | data chat |
| +0x3A | u16 | Hit stagger, set on hits with flag 0x40 (from the hit's +2, halved unless player block +0x284 bit 0) | `0x800796E0` |
| +0x3C / +0x3E | u16 | Hit flash: +0x3C = 100, +0x3E = damage/4, on hits with flag 0x80 | `0x800796F8` |
| +0x43 | u8 | 5 from match start `?` | data chat |
| +0x45 | u8 | **State** (list below) | snap + code |
| +0x46 | u8 | **Movement class**: 0 still, 2 turning on the spot, 0x10 moving on the ground, **0xC0 airborne**, 0x80 staggered | snap |
| +0x48 | u8 | **Refire cooldown**: the gun fire routine only fires at 0, otherwise counts it down (5 after a rifle shot) | `0x80081034` + snap |
| +0x49 | u8 | 255 during the fight (the match timer only runs while P1's is 255) | timer code |
| +0x4A | u8 | State when the current animation started (the state setter writes both +0x45 and +0x4A) | `0x80080060` |
| +0x58 | u32 | **Animation-finished callback**, called with (object, +0x4A); picks the next state. ACs: `0x8008389C` | `0x8007F088` |
| +0x5C / +0x60 | u32 | Two more callbacks (ACs: `0x80085180`, `0x80085668`) `?` | read |
| +0x64 | u32 | On-hit callback after the AP write. ACs: `0x80082430` (sets the stagger states 0x80 / 0x81) | `0x80079748` |
| +0x68 | u32 | Pointer, 0x801A2E10 for P1 (model data `?`) | read |
| +0x6C … +0x6F | | Animation player (`0x80053F50(obj + 0x6C, obj + 0xD4)` each state change) | `0x8008000C` |
| +0x70 | u8 | 2 from match start `?` | data chat |
| +0x72 | u16 | **Hit cylinder radius** (75) | `0x80078A50` |
| +0x74 | u16 | **Hit cylinder height** (212); centre = y − height/2 | `0x80078A5C`, `0x800832E0` |
| +0x76 | s16 | **Frame counter of the current state** (resets to 0 on every state change; blade hit window = 7–11) | snap + blade code |
| +0x8C … +0xCA | | Animation channel records (start / end frame pairs, e.g. 50/51, 9888/9889) `?` | snap |
| +0xD4 / +0xD6 / +0xD8 | s16 | **Velocity x / y / z** in 1/64 units per step (walk ≈ 960 x; jump −3264 y up, +5056 falling; knock-back from a rifle hit −448 / −64). Not cleared when the AC stops | snap |
| +0x118 | s16 | **Missile interceptor burst counter** (15 at a burst start, keeps counting down below 0; see `parts-tables.md`, "Missile interceptor") | snap + data chat |
| +0x150 | u16 | 15 while rising on the booster; cleared at swing start `?` | snap |
| +0x152 … +0x158 | | Two (id, value) pairs, e.g. (0xA0B4, 140), changed by firing and boost-dashing: sound / effect handles `?` | snap |
| +0x164 | u32 | 6 while rising on the booster; cleared at swing start `?` | snap |
| +0x168 | u16 | **AP × 4** (default AC 33,536 = 8,384 AP) | write trace on a hit |
| +0x16A | u16 | Max AP × 4 | read |
| +0x16C | u16 | Stability × 25 ÷ 4 (default AC 237) | data chat + read |
| +0x16E | u16 | Stealth | data chat |

**States (+0x45)** — seen in snapshots or written by the code:

| Value | State | Source |
|---|---|---|
| 0 | standing | snap |
| 1 | walking | snap |
| 5 | strafing (R1) | snap |
| 9 | turning on the spot (Left) | snap |
| 1 … 8 | ground movement group (jump table at `0x80085730`) | code |
| 0x20 / 0x21 | blade swing, ground / airborne (+0x46 = 0xC0) | blade code |
| 0x22 / 0x23 | blade / action states set by `0x80081B18` and `0x800828BC` `?` | code |
| 0x30 … 0x37 | set by the animation-finished handler `0x8008389C` from the animation byte +0x6C, likely landing / recovery variants `?` | code |
| 64 (0x40) | jump take-off (Cross on the ground) | snap |
| 65 (0x41) | rising on the booster | snap |
| 0x41 … 0x48 | airborne group (jump table at `0x8007FCD8`) | code |
| 74 / 75 (0x4A / 0x4B) | falling | snap + code |
| 96 / 97 (0x60 / 0x61) | landing | code, Armored-CoreR |
| 0x80 / 0x81 | **hit stagger** (set by the on-hit callback `0x80082430`) | snap + code |

State setter: **`0x8007FFF0(object, animation, state)`**: starts the animation, scales the
velocity ×64, writes +0x45 and +0x4A.

VS engine objects: P1 0x801D4BC0, P2 0x801D4D38 (player block +0x398 points to them).
Arena engine (codes): P1 AP at 0x801D8D70 = an object at 0x801D8C08 if the layout matches.

## Player block fields (0x80045198)

| Offset | Size | Meaning | Proven by |
|---|---|---|---|
| +0x000 / +0x002 | u16 | Button word copies (match +0x04 / +0x06 in the snapshots) `?` | snap |
| +0x004 / +0x006 | u16 | Button word / previous (read by the AC code) | repo |
| +0x00A | u16 | 5 after the first jump / landing (set in `0x8008389C`) `?` | snap + code |
| +0x016 | u16 | Booster / energy reserve (28,000 full; 18,844 → 6,963 while boosting) | code + snap |
| +0x030 … +0x03F | | Blade line from the previous swing frame | blade code |
| +0x07C | u32 | **Lock target**: pointer to the locked AC object (P2's 0x801D4D38 when P2 is in front; 0 when lost). Also the blade lunge target | snap + blade code |
| +0x07C … +0x1FB | 8 × 48 | **Lock slots**, one per locked target (+0x7C is slot 0's target, +0xA2 its lock count). Layout in "Missiles: lock slots, volleys and steering" | code `0x80080638` |
| +0x01A | u16 | Locks still free (missile weapon) | code |
| +0x020 / +0x022 | u16 | Missile lock cursor pitch / yaw | code + `stat-block.md` |
| +0x078 | u32 | Lock slot being worked on | code |
| +0x24C | u16 | "HIT" HUD blink: 40 on landing a hit on the other AC, counts down | code `0x8008821C`, `0x8005699C` |
| +0x24E | u16 | Set (37) when this AC is hit, counts down (damage indicator `?`) | snap |
| +0x250 … +0x393 | 324 bytes | Compiled stat block (weapon slots, ammo, range, lock values…) — **data chat's lane, see `stat-block.md`**; writes found here get handed over in README.md | — |
| +0x398 | u32 | Pointer to AC object | repo |

## Damage (VS engine, measured 2026-11-17)

Hit handler `0x800795DC(target object, hit record, a2, a3)`:

1. Target is P1's or P2's AC object → sets the "current player block" pointer
   `0x8003EAA0` to that player's block and calls `0x80079500(hit)`. Any other object
   (non-AC targets) → `0x80079580(armour class, hit)`.
2. `0x80079500`: `raw = hit+2` (attack). Shell hits (hit byte 0 & 0x30 = 0x10) use
   `raw × player block +0x272 >> 6`; energy hits (0x20) use `+0x273`. Then
   **damage = raw × 2 / 3**.
3. AP (object +0x168, in quarters) −= damage × 4, floored at 0 (`0x80079704…0x80079720`).
4. Hit flags: 0x40 → stagger (+0x3A), 0x80 → flash (+0x3C/+0x3E); then the object's
   +0x64 callback.

Non-AC targets (`0x80079580`): energy hits scale by `(8 − class) / 8 + 1`, the class from
bits 8–11 of a value on the target's model; shell hits do full damage.

Check: WG-RF35 (attack 218) on the default AC (shell factor 27): 218 × 27 >> 6 = 91,
× 2 / 3 = **60 AP per hit** — matches the HUD (8,384 → 8,324 → …).

Hit record (`0x801D6F88` for P2's rifle): byte 0 flags (= weapon detail +0x12, 0x17),
+2 attack (218), +4 impact (3) — the weapon detail table values.

## Blade (VS engine, measured 2026-11-17)

The default VS AC carries an LS-2001 blade. It is not in the Triangle weapon cycle; it has
its own button, **Circle**. A swing lunges about 500 units forward along the facing
(facing 0 = +z).

- One blade hit on the default AC = **246 AP**. Same damage routine as guns
  (AP write at `0x80079720`), treated as an **energy** hit: 738 × 32 (player block +0x273)
  >> 6 = 369, × 2 / 3 = **246**. Repeated 4 times, same result each time.
- In RAM: P1 player block **+0x2B8 = 738** (compiled +0x68, blade attack) and
  **+0x26C = 2,050** (compiled +0x1C, charge drain). This confirms the data chat's reading:
  blade record **+0x30 = attack**, +0x2C = charge drain.
- Armored-CoreR's "492 seen" is two blade hits (one swing once landed 492 here too). The
  "683 expected" came from the swapped fields and can be dropped.
- Reach: one hit at 700 units, two hits from 650 in (see "Blade code"), miss at 800.

### Blade code (VS engine, read statically from FDAT 209, 2026-11-17, not yet run-tested)

**Swing start** (inside the AC update, `0x80083224`):
- Needs the blade button (player block +0x29C) newly pressed (+0x04 set, +0x06 clear),
  `0x8008F8B8` false, blade attack +0x2B8 ≠ 0, energy not locked (+0x18 bit 0 clear),
  no swing already running (AC object +0x36 bit 0x10 clear), and energy +0x16 **greater
  than** the charge drain +0x26C. Then +0x16 −= drain.
- Lunge target: `0x80078230` searches 3,000 units ahead within ±512 (±45°) of the facing;
  the found AC goes to player block **+0x7C**. While swinging, the AC turns towards it
  each frame at turn speed +0x20E (`0x8007EB48` start).
- AC object **+0x45 = 0x20** (ground swing) or **0x21** (when +0x46 = 0xC0, likely
  airborne `?`); **+0x36 |= 0x10** (swinging).

**Each swing frame** (`0x8007EB48`, frame counter = AC object **+0x76**):
- Frame 6: stores the blade line (hand → tip, model joints player block +0x2A8 / +0x2A9,
  via `0x80078888`) in player block +0x30 … +0x3F.
- **Frames 7–11 are the hit window** (5 frames): `0x8007E9EC(ac, player block +0x2B6)`.
  The hit record is **player block +0x2B6** (so +0x2B6 = flags, +0x2B8 = attack 738).
- From frame 12 the swing winds down; +0x36 bit 0x10 is cleared at the end
  (`0x8007EFD0`).
- Blade waves (player block +0x283 = 1…4, a jump table at 0x80051154) spawn a
  projectile through `0x8008E664` once per swing (flag player block +0x40).
  +0x283 = 4 skips the melee sweep entirely (wave only `?`).

**Blade sweep** `0x8007E9EC`: each active frame it tests **two** blade lines, the
**halfway** line between last frame's and this frame's position, then this frame's
line (`0x8007E82C` twice), and then saves this frame's line to +0x30. Each test:
1. `0x80071ED4`: stage first (`0x80077EC8`), then both AC objects except the swinger
   (`0x800789DC`, line vs the target's cylinder: radius object +0x72, height +0x74;
   skipped if object **+0x32 bit 2**, a flag set at object creation from the model),
   then other objects (`0x8008CFB8`). The first thing hit → hit dispatcher **`0x8008821C`**
   → (AC) `0x800795DC` damage, plus a spark (`0x8008C164`) and a sound (`0x8008B7B8`).
2. `0x80074FA4`: stage geometry; destructible pieces (owner +0x30 bit 17) also take the hit.

**Why one swing can hit twice (492):** there is **no "already hit this swing" check**.
Each of the up to **10 line tests** (5 frames × 2) that touches the target's cylinder
deals a full hit. Normally the blade arc crosses the target in only one sample → 246.
When two samples overlap the target → 2 hits = 492. Theoretical maximum 10 hits.

**Measured (VS engine, 2026-11-17, harness script `blade4.py`):** both ACs reset to the same spot
before each swing (P2 at (5000, −1251, −3000), P1 straight behind it at distance d, facing it),
default AC vs default AC, write trace on P2's AP:

| Distance | Result (11 swings) |
|---|---|
| 800 | miss |
| 700 | 1 hit, 246 (twice) |
| 650, 600, 550, 500, 400, 300 | **2 hits, 492, every time** |

So **close range = double hit** for the default LS-2001 blade; 3+ hits never seen. The two
AP writes land 3–4 video frames apart, i.e. on **two different swing frames**, not the
halfway + current pair within one frame. (The earlier "from 400 it missed" was P2 knocked out of
place by the previous swing.)

**Hit dispatcher side note:** when one AC hits the other, the attacker's player block
**+0x24C = 40**; it counts down each frame and drives the HUD "HIT" blink (`0x8005699C`).

## Weapons in flight (VS engine, 2026-11-17)

**Fire routines by weapon kind** (slot +0 = kind, slot record = player block +0x2D8 + slot × 62,
selected slot = player block +0x10; picked in the AC update at `0x80083160`):
0 gun → **`0x80080DF4`**, 1 missile → **`0x80081888`**, 2 cannon → **`0x80081B18`** (the cannon one
sets the AC states 0x22 / 0x23, the stop-to-fire stance `?`).

Gun fire `0x80080DF4`: needs AC +0x6D = 255 and **refire cooldown AC +0x48 = 0**, the fire
button (player block +0x29A) or AC +0x47 set, and ammo (slot +16) > 0. Slot +9 bits 0–1 =
shots per trigger. The **model ID (slot +1)** picks the family, its shot table (data chat's
shot-speed tables) and its spawner; every spawner installs a per-frame **update routine**:

| Model ID | Spawner | Update routine (record +0x18) |
|---|---|---|
| 0–19 (bullets; rifle = 0) | `0x8008AC30` (loops for multi-shot; ID 8 = 5 shots, −4 ammo) | **`0x80088674`** (measured on the rifle) |
| 0–19, other branch `?` | `0x8008DEC0` | `0x8008DB18` |
| 20–39 | `0x8008E664` (blade waves use it too) | `0x8008E1B0` |
| 40–69 | `0x8008D8CC` | `0x8008D584` |
| 70–89 | `0x8008CBCC` | `0x8008C948` |
| 90+ | `0x8008D38C` | `0x8008D0A4` |
| missiles (kind 1) | `0x8008B070` | **`0x8008907C`** (measured) |

Every update routine reaches the hit dispatcher `0x8008821C` except the 90+ one (`0x8008D0A4`, not traced).

### The object pool (256 × 228 bytes at 0x801D6F6C)

Shared by shots, missiles and effects (smoke, sparks, explosions have their own update
routines, e.g. `0x80089F6C`, `0x8008A000`). Cleared at match start by `0x800881D0`
(+0x06 = −1 in every record). Measured by dumping the pool while a rifle shot and a missile
were in flight:

| Offset | Size | Meaning |
|---|---|---|
| +0x00 / +0x02 / +0x04 | s16 | Position x / y / z |
| +0x06 | s16 | **−1 = free**, 0 = in use |
| +0x08 | u32 | Owner (the firing AC object) |
| +0x0E | u16 | 83–84 `?` |
| +0x10 / +0x12 | u16 | 1 (2 for some effects) / 255 `?` |
| +0x16 | u16 | **Life left**, counts down each step (rifle 13, missile 60) |
| +0x18 | u32 | **Update routine** (table above) |
| +0x1C … +0x22 | | **Hit record** passed to the dispatcher: +0x1C flags (0x17 for both), +0x1E attack (rifle 218, missile 830), +0x20 impact (3 / 12), +0x22 2 / 94 `?`. This is the 0x801D6F88 record seen in the damage test |
| +0x24 | s16 | Rifle: **speed per step (701)** |
| +0x2A / +0x2C / +0x2E | s16 | Missile: **target position** (P2's x, y − 106, z) |
| +0x38 | u32 | Missile: **target object** (P2's 0x801D4D38) |
| +0xBC / +0xBE / +0xC0 | s16 | Rifle: **position one step earlier** (the hit test runs along this line) |
| +0xC4 … +0xCA | | Rifle: 25850, 250, 4, 30 `?` |

Not done yet: the families 20+ in play (needs a build with those weapons). Missiles: see below.

### Missiles: lock slots, volleys and steering (VS engine, static read 2026-11-17, not run-tested)

Read from FDAT 209 only. Angles: 4,096 = 360°. "Step" = one game step (~22 per second).
Weapon slot offsets are decimal, as in `stat-block.md`. The lock build-up itself (cursor speed
from slot +32, lock time slot +9) is in `stat-block.md`, "Lock-on"; this adds the rest.

**Lock slots**: 8 × 48 bytes from player block +0x7C (+0x7C … +0x1FB), one per locked target.
Offsets from the slot start (slot 0 = player block +0x7C):

| Slot off | Meaning |
|---|---|
| +0x00 | Target object (0 = empty) |
| +0x08 | Lock timer (counts up while the cursor sits on the target) |
| +0x10 / +0x12 / +0x14 | Target position (refreshed from the sight sweep each step) |
| +0x16 | Flags: bit 1 = lockable, bits 3+4 = locked |
| +0x18 / +0x1A | Direction to the target (pitch / yaw) |
| +0x26 | **Locks on this target** (max 6) |

The snapshots' "+0x86 / +0x9A count up" are slot 0 +0x0A / +0x1E: driver still open.

Other player-block fields used (`0x80080638`):
- **+0x1A** = locks still free (slot +34 − locks held).
- **+0x1C** = last missile launched at the current target.
- **+0x20 / +0x22** = cursor pitch / yaw (rests at +0x218 / 0).
- **+0x26** = cursor shown.
- **+0x28 / +0x2A / +0x2C** = cursor point in the world.
- **+0x78** = lock slot being worked on.

**Build-up extras**:
- Candidates come from the sight sweep `0x80078230` with the inner lock box (slot +28 / +30).
- A lock is dropped when its direction leaves the outer box (± slot +24 pitch, ± slot +26 yaw)
  or the target leaves the sweep.
- It stops gaining locks once the locks held reach slot +34 or the current ammo (slot +16).
- Each new lock plays sound 0x2B (0x2C for the last free one), and **the cursor jumps back
  to the centre**. So every extra lock on the same target makes the cursor travel out again.
- While locking, a tick sound 0x2F plays every 4 steps.

**Firing** (`0x80081888`):
- It fires when there is at least one lock and either the fire button is held (pad +0x04 &
  +0x29A), or no locks are free and slot +12 bit 0x80 (SP-M/AUTO) is set.
- AC +0x47 = locks × 4. It counts down 1 per step, and every 4th step a volley goes out from
  side A and side B (`0x800815A4` twice). That uses one lock, and the target gets
  `0x8008F3C8(target, 2, ac)`. After the last volley AC +0x48 = slot +35.
- `0x800815A4` launches each missile:
  - ammo slot +16 goes −1;
  - the start direction is the target direction plus random spread;
  - the start speed is 100 plus the launcher's motion;
  - the spawner is `0x8008B070`.

**Launcher table `0x800912B0`**, 8 bytes per model ID (slot +1):
- +0 = flight kind
- +1 = ignition delay (steps)
- +2 / +3 = missiles per volley from side A / B
- +4 / +6 = random spread (pitch / yaw)

| Model | Kind | Delay | A+B | Spread |
|---|---|---|---|---|
| 0 | 0 | 1 | 1+0 | 300 / 600 |
| 1 | 1 | 1 | 1+0 | 288 / 578 |
| 2 | 0 | 1 | 1+1 | 0 |
| 3 | 1 | 1 | 3+3 | 0 |
| 4 | 2 | 2 | 1+0 | 256 / 512 |
| 5 | 3 (splits) | 1 | 1+0 | 256 / 512 |
| 6 | 3 (splits) | 1 | 1+1 | 0 (+0xC0 pitch) |
| 7 | 0 | 1 | 2+0 | 0 |
| 8 | 0 | 1 | 3+0 | 0 |
| 9 | 5 (never lets go) | 3 | 1+0 | 0 |
| 10 | 4 (aims during delay) | 20 | 1+0 | 0 |
| 11 | 11 (never lets go) | 1 | 1+0 | 0 |

**Missile pool record** (update `0x8008907C`, spawner `0x8008B070`):
- +0x16 = life (flight table +0x02)
- +0x22 / +0x24 / +0x26 = **velocity**
- +0x38 = target
- +0x3C = age
- +0x3E = ignition delay
- +0x40 = flight kind
- +0x42 = flags (bit 0 = target has been in the cone, bit 1 = **gave up**)
- +0xBC / +0xBE = heading (pitch / yaw)

**Flight table `0x80093114`**, 40 bytes per kind:

| Kind | Fuel +0x00 | Life +0x02 | Thrust +0x0C | Top speed +0x0E | Turn/step +0x10 | Cone ± +0x12 |
|---|---|---|---|---|---|---|
| 0, 9 | 50 | 60 | 80 | 350 | 110 (9.7°) | 1024 (90°) |
| 1 | 80 | 100 | 70 | 320 | 100 (8.8°) | 90° |
| 2, 3, 4, 12 | 120 | 160 | 60 | 300 | 90 (7.9°) | 90° |
| 5 | 180 | 200 | 30 | 100 | 50 (4.4°) | 90° |
| 8 | 80 | 100 | 40 | 300 | 45 (4.0°) | 800 (70°) |
| 10 | 80 | 100 | 40 | 320 | 60 (5.3°) | 90° |
| 11 | 250 | 300 | 30 | 100 | 60 | 1536 (135°) |

The rest of each flight record:
- +0x08 / +0x0A = exhaust puff (`0x8008A660`);
- +0x16 / +0x20 / +0x24 = hit effect and blast data.

Kinds 8, 9, 10, 12 are not used by the launcher table. The other spawn call (`0x8007BED4`) may use them `?`.

**Each step**:
1. Before ignition (age < +0x3E): coasts with no thrust, under gravity (0x8003EA48). Kind 4 already
   turns toward the target.
2. At ignition: sound 0x6E, then an exhaust trail. **Kind 3 splits into 4 kind-0 missiles 6 steps
   later** (yaw ±0x1C0, two with pitch −0x200), same target.
3. **Homing** (`0x80088F60` gives the direction to the target):
   - inside the cone (± table +0x12, pitch and yaw) → bit 0 is set;
   - **leaving the cone after having been inside → bit 1 is set, and it gives up for good**. It then flies on
     with a 10% chance per step of a random ±32 wobble. Before it has ever been inside, it keeps turning
     toward the target;
   - kinds 5 and 11 clear bit 1 every step, so they never give up;
   - the heading turns by at most table +0x10 per step (`0x800136E0`).
4. Thrust: velocity += heading × table +0x0C, capped at table +0x0E. Then position += velocity
   and the collision sweep runs (`0x80088BF8` → `0x80074FA4`).
5. Burn-out (age ≥ table +0x00): the nose eases 16 per step toward pitch 0x400 and velocity +0x24
   gains 8 per step, so it drops. Life ends it.
6. When nothing is hit: an exhaust puff, and **every 8 steps the target gets `0x8008F3C8(target, 1, …)`**
   (likely the "missile incoming" signal the interceptor reacts to `?`).
7. On a hit: hit effect `0x8008C164`, the hit dispatcher `0x8008821C`, a blast `0x8008BB78` on
   some targets, then removal (`0x8008B60C`).

Arena engine: the same handler is most likely at `0x8008FFB4` (pattern match only).

## Movement and energy (VS engine, code + measured 2026-11-17)

Checked in this repo's debug build in plain VS, default AC on both sides, P2 idle. Armored-CoreR's
numbers are **not** taken as given (the user rates that research as rough); each line below was
measured here or read in FDAT 209. Speeds are in 1/64 world units per step, angles 4,096 = 360°.

**How the AC moves.** Every position write during walking, turning, boosting and rising comes from
one place: the animation player's per-axis stepper (`0x80055034` → `0x80054FF8`, AC +0x6C, steppers
at +0x6C +0x20 / +0x28 / +0x30). Each movement handler turns velocity (+0xD4 … +0xD8) into a step
vector (velocity ÷ 64) and loads the stepper through `0x80054124` + `0x800540B8`.

**The 75-per-axis speed limit is the AC's size.** `0x80054124(step, out, half-width, half-height, n)`
cuts any axis whose step is larger than the limit down to the limit. The limit is taken from AC
+0x28 (width, 100) and +0x2A (height, 212):
- three quarters of them (**75** sideways, **159** up/down) in `0x8007B60C`, `0x8007B8B0`, `0x800859F4`;
- half (50 / 106) in `0x8007B108`, `0x8007B454`, `0x8007BC6C`, `0x8007E714`.
It looks like protection against moving through walls. **Tested**: ground boost tops out at 75 a step
with AC +0x28 = 100. With +0x28 = 150 it reaches the true top speed, 83 a step (5,320 ÷ 64).
AC +0x28 / +0x2A come from a 44-byte object-type record (pointer AC +0x04, set by `0x8007CE84`), not from the parts.

**Handlers and values** (player block, set at match start; see `stat-block.md`):

| What | Handler | Value used | Measured (default AC) |
|---|---|---|---|
| Turning | `0x8007E1DC` (write `0x8007E268`), also in the air and boost handlers | +0x20C accel, +0x20E top | +6 per step up to **38**, stops at once on release ✔ |
| Ground boost | `0x8007F7BC` | +0x208 accel, +0x20A top | velocity +531 per step; 83 a step at the top, capped to 75 by the size limit ✔ |
| Rise / air | `0x8007FBAC` | +0x200 accel, +0x202 top, +0x204 / +0x206 air boost | take-off −3,200, then −64 per step to **−5,000** ✔ |
| Gravity | `0x8007FBAC` (`0x8007FC88`) | +256 per step, fall limit 9,600 | +256 per step after release ✔ |
| Walking | animation root motion | none | 11 ×3, 25 ×4, then 31 ×4 / 37–38 ×4 repeating, the same facing 0°, 45° and 90° ✔ |

- Boost button = **player block +0x298** (the pad mask the energy and air code test). The data chat had this as "?".
- **Second turn set +0x210 / +0x212** (from the selected slot +7 / +8) is used only by the handler
  `0x8007E3E0`. `0x8007B454` installs it from the hit callback `0x80082430` and the animation callback
  `0x8008389C`, so it is likely the turn rate while staggered or recovering `?`. That handler also slows
  velocity by 1/8 per step and zeroes it below 512.
  - Live values: rifle selected 1 / 1, missile selected 3 / 28.

**Energy** (`0x80082D90`, player block +0x16; capacity +0x262, rise cost +0x264, refill standing
+0x266, red zone +0x268, refill moving +0x26A):

| Rule | Code | Measured |
|---|---|---|
| Rise / air boost (class 0xC0) costs +0x264 per step | ✔ | −436 ✔ |
| Ground boost (class 0xB0) costs half | ✔ | −218 ✔ |
| Other classes while holding boost: no cost | ✔ | |
| Refill standing +0x266; moving +0x26A when class 0x10 / 0xB0 (two-legged), or 0xB0 with speed (caterpillar, four-legged) | ✔ | +210 standing, +127 walking / coasting ✔ |
| Below the red zone, cost and refill ÷ 4 | ✔ | −54, +31, +52 ✔ |
| Cost > energy left: energy = 0, **booster lock** (+0x18 bit 0) | ✔ | ✔ |
| While locked: refill ÷ 2, **even below the red zone** (no ÷ 4), no boosting; lock clears only at full | ✔ | +63 moving, +105 standing ✔ |
| Refill adds before it checks the cap, so the bar overshoots by up to one refill for a step, then snaps back to the capacity | ✔ | +210 then −57 ✔ |

**Test AC and where each energy number comes from.** The AC is the default build, loadout `0x80036FB0` = 0,0,5,1,0,1,0,40,0,0,–,3. The stats below are read from the menu module's part tables (FDAT 206). For the part list itself, see `parts-tables.md`.

| Part | Name | Stats used |
|---|---|---|
| Generator 0 | GPS-VVA | output +0x2A = 6,000, capacity +0x2C = 28,000, red zone +0x2E = 7,800 |
| Booster 0 | B-P320 | drain +0x2E = 4,360 |
| Core 0 | XCA-00 | drain +0x2A = 1,046 |
| Head 5 | HD-GRY-NX | drain +0x2A = 218 |
| Arms 1 | AN-201 | drain +0x2A = 877 |
| FCS 0 | COMDEX-C7 | drain +0x2A = 24 |
| Legs 3 | LN-1001-PX-0 | moving drain +0x2A = 1,844, standing drain +0x34 = 541 |

| Number | From the parts | Compiled | Measured |
|---|---|---|---|
| Capacity | generator +0x2C | 28,000 | 28,000 |
| Red zone | generator +0x2E | 7,800 | quarter rates start below 7,800 |
| Rise cost / step | 4,360 ÷ 10 | 436 | −436 (ground boost −218) |
| Refill standing | (6,000 − (1,046 + 218 + 877 + 24 + 541 = 2,706)) × 64 ÷ 1,000 = 210.8 | 210 | +210 |
| Refill moving | (6,000 − (1,046 + 218 + 877 + 24 + 1,844 = 4,009)) × 64 ÷ 1,000 = 127.4 | 127 | +127 |

So the drain sums use core, head, arms, FCS and legs only. The weapon, blade and booster drain fields don't enter. All 5 numbers match the parts exactly, with fractions rounded down. Other builds are not measured yet.

Also in `0x80082D90`: △ cycles the weapon (+0x296 mask → player block +0x11), which re-runs `0x80082834`
and drops the locks (`0x80080084`).

Not done: landing recovery after long falls; air-boost release rate; other leg types and heavy builds.

## Match start and end (VS engine, code + tested 2026-11-17)

**Engine entry `0x80051F4C`** (FDAT 209). In order:

1. **Backs up both player blocks** (928 bytes each) to `0x80093750` (P1) and `0x80093AF0` (P2).
   **On exit it copies them back.** So anything written into a player block during the fight
   (results, stat-block changes) is lost when the engine returns, unless it was copied out first.
   This answers the open "copy or live?" question: live during the fight, restored afterwards.
2. `0x800558C0` = VS load: stage and models, then for P1 and P2 in turn:
   - set the current player block `0x8003EAA0`;
   - `0x80055214`;
   - `0x8007D23C` (AC object setup);
   - **`0x8008342C` (per-AC match start, see `stat-block.md`)**.
3. `0x800881D0` (clear the object pool), `0x80087658` × 2 (cameras), stage init (match struct +0x04 → its +4),
   `0x8007D7B4` × 2 (records at `0x801D6ED0` / +0x10), `0x800527EC`.
4. **Main loop** from `0x80052124`, one game step per pass:
   - clear player block +0x08 bits 0–2 on both;
   - update objects, ACs, HUD and the stage (match struct +0x04 → +0);
   - run the end checks below;
   - draw both views;
   - steps elapsed +0x12 += 1.

**Match struct `0x80097EA0`:**

| Off | Meaning |
|---|---|
| +0x04 | Stage code table (+0 per step, +4 init); the stage code sits around `0x80098000` |
| +0x0C | Flags: 0x40 = time up, 0x80 = draw, 0x30 = result shown |
| +0x10 | **End countdown** (0 = fighting). Set to 30 at time up, then +90 when a result is decided. It waits at 30 for the CONTINUE? answer |
| +0x12 | Steps elapsed (capped 0x4000). The clock only runs once this is ≥ 16 |
| +0x14 | Time left in steps (the `0x80097EB4` timer) |

**How a round ends** (checked every step while +0x10 = 0):

| Way | Check | Tested |
|---|---|---|
| Time up | time left reaches 1 → +0x10 = 30, flag 0x40. Then **the higher AC +0x168 (AP × 4) wins**; equal AP is a draw | ✔ (P2 AP set lower → "TIME UP", P1 counted the win) |
| Knock-out | AC **+0x49** (255 while fighting) drops when AP hits 0 (on-hit `0x80079774`, then the wreck sequence in the AC update `0x800828BC` → `0x80079828`), ending at **0xFE** = wrecked (0 also counts as out). After 12 steps: one out → the other wins; both out → draw | ✔ (P2 AP → 1, one rifle shot: P2 +0x49 = 0xFE, "WIN" / "LOSE") |
| Stage flag | player block +0x08 bits 0–2 = exactly 1 → that player loses (both → draw). The stage code at `0x80098074` writes 1, 3, 7 there every step `?` | code only |

**What a result writes**, the winner P1 shown (P2 mirrors it):
- player block +0x08: winner |= 0x10, loser |= 0x20 (both |= 0x30 at the CONTINUE? screen);
- the winner's player block **+0x394 |= 1 << round** (round history, lost on exit by step 1);
- **win/loss counters** (u16, capped 999), shown on the VS menu as the 1P / 2P WIN / LOSE table:
  - `0x8003EAB0` = 1P wins
  - `0x8003EAB2` = 1P losses
  - `0x8003EAB4` = 2P wins
  - `0x8003EAB6` = 2P losses
- **round number `0x8003EA94`** += 1 per decided round. It survives rematches, and VS START resets it to 0.
- A draw changes no counters.

**CONTINUE? screen** (+0x10 = 30; either player answers; raw pad bits as in `stat-block.md`):

| Button | Result | Return value | Tested |
|---|---|---|---|
| ✕ YES (0x4000) | rematch: the exe reloads the VS engine with the same setup | 2, replay flag `0x8003EA92` = 0 | ✔ new 2:00 round straight away |
| START (0x0008) | replay | 2, `0x8003EA92` = 1 → FDAT 211 (see `mode-launch.md`) | code only |
| ○ NO (0x2000) | countdown runs out, back to the VS menu | 0 | ✔ VS menu showed 1P WIN 1 / 2P LOSE 1 |

Exit (`0x800526C0`): stops sound and vibration, restores both player blocks, returns the value above.

## Match timer (VS engine, measured)

- Time left = **game steps** at `0x80097EB4` (u16, timer struct `0x80097EA0` +0x14).
  About **22 steps per second** (2:00 ≈ 2,640). Writing it moves the HUD clock.
- Decremented once per step at `0x800523CC` (called from `0x80052180`), only while byte
  `0x801D4C09` (P1 object +0x49) is 255 and the value is ≥ 2.
- For tests, the harness writes 2,600 back into it to keep the clock from running out.

## Test harness (this chat)

Debug-tools Linux build from branch `mapping-debug` (draft release, not for players),
run headless with the debug server on port 4370. Pad input via `press` (port 1/2),
RAM via `read_ram`/`write_ram`, writers via `wtrace_add` + `wtrace_dump`. VS from the
title screen: Start, Start, Down, Cross, Cross. Cross confirms; Square fires; **Circle
swings the blade; Cross boosts** (user-confirmed; earlier notes had Circle as boost);
Triangle changes weapon; L1/R1 strafe; L2/R2 look; Left/Right turn.

## Rogue Arena fight recorder (arena engine)

`games/master-of-arena/moa_run_record.c` logs every Rogue Arena fight to
`<run file>.rec.csv` (beside the memory card): per fight a header with both AC object
pointers (player block +0x398), a layout check (max AP non-zero, AP ≤ max), raw dumps of
both player blocks and AC objects, then a row whenever any value changed, checked on every
call of the arena controller sampler `0x8007F69C` (t = call number; buttons; per AC: x, y,
z, facing, state +0x45, movement +0x46, AP, energy, locked, refire +0x48). The rows are at
most one per game step (values only change on steps), so t against row count also shows
how often the sampler runs per step and a summary line (air %, boosting %, locked %, blade
swings, shots, range, hits). Its first test log answers "AC object base in the arena
engine" (expected 0x801D8C08 for P1 from the AP code). Later use: shaping a ghost's AI
record from how the player fought.

## Open questions

- Blade double hit: solved (no per-swing hit guard; 2 hits from ≤ 650 units, 1 at 700).
  Open: whether other blades (longer reach, Moonlight) can reach 3+ hits.

- Base address of the AC object array in the arena engine, and AP's offset in it.
- Player block copy or live? Answered: live during the fight; the engine backs both up at entry and restores them at exit (see Match start and end).
- Mission engine AC object (0x801E73B0 is player AP there): same layout?
- Where the enemy list lives in missions (non-AC enemies).

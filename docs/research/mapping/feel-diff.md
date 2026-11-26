# Lane 2 — Phantasma vs Master of Arena: game feel

Lane 2 of the mapping restart (`PLAN.md`). Compares how an AC moves and fights on **Project Phantasma US
(`PP`, SLUS-00670)** and **Master of Arena US (`MoA-US`, SLUS-01030)**, says where in code or data each
difference lives, and lists what must survive when MoA content is imported into PP.

Static read, 2026-11-26. Nothing here was run. Status per finding: `tested` (seen in a running game),
`code` (read from code), `data` (read from disc data), `guess`.

Disc locations follow `docs/disc-patch-track.md`:
- VS engine code: FDAT.T entry **204** on PP (file offset = address − `0x800490D0`), entry **209** on MoA-US
  (offset = address − `0x80050DD8`). Written below as `PP 204+0x…` / `MoA 209+0x…`.
- Menu module data (part tables): FDAT.T entry **201** on PP (offset = address − `0x800490D0`), entry **206**
  on MoA-US (offset = address − `0x80050DD8`).
- Animation sets: `GG/P<n>/PA<nn>.T` entry 1 (same index on both discs; 4-byte size, then the set).

---

## 1. One-page summary for the owner

**What MoA changed in how a fight feels, in plain words.**

1. **Hit stun works differently.** In PP the freeze after a hit grows with the weapon's impact and
   shrinks with stability: from 1 frame up to 30. MoA rewrote it: at most 5 frames, but it reaches
   those 5 frames quickly, and a very sturdy AC **ignores** low-impact hits completely. On a normal AC,
   light and mid hits stun a little *longer* in MoA, the heaviest hits longer in PP.
2. **Energy refills much more slowly in Phantasma.** MoA raised the output of the five cheaper
   generators (the starting GPS-VVA from 4,728 to 6,000). With the starting AC, PP refills
   129 per step standing and **46 while moving**; MoA refills 210 and 127. So in PP, boosting has
   to be rationed far more.
3. **Locks are harder to hold in Phantasma.** MoA made the lock box of
   most rifles, bazookas and energy rifles (lock class 2) about 45% bigger on every FCS, and
   moved both bazookas into that class. Smaller boxes in PP mean side-steps break locks more easily.
   MoA also drops every lock when a blade swing ends; PP keeps the lock.
4. **Missile defence and jamming are stronger in Phantasma.** The core's missile interceptor has a
   cone twice as wide and kills twice as often (1 shot in 16 instead of 1 in 32). SP-JAM breaks a
   missile lock on its owner with a 10% chance per frame (MoA 5%).
5. **Same in both:** walking, turning, ground boost, jump, rise, gravity, landing, recovery turn,
   the speed cap, the state machine that reads the pad, missile flight, the step loop. MoA's arena
   AI is different code (Lane 6), and MoA buffed or rebalanced a handful of weapons (§5).
6. **The tricks (keep-working list, not investigated further).** Kaze, strafe dancing / lock
   cancelling, the heavy-biped high jump and the wall/floor clip already work in Phantasma. While
   comparing, it turned out what they lean on, which tells us what not to touch: Kaze and probably the
   wall clip come from PP's own **movement animation data** for four body combinations (files PA11,
   PA12, PA13, PA27). MoA replaced exactly those four, which most likely removed Kaze there. The high jump
   comes from animation data that is the same on both discs. Strafe dancing probably benefits from PP's
   smaller lock boxes (point 3). Details in §6.

**What this means for the import.** The Phantasma feel lives in (a) PP's fight code, (b) PP's
animation sets and (c) PP's part numbers. Import MoA's *new* parts, arenas and missions, but **do not
copy MoA's versions of PA11, PA12, PA13 and PA27, MoA's generator numbers, MoA's FCS lock boxes or
MoA's hit-stun routine**, or the feel moves toward MoA. Parts that exist on both discs should keep
PP's numbers (§5 lists every one MoA changed).

---

## 2. How the comparison was done

- Tools (in `tools/feeldiff/`, they read the extracted work packs in the sandbox and print addresses
  and short disassembly only; no game code goes in the repo):
  `mdis.py` (disassemble by address), `ld.py` (side-by-side diff that ignores register choice, field
  offsets, immediates and load width, so only real logic changes remain), `scan.py` (runs `ld.py` over
  every pair in `maps/moa-to-pp-functions.tsv`), `xref.py` (callers), `grepop.py`, `dataref.py` (maps a
  MoA data address to PP through the code that uses it), `anim.py` (root-motion tracks of the PA
  animation sets), `parts.py` (part records by name, field by field).
- The two discs were built with **different compiler settings**: the same C gives different
  instruction order and register use. So the pair table's "changed" count is far too high; after
  `ld.py`'s normalisation most "changed" movement routines turn out identical in logic.
- Struct shifts confirmed: AC object fields −8/−9 (MoA +0x45 state = PP +0x3C, +0xD4 velocity = +0xCC,
  +0x58 callback = +0x50, +0x6C animation player = +0x64, +0xDA stun timer = +0xD2, +0x168 AP = +0x160).
  Player block fields +8 (MoA +0x298 boost mask = PP +0x2A0). The AC object is 8 bytes shorter on PP
  (VS P1 → P2 step 0x170, MoA 0x178): MoA added AC +0x3A/+0x3C/+0x3E and the stealth byte +0x16E.

---

## 3. Movement routines, paired (VS engine)

"Same" = identical logic after normalisation (only field offsets, registers or addresses differ).

| Routine | MoA-US | PP | Result | Status |
|---|---|---|---|---|
| State setter (start animation, write state) | `0x8007FFF0` | `0x80077D94` | same | code |
| Pad → wanted state (stand/walk/strafe/boost/jump decider, AC +0x5C callback) | `0x80085180` | `0x8007CD10` | same (compiler only) | code |
| Ground-boost state handler (states 1–8, accel +0x208) | `0x80085704` | `0x8007D27C` | same | code |
| Ground boost | `0x8007F7BC` | `0x80077560` | same | code |
| Rise / air / gravity / air boost | `0x8007FBAC` | `0x80077950` | same | code |
| Turning | `0x8007E1DC` | `0x800760E8` | same | code |
| Turn helper | `0x8007DC80` | `0x80075B8C` | same | code |
| Recover turn (second turn set +0x210/+0x212) | `0x8007E3E0` | `0x800762EC` | same | code |
| Animation-finished callback (picks next state, landing) | `0x8008389C` | `0x8007B42C` | same | code |
| Animation player / per-axis stepper | `0x80053F50`, `0x80055034`, `0x80054FF8` | `0x8004C55C`, `0x8004D640`, `0x8004D604` | identical | code |
| Step-size cap (¾ / ½ of body size) | `0x80054124` | `0x8004C730` | identical | code |
| Stepper installers | `0x8007B8B0`, `0x8007B108`, `0x8007B454`, `0x800859F4`, `0x8007E714` | `0x80073B00`, `0x80073374`, `0x800736C0`, `0x8007D56C`, `0x80076620` | same | code |
| Stepper installer (walk, wall test) | `0x8007B60C` | `0x80073878` | **MoA adds** a call to AC +0x5C with reason 5 after a wall hit; the AC callback ignores any reason ≠ 0, so no effect for ACs | code |
| Wall / floor tests | `0x80078C4C`, `0x80078AB0` | `0x80070F40`, `0x80070DA4` | same | code |
| Stun countdown handler (AC +0xDA / PP +0xD2) | `0x8007FECC` | `0x80077C70` | same | code |
| Stun / knock-back setters | `0x8007BC1C`, `0x8007BC6C` | `0x80073E6C`, `0x80073EBC` | same | code |
| **Stun length from impact** | `0x80079AA4` (MoA only) | `0x80071D0C` (PP only) | **different** (§4.2) | code |
| **AC hit callback** | `0x80082430` | `0x80079FE8` | **different** (§4.2) | code |
| **Damage routine** | `0x800795DC` | `0x800718D0` | **MoA adds** jam (+0x3A) and burn (+0x3C/+0x3E) from hit flags 0x40 / 0x80 | code |
| Energy rules (inside the per-frame AC routine) | `0x800828BC` (energy at `0x80082D90`) | `0x8007A48C` | same rules; interceptor numbers differ (§4.5) | code |
| Stat scale-in at match start | `0x8008342C` | `0x8007AFD0` | same formulas; MoA also copies stealth (+0x16E); AC +0x43 = 5 (MoA) vs PP +0x3A = 7 (no reader found in the VS module) | code |
| **Blade swing / lunge** | `0x8007EB48` | `0x80076A54` | **different** (§4.4) | code |
| Lock-on, guns and cannons | `0x800800E4` | `0x80077E84` | MoA adds "no new lock while jammed" | code |
| Lock-on, missiles (SP-JAM break) | `0x80080638` | `0x800783D0` | same logic, different chance (§4.5) | code |
| Drop all locks | `0x80080084` | `0x80077E28` | same | code |
| Gun fire | `0x80080DF4` | `0x80078B8C` | MoA adds alternate-side fire and a burst-continue flag (AC +0x47) for its new weapons | code |
| Missile fire / cannon fire | `0x80081888`, `0x80081B18` | `0x80079444`, `0x800796D4` | same | code |
| Missile in flight | `0x8008907C` | `0x80080BAC` | same, plus MoA's new missile kind 11 | code |
| VS main loop (steps, VSync wait) | `0x80051F4C` | `0x8004A228` | same pacing (one frame wait per step); pad bit layout differs (§6) | code |
| Trig, step helpers, sqrt (boot EXE) | `0x80014AE8`, `0x800136E0`, `0x80014790`, `0x80028E74` | `0x80015124`, `0x80013D28`, `0x80014DC0`, `0x80029760` | identical | code |

---

## 4. Differences found

### 4.1 Difference table

| # | What | PP | MoA-US | Where (PP / MoA) | Status |
|---|---|---|---|---|---|
| 1 | Back-right walk start (anim 18) in body sets 11, 13, 27 | slides forward-right at ~75 units/frame (sets 11: (−747, 0, −600) in 8 frames; 13/27: (−999, 0, −700) in 10) | normal small step back-right ((−122, 0, 24) / (−149, 0, 150)) | `GG/P0/PA11.T`, `PA13.T`, `GG/P1/PA27.T` entry 1, anim 18 at +0x32A / +0x30A / +0x30A (both discs) | data |
| 2 | Walk / strafe tracks (anims 2, 4, 6, 9, 12) in body set 12 | 16-frame tracks (another legs' timing) | 12-frame tracks like every other light-legs set | `GG/P0/PA12.T` entry 1 from +0xB2 (both) | data |
| 3 | Body model + joint animation (entry 0) of sets 11, 12, 13, 27 | PP version | rebuilt (13 to 47 KB of bytes differ, 120–180 bytes shorter) | `PA11/12/13/27.T` entry 0 | data |
| 4 | Hit stun length | 8 × impact ÷ stability, clamp 1–30 frames; every hit with impact staggers | 41 × impact ÷ stability, clamp 0–5; **0 = no stagger** | PP `0x80071D0C` (204+0x28C3C), called `0x8007A2B8`; MoA `0x80079AA4` (209+0x28CCC), called `0x8008257C` | code |
| 5 | Jam on hit (AC +0x3A): sparks, all locks dropped each frame, no new lock | not present | set by hits with flag 0x40 (only WR-RS7 has it), length = hit +2, halved unless the victim's player block +0x284 bit 0 | MoA `0x800796A4`, `0x8007CE84` (MoA only), `0x800830CC`, `0x8008012C` | code |
| 6 | Burn on hit (AC +0x3C/+0x3E) | not present | flag 0x80 (only WG-FGI-00) | MoA `0x800796E4`, `0x8007CE84` | code |
| 7 | Locks after a blade swing | kept | **all dropped** when the swing ends | MoA `0x8007EFDC` (209+0x2E204); PP `0x80076E1C` has no such call | code |
| 8 | Blade wave type 4 | not present (types 0–3) | added (new MoA blade) | MoA `0x8007EB48`, `0x8007F264` | code |
| 9 | Missile interceptor cone | ±(core +0x35) × 8 | ±(core +0x35) × 4 | PP `0x8007A65C` (204+0x3158C) `sll 3`; MoA `0x80082A8C` `sll 2` | code |
| 10 | Missile interceptor kill chance per shot | rand < 0x800 (1 in 16) | rand < 0x400 (1 in 32) | PP `0x8007A830` (204+0x31760); MoA `0x80082C60` | code |
| 11 | SP-JAM: missile lock on a jammer breaks per frame | rand < 0xCCD (10%) | rand < 0x667 (5%) | PP `0x80078720` (204+0x2F650); MoA `0x80080988` | code |
| 12 | Generator output (refill) | GPS-VVA 4,728, GPS-V6 4,728, GRD-RX5 5,300, GRD-RX6 6,000, GRD-RX7 6,810 | 6,000, 6,000, 6,560, 6,800, 7,440 | generator record +0x2A; PP table `0x800B95C8` (201+0x704F8), stride 48 | data |
| 13 | FCS lock box for class 2 (rifles, bazookas, energy rifles) | e.g. COMDEX-C7 box 110 × 110, inner 85 × 85 | 160 × 160, inner 125 × 125 (all FCS about +45%) | FCS record +0x48 … +0x4E; PP table `0x800B8FC8` (201+0x6FEF8), stride 96 | data |
| 14 | Lock class of weapons | WG-B2120, WG-B2180 class 3; WG-RFM118, WG-PB26 class 0 | all four class 2 | hand detail +0x06; PP `0x800B9D74` (201+0x70CA4) | data |
| 15 | Pad word in the player block | 32-bit at +0x08, bytes swapped (✕ 0x40, ○ 0x20, START 0x800) | 16-bit at +0x04 (✕ 0x4000, ○ 0x2000, START 0x08) | e.g. pause `0x80074BE0` vs `0x8007CA00` | code |
| 16 | Wall notification from the walk stepper | none | +0x5C(ac, 5) on wall contact, ignored by ACs | MoA `0x8007B868` | code |
| 17 | Hit-shape list, kind 0xC00 | not present | added | MoA `0x8007BED4` vs PP `0x80074124` | code |

### 4.2 Hit stun in detail (code)

Both discs: the hit callback (MoA `0x80082430`, PP `0x80079FE8`) reads the hit's impact byte (hit
record +4, blade +0x32 / gun detail +0x16). If it is 0 there is no stagger. Otherwise:

- **PP**: `n = 50 × impact ÷ (AC +0x164)` where +0x164 = stability × 25 ÷ 4, so
  n = 8 × impact ÷ stability; clamped to 1…30. The AC always enters the stagger states (0x80 / 0x81 on
  the ground, 0x25–0x2C in the air) and n goes to the stun timer AC +0xD2 through `0x80073E6C`.
- **MoA**: `n = 256 × impact ÷ (AC +0x16C)` ≈ 41 × impact ÷ stability, clamped 0…5. **n = 0 skips the
  stagger** (no state change, no knock-back). n goes to AC +0xDA through `0x8007BC1C`.
- The stun handler (MoA `0x8007FECC`, PP `0x80077C70`) counts the timer down and keeps the busy bit
  (AC +0x36 bit 0x10) until it reaches 0. Same on both.

Examples with the default AC (AC stability field 237, per `battle-system.md`) and PP hand-gun impacts
(hand detail +0x16: 0 on most rifles and machine guns, 3, 4, 7, 9, 10, 12, 15, 28, 40 on the rest):

| Impact | PP frames | MoA frames |
|---|---|---|
| 0 | no stagger | no stagger |
| 3 | 1 (0, raised to the minimum) | 3 |
| 9 | 1 | 5 (cap) |
| 28 | 5 | 5 |
| 40 | 8 | 5 |

So on a normal AC, **MoA stuns longer for light and mid hits (always near its 5-frame cap) and PP
longer only for the heaviest hits (up to 30 frames on fragile ACs)**. MoA's "no stagger at all" needs a
stability field above 256 × impact, i.e. a very sturdy AC hit by a low-impact weapon. Which one feels
"stickier" depends on the weapon mix; needs a live check (§7, test 4).

### 4.3 Animation data (data)

The 72 body sets are picked by the body table (PP `0x800BC7B8`, 201+0x736E8; MoA `0x800D4A64`), 12
bytes each: core, arms list, legs list. Sets used here:

| Set | Core | Arms | Legs | PA file |
|---|---|---|---|---|
| 11 | XCA-00 (and XXA-S0 / XXL-D0 on MoA, looked up as core 0) | AN-K1 | medium two-legged: LN-1001, LN-1001-PX-0, LN-1001B, LN-502, LN-D-8000R | `GG/P0/PA11.T` |
| 12 | XCA-00 | AN-3001, ANKS-1A46J | light two-legged: LN-SSVT, LN-501, LN-2KZ-SP | `GG/P0/PA12.T` |
| 13 | XCA-00 | AN-K1 | heavy two-legged: LN-3001, LN-SSVR, LN-3001C, LNKS-1B46J (MoA adds LN-S3) | `GG/P0/PA13.T` |
| 27 | XCH-01 | AN-K1 | heavy two-legged | `GG/P1/PA27.T` |

Of the 72 sets, the root-motion entry (entry 1) differs only in these four. Animation 18 is the
back-right walk start (Down + R1; x negative = right, z negative = forward; anim 14 = back start,
16 = back-left start). Only entry 0 of set 54 differs besides (3 bytes).

Other set differences between the discs are MoA's new parts in the lists (arms AN-891-S and AW-R/4,
legs LN-S3, LB-H230, LF-TR-0, LC-HTP-H5). Leg 9 is a DUMMY slot on PP and LN-S3 on MoA.

### 4.4 Blade (code)

- MoA adds blade wave type 4 (player block +0x283 = 4 skips the normal wave and the melee sweep;
  MoA's new LS-1000W), in `0x8007EB48` and `0x8007F264`.
- MoA drops all locks (`0x80080084`) where the swing ends and the busy bit is cleared
  (`0x8007EFDC`). PP only clears the blade joint bit (`0x80076E1C` → `0x800764E4`).
- Wave arming: PP clears player block +0x48 at the end of every swing step; MoA clears its +0x40 only
  after a wave was fired. Both fire the wave only while boost is held and the flag is 1. Effect on
  feel not settled `guess`.
- Lunge (turn toward the lock target at turn speed +0x20E / PP +0x216) and hit window: same.

### 4.5 Missiles and jamming (code)

See table rows 9–11. The flight table (MoA `0x80093114`, PP `0x8008A888`, 40 bytes per kind) is the
same on both discs apart from byte +0x17 (MoA 0x30 in every row; meaning unknown) and MoA's extra
rows for its new missiles. Launcher table (MoA `0x800912B0`, PP `0x80088A9C`) and the gun shot-speed
tables (MoA `0x80092980` / `0x80092C20` / `0x80092824`, PP `0x8008A0F4` / `0x8008A394` / `0x8008A004`) match
apart from MoA's added rows.

---

## 5. Part numbers MoA changed (parts on both discs; PP → MoA)

From `tools/feeldiff/parts.py` (records matched by name + type text). Field names from `parts-tables.md`.

| Part | Field | PP → MoA |
|---|---|---|
| GPS-VVA, GPS-V6 | generator output +0x2A | 4,728 → 6,000 |
| GRD-RX5 / RX6 / RX7 | generator output | 5,300 → 6,560 / 6,000 → 6,800 / 6,810 → 7,440 |
| COMDEX-C7, COMDEX-G0 | FCS class 2 box / inner (+0x48–0x4E) | 110 / 85 → 160 / 125 |
| COMDEX-G8 | same | 80 / 64 → 135 / 115 |
| QX-21, QX-AF | same | 160 / 125, 150 / 120 → 210 / 180 (QX-AF class 3 value 100 → 96) |
| TRYX-BOXER, TRYX-QUAD | small changes to class 0 range / class 3 values; QUAD class 2 boxes +50% | |
| QX-9009, FBMB-18X, RATOR | reworked across all classes (ranges, boxes, lock values; RATOR range 12,000 → 22,000) | |
| Legs, all two-legged and reverse-joint | carry limit +0x32 | +50 (two-legged), +100 (reverse-joint) |
| LN-3001, LN-SSVR, LN-3001C, LNKS-1B46J, LN-2KZ-SP, all caterpillars | shell / energy defence (+0x2E / +0x30) | lowered by 10–90 |
| HD-G780 | defences | 186 / 448 → 146 / 292 |
| AN-3001, ANKS-1A46J | shell defence | 487 → 477, 679 → 659 |
| WG-RF/5, WG-RF/P | attack | 530 → 795, 612 → 918 |
| WG-B2120, WG-B2180 | attack, lock class | 1,250 → 1,730, 1,930 → 2,330; class 3 → 2 |
| WG-RFM118, WG-PB26 | ammo, lock class | 200 → 160, 15 → 24; class 0 → 2 |
| WA-Finger (weapon arms) | ammo, interval, attack | 3,000 → 500, 1 → 2, 250 → 155 |
| WM-AT | ammo, range, weight | 10 → 4, 19,999 → 5,800, 910 → 1,507 |
| WG-1-KARASAWA, WG-XFwPPk, WC-IR24 | weight | 1,000 → 1,120, 893 → 998, 528 → 1,006 |

The compiled-stat formulas checked here (refill = (output − drains) × 64 ÷ 1,000, clamp 1–384; rise cost)
are the same on both discs (PP menu `0x8009A9A8`, MoA `0x800A82A0`), so the generator numbers alone
explain the energy difference. With the default AC (drains 2,706 standing / 4,009 moving):
PP refill 129 standing / 46 moving, MoA 210 / 127 `code` (MoA side `tested` in `battle-system.md`).

---

## 6. Keep-working list: the tricks and what they depend on

The owner confirmed all four work on PP (2026-11-26); per `PLAN.md` they are not investigated further. This
is what was seen in passing, so later changes don't break them.

| Trick | Depends on (found while comparing) | PP | MoA | Status |
|---|---|---|---|---|
| **Kaze** (Down + R1 + boost on one frame from standing, rightwards only; AN-K1, medium/heavy two-legged, starting/S0 core or heavy core with heavy legs) | Animation 18 (back-right start) of sets 11, 13, 27 carries a forward-right slide of ~75 units per frame, the ground-boost top speed. Pressing boost in the same frame most likely carries that speed into the ground boost. The code path is the same on both discs | works | removed (data fixed) | data; boost part `guess` |
| **Strafe dancing / lock cancelling** | Same movement and lock code on both. PP's class-2 lock boxes are smaller (row 13) and PP keeps locks through blade swings; △ weapon change drops locks on both (`0x80080084`). | stronger | weaker | code + data; effect `guess` |
| **Heavy two-legged high jump** (with weapon arms) | Heavy two-legged + weapon-arms sets (31–39, 63–65 groups, e.g. PA32) use a 9-frame take-off with 499 lift, not the heavy legs' 10-frame, 249-lift take-off (PA08 / PA13). Same bytes on both discs | works | works | data |
| **Wall / floor clip** (light two-legged, starting/S0 core, AN-3001-type arms, timed on the left hip) | Body set 12 is exactly that combination, and it is the set whose walk/strafe tracks MoA replaced (row 2) and whose joint animation MoA rebuilt (row 3). MoA's added wall notification (row 16) does nothing for ACs, so the fix is in the data, not the code. How the 16-frame track lets the AC through a wall is not worked out | likely works | likely fixed | data; cause `guess` |

---

## 7. Optional confirmation tests (Project Phantasma on an emulator, then MoA for comparison)

Plain VS, 2 players, unlimited time, any flat stage. Player 2 just stands.

1. **Hit stun.** Player 2 with WG-RF/5 (impact 9) shoots player 1 (default AC) while player 1 holds
   forward. Watch how long player 1 stops on each hit. Expect PP: about 1 frame, barely visible;
   MoA: about 5 frames. Then player 2 with WG-HG1 (impact 40): PP about 8 frames, MoA 5, so now PP stops longer.
2. **Energy.** Default AC: hold ✕ until the bar is low, then walk (no boost) and time how long the bar
   takes to refill. Expect PP to be roughly 2.5 times slower than MoA while walking.

These only confirm what the code read predicts (rows 4 and 12); they are optional. The tricks need no
test (owner: they work on PP).

---

## 8. What must survive when MoA content is imported into PP

- PP's VS / arena fight code as it is (all of §3).
- PP's animation sets PA00–PA71 (especially **PA11, PA12, PA13, PA27**). MoA's new parts need their own
  body sets added *beside* PP's, never MoA's versions of these four.
- PP's numbers for parts that exist on both discs (§5), unless the owner chooses otherwise per part.
- MoA's hit extensions (jam +0x3A, burn +0x3C/+0x3E, blade wave 4, gun burst flag, hit-shape 0xC00,
  missile kind 11) are needed **only** for MoA's new weapons (WR-RS7, WG-FGI-00, LS-1000W, the dual /
  burst weapons). Importing those weapons means porting those small pieces of code into PP, and
  growing PP's AC object (8 bytes MoA added) or finding room for them elsewhere.
- Options the owner may want as settings later: MoA's hit stun, MoA's lock-drop after a blade swing.

## 9. Open

- Nothing here is `tested` yet on PP; §7 lists two optional checks.
- AC +0x43 (MoA 5) / PP +0x3A (7): no reader found in the VS modules; may be read in the arena module
  or the boot EXE.
- Flight table byte +0x17 (MoA 0x30): meaning.
- The arena engine (PP 205 / MoA 210) was not compared; its movement code should be checked the same
  way (Lane 6 owns the AI part of it).

## Handed over

(Lines other lanes add for Lane 2 go here.)

---

## Handed over to other lanes (copied into their docs' lists when those exist)

- **Lane 1**: PP data addresses found through code (`tools/feeldiff/dataref.py`): player block pointer
  `0x8003BD00` (MoA `0x8003EAA0`), player block P1 `0x800424E8` (MoA `0x80045198`, P2 +0x3A8 vs +0x3A0),
  VS AC objects P1 `0x801D80E0` (MoA `0x801D4BC0`, P2 +0x170 vs +0x178), match struct `0x8008F5B0`
  (MoA `0x80097EA0`), player block backup `0x8008AE40` (MoA `0x80093750`), flight table `0x8008A888`,
  launcher table `0x80088A9C`, cannon table `0x80088AF4`, body-combination list `0x80088B24` (MoA
  `0x80091340`), body table (menu) `0x800BC7B8`, part tables (menu) head `0x800B64FC`, core `0x800B6934`,
  legs `0x800B6B9C`, arms `0x800B76DC`, hand `0x800B7BBC`, back `0x800B7F58`, blade `0x800B8B54`, booster
  `0x800B8D28`, FCS `0x800B8FC8`, generator `0x800B95C8`, options `0x800B9898`, weapon details hand
  `0x800B9D74`, back `0x800B9F6C`, weapon arms `0x800B9BF4`. The pair table's "changed" verdicts are
  mostly compiler noise; `ld.py`'s normalisation is a better test.
- **Lane 4**: PP pad word is 32-bit at player block +0x08 with bytes swapped against MoA; PP stun
  timer is AC +0xD2; PP has no AC +0x3A/+0x3C/+0x3E/+0x16E (MoA-only fields); PP vibration port numbers
  0 / 0x10 (MoA 0 / 1).
- **Lane 5**: §5 part changes; lock class changes; flight table byte +0x17; hand/back/arms detail table
  addresses above; leg 9 is DUMMY on PP.

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

1. **Hits barely stun in Phantasma.** In PP the freeze after a hit is impact × 50 ÷ stability frames,
   at least 1. But PP's stability numbers are about 6× bigger than MoA's (Lane 5), so on nearly every
   build and weapon this comes out at the **1-frame minimum**. MoA rewrote the rule: a normal AC is
   frozen **3 to 5 frames** by most hits, while a very sturdy AC can ignore low-impact hits completely.
   So Phantasma lets you keep moving under fire; MoA makes hits "stick".
2. **Energy refills much more slowly in Phantasma.** MoA raised the output of the five cheaper
   generators (the starting GPS-VVA from 4,728 to 6,000). With the starting AC, PP refills
   129 per step standing and **46 while moving**; MoA refills 210 and 127. So in PP, boosting has
   to be rationed far more.
3. **Locks are harder to hold in Phantasma.** MoA made the lock box of
   most rifles and energy rifles (lock class 2) about 45% bigger on every FCS, and
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
| 4 | Hit stun length | impact × 50 ÷ AC +0x164, clamp 1–30 frames; with PP's large stability almost always 1 | impact × 256 ÷ AC +0x16C, clamp 0–5; **0 = no stagger**; usually 3–5 | PP `0x80071D0C` (204+0x28C3C), called `0x8007A2B8`; MoA `0x80079AA4` (209+0x28CCC), called `0x8008257C` | code |
| 5 | Jam on hit (AC +0x3A): sparks, all locks dropped each frame, no new lock | not present | set by hits with flag 0x40 (only WR-RS7 has it), length = hit +2, halved unless the victim's player block +0x284 bit 0 | MoA `0x800796A4`, `0x8007CE84` (MoA only), `0x800830CC`, `0x8008012C` | code |
| 6 | Burn on hit (AC +0x3C/+0x3E) | not present | flag 0x80 (only WG-FGI-00) | MoA `0x800796E4`, `0x8007CE84` | code |
| 7 | Locks after a blade swing | kept | **all dropped** when the swing ends | MoA `0x8007EFDC` (209+0x2E204); PP `0x80076E1C` has no such call | code |
| 8 | Blade wave type 4 | not present (types 0–3) | added (new MoA blade) | MoA `0x8007EB48`, `0x8007F264` | code |
| 9 | Missile interceptor cone | ±(core +0x35) × 8 | ±(core +0x35) × 4 | PP `0x8007A65C` (204+0x3158C) `sll 3`; MoA `0x80082A8C` `sll 2` | code |
| 10 | Missile interceptor kill chance per shot | rand < 0x800 (1 in 16) | rand < 0x400 (1 in 32) | PP `0x8007A830` (204+0x31760); MoA `0x80082C60` | code |
| 11 | SP-JAM: missile lock on a jammer breaks per frame | rand < 0xCCD (10%) | rand < 0x667 (5%) | PP `0x80078720` (204+0x2F650); MoA `0x80080988` | code |
| 12 | Generator output (refill) | GPS-VVA 4,728, GPS-V6 4,728, GRD-RX5 5,300, GRD-RX6 6,000, GRD-RX7 6,810 | 6,000, 6,000, 6,560, 6,800, 7,440 | generator record +0x2A; PP table `0x800B95C8` (201+0x704F8), stride 48 | data |
| 13 | FCS lock box for class 2 (rifles, energy rifles, some back cannons; missiles are class 1, bazookas class 3 on PP) | e.g. COMDEX-C7 box 110 × 110, inner 85 × 85 | 160 × 160, inner 125 × 125 (all FCS about +45%) | FCS record +0x48 … +0x4E; PP table `0x800B8FC8` (201+0x6FEF8), stride 96 | data |
| 14 | Lock class of weapons | WG-B2120, WG-B2180 class 3; WG-RFM118, WG-PB26 class 0 | all four class 2 | hand detail +0x06; PP `0x800B9D5C` (201+0x70C8C) | data |
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

**Stability is not the same number on both discs.** Lane 5 (`content-data.md`, section 2) found PP's
compiled stability (stat +0x24) is about 6× MoA's, and most PP builds reach its top value, 255. So PP's
AC +0x164 is usually 255 × 25 ÷ 4 = 1,593, while MoA's default AC has 237. Examples with PP hand-gun
impacts (hand detail +0x16: 0 on many guns, 3–15 on most others, 28 on WG-B2120, 40 on WG-HG1):

| Impact | PP frames (field 1,593) | MoA frames (default AC, field 237) |
|---|---|---|
| 0 | no stagger | no stagger |
| 3 | 1 (0, raised to the minimum) | 3 |
| 9 | 1 | 5 (cap) |
| 28 | 1 | 5 |
| 40 | 1 | 5 |

So **in PP almost every hit is a 1-frame flinch; in MoA most hits freeze a normal AC for 3–5 frames.**
PP only goes above 1 frame on builds with low stability (below 255 on stat +0x24) hit by high-impact
weapons. MoA's "no stagger at all" needs a stability field above 256 × impact. A live check would confirm
(§7, test 1).

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
   forward. Watch how long player 1 stops on each hit. Expect PP: a 1-frame flinch, barely visible;
   MoA: about 5 frames, a clear stop. Same with WG-HG1 (impact 40): PP still about 1 frame.
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
- Imported MoA parts must have their stability compiled the PP way (PP's formula, Lane 5), or hit stun on
  builds using them changes.

## 8b. Planned feature: Kaze as an option (owner, 2026-11-26)

The owner wants Kaze kept and turned into a feature, working **left as well as right**.

- **Setting:** "Kaze: Original / Both sides / Off" (a launcher toggle first; an option part such as
  "SP-DASH" may follow later, using the same switch). Possibly also "Both sides on every two-legged
  build". Default: Original.
- **Original:** PP as shipped: anim 18 (Down + R1) of body sets 11, 13, 27 keeps its slide.
- **Both sides:** also give anim 16 (back-left start, Down + L1) of the same sets the mirrored slide
  (x sign flipped: set 11 (+747, 0, −600) over 8 frames, sets 13/27 (+999, 0, −700) over 10 frames, same key
  frames as anim 18). The pad decider treats Down + L1 (state 7) and Down + R1 (state 8) alike, so the
  rest should follow `guess`.
- **Off:** load the normal tracks MoA uses (set 11 (−122, 0, 24); sets 13/27 (−149, 0, 150)).
- **How:** the recomp picks which bytes of PA11/13/27 entry 1 to load; no code change. For the disc patch
  track, the extra keys grow entry 1 by a few bytes (each entry 1 is one 2,048-byte sector; free room not
  yet checked). An option part needs a new part entry, a free stat bit and a small hook (Lanes 5 and 8 first).
- **Before building:** confirm the cause on PP: XCA-00 + AN-K1 + LN-1001, standing, Down + R1 without boost
  should shoot forward-right; with AN-201 arms only a small back step. Then test Down + L1 once "Both sides"
  exists.
- The body animation stays the normal "step back" while sliding, as in today's Kaze.

## 9. Open

- Nothing here is `tested` yet on PP; §7 lists two optional checks.
- ~~AC +0x43 (MoA 5) / PP +0x3A (7)~~: found in round 2 topic 5, it is a surface mask for the stage hit
  query (§10.3, `battle-system.md`).
- Flight table byte +0x17 (MoA 0x30): meaning.
- ~~The arena engine (PP 205 / MoA 210) was not compared~~: done in round 2 topic 5 (§10).

## 10. Arena engine, PP 205 / MoA 210 (round 2, topic 5, 2026-11-26)

**Answer: the arena engine moves exactly like the VS engine on each disc.** Every movement routine of §3 has
a copy in the arena module, with the same logic and the same numbers, and the PP ↔ MoA differences inside
the arena module are the same ones §4 lists for VS. So everything in §3–§8 holds for arena fights too.

### 10.1 How we know (code)
For each §3 routine, the arena copy was found by code shape on each disc separately (normalised compare,
`ld.py` rules, over every routine of FDAT 205 / FDAT 210), then compared three ways: VS ↔ arena on PP,
VS ↔ arena on MoA (raw, only addresses masked, so numbers count), and MoA arena ↔ PP arena (normalised).
Script: our own throwaway helper over the extracted packs; it prints addresses only.

| Routine (§3) | PP VS → PP arena | MoA VS → MoA arena | MoA arena ↔ PP arena |
|---|---|---|---|
| State setter | `0x80077D94` → `0x800799C4` | `0x8007FFF0` → `0x80081CC0` | same |
| Pad → wanted state | `0x8007CD10` → `0x8007E920` | `0x80085180` → `0x80086E30` | same |
| Ground-boost state handler | `0x8007D27C` → `0x8007EE8C` | `0x80085704` → `0x800873B4` | same |
| Ground boost | `0x80077560` → `0x80079190` | `0x8007F7BC` → `0x8008148C` | same |
| Rise / air / gravity / air boost | `0x80077950` → `0x80079580` | `0x8007FBAC` → `0x8008187C` | same |
| Turning / turn helper / recover turn | `0x800760E8` / `0x80075B8C` / `0x800762EC` → `0x80077D18` / `0x800777BC` / `0x80077F1C` | `0x8007E1DC` / `0x8007DC80` / `0x8007E3E0` → `0x8007FEAC` / `0x8007F950` / `0x800800B0` | same |
| Animation-finished callback | `0x8007B42C` → `0x8007D03C` | `0x8008389C` → `0x8008554C` | same |
| Stepper installers | `0x80073B00`, `0x80073374`, `0x800736C0`, `0x8007D56C`, `0x80076620` → `0x80075708`, `0x80074F7C`, `0x800752C8`, `0x8007F17C`, `0x80078250` | `0x8007B8B0`, `0x8007B108`, `0x8007B454`, `0x800859F4`, `0x8007E714` → `0x8007D578`, `0x8007CDD0`, `0x8007D11C`, `0x800876A4`, `0x800803E4` | same |
| Stepper installer (walk, wall test) | `0x80073878` → `0x80075480` | `0x8007B60C` → `0x8007D2D4` | MoA's reason-5 callback, as in VS |
| Wall / floor tests | `0x80070F40` / `0x80070DA4` → `0x80072B10` / `0x80072974` | `0x80078C4C` / `0x80078AB0` → `0x8007A8DC` / `0x8007A740` | same |
| Stun countdown / stun / knock-back setters | `0x80077C70`, `0x80073E6C`, `0x80073EBC` → `0x800798A0`, `0x80075A64`, `0x80075AB4` | `0x8007FECC`, `0x8007BC1C`, `0x8007BC6C` → `0x80081B9C`, `0x8007D8D4`, `0x8007D924` | same |
| Stun length from impact | `0x80071D0C` → `0x80073914` | `0x80079AA4` → `0x8007B76C` | different, as §4 #4 |
| AC hit callback | `0x80079FE8` → `0x8007BC08` | `0x80082430` → `0x800840F0` | different, as VS |
| Damage routine | `0x800718D0` → `0x800734A0` | `0x800795DC` → `0x8007B26C` | MoA's jam / burn, as VS |
| Per-frame AC routine (energy) | `0x8007A48C` → `0x8007C09C` | `0x800828BC` → `0x8008456C` | interceptor cone ×8 vs ×4, as §4 #9 |
| Stat scale-in | `0x8007AFD0` → `0x8007CBE0` | `0x8008342C` → `0x800850DC` | +0x3A = 7 vs +0x43 = 5, as VS |
| Blade swing | `0x80076A54` → `0x80078684` | `0x8007EB48` → `0x80080818` | different, as §4.4 |
| Lock-on guns / missiles | `0x80077E84` / `0x800783D0` → `0x80079AB4` / `0x8007A000` | `0x800800E4` / `0x80080638` → `0x80081DB4` / `0x80082308` | as VS |
| Gun fire | `0x80078B8C` → `0x8007A7BC` | `0x80080DF4` → `0x80082AC4` | as VS (MoA alternate-side fire) |
| Main loop | `0x8004A228` → `0x8004A654` | `0x80051F4C` → `0x80052378` | one VSync per step in all four (the arena loop passes a register that holds 1) |

PP VS ↔ PP arena, raw: the per-frame AC routine, ground boost, air, scale-in and the ground-boost state handler
differ only in addresses (no number changed). The two places where the arena copy really differs from its own
VS copy are the same on both discs:
- **Damage routine**: the arena adds the AI learning calls after a hit (PP `0x80083988` when the player is hit,
  `0x80083810` when the opponent is; MoA `0x800898A4` / `0x80089744`), already in `ai.md`.
- **Hit callback / walk-stepper installer**: VS picks the vibration port by player (PP 0 / 0x10, MoA 0 / 1);
  the arena always uses port 0 (one human player).

### 10.2 What it means
- Arena fights on PP feel the same as VS fights on PP; PP-vs-MoA feel differences in the arena are exactly the
  VS ones (stun, jam / burn, blade, interceptor, back-right walk start in the PA data).
- A movement change (for example Kaze "Both sides", §8) has to be made in **five copies** (missions, link, VS,
  arena, replay; VS was compared in round 1, arena now, missions / link / replay in §11). All five are the
  same code, so one hook written for VS fits every copy (addresses in §10.1 and §11.2).

### 10.3 AC +0x3A (PP 7, MoA +0x43 = 5), from topic 5
Read in `battle-system.md` (AC object): it is the **surface mask** the AC passes to the stage hit query
(`0x8006E9D0` → `0x8006E958`, VS): hit results whose surface flags share a bit with the mask are skipped.
PP's 7 skips surfaces flagged 1, 2 or 4; MoA's 5 does not skip surfaces flagged only 2. So on a stage with
"flag 2" surfaces, a MoA AC stops where a PP AC passes. Which stages have such surfaces is for `stages.md`.
Status code (mask use), effect in game `guess`.

### 10.4 Handed over to the build chat (routine starts found in topic 5, PP, all `code`)
FDAT 205 (arena): `0x800799C4`, `0x8007E920`, `0x8007EE8C`, `0x80079190`, `0x80079580`, `0x80077D18`, `0x800777BC`,
`0x80077F1C`, `0x8007D03C`, `0x80075480`, `0x80075708`, `0x80074F7C`, `0x800752C8`, `0x8007F17C`, `0x80078250`,
`0x80072B10`, `0x80072974`, `0x800798A0`, `0x80075A64`, `0x80075AB4`, `0x80073914`, `0x8007BC08`, `0x800734A0`,
`0x8007C09C`, `0x8007CBE0`, `0x80078684`, `0x80079AB4`, `0x8007A000`, `0x8007A7BC` (the §10.1 table).
FDAT 204 (VS): `0x8004B8D0` (play sound), `0x8004C06C` (music volume), `0x8004AE38` (sound block at start),
`0x8006E9D0`, `0x8006E958` (stage hit query with mask), `0x80083738` (effect spawner), `0x80051B44` (model
set-up). FDAT 201: `0x80098EF0` (menu entry), `0x8009A238` (stat-block builder), `0x800AA428` (settings
defaults), `0x8004F2D8` (menu file loader). Exe: `0x80017244` (register a file name), `0x800179B0` (vibration),
`0x80020578` (master volume), `0x80017D2C` (sound reset, inside its routine).

## 11. Missions, link and replay engines (round 2 review, 2026-11-26)

**Answer: all five fight engines move the same way on each disc.** Every §3 movement routine has a copy in the
missions (PP 202 / MoA 207), link (203 / 208) and replay (206 / 211) modules, with the same logic and the same
numbers as the VS copy. The Phantasma-vs-MoA differences inside each copy are the VS ones (§4), in every engine.
So §3–§8 hold for missions, link and replay too, and a movement change (Kaze) is one change written five times
at the addresses below.

### 11.1 How we know (code)
Same method as §10.1, per disc: each VS routine's twin found by code shape over every routine of the module,
then VS ↔ twin compared raw (addresses masked) and the twins compared across the discs. A second pass compared
the small numbers each routine uses (constants under 0x4000, not stack offsets), VS copy against each other copy.

- **Link and replay** are VS with other addresses: shape match 1.00 for most routines (0.95–0.99 for the hit
  callback, 0.81 for the link damage routine, which only drops VS's second-player lookups), and their numbers
  match VS's except data-address halves.
- **Missions** looks more different (shape 0.78–0.97 in a few routines), but the numbers agree once read right:
  the missions copy fetches player 1's block once (only one player in a mission) instead of going through the
  "current player" pointer (`0x8003BD00`) each time, so the same stat fields show up rebased (e.g. 9504 − 9448 =
  field +56, as in VS). The pad routine stores the same 5 at block +0x12; the extra −6 is reordered compiler
  output. One real extra: the VS copy of the stat scale-in also does the model set-up (`0x80051B44`, AC +0x60),
  which the missions engine does elsewhere. Not movement.
- **Two-player bits** (only VS has two ACs): the VS hit callback / damage / walk-stepper pick the second AC
  (+0x170, MoA +0x178) and its vibration port (PP 0x10, MoA 1). Link and replay use one fixed slot, like the arena.
- **PP vs MoA in each engine** shows the same differences everywhere: stun length (§4 #4), hit callback, damage
  (MoA jam / burn), per-frame interceptor cone, blade (§4.4), gun fire (MoA alternate-side fire), walk-stepper
  reason-5 callback, of the same size in all four non-arena engines.

### 11.2 The five copies (all `code`, static)
| Routine | PP VS 204 | PP missions 202 | PP link 203 | PP replay 206 | MoA VS 209 | MoA missions 207 | MoA link 208 | MoA replay 211 |
|---|---|---|---|---|---|---|---|---|
| State setter | `0x80077D94` | `0x800795E4` | `0x80078AA0` | `0x80077A5C` | `0x8007FFF0` | `0x80081944` | `0x80080D70` | `0x8007FC9C` |
| Pad → wanted state | `0x8007CD10` | `0x8007E2D0` | `0x8007DA08` | `0x8007C9B8` | `0x80085180` | `0x80086840` | `0x80085EEC` | `0x80084E0C` |
| Ground-boost state handler | `0x8007D27C` | `0x8007E7E4` | `0x8007DF74` | `0x8007CF24` | `0x80085704` | `0x80086D3C` | `0x80086470` | `0x80085390` |
| Ground boost | `0x80077560` | `0x80078DF0` | `0x8007826C` | `0x80077228` | `0x8007F7BC` | `0x80081150` | `0x8008053C` | `0x8007F468` |
| Rise / air / gravity / air boost | `0x80077950` | `0x800791BC` | `0x8007865C` | `0x80077618` | `0x8007FBAC` | `0x8008151C` | `0x8008092C` | `0x8007F858` |
| Turning | `0x800760E8` | `0x80077A1C` | `0x80076DF4` | `0x80075DB0` | `0x8007E1DC` | `0x8007FC98` | `0x8007EF5C` | `0x8007DE88` |
| Turn helper | `0x80075B8C` | `0x800774C0` | `0x80076898` | `0x80075854` | `0x8007DC80` | `0x8007F73C` | `0x8007EA00` | `0x8007D92C` |
| Recover turn | `0x800762EC` | `0x80077C20` | `0x80076FF8` | `0x80075FB4` | `0x8007E3E0` | `0x8007FE9C` | `0x8007F160` | `0x8007E08C` |
| Animation-finished callback | `0x8007B42C` | `0x8007CA28` | `0x8007C124` | `0x8007B0D4` | `0x8008389C` | `0x80084F98` | `0x80084608` | `0x80083528` |
| Stepper installer (walk, wall test) | `0x80073878` | `0x80075004` | `0x80074090` | `0x8007381C` | `0x8007B60C` | `0x8007CEBC` | `0x8007BEAC` | `0x8007B5E0` |
| Stepper installer a | `0x80073B00` | `0x8007528C` | `0x80074318` | `0x80073AA4` | `0x8007B8B0` | `0x8007D160` | `0x8007C150` | `0x8007B884` |
| Stepper installer b | `0x80073374` | `0x80074B00` | `0x80073B8C` | `0x80073318` | `0x8007B108` | `0x8007C9B8` | `0x8007B9A8` | `0x8007B0DC` |
| Stepper installer c | `0x800736C0` | `0x80074E4C` | `0x80073ED8` | `0x80073664` | `0x8007B454` | `0x8007CD04` | `0x8007BCF4` | `0x8007B428` |
| Stepper installer d | `0x8007D56C` | `0x8007EAC4` | `0x8007E264` | `0x8007D214` | `0x800859F4` | `0x8008701C` | `0x80086760` | `0x80085680` |
| Stepper installer e | `0x80076620` | `0x80077F3C` | `0x8007732C` | `0x800762E8` | `0x8007E714` | `0x800801B8` | `0x8007F494` | `0x8007E3C0` |
| Wall test | `0x80070F40` | `0x80072724` | `0x8007175C` | `0x80070EE4` | `0x80078C4C` | `0x8007A580` | `0x800794E0` | `0x80078C20` |
| Floor test | `0x80070DA4` | `0x80072588` | `0x800715C0` | `0x80070D48` | `0x80078AB0` | `0x8007A3E4` | `0x80079344` | `0x80078A84` |
| Stun countdown | `0x80077C70` | `0x800794C4` | `0x8007897C` | `0x80077938` | `0x8007FECC` | `0x80081824` | `0x80080C4C` | `0x8007FB78` |
| Stun setter | `0x80073E6C` | `0x800755E8` | `0x80074680` | `0x80073E00` | `0x8007BC1C` | `0x8007D4BC` | `0x8007C4B8` | `0x8007BBE0` |
| Knock-back setter | `0x80073EBC` | `0x80075638` | `0x800746D0` | `0x80073E50` | `0x8007BC6C` | `0x8007D50C` | `0x8007C508` | `0x8007BC30` |
| Stun length from impact | `0x80071D0C` | `0x80073498` | `0x80072524` | `0x80071CB0` | `0x80079AA4` | `0x8007B354` | `0x8007A344` | `0x80079A78` |
| AC hit callback | `0x80079FE8` | `0x8007B714` | `0x8007ACEC` | `0x80079CA0` | `0x80082430` | `0x80083C68` | `0x800831A8` | `0x800820CC` |
| Damage routine | `0x800718D0` | `0x800730A4` | `0x800720EC` | `0x80071874` | `0x800795DC` | `0x8007AF00` | `0x80079E70` | `0x800795B0` |
| Per-frame AC routine (energy) | `0x8007A48C` | `0x8007BB58` | `0x8007B184` | `0x8007A134` | `0x800828BC` | `0x80084094` | `0x80083628` | `0x80082548` |
| Stat scale-in | `0x8007AFD0` | `0x8007C628` | `0x8007BCC8` | `0x8007AC78` | `0x8008342C` | `0x80084B8C` | `0x80084198` | `0x800830B8` |
| Blade swing | `0x80076A54` | `0x80078350` | `0x80077760` | `0x8007671C` | `0x8007EB48` | `0x800805CC` | `0x8007F8C8` | `0x8007E7F4` |
| Lock-on guns | `0x80077E84` | `0x800796CC` | `0x80078B90` | `0x80077B4C` | `0x800800E4` | `0x80081A30` | `0x80080E64` | `0x8007FD90` |
| Lock-on missiles | `0x800783D0` | `0x80079BE8` | `0x800790DC` | `0x80078098` | `0x80080638` | `0x80081F54` | `0x800813B8` | `0x800802E4` |
| Gun fire | `0x80078B8C` | `0x8007A340` | `0x80079898` | `0x80078854` | `0x80080DF4` | `0x800826AC` | `0x80081B74` | `0x80080AA0` |

The arena copies are in §10.1. "Stepper installer a–e" = the five installers of §10.1, in order.

### 11.3 What it means
- Kaze "Both sides" (§8) or any other movement change: the same edit at 29 places in each of five engines per
  disc, but only one version of it. A hook written for VS fits the other four copies; the missions copy starts
  from player 1's block directly.
- Nothing found that makes missions, link or replay feel different from VS on the same disc.

### 11.4 Handed over to the build chat (PP routine starts, all `code`)
The PP missions, link and replay columns of §11.2 (87 starts in FDAT 202, 203 and 206).

## Handed over

(Lines other lanes add for Lane 2 go here.)

---

## Handed over to other lanes

Delivered 2026-11-26 as one line each in the "Handed over" lists of `foundations.md` (Lane 1),
`battle-system.md` (Lane 4) and `content-data.md` (Lane 5) on their branches. Lane 4 and Lane 5 had
already found the pad format, stun formula and part changes independently. Kept here for reference:

- **Lane 1**: PP data addresses found through code (`tools/feeldiff/dataref.py`): player block pointer
  `0x8003BD00` (MoA `0x8003EAA0`), player block P1 `0x800424E8` (MoA `0x80045198`, P2 +0x3A8 vs +0x3A0),
  VS AC objects P1 `0x801D80E0` (MoA `0x801D4BC0`, P2 +0x170 vs +0x178), match struct `0x8008F5B0`
  (MoA `0x80097EA0`), player block backup `0x8008AE40` (MoA `0x80093750`), flight table `0x8008A888`,
  launcher table `0x80088A9C`, cannon table `0x80088AF4`, body-combination list `0x80088B24` (MoA
  `0x80091340`), body table (menu) `0x800BC7B8`, part tables (menu) head `0x800B64FC`, core `0x800B6934`,
  legs `0x800B6B9C`, arms `0x800B76DC`, hand `0x800B7BBC`, back `0x800B7F58`, blade `0x800B8B54`, booster
  `0x800B8D28`, FCS `0x800B8FC8`, generator `0x800B95C8`, options `0x800B9898`, weapon details hand
  `0x800B9D5C`, back `0x800B9F54`, weapon arms `0x800B9BDC` (as in Lane 5). The pair table's "changed" verdicts are
  mostly compiler noise; `ld.py`'s normalisation is a better test.
- **Lane 4**: PP pad word is 32-bit at player block +0x08 with bytes swapped against MoA; PP stun
  timer is AC +0xD2; PP has no AC +0x3A/+0x3C/+0x3E/+0x16E (MoA-only fields); PP vibration port numbers
  0 / 0x10 (MoA 0 / 1).
- **Lane 5**: §5 part changes; lock class changes; flight table byte +0x17; hand/back/arms detail table
  addresses above; leg 9 is DUMMY on PP.

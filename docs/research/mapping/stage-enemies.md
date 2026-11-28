# Stage and mission enemies — how they move, aim, fire and hurt (Project Phantasma base)

Lane "stage enemies", one overnight chat, 2026-11-28. Static read only (no running game). Base disc **PP** (Project
Phantasma USA, SLUS-00670); **MoA-US** (SLUS-01030) and **AC1** (SCUS-94182) notes where they were cheap.
Every address carries its disc. Tags: `code` = read from code, `data` = read from disc data, `guess`, `tested` (none
yet: the live checks are tests 38–42 in `ROUND1.md`).

Decisions taken without the owner (unattended run):
- "Stage enemies" = the enemies of the **missions engine** (PP FDAT 202 with a mission module FDAT 5n). Arena / VS
  stages have no enemies other than ACs (the arena AI is `ai.md`). The stage-code files FDAT 100–199 were not read.
- Enemy **kinds** are named here by their code ("families" A–J below), not by model, because each mission carries its
  own copy of the code and nothing in the data says "helicopter". Names like "MT" or "turret" are `guess` from the
  behaviour (moves or not, turns its body or only a gun, range) and from the briefings.
- The survey tool is new and kept in the repo (`tools/mapping/enemies/enemy_survey.py`); it prints addresses and
  numbers only, like the other mapping tools.

## 0. Plain summary (for the owner)

- **Enemy shots hurt the player half as much in missions as the same attack would in VS.** The missions engine works
  out damage with the AC formula (attack × shell or energy factor ÷ 64 × 2 ÷ 3) but takes only **2 quarter-points per
  damage point** off the player's AP, where VS takes 4. AP is kept in quarters, so a hit shows up as **half** the
  VS number. Same on MoA and on AC1. `code` — **This alone makes Armored-CoreR's mission enemies hurt twice as much
  as they should:** it uses the VS rule for them.
- There is **no difficulty setting and no hidden scaling** found: the attack value in the mission map (per placed
  enemy) goes straight into the shot. `code` for the path read; "nothing else" is `guess` (no other reader found).
- Enemies **don't lead their shots** (they aim at where you are, the middle of your body), and most have **no random
  spread** either: they fire along their gun barrel, and the barrel turns toward you at a fixed speed (16 or 32 units
  a tick, where 4,096 is a full turn). So moving sideways fast makes them miss, because the barrel lags. Enemy ACs are
  the exception: their shots get a random spread. `code`
- An enemy only fires when you're inside a **box of angles and a range** set per kind (7,000 to 30,000 units), it
  usually needs a clear **line of sight**, and the light enemies roll dice each tick before a burst (e.g. the common
  MT: 8% a tick, then 3 shots in 6 ticks). Others simply fire every N ticks while aimed. `code`
- Enemies only **exist near you**: an enemy is created when the map area it belongs to becomes active (you are in it
  or next to it), so far-away enemies don't shoot. Stealth parts don't matter: every enemy's sight test ignores the
  stealth flag. `code`
- **Enemy ACs in missions use neither the arena AI nor shared engine code.** Each mission module carries its own
  ~25-routine AC brain driven by a small **script** (bytecode in the module's data), with its own weapon table.
  Same on MoA (e.g. MoA mission 6's "tank AC"). `code`; the script itself is not decoded yet.

## 1. Where things are (code, PP)

Engine = PP **FDAT 202** loaded at `0x800490D0`. Mission module = PP **FDAT 5n** loaded at `0x80097A38` (n = engine
mission number, `scenario.md` §4a; PP FDAT 85 / 90 are empty, so missions 17–18 don't exist). MoA-US twins from
`pp-disc/maps/moa-to-pp-functions.tsv` (all "same shape" or identical unless said).

| What | PP | MoA-US | Notes |
|---|---|---|---|
| Hit routine (any object) | `0x800730A4` | `0x8007AF00` | player object → AC formula, others → unit formula; AP field +0x160 (MoA +0x168) |
| AC damage formula (player hit) | `0x80072FD8` | `0x8007AE34` | = VS `0x800717F4` with the player block 1 factors +0x27A / +0x27B (`0x80042762/3`) |
| Unit damage formula (enemy hit) | `0x80073048` | `0x8007AEA4` | identical |
| Target pick | `0x800714C0` | `0x8007931C` | nearest object of another team, spreads picks (see Armored-CoreR `gen1-stages.md`) |
| **Sight test** | `0x800719CC` | `0x80079828` | range + angle box, see §3 |
| Line of sight | `0x80075F60` | `0x8007DEA4` | |
| Shot spawn | `0x800758A0` | `0x8007D774` (changed, 0.91) | fires from a gun joint along the joint's direction |
| Set state | `0x800736EC` | `0x8007B598` | |
| Explosion / remove | `0x8007321C` / `0x80076B7C` | `0x8007B0D8` / `0x8007EE04` | |
| rand() 0…32,767 | exe `0x8002CC30` (BIOS A0 0x2F) | | rand(n) = rand × n >> 15 at exe `0x80015774` |
| Turn toward (max step) | exe `0x80013D28` | exe `0x800136E0` | angles in 4,096ths of a turn |

Object fields used by the enemy code (PP; MoA is +9 for +0x3A…+0x41 and +8 from +0x50, `missions-import-pp.md` §1b):

| PP field | Meaning | MoA-US |
|---|---|---|
| +0x00 | pointer to its placement record (44 bytes in RAM = 4-byte header + the 40 on disc) | same |
| +0x12 | facing (yaw) | same |
| +0x36 & 7 | team (enemies 2, player 1 `guess`) | same |
| +0x3E | burst counter | +0x47 |
| +0x3F | wait between bursts / shots | +0x48 |
| +0x40 | 255 = alive; anything else = dying | +0x49 |
| +0x48 | current target object | +0x50 |
| +0x58 | per-frame routine (gun) | +0x60 |
| +0x5C | hit / message routine | +0x64 |
| +0x76 / +0x78 | gun pitch / yaw (yaw stored negated) | +0x7E / +0x80 |
| +0x108 | muzzle number (flips with ^2) / AC brain data pointer | +0x110 |
| +0x148, +0x14A…+0x14C | muzzle light time 15, colour 180 / 160 / 140 | +0x150 … |
| +0x160 | AP (units: raw points; player: quarters) | +0x168 |

## 2. Enemy records (data)

Placements = map file (FDAT 5n + 4) **block 12**, 40 bytes each on disc, 256 read on PP (`missions-import-pp.md`
§1a). The fields an enemy uses (disc offsets; RAM record = +4):

| Disc offset | Meaning | Read by | Status |
|---|---|---|---|
| +0x06 s16 | object type (1…15 → module handler at module +0x20 + 4 × type) | spawn | code |
| +0x10 u16 bits 8–11 | **energy class n** (signed): energy hits on this enemy do attack × (8 − n) ÷ 8 + 1 | `0x80073048` | code |
| +0x16 u16 | **AP** (raw points, not quarters) | object setup `0x80076880` | code |
| +0x18 u16 | **attack** of its shots (hit record +2) | the gun routines (`lhu 0x1C(record)`) | code |
| +0x1C u8 | **impact** of its shots (hit record +4; staggers the player) | the gun routines | code |

There is no per-enemy defence: enemies take the raw attack from solid hits, and the energy rule above. No speed or
behaviour field was found in the placement; speed, ranges, turn rates and fire rules are **constants in each
mission's code** (§4). Hit flags of enemy shots are 0x17 (solid) for almost every gun; the common MT picks 0x10 or
0x20 (solid / energy) from its object +0xFC == 2 (`code`, `0x80099748`).

Per mission, the placed enemies (PP, `enemy_survey.py --plc`; type → count × AP / attack / impact / n; type 0 and
AP-0 entries left out) `data`:

| # | Mission | Enemies (type ×count AP/attack/impact[/n]) |
|---|---|---|
| 0 | Infiltrate Amber Base | t1 ×2 6000/4000/60; t2 ×10 1500/150/0; t3 ×3 2500/500/3; t4 ×5 1000/100/0 |
| 1 | Search and Destroy | t1 ×1 5000/100/0 (AC); t2 ×3 3000/800–1000/8–16 |
| 2 | Rescue the Allies | t1 ×5 5000/300/5 (AC); t2 ×1 6000/500/8; t3, t4 ×2 each 4500/0 |
| 3 | Raid the Laboratory | t1 ×23 500/100/0; t3 ×1, t4 ×2 10000/500/16; t5 ×1 20000/2000/4 (AC) |
| 4 | Disrupt Shipping Lanes | t1 ×3 5000/300/5 (AC); t2 ×7 3500/0; t3 ×5 4000/1000/8; t4 ×4 1000/100/0 |
| 5 | Capture the VIP | t1 ×3 5000/500/0; t3 ×3 4000/2500/16; t4 ×2 1000/100/0; t7 ×2 10000/500/16 |
| 6 | Protect the VIP | t1 ×4 1000/100/0; t2 ×1 30000/200/10 (AC); t4 15000/500/4 and 5000/200/4 (AC); t5 ×6 3500/200/0 |
| 7 | Destroy Main Facility | t1 ×27 200/100/3; t2 ×51 500/50/0; t3 ×4 5000/650/20 (AC); t5 ×2 2000/1000/8; t6 ×3 30000/200/10 |
| 8 | Confrontation | t1 ×3 3000/100/0 (AC); t2 ×3 20000/100/0/n1; t3 15000/800/0/n5; t5 ×3 2000/1000/16 |
| 9 | Search the Facility | t1 ×2 3500/1000/8; t2 ×6 3000/400/0; t4 23000/3000/4 (AC); t6 ×3 5000/100/0 (AC) |
| 10 | The Final Battle | t1 60000/2000/0/n5; t2 30000/200/10 |
| 11 | The Underground Maze | t1 ×32 500/150/0; t2 ×11 3500/200/0; t3 ×7 1500/150/0 |
| 12 | Disrupt Military Units | t1 ×3 5500/300/5 (AC); t2 ×2 20000/200/0/n1; t3 ×11 1000–1200/100–200; t4 30000/200/10 (AC) |
| 13 | Destroy Receiving Base | t1 ×2 3500/100/0/n2 (AC); t2 ×9 3000/800/3; t4 ×3 1500/100/0 |
| 14 | Destroy the Detachment | t1 ×7 6000–7000/300–800/5 (AC); t2 ×3 4500/500/0/n2; t3 30000/200/10 (AC) |
| 15 | Intercept Enemy Units | t1 ×4 5000/800/5/n2 (AC); t2 ×8 1000/200/0; t3 30000/200/10 (AC); t4 ×4 5000/300/0 |
| 16 | Destroy Secret Base | t1 ×13 300/300/0; t2 ×2 2000/1000/8; t3 ×4 1500/500/0 |
| 19 | (test mode) | t1 ×2 5200/110/0/n2 (AC) |

"(AC)" = the type runs the enemy-AC brain (family J, §4). For those the placement attack is probably **not** the
damage: the AC brain fires from its own weapon table (hit record at weapon +0x22 flags / +0x24 attack / +0x26 impact,
`0x8009B9C8` in PP mission 19) `code`; whether placement +0x18 is read for ACs at all: `guess` (not seen).

## 3. Noticing and aiming (code, PP `0x800719CC`)

Called by every enemy gun as `sight(target, from, base_yaw, base_pitch, stealth_mask, team, range, pitch_min,
pitch_max, yaw_min, yaw_max, out)`. It says "can see" only if all hold:
1. target exists and is on **another team**;
2. the target's map section is in the **active set** (the player's section and its neighbours; so the player always
   passes, but enemies far away never exist in the first place: placements spawn only when their section is active
   or the player walks into their trigger box, `missions.md` "Spawn rule");
3. if the target carries a bit of `stealth_mask` in +0x38: 50% chance to fail each call. **Every enemy passes mask 0**
   (all call sites read), so stealth does nothing against mission enemies;
4. target point = its position with y raised by half its height (+0x6C ÷ 2): the **middle of the body, no lead**;
5. |dx|, |dy|, |dz| ≤ range, then dx² + dz² ≤ range² (flat distance; the vertical only box-checked).

Out: target, distance (sqrt), yaw and pitch to it; flag bit 1 (value 2) set when pitch ∈ [pitch_min, pitch_max]
(absolute) and yaw − base_yaw ∈ [yaw_min, yaw_max]. The guns act only when bit 2 is set.

**Reaction delay:** none as such. Once in the box, the gun turns at its turn step per tick and counts as "on aim" when
the last tick's turn was under the step (i.e. it has caught up). The time to notice = time to swing the barrel round
(e.g. 1,024 units at 16 a tick = 64 ticks) plus the dice (§4). With nothing in sight the gun swings back to centre
at the same speed.

**Fire direction:** the shot leaves gun joint A along the line joint A → joint B of the model (`0x80072360`, shot
table entry +0 / +2), so it goes where the barrel points, not straight at the target. `code`. Enemy ACs add a random
spread: yaw + rand(w) − w/2 and pitch + rand(h) − h/2 from their aim record +4 / +6 (`0x8009B9A4`, PP mission 19)
`code`; the w / h values per AC not tabulated.

## 4. Enemy families (code, PP; from `enemy_survey.py`)

Each family = the same code and numbers found in several modules. Angles in 4,096ths of a turn; ranges in world units
(an AC is about 212 tall); ticks = engine steps.

| Fam. | Guess at kind | Range | Pitch box | Yaw box | Turn / tick | Fire rule | Where (mission:type) |
|---|---|---|---|---|---|---|---|
| A | **light MT** (walks, gun on body; Armored-CoreR's "MT") | 7,000 | ±128 | ±1,024 | gun 16 | on aim, no burst: **2,622 / 32,768 a tick** (8%) start a burst; line of sight needed except 328 / 32,768 of the time; burst counter 6, a shot on ticks 5, 3, 1 = **3 shots**, muzzles alternate. Wait +0x3F = 10 after a burst is never reached (dead code), so the next roll is the tick after the last shot | 0:3, 7:2, 13:2 |
| B | **turret / gun emplacement** that turns its whole body | 10,000 | −708 … +1,024 | ±2,048 | body 32, gun 32 (back to centre 16) | on aim (within 32): **1 shot, then wait 30 ticks**; no dice, no line-of-sight test; the shot leaves with gun pitch forced to 0 (flat) `guess` on why | 0:2, 7:1, 9:2, 11:3, 16:3 |
| C | **fixed gun** (no turning) | 8,000 | ±800 | ±1,100 | — | needs line of sight; **6 shots, one a tick**, then wait **15 + rand(0…15)** ticks | 3:1, 11:1, 16:1 |
| D | **mobile gun** (turns 32) | 12,000 | −128 … +192 | ±1,024 | 32 | 1 shot, then wait **30 + rand(0…7)**; second shot path with wait 40 `?` | 1:2, 4:3, 5:3 |
| E | **long-range cannon** (narrow cone, turns body) | 20,000 | −256 … +576 | ±64 | 16 | needs line of sight; 1 shot then wait **5 ticks** while on aim; a 17 / 32,768 a tick roll elsewhere (movement `?`) | 0:4, 6:1, 12:3, 15:2 |
| F | **two-gun vehicle** (two sight calls) | 10,000 / 20,000 | −128 … +512 and −384 … +576 | ±512 / ±832 | 32 / 16 | three shot sites; 17 / 32,768 roll `?` | 4:4, 5:4, 7:5, 9:1, 13:4, 16:2, 8:3, 10:1 |
| G | **multi-mode** (many shot sites) | 7,000 | `?` | `?` | 32 / 64 / 4 | 7 shot sites, not read | 6:5, 11:2, 14:2 |
| H | **fast turner** | 8,000 | `?` | `?` | 64 | 1,639 / 32,768 roll, three shot sites; burst with wait 6 (PP mission 8 `0x80099B5C`) | 8:2, 12:2 |
| I | **close + far modes** | 1,000 / 2,000 / 7,000 / 10,000 | ±1,024 / ±1,408 | ±512 | 32 | rolls 656, 6,554, 328 / 32,768 | 5:1, 15:4 |
| J | **enemy AC** (own brain, §5) | weapon +0x42 & 0xFFF | weapon +0x14 / +0x16 | weapon +0x18 / +0x1A | `?` | scripted; rolls 9,831 / 32,768 (30%); random spread | 1:1, 2:1, 3:5, 4:1, 6:2, 6:4, 7:3, 8:1, 9:4, 9:6, 12:1, 12:4, 13:1, 14:1, 14:3, 15:1, 15:3, 19:1 |

Read in full: A (`0x80099650` / `0x800994B0`, PP mission 0), B (`0x80098A04` / `0x80098874`), C (`0x80098430`, mission
11), D (`0x80099208`, mission 5), E (`0x8009A914`, mission 0) `code`. F–I: numbers from the survey only (call
arguments and compare constants), behaviour not read `code` (numbers) / `guess` (meaning). Types not listed have no
sight test (doors, generators, trucks, scenery) `code`.

**Movement** (A, from the MoA read in Armored-CoreR `gen1-stages.md`, PP code same shape): a decision on events
0/3/4: farther than 1,250 → step in, nearer than 500 → step back, else a weighted random step; a hit swaps the step
for its opposite. Uses the unit's own walk animations for speed. Other families' movement not read.

**Every-tick muzzle light:** after each shot +0x148 = 15 ticks, colour 180 / 160 / 140 (same in A–E) `code`.

## 5. Damage to the player (code, all three discs)

Hit routine PP `0x800730A4` (MoA `0x8007AF00`, AC1 `0x80075350`), when the target is the player object (PP
`0x801E2DE0`, MoA `0x801E7248`, AC1 `0x801A26B8`):
1. damage = `0x80072FD8(hit)`: shell (flags & 0x30 = 0x10) attack × player block +0x27A >> 6, energy (0x20) ×
   +0x27B >> 6, other kinds attack unchanged; then × 2 ÷ 3. **The same formula as VS.**
2. **AP (+0x160, in quarters) −= damage × 2.** VS (`0x800718D0`, PP 204) subtracts damage × **4**. The player's
   mission AP is the build's AP × 4 (player block +0x278 → object +0x160 at `0x8007C7FC`; the HUD shows (AP + 3) ÷ 4,
   `0x8004EC40`). So **a mission hit costs half its VS cost.** Same on MoA (`sll 1` at `0x8007AF40`) and AC1 (`sll 1`
   at `0x80075390`).
3. Then the object's +0x5C (MoA +0x64) hit routine (stagger by impact, as in VS).

Hits on enemies: AP (raw) −= attack for solid, attack × (8 − n) ÷ 8 + 1 for energy (`0x80073048`). Player weapons
are not scaled by the player's factors here.

Example: family A on PP mission 0 (attack 500, impact 3) against the default AC (shell factor 27): 500 × 27 >> 6 =
210, × 2 ÷ 3 = 140, shown AP loss **70 a shot, 210 a burst**. The VS rule would give 140 / 420.

No difficulty or mission-number scaling of enemy attack was found in the engine path (shot spawn passes the
placement's attack straight into the hit record) `code` for the path, `guess` for "nowhere else".

## 6. Mission enemy ACs (code)

- The type handler (PP mission 19, `0x800981B8`) sets team 2 and **+0x108 = a pointer into the module's own data**
  (`0x80099A80`), then runs the brain's set-up `0x8009DA34`. That data is a **bytecode script** (16-bit words, 9999
  ends a block) read by an interpreter in the module (`0x8009C3D4` and others). `code`; opcodes not decoded.
- The brain is **~25–30 routines inside each mission module** (the survey counts them per type), not the arena AI
  (`ai.md`: none of its routines are in FDAT 202) and not engine code. The same brain shape appears in PP missions 1,
  2, 3, 4, 6, 7, 8, 9, 12, 13, 14, 15, 19 and in MoA mission 6 ("tank AC", MoA type 3 `0x800A3F24`) `code`.
- Its sight test takes range and angle limits from its weapon record (+0x42 & 0xFFF, +0x14 … +0x1A), it fires
  through the engine's shot spawn with its own hit record (+0x22 / +0x24 / +0x26), and adds a random spread. `code`
- Named ACs (Stalker, Nine-Ball…) are MoA missions (`missions.md`); on PP the boss of The Final Battle (#10 t1, AP
  60,000, n 5) uses a different, smaller brain (family F). Which placement is which named Raven on PP: not read.
- So an AC in a mission does **not** use the 60-byte arena AI record. To match the originals Armored-CoreR needs the
  script per mission (open item).

## 7. MoA and AC1 differences (cheap checks)

- MoA's mission 6 MT (the one Armored-CoreR ported) has pitch box **±256**; PP's family A uses **±128**. Range 7,000,
  yaw ±1,024, turn 16 and the 2,622 / 328 dice are the same. MoA's shot table entry (module `0x800AD13C`) has speed
  700, range 7,000. `code`
- Damage ×2 in missions: same on all three discs (§5). `code`
- MoA mission 6 type 1 (big MT) is close to family A with two sights (10,000 ±512 and 7,000 ±2,047) `code`.

## 8. For Armored-CoreR

| Value | Use | Source |
|---|---|---|
| Player AP loss from any mission hit | **(attack × factor >> 6) × 2 ÷ 3, then ÷ 2** (half the VS loss) | §5 `code` |
| Enemy AP loss | solid: attack; energy: attack × (8 − n) ÷ 8 + 1 | §5 `code` (already right) |
| Attack / impact / AP / n | per placement (map block 12 +0x18 / +0x1C / +0x16 / +0x10 bits 8–11) | §2 `data` |
| Aim point | target centre (y − height ÷ 2), no lead | §3 `code` |
| Shot direction | along the gun barrel (no spread) for non-AC enemies; random spread for ACs | §3 `code` |
| Light MT (A) | range 7,000; pitch ±128 (PP) / ±256 (MoA); yaw ±1,024; gun 16 a tick; 8% a tick on aim; 3 shots in 6 ticks; LOS 99% | §4 `code` |
| Turret (B) | 10,000; −708 … +1,024; ±2,048; body and gun 32 a tick; 1 shot / 31 ticks on aim; no LOS | §4 `code` |
| Fixed gun (C) | 8,000; ±800; ±1,100; 6 shots one a tick, then 15–30 ticks; LOS | §4 `code` |
| Mobile gun (D) | 12,000; −128 … +192; ±1,024; 32 a tick; 1 shot / 31–38 ticks | §4 `code` |
| Cannon (E) | 20,000; −256 … +576; ±64; 16 a tick; 1 shot / 6 ticks; LOS | §4 `code` |
| Stealth | ignored by every enemy | §3 `code` |
| Who is active | only enemies whose map section is active (player's section + linked) or whose trigger box the player entered | §3, `missions.md` |

What in Armored-CoreR today makes stage enemies hurt more (from its `src/gen1/acr_gen1_battle.c` and
`docs/research/gen1-stages.md`): (1) `damage_by` applies the VS loss to the AC, 2× the missions rule; (2) every unit
of the map joins the fight at once (the game spawns them by section as you approach); (3) without a stage the
line-of-sight test is skipped, so units fire through walls. The MT numbers it uses (MoA mission 6) are otherwise right.

## 9. Open

- Families F–I: movement and the meaning of their rolls (17, 1,639, 656, 6,554 / 32,768).
- The enemy-AC script: opcodes, per-AC weapons and spread values; which PP placement is which named Raven.
- The player's +0x5C hit routine in missions (stagger, like VS?) not re-read.
- The missions engine's tick rate (ticks a second) not read here.
- Kinds by model (helicopter, tank, drone): would need the type table's model / animation data (map block 11).

## How we know

Disassembly of the work packs in the sandbox (capstone), the survey tool over all 18 PP mission modules and MoA
mission 6, and `moa-to-pp-functions.tsv` / `xdisc/ac1-pp/functions.tsv` for the pairs. No game code in this doc.

## Handed over (to this lane)

—

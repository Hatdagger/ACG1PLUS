# Lane 5 — Data and content formats (Phantasma base, MoA import)

Lane 5 of the 2026-11-26 mapping plan (`PLAN.md`). Goal: the formats needed to **import Master of Arena
content into Project Phantasma**: parts, the compiled stat block, AC builds, arenas, prizes.

Discs: **PP** = Project Phantasma US (SLUS-00670), **MoA-US** = Master of Arena US disc 1 (SLUS-01030) v1.0.
Menu modules: PP = `GG/COM/FDAT.T` entry **201**, loads at `0x800490D0`; MoA-US = entry **206**, loads at
`0x80050DD8`. "FDAT 201 +0x…" below is the byte offset inside that entry (address − load address).
Old MoA notes this builds on: `parts-tables.md`, `stat-block.md`, `arena-prizes.md`.

Status words: **tested** = seen running; **code** = read from the code; **emulated** = the game's own routine
run on the disc's own bytes in a CPU emulator (see "How we know"); **guess**.

## In plain words (for the owner)

- **Parts are stored the same way on both discs.** Same record sizes, same fields, and almost the same part
  numbers. MoA mostly *filled empty slots* with its new parts. One catch: MoA squeezed two new missiles into
  the middle of the back-weapon list, so every back weapon from number 17 up is one higher in MoA.
- **MoA quietly rebalanced parts.** Generators give more energy output, many legs carry more and have lower
  defence, most FCS lock boxes are bigger, and a handful of weapons changed (e.g. WG-RF/5 and the bazookas
  hit harder, WA-Finger has far less ammo, WM-AT is much heavier with less ammo and range).
- **The stat block the game builds before a fight has the same layout on both discs**, so MoA builds can be
  fed to Phantasma's engines without moving fields around. But a few *formulas* changed:
  - **Stability**: Phantasma's number is about 6× bigger, so most PP builds hit the top value (255) while
    MoA builds sit near the bottom. Stability decides how much a hit knocks you around. This is a big
    candidate for "why Phantasma feels different" (handed to Lanes 2 and 4 to confirm in the fight code).
  - **Damage taken**: same formula, but MoA rounds in the player's favour, so MoA ACs take about 3% less
    damage than the same AC in Phantasma.
  - **Boost thrust**: MoA is about 1.6% stronger.
  - **M118-TD** (ammo pack): +50% ammo in Phantasma, +20% in MoA. **SP-DEhf** (Phantasma) halves the
    energy per shot; MoA replaced it with **SP-DEtq**, which only cuts it to ¾.
- **Phantasma's arena data is all on its disc**, including the prize money and the 9 prize parts. The old
  importer in AC1Recomp didn't find those and used made-up money. Phantasma has no per-opponent AI records in
  that data, unlike MoA (handed to Lane 6).
- Room for MoA parts in Phantasma: there are free slots for nearly everything, except **hand weapons**
  (Phantasma's list is full at 21; MoA has 26) and the seven extra back weapons need new numbers.

## 1. Part tables (data)

Same 18-byte name + 18-byte type text at the start of every record, same fields after (see
`parts-tables.md` for every field; all offsets there hold on PP). Located on PP from the routine that points at
all of them (`0x80070C5C`, PP FDAT 201, status code) and checked record by record against MoA (emulated +
byte compare).

| Category | Stride | PP address (FDAT 201 +offset) | PP slots | MoA-US address (FDAT 206 +offset) | MoA slots |
|---|---|---|---|---|---|
| head | 60 | `800B64FC` (+0x6D42C) | 18 | `800CE690` (+0x7D8B8) | 18 |
| core | 56 | `800B6934` (+0x6D864) | 11 | `800CEAC8` (+0x7DCF0) | 11 |
| legs | 72 | `800B6B9C` (+0x6DACC) | 40 | `800CED30` (+0x7DF58) | 40 |
| arms | 52 | `800B76DC` (+0x6E60C) | 24 | `800CF870` (+0x7EA98) | 24 |
| hand weapon | 44 | `800B7BBC` (+0x6EAEC) | **21** | `800CFD50` (+0x7EF78) | 26 |
| back weapon | 52 | `800B7F58` (+0x6EE88) | **59** | `800D01C8` (+0x7F3F0) | 60 |
| blade | 52 | `800B8B54` (+0x6FA84) | **9** | `800D0DF8` (+0x80020) | 6 |
| booster | 48 | `800B8D28` (+0x6FC58) | 14 | `800D0F30` (+0x80158) | 14 |
| FCS | 96 | `800B8FC8` (+0x6FEF8) | 16 | `800D11D0` (+0x803F8) | 16 |
| generator | 48 | `800B95C8` (+0x704F8) | 15 | `800D17D0` (+0x809F8) | 15 |
| optional part | 44 | `800B9898` (+0x707C8) | **19** | `800D1AA0` (+0x80CC8) | 13 |
| weapon-arms detail (arms ID − 12) | 24 | `800B9BDC` (+0x70B0C) | 16 | `800D1DE4` (+0x8100C) | 16 |
| hand weapon detail | 24 | `800B9D5C` (+0x70C8C) | 21 | `800D1F64` (+0x8118C) | 26 |
| back weapon detail | 24 | `800B9F54` (+0x70E84) | 59 | `800D21D4` (+0x813FC) | 60 |

Tables sit back to back in the same order on both discs; no free bytes between them. PP's option table really
has 19 records (11 parts + 8 `DUMMY`), which is why MoA's menus loop over 19 option IDs while MoA's table has 13
(`parts-tables.md`, "Room for more optional parts"): MoA kept PP's loops and shrank the table.

### 1a. Part IDs, PP ↔ MoA (data, code)

**Every category keeps the same IDs except back weapons.** MoA inserted WM-MVG812 and WM-X15-EX at back 16 /
17, so **PP back ID n ≥ 17 = MoA back ID n + 1** (PP 16 is `DUMMY`; MoA 16 is WM-MVG812). Hand IDs 0–20 are the
same; MoA appended 21–25 after them. This matches the old importer's rule (`moa_phantasma.c`: back slots ≥ 17
get +1).

MoA-only parts (MoA ID; "free" = the same ID is `DUMMY` on PP, so the part can keep its MoA number):
- Head: HD-12-RADAR 11, HD-HELM 12, HD-H10 13, HD-4004 14 (all free)
- Core: XXL-D0 4 (free)
- Legs: LN-S3 9, LB-H230 20, LF-TR-0 24, LC-HTP-H5 32 (all free)
- Arms: AN-891-S 8, AW-R/4 21 (free)
- Hand: WG-HG770 21, WG-XW11 22, WG-FGI-00 23, WG-RF/E 24, WG-MG500/E 25 (**no room**: PP's table ends at 20)
- Back: WM-MVG812 16, WM-X15-EX 17, WR-RS7 23, WRR-10 24, WX-ED2 55, WX-C/4 56, P77-ST 57 (MoA numbers;
  need PP numbers, see the import sheet)
- Blade: LS-1000W 4 (free). Booster: B-PT000 7 (free). FCS: P/CV 10 (free)
- Options: SP-DEtq 11 (PP 11 is SP-DEhf, a different part), SP-ABS_Re 12 (free)

PP-only: **SP-DEhf** (option 11). Core 3 is named `XXA_S0` on PP and `XXA-S0` on MoA (same part).

Free PP slots: head 11–17, core 4–10, legs 7 9 15 20 24 25 29 32–39, arms 8–11 21–23, hand none, back 16 22–26
36–38 47–50 54–58, blade 4–8, booster 7–13, FCS 10–15, generator 9–14, options 12–18.

### 1b. Same part, different numbers (data)

Every part on both discs was compared byte by byte. Only these differ (PP → MoA):

| Part | Change |
|---|---|
| HD-G780 | shell def 186 → 146, energy def 448 → 292 |
| XXA-S0 (core 3) | shell def 600 → 580, energy def 580 → 560 |
| Legs LN-1001, LN-1001-PX-0, LN-1001B, LN-D-8000R | carry limit +30 to +50 |
| Legs LB-4400, LB-4401, LB-4303, LB-1000-P, LBKS-2B45A (reverse joint) | carry limit +100 |
| Legs LN-3001, LN-SSVR, LN-3001C, LN-2KZ-SP, LNKS-1B46J | shell or energy def lower (−10 to −78) |
| Legs LC-MOS18, LC-UKI60, LC-HTP-AAA, LC-MOS4545 (caterpillars) | shell def −70 to −85, energy def −37 to −85 |
| Arms AN-3001, ANKS-1A46J | shell def −10 / −20 |
| WG-1-KARASAWA | weight 1,000 → 1,120; type name 12 → 27 (see 1c) |
| WG-XFwPPk | weight 893 → 998 |
| WM-AT | weight 910 → 1,507; ammo 10 → 4; range 20,000 → 5,800 |
| Generators GPS-VVA, GPS-V6, GRD-RX5, GRD-RX6, GRD-RX7 | **output** (+0x2A) 4,728/4,728/5,300/6,000/6,810 → 6,000/6,000/6,560/6,800/7,440 |
| Every FCS except none | lock boxes and ranges retuned, mostly the missile class (class 2) boxes bigger in MoA; QX-9009, FBMB-18X and RATOR changed in every class (RATOR's ranges 12,000–20,000 → 20,000–25,000) |
| WG-RF/5, WG-RF/P | attack 530 → 795, 612 → 918 |
| WG-B2120, WG-B2180 | lock class 3 → 2; attack 1,250 → 1,730, 1,930 → 2,330 |
| WG-RFM118 | lock class 0 → 2; ammo 200 → 160 |
| WG-PB26 | lock class 0 → 2; ammo 15 → 24 |
| WA-Finger | ammo 3,000 → 500; fire interval 1 → 2; attack 250 → 155 |
| AW-XC5500 / AW-XC65 | type names swapped (4 ↔ 5: LASER CANNON / PLASMA CANNON) |

Everything else (all other heads, cores, arms, boosters, blades, options, the other weapons) is byte-identical.

### 1c. Weapon type names (data, open)

Weapon detail +7 indexes a 16-byte name table (MoA `0x800D5488`, 33 names). WG-1-KARASAWA uses 12 on PP and
27 on MoA; MoA's 27 is LASER RIFLE. PP's table has not been located yet; whether PP's 12 is "PLASMA RIFLE" or PP
simply has fewer names is **open** (needed only for HUD text of imported weapons).

## 2. Compiled stat block on PP (data + code)

**Code.** PP's compile routine is **`0x8009A238`** (PP FDAT 201 +0x51168), called twice from the VS START
routine `0x800A654C` (calls at `0x800A6610`, `0x800A6788`), exactly where MoA calls `0x800A7BB4` from
`0x800B2F9C` (`0x800B3058`, `0x800B31F4`). The callers are 90% alike (Lane 1's function map); the compile
routines themselves were rewritten (no automatic match), so the block was compared by running both (below).

Before the call, VS START copies the 12-byte garage loadout from `0x80042C38` to **`0x800342B4`** (MoA
`0x80045940` → `0x80036FB0`), 7 bytes from `0x80042C50` to **`0x8003BD24`** (MoA: 8 bytes `0x8004A7D0` →
`0x8003EAC8`), and the option mask `0x8003BD20` → **`0x8003BCCC`** (MoA `0x8003EAC0` → `0x8003EA70`). PP
`0x8003BD24` is the Human PLUS level (copied to stat +0x03 at `0x8009E2D4`), as MoA `0x8003EAC8` is.
Loadout byte order is the same as MoA (0 core, 1 booster, 2 head, 3 arms, 4 blade, 5 hand, 6 back L, 7 back R,
8 generator, 9 FCS, 10 unused, 11 legs). Status code.

**Where.** Block = **`0x80042740` + player × 936** (MoA `0x800453E8` + player × 928). So PP's player block is
8 bytes longer per player, which matches the "+8" field shift noted in `PLAN.md`. If the block sits at player
block +0x258 on PP (MoA +0x250), PP's player block 1 starts at `0x800424E8` (**guess**, for Lane 4 to confirm).

**Layout: identical to MoA's** (`stat-block.md` holds for PP, every offset) with one exception:

| Offset | PP | MoA |
|---|---|---|
| +0x86 | **Number of usable weapons** | Stealth (1 with P77-ST) |
| +0x87 | 0, unused | Number of usable weapons |

PP has no P77-ST, so MoA added the stealth byte by moving the weapon count up one. Anything that reads the count
(match start's ammo refill, weapon switching) must read +0x86 on PP (handed to Lane 4).

**Formulas that differ** (W = carried weight, the same in both):

| Field | PP (code `0x8009A238`) | MoA (code `0x800A7BB4`) | Effect |
|---|---|---|---|
| +0x06/+0x08 thrust | ⌊⌊power × 4096 ÷ W⌋ × 18 ÷ 4096⌋ (= ÷ 227.6), 1–64 (`0x8009A6B4`) | ⌊power × 4096 ÷ W⌋ × 74,898 ÷ 2²⁴ (= ÷ 224) (`0x800A8058`) | MoA ~1.6% more thrust; +0x07/+0x09 (×0.8) and +0x0A/+0x0B (×1.2) follow |
| +0x0C turn acceleration | legs +0x38 × 4096 ÷ W first, then scaled (`0x8009A7A0`) | legs +0x38 scaled first, then ÷ W (`0x800A8134`) | Same formula, different rounding: off by 1 in ~5% of builds (emulated) |
| +0x22 shell damage factor | 45 − ⌊(D − 745) ÷ 50⌋, 1–45 (`0x8009AD18`) | ⌊(45 × 4096 − (4096 × (D − 745)) ÷ 50) ÷ 4096⌋ (`0x800A844C`) | MoA keeps the fraction, so it is **1 lower** unless D − 745 is a multiple of 50 |
| +0x23 energy damage factor | same with 875 (`0x8009AE48`) | same with 875 | as above |
| +0x24 stability | ⌊S × W × 384 ÷ 8,144,000⌋ = S × W ÷ 21,208, 32–255 (`0x8009AEFC`) | about S × W ÷ 127,250, 32–255 (`0x800A85B4`) | **PP ≈ 6× MoA** |
| Slot +7 (back weapon turn) | — | — | Same formula, rounding differs by 1 on some legs |
| M118-TD ammo (slots +14, +16) | ×1.5 with one, ×2 with two (`0x8009E004`) | ×1.2 with one, ×1.4 with two (`0x800AA76C`) | |
| SP-DEhf / SP-DEtq (option bit 11, slot +18) | energy per shot ÷ 2 | × 3 ÷ 4 | different part |

D = head + core + arms + legs defence (shell +0x2E, energy +0x30); S = legs stability +0x40, +800 with SP-ABS
(bit 3). SP-ABS_Re (bit 12, MoA only) adds +1,600; PP has no bit 12.

Every other byte of the block came out identical on 400 random builds with the parts made identical (see "How
we know"), including all option effects (bits 0–11 touch the same fields on both).

**Stability in the fight.** MoA's match start turns +0x24 into AC +0x16C = stability × 25 ÷ 4 and knockback
level = impact × 256 ÷ that (`stat-block.md`). If PP's engine uses the same maths, PP ACs are knocked back far
less. That needs the PP fight code (handed to Lanes 2 and 4).

## 3. AC build record, 64 bytes (data)

Same on both discs (PP `MENU_TIM.T` entry 382, MoA arena build entries; old code `moa_card.c`):

| Off | Size | Meaning |
|---|---|---|
| +0 | u8 | core |
| +1 | u8 | head |
| +2 | u8 | arms (12+ = weapon arms) |
| +3 | u8 | legs |
| +4 | u8 | hand weapon (255 = none; also 255 with weapon arms) |
| +5 | u8 | blade (255 = none) |
| +6, +7 | u8 | back L, back R (255 = none) |
| +8 | u8 | FCS |
| +9 | u8 | generator |
| +10 | u16 | option mask (bit = option ID). Almost every PP opponent has `0x07F8` (options 3–10) |
| +12 | u8 | booster |
| +13 | u8 | flag (1 on most PP opponents, 0 on a few) |
| +14 | u8 | paint mode (0 = custom) |
| +15 | u8 | colour variant |
| +16 | 48 B | paint |

Note this order differs from the garage loadout (section 2) and from the card record order used in the MoA
save (`moa_card.c`).

## 4. Arena data in `GG/MS/MENU_TIM.T` (data)

PP's file has **524 entries**; MoA's **886**. Every entry ends with a seal: sum of its 32-bit words +
`0x12345678` in the last 4 bytes (checked on PP 483).

**PP: one arena, 50 slots (49 opponents + the player), entries 232–483:**

| Entries | Size each | What |
|---|---|---|
| 232–281 | 1 sector | Opponent description 1 (text ends with `>` then ^Z) |
| 282–331 | 2 sectors | Emblem (TIM) |
| 332–381 | 8 sectors | Portrait (TIM) |
| 382 | 2 sectors | **Builds**: 50 × 64-byte records (section 3) |
| 383–431 | empty | — |
| 432–481 | 1 sector | Opponent description 2 |
| 482 | 2 sectors | **Names**: 24-byte slots, `>`-terminated: 49 pilot names, then 49 AC names |
| 483 | 1 sector | **Rewards**: 8 bytes per rank (below) |
| 484–492 | 6 sectors | Prize part pictures (prize id 0–8) |
| 493–523 | empty | — |

**MoA: 9 arenas.** Counts (`0x800D5A08`): 50, 10, 10, 10, 10, 16, 31, 10, 20; starts `0x800D5A14`. Arena *a*'s
block starts at entry 140 + 4 × start + 4 × *a* and holds, per opponent in turn: description 1, emblem, portrait,
description 2 (one group of N each), then builds, names, rewards, **AI records** (36 bytes each). MoA arena 0's
first build record is byte-identical to PP's rank 1 (Necron); its names list differs. MoA names end in 0, not `>`;
MoA descriptions have no `>`/^Z ending (the old importer stripped them).

**There is no AI record entry in PP's arena block.** Where PP's arena AI gets its settings is Lane 6's question.

### 4a. Rewards and prizes (data + code)

Reward record, same on both: +0 u32 prize money, +4 u16 prize id (`0xFFFF` = none), +6 u16 unused.

**Payout code on PP**: `0x800AE84C` (MoA `0x800BC420`, same shape): reads record[opponent] through state
+0xA8 (MoA +0xB8); opponent = byte **`0x8003BD2E`** (MoA `0x8003EAD5`); credits **`0x8003BCD0`** (MoA
`0x8003EA74`), clamped to 99,999,999 and −9,999,999. Prize window `0x800AE940` (MoA `0x800BC514`): picture
`MENU_TIM.T` entry **484 + id** (MoA 844 + id). Grant `0x800AEA00` (MoA `0x800BC690`): owned index = byte
**`0x800BD764`[id]** (9 bytes; MoA `0x800D5C9C`, 23 bytes), owned count **`0x800342F0`**[index] += 1, and
**prize id 6 gets one more** (PP gives M118-TD twice; MoA instead gives +1 more for every back weapon). Status
code.

PP prize money by rank (1 → 50): 3,000,000; 1,200,000; 1,000,000; 800,000; 300,000; 240,000; 180,000; 120,000;
100,000; 90,000; 81,000; 75,000; 70,000; 62,000; 56,000; 54,000; 52,000; 51,000; 49,000; 47,000; 46,000; 44,000;
42,000; 41,000; 40,000; 38,000; 36,000; 34,000; 33,000; 32,000; 30,000; 28,000; 26,000; 25,000; 24,000; 23,000;
22,500; 21,000; 20,000; 19,000; 18,000; 15,000; 14,000; 12,000; 10,000; 9,000; 8,000; 7,000; 6,000; 5,000.

PP prizes:

| Rank | Id | Part |
|---|---|---|
| 1 | 0 | WA-Finger |
| 3 | 1 | WM-AT |
| 10 | 2 | RZ-Fw2 |
| 18 | 3 | AW-DC/2 |
| 25 | 4 | WG-PB26 |
| 31 | 5 | HD-G780 |
| 38 | 6 | M118-TD (×2) |
| 44 | 7 | WC-SPGUN |
| 48 | 8 | GBX-TL |

Settles an old MoA `?`: MoA prize id 22 (owned index 136) is **SP-ABS_Re** (`arena-prizes.md` §4).

### 4b. Owned parts and shop stock (data)

PP owned counts at **`0x800342F0`** (MoA `0x80036FEC`), one byte per part in the order head, core, arms, legs,
generator, FCS, options (19), booster, back, hand, blade. PP: 18 + 11 + 24 + 40 + 15 + 16 + 19 + 14 + 59 + 21 + 9
= **246 bytes** (MoA 249: back 60, hand 26, blade 6). So PP's index ranges are: head 0, core 18, arms 29, legs 53,
generator 93, FCS 108, options 124, booster 143, back 157, hand 216, blade 237 (checked through the prize
table: every PP prize index lands on a real part). Shop stock: MoA keeps it at owned + 0x100; PP's code also
uses owned + 0x200 (`0x800344F0`) as MoA does (`0x800371EC`), so PP stock is most likely **`0x800343F0`**
(guess). Status code (owned), guess (stock).

## 5. Import sheet: MoA content → PP

| MoA content | PP format | Conversion |
|---|---|---|
| Part record (any category) | Same record, same fields | Copy as is. Into a free PP slot where the ID is free (1a); hand weapons 21–25 and the 7 back weapons need new homes |
| Hand weapons 21–25 | **No room**: PP hand table (21) runs straight into the back table, hand detail (21) into back detail; owned table has 21 hand bytes followed by blades | Needs the tables moved (a recomp hook on the table pointers, or a disc patch that rebuilds the menu module's data) and the owned-table layout widened |
| Back weapons (MoA ID m) | PP ID | m ≤ 15: same. m = 16, 17: MoA-only. m ≥ 18: PP m − 1, except MoA-only 23 (WR-RS7), 24 (WRR-10), 55–57 (WX-ED2, WX-C/4, P77-ST) → put into free PP back slots (22–26, 36–38, 47–50, 54–58) |
| Weapon detail (24 B) | Same | Copy; check the type name index (1c) |
| Option parts | Same record; mask bit = ID | SP-ABS_Re → PP 12 (free) needs its effect added to PP's compile (stability +1,600). SP-DEtq → a free ID (13+); its effect (× 3/4) too. PP option bit 11 = SP-DEhf (÷ 2) |
| P77-ST (stealth) | No stealth byte on PP (+0x86 is the weapon count) | Needs its own field and engine support (Lane 4) |
| Rebalanced parts (1b) | — | Owner's choice: keep PP numbers (keeps PP's balance) or take MoA's |
| 64-byte build | Same | Back weapons +6, +7: MoA → PP rule above; options: bit 11 means a different part, bit 12 doesn't exist on PP |
| Arena opponents | PP arena block (section 4) | Descriptions: add `>` + ^Z; names: `>`-terminated; builds: convert IDs; rewards: same record; AI: no PP slot (Lane 6) |
| Prize parts | 9-byte id → owned index table + pictures 484+ | More than 9 prizes need a bigger table (hook `0x800AEA00`) and more picture entries (PP has 31 empty entries 493–523) |
| Stat block | Same layout | Fed from PP's own compile; nothing to convert. Formula differences (section 2) decide the feel |

## 6. How we know

- **Tables**: found by matching MoA part names in PP FDAT 201, then the exact starts from the code that loads
  every table base (`0x80070C5C` and others); every record compared byte by byte.
- **Stat block**: PP's boot program + FDAT 201 and MoA's + FDAT 206 were loaded into a MIPS emulator (unicorn)
  and each game's own compile routine was run on the same builds: 300 random builds of identical parts, then
  400 random builds with PP's part records written over MoA's tables (so both read identical parts), then each
  option bit alone. Any byte that still differed was then read in the code (addresses in section 2). The
  scripts are in `tools/mapping/content/` (they read the player's own disc files; nothing from the disc is
  stored).
- **Arena / rewards**: PP `MENU_TIM.T` entries read directly; the payout routines read in code.

## 7. Open

- PP weapon type name table (1c).
- Part model archives (`*_T.T`): entry counts match the table sizes per kind (BWL/BWR 59 vs 60, WEL 9 vs 6, WER
  21 vs 26, MENU_TMD 249 vs 252, the rest equal), but how body-part archives group 16 entries per part isn't
  decoded; PP has data in some `DUMMY` slots. Needed to import a part's model.
- PP shop stock address (guess `0x800343F0`) and new-game stock values.
- PP's 7-byte copy to `0x8003BD24` vs MoA's 8 bytes to `0x8003EAC8`: which byte MoA added.
- Paint (48 bytes) and emblem formats: copied unchanged by the old importer, not mapped field by field here.
- Whether PP's fight code reads stability and damage factors the same way as MoA's (Lanes 2, 4).

## Handed over

To Lane 2 (feel):
- Stability +0x24 is ~6× larger on PP (section 2); damage factors 1 higher on PP; thrust 1.6% lower on PP;
  generator output higher and leg defences lower in MoA (1b). Probably part of the feel difference.

To Lane 4 (battle):
- PP stat block `0x80042740` + player × 936; player block 1 likely `0x800424E8` (guess).
- PP usable-weapon count is stat +0x86 (MoA +0x87); PP has no stealth byte.

To Lane 6 (AI):
- PP's arena block in `MENU_TIM.T` has no AI records (MoA has a 36-byte record per opponent after the rewards
  entry). The PP arena AI's settings come from somewhere else.

To Lane 8 (scenario):
- PP owned-parts table `0x800342F0` (246 bytes), credits `0x8003BCD0`, arena opponent byte `0x8003BD2E`.

From other lanes to Lane 5:


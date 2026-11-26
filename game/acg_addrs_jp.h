/* Master of Arena, Japanese disc 1 (SLPS-01855): the addresses
 * game/acg_quick_start.c uses. Same names and order as acg_addrs_us.h.
 *
 * Found 2026-11-23 with tools/port_addrs.py (generated/ + US FDAT 206 against
 * generated-jp/ + JP FDAT 206), then checked by hand (docs/research/jp-port.md):
 * routines by their code, variables by every use agreeing, the picture RECTs by
 * value (module data sits 0x1270 lower than on the US disc). The menu module
 * loads at 0x80050DF8: the title routine stores its own address, 0x800AA118.
 * LOAD's unpack and the save writer are the same code with only addresses
 * changed, so the save layout below is the US one.
 * Included by acg_quick_start.c only.
 */
#ifndef ACG_ADDRS_JP_H
#define ACG_ADDRS_JP_H

/* ---- boot exe + menu module (FDAT 206, loads at 0x80050DF8) ---------------- */

#define G_TITLE_OPEN      0x800AA118u  /* (skip): opens the title */
#define G_TITLE_WORD      0x27BDFFE8u  /* its first code word: addiu sp,sp,-24 */

#define G_TO_TITLE        0x8003EA9Eu  /* byte: the menu entry opens the title */
#define G_NO_LOGOS        0x8003EAA3u  /* byte: title opens without logos */
#define G_SCORE           0x8003EAB4u  /* byte: 255 on SCENARIO */
#define G_MODE            0x8003EA4Eu  /* byte: 0 = scenario */
#define G_PLUS_FLAGS      0x8003EAE8u  /* 8 bytes: Human PLUS, Ex Arena, overweight */
#define G_LOADED_FLAG     0x8003EAA6u  /* LOAD sets it to 1 */
#define G_GARAGE          0x80036FD4u  /* 12 part ids */
#define G_OWNED           0x80037010u  /* owned parts, 256 bytes */
#define G_OWNED_COPY      0x8003EB40u  /* the card screen's copy of them */

#define G_SCENARIO_AC     0x80063C2Cu  /* garage = the scenario starting AC */
#define G_ARENA_RESET     0x800B98B4u
#define G_VS_RESET        0x800C43DCu
#define G_SETTINGS_DEF    0x800B5A8Cu
#define G_SETTINGS_APPLY  0x800B5B14u
#define G_KILL_GROUP      0x80057E3Cu  /* (group) */
#define G_PICTURE         0x80057FC4u  /* (archive, entry): read, to VRAM, at once */
#define G_TIM_QUEUE       0x800589B4u  /* (archive, entry): queued, to VRAM */
#define G_NEW_TASK        0x800578CCu  /* (group) -> task */
#define G_NO_TASK         0x801CD328u  /* what G_NEW_TASK gives when full */
#define G_PART_LISTS      0x800906F8u  /* owned-part bitmasks */
#define G_SLOT_DEFAULTS   0x80082ADCu  /* fill empty slots, fit rules */
#define G_EMPTY_SLOTS     0x800673D4u  /* (1): stand-ins for empty slots */
#define G_AC_TEXTURES     0x80081C80u  /* queue the garage AC's part textures */
#define G_MENU_ALLOC      0x800586B4u  /* (bytes) -> buffer in main RAM */
#define G_MENU_FREE       0x8005870Cu  /* (buffer) */
#define G_DISC_BUSY       0x80058CF0u  /* low 16 bits non-zero = disc queue busy */
#define G_HUB_OPEN        0x80055654u
#define G_NEWGAME_STATE   0x800ABB0Cu  /* NEW GAME windows, then 0x800ABCF8 name entry */
#define G_NAME_ENTRY      0x800ABCF8u
#define G_NULL_DRAW       0x80057F38u
#define G_LOAD_N          0x80073004u  /* LOAD: addiu v0,zero,'N' (checked only) */
#define G_LOAD_COPY       0x800731A4u  /* LOAD: first copy (checked only) */

#define G_LOAD_IMAGE      0x8002C7C0u  /* exe: LoadImage(rect, src) */
#define G_STORE_IMAGE     0x8002C820u  /* exe: StoreImage(rect, dst) */
#define G_MOVE_IMAGE      0x8002C880u  /* exe: MoveImage(rect, x, y) */
#define G_DRAW_SYNC       0x8002C514u
#define G_EMBLEM_RECT     0x800CAE00u  /* (544,384) */
#define G_EMBLEM_CLUT     0x800CAE08u  /* (256,510) */
#define G_WEAPON_RECT     0x800CA648u  /* back weapon block, ids >= 52 */
#define G_WEAPON_CLUT     0x800CA650u
#define G_HUB_TILE_RECT   0x800D339Cu  /* (416,0,16,64): hub background tile */
#define G_HUB_TILE_COPY   0x8004ACC8u
#define G_HUB_CLUT_RECT   0x800D33A4u  /* (368,225,16,1): its 16 colours */
#define G_HUB_CLUT_COPY   0x8004B710u

#define ARCH_MENU_TIM     4u

/* What LOAD itself calls after its unpack (§4a step 2). */
#define G_AFTER_LOAD_1    0x80098740u
#define G_AFTER_LOAD_2    0x80093894u
#define G_AFTER_LOAD_3    0x80093294u
#define G_AFTER_LOAD_4    0x800A1968u
#define G_AFTER_LOAD_5    0x800A1670u

static const uint32_t k_after_load[] = {
    G_SETTINGS_APPLY, G_AFTER_LOAD_1, G_AFTER_LOAD_2, G_AFTER_LOAD_3, G_AFTER_LOAD_4,
    G_AFTER_LOAD_5 };

/* The first code word of every routine (and two data words) used below, read
 * off the Japanese module. Proves the menu is the module in memory, and that it
 * is this disc's. */
static const struct { uint32_t at, word; } k_code[] = {
    { G_TITLE_OPEN,     0x27BDFFE8u }, { G_SCENARIO_AC,    0x00002821u },
    { G_ARENA_RESET,    0x00003021u }, { G_VS_RESET,       0x27BDFFE0u },
    { G_SETTINGS_DEF,   0x3C028005u }, { G_SETTINGS_APPLY, 0x27BDFFD8u },
    { G_KILL_GROUP,     0x2484FFFEu }, { G_PICTURE,        0x27BDFFE8u },
    { G_AFTER_LOAD_1,   0x3C02801Au }, { G_AFTER_LOAD_2,   0x24050018u },
    { G_AFTER_LOAD_3,   0x3C02801Au }, { G_AFTER_LOAD_4,   0x27BDFFE8u },
    { G_AFTER_LOAD_5,   0x3C02801Au }, { G_PART_LISTS,     0x27BDFFC8u },
    { G_SLOT_DEFAULTS,  0x27BDFFE0u }, { G_EMPTY_SLOTS,    0x27BDFFE0u },
    { G_AC_TEXTURES,    0x27BDFFE0u }, { G_HUB_OPEN,       0x27BDFFB8u },
    { G_NEW_TASK,       0x00804021u }, { G_TIM_QUEUE,      0x27BDFFD8u },
    { G_NEWGAME_STATE,  0x27BDFFD0u }, { G_NAME_ENTRY,     0x27BDFFE8u },
    { G_LOAD_N,         0x2402004Eu }, { G_LOAD_COPY,      0x244C6FD4u },
    { G_DISC_BUSY,      0x27BDFFE8u }, { G_MENU_ALLOC,     0x27BDFFE0u },
    { G_MENU_FREE,      0x27BDFFE8u },
    { G_HUB_TILE_RECT,  0x000001A0u }, { G_HUB_CLUT_RECT,  0x00E10170u },  /* RECT x,y */
};

/* LOAD's inline unpack (0x80073194 .. 0x80073714; US 0x80073610 ..): every
 * byte it writes comes from the save, unchanged. Same save offsets as the US. */
static const struct { uint32_t dst; uint16_t from, bytes; } k_unpack[] = {
    { 0x80036FD4u,  440,  12 },   /* garage part ids */
    { 0x8003EA96u,  452,   2 },   /* paint pattern / variant (texture set) */
    { 0x8003EA94u,  454,   2 },   /* option parts */
    { 0x8003EA98u,  456,   4 },   /* credits */
    { 0x80037210u,  460,  36 },   /* AC name, pilot name */
    { 0x8004B528u,  496,  96 },   /* owned-part bitmasks */
    { 0x80036FE0u,  592, 560 },   /* paint, owned parts, shop stock ... */
    { 0x8004AC70u, 3232,  44 },   /* mission progress */
    { 0x8004B588u, 3276, 288 },   /* settings, arena tables */
    { 0x8003EAE8u, 3564,   8 },   /* Human PLUS and the flags after it */
    { 0x8004B6A8u, 3572, 100 },   /* rest of the arena state */
};

/* Save layout: same as the US (save writer 0x800710E0 = US 0x8007155C). */
#define SAVE_SUM_AT    384u        /* u32: sum of (~byte) over the range below */
#define SAVE_SUM_FROM  432u
#define SAVE_SUM_BYTES 3240u
#define SAVE_VERSION   3672u       /* byte: 1 */
#define SAVE_EMBLEM    1152u       /* 64x64 4-bit pixels -> (544,384) */
#define SAVE_CLUT      3200u       /* 16 colours -> (256,510) */

#endif

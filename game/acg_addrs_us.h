/* Master of Arena, US disc 1 (SLUS-01030): every address and code word
 * game/acg_quick_start.c uses. Mapped in docs/research/mapping/boot-loads.md.
 * The Japanese disc's are in acg_addrs_jp.h, same names, same order; the
 * list for tools/port_addrs.py is tools/port/quick-start-us.txt.
 * Included by acg_quick_start.c only.
 */
#ifndef ACG_ADDRS_US_H
#define ACG_ADDRS_US_H

/* ---- boot exe + menu module (FDAT 206, loads at 0x80050DD8) ---------------- */

#define G_TITLE_OPEN      0x800AAE94u  /* (skip): opens the title */
#define G_TITLE_WORD      0x27BDFFE0u  /* its first code word: addiu sp,sp,-32 */

#define G_TO_TITLE        0x8003EA7Au  /* byte: the menu entry opens the title */
#define G_NO_LOGOS        0x8003EA7Fu  /* byte: title opens without logos */
#define G_SCORE           0x8003EA94u  /* byte: 255 on SCENARIO */
#define G_MODE            0x8003EA2Au  /* byte: 0 = scenario */
#define G_PLUS_FLAGS      0x8003EAC8u  /* 8 bytes: Human PLUS, Ex Arena, overweight */
#define G_LOADED_FLAG     0x8003EA82u  /* LOAD sets it to 1 */
#define G_GARAGE          0x80036FB0u  /* 12 part ids */
#define G_OWNED           0x80036FECu  /* owned parts, 256 bytes */
#define G_OWNED_COPY      0x8003EB20u  /* the card screen's copy of them */

#define G_SCENARIO_AC     0x800631FCu  /* garage = the scenario starting AC */
#define G_ARENA_RESET     0x800BA854u
#define G_VS_RESET        0x800C566Cu
#define G_SETTINGS_DEF    0x800B69F8u
#define G_SETTINGS_APPLY  0x800B6A80u
#define G_KILL_GROUP      0x80057B28u  /* (group) */
#define G_PICTURE         0x80057CB0u  /* (archive, entry): read, to VRAM, at once */
#define G_TIM_QUEUE       0x800586A0u  /* (archive, entry): queued, to VRAM */
#define G_NEW_TASK        0x800575B8u  /* (group) -> task */
#define G_NO_TASK         0x801D0BA0u  /* what G_NEW_TASK gives when full */
#define G_PART_LISTS      0x80090FD8u  /* owned-part bitmasks */
#define G_SLOT_DEFAULTS   0x80083130u  /* fill empty slots, fit rules */
#define G_EMPTY_SLOTS     0x80067244u  /* (1): stand-ins for empty slots */
#define G_AC_TEXTURES     0x800822B0u  /* queue the garage AC's part textures */
#define G_MENU_ALLOC      0x800583A0u  /* (bytes) -> buffer in main RAM */
#define G_MENU_FREE       0x800583F8u  /* (buffer) */
#define G_DISC_BUSY       0x800589DCu  /* low 16 bits non-zero = disc queue busy */
#define G_HUB_OPEN        0x800552C0u
#define G_NEWGAME_STATE   0x800AC820u  /* NEW GAME windows, then 0x800ACA0C name entry */
#define G_NAME_ENTRY      0x800ACA0Cu
#define G_NULL_DRAW       0x80057C24u
#define G_LOAD_N          0x80073480u  /* LOAD: addiu v0,zero,'N' (checked only) */
#define G_LOAD_COPY       0x80073620u  /* LOAD: first copy (checked only) */

#define G_LOAD_IMAGE      0x8002C75Cu  /* exe: LoadImage(rect, src) */
#define G_STORE_IMAGE     0x8002C7BCu  /* exe: StoreImage(rect, dst) */
#define G_MOVE_IMAGE      0x8002C81Cu  /* exe: MoveImage(rect, x, y) */
#define G_DRAW_SYNC       0x8002C4B0u
#define G_EMBLEM_RECT     0x800CC070u  /* (544,384) */
#define G_EMBLEM_CLUT     0x800CC078u  /* (256,510) */
#define G_WEAPON_RECT     0x800CB8B8u  /* back weapon block, ids >= 52 */
#define G_WEAPON_CLUT     0x800CB8C0u
#define G_HUB_TILE_RECT   0x800D460Cu  /* (416,0,16,64): hub background tile */
#define G_HUB_TILE_COPY   0x8004ACA8u
#define G_HUB_CLUT_RECT   0x800D4614u  /* (368,225,16,1): its 16 colours */
#define G_HUB_CLUT_COPY   0x8004B6F0u

#define ARCH_MENU_TIM     4u

/* What LOAD itself calls after its unpack (§4a step 2). */
#define G_AFTER_LOAD_1    0x80099098u
#define G_AFTER_LOAD_2    0x800941F4u
#define G_AFTER_LOAD_3    0x80093BC4u
#define G_AFTER_LOAD_4    0x800A2318u
#define G_AFTER_LOAD_5    0x800A2020u

static const uint32_t k_after_load[] = {
    G_SETTINGS_APPLY, G_AFTER_LOAD_1, G_AFTER_LOAD_2, G_AFTER_LOAD_3, G_AFTER_LOAD_4,
    G_AFTER_LOAD_5 };

/* The first code word of every routine (and two data words) used below. The
 * menu module and the arena / VS engines all load at 0x80050DD8, so this
 * proves the menu is the module in memory, and that it is this disc's. */
static const struct { uint32_t at, word; } k_code[] = {
    { G_TITLE_OPEN,     0x27BDFFE0u }, { G_SCENARIO_AC,    0x00002821u },
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
    { G_LOAD_N,         0x2402004Eu }, { G_LOAD_COPY,      0x244C6FB0u },
    { G_DISC_BUSY,      0x27BDFFE8u }, { G_MENU_ALLOC,     0x27BDFFE0u },
    { G_MENU_FREE,      0x27BDFFE8u },
    { G_HUB_TILE_RECT,  0x000001A0u }, { G_HUB_CLUT_RECT,  0x00E10170u },  /* RECT x,y */
};

/* LOAD's inline unpack of a Master of Arena save (0x80073610 .. 0x80073B90,
 * boot-loads.md §4e): every byte it writes comes from the save, unchanged. */
static const struct { uint32_t dst; uint16_t from, bytes; } k_unpack[] = {
    { 0x80036FB0u,  440,  12 },   /* garage part ids */
    { 0x8003EA72u,  452,   2 },   /* paint pattern / variant (texture set) */
    { 0x8003EA70u,  454,   2 },   /* option parts */
    { 0x8003EA74u,  456,   4 },   /* credits */
    { 0x800371ECu,  460,  36 },   /* AC name, pilot name */
    { 0x8004B508u,  496,  96 },   /* owned-part bitmasks */
    { 0x80036FBCu,  592, 560 },   /* paint, owned parts, shop stock ... */
    { 0x8004AC50u, 3232,  44 },   /* mission progress */
    { 0x8004B568u, 3276, 288 },   /* settings, arena tables */
    { 0x8003EAC8u, 3564,   8 },   /* Human PLUS and the flags after it */
    { 0x8004B688u, 3572, 100 },   /* rest of the arena state */
};

#define SAVE_SUM_AT    384u        /* u32: sum of (~byte) over the range below */
#define SAVE_SUM_FROM  432u
#define SAVE_SUM_BYTES 3240u
#define SAVE_VERSION   3672u       /* byte: 1 */
#define SAVE_EMBLEM    1152u       /* 64x64 4-bit pixels -> (544,384) */
#define SAVE_CLUT      3200u       /* 16 colours -> (256,510) */

#endif

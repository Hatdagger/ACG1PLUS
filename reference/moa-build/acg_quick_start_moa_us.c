/* Master of Arena: the launcher's START GAME choices, on the game side.
 *
 * The launcher hands over its choice through acg_start_set() (acg_start.h).
 * Nothing changes until the game is about to open its title screen
 * (0x800AAE94, called by the menu module's entry when it would show the
 * title). There, with the choice made:
 *
 *   LOAD GAME    the save goes into memory and the hub opens instead
 *   NEW GAME     the game's name entry opens instead
 *   LAUNCH GAME  the title opens, without the logos and intro movie unless
 *                FULL BOOT is on (the game's own "skip" argument)
 *
 * LOAD and NEW happen once per start; after that (a player who quits back to
 * the title) the title opens without logos. Every road replays what the game
 * itself does, as mapped in docs/research/mapping/boot-loads.md: §3 (the
 * title's SCENARIO row), §4a (LOAD and the card screen's finish), §4b (NEW
 * GAME). Before anything is changed, the first code word of every routine
 * used is checked, so a different disc or module just gets the normal title.
 *
 * How it hooks: a function override (psxrecomp runtime/include/
 * func_override.h) on the title open, guarded by its first code word. The
 * old fork used a function-entry hook, which only works for addresses listed
 * in game.toml and regenerated code; the override needs neither and covers
 * code loaded from the disc. Offline only: it stands aside during netplay.
 *
 * US disc 1 (SLUS-01030) addresses. The Japanese disc (step 0.2) needs its own.
 */
#include "acg_start.h"

#include "cpu_state.h"
#include "func_override.h"
#include "mod_netplay.h"
#include "mod_plugins.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- the game: boot exe + menu module (FDAT 206), see boot-loads.md -------- */

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

/* After the LOAD unpack: what LOAD itself calls (§4a step 2). */
static const uint32_t k_after_load[] = {
    G_SETTINGS_APPLY, 0x80099098u, 0x800941F4u, 0x80093BC4u, 0x800A2318u, 0x800A2020u };

/* The first code word of every routine (and two data words) used below. The
 * menu module and the arena / VS engines all load at 0x80050DD8, so this
 * proves the menu is the module in memory, and that it is this disc's. */
static const struct { uint32_t at, word; } k_code[] = {
    { G_TITLE_OPEN,     0x27BDFFE0u }, { G_SCENARIO_AC,    0x00002821u },
    { G_ARENA_RESET,    0x00003021u }, { G_VS_RESET,       0x27BDFFE0u },
    { G_SETTINGS_DEF,   0x3C028005u }, { G_SETTINGS_APPLY, 0x27BDFFD8u },
    { G_KILL_GROUP,     0x2484FFFEu }, { G_PICTURE,        0x27BDFFE8u },
    { 0x80099098u,      0x3C02801Au }, { 0x800941F4u,      0x24050018u },
    { 0x80093BC4u,      0x3C02801Au }, { 0x800A2318u,      0x27BDFFE8u },
    { 0x800A2020u,      0x3C02801Au }, { G_PART_LISTS,     0x27BDFFC8u },
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

#define DISC_WAIT_MAX  20000000u   /* safety cap; each check runs a little game time */

/* ---- the launcher's choice ---------------------------------------------------- */

static int s_mode = ACG_START_NONE;
static int s_full_boot = 0;
static int s_pending = 0;          /* LOAD / NEW not done yet for this start */
static uint8_t s_save[ACG_SAVE_BYTES];

int acg_start_save_ok(const unsigned char* save, unsigned bytes)
{
    if (!save || bytes < ACG_SAVE_BYTES) return 0;
    if (save[0] != 'S' || save[1] != 'C' || save[SAVE_VERSION] != 1u) return 0;
    uint32_t sum = 0;
    for (uint32_t i = 0; i < SAVE_SUM_BYTES; i++) sum += ~(uint32_t)save[SAVE_SUM_FROM + i];
    const uint32_t want = (uint32_t)save[SAVE_SUM_AT] | (uint32_t)save[SAVE_SUM_AT + 1] << 8 |
                          (uint32_t)save[SAVE_SUM_AT + 2] << 16 | (uint32_t)save[SAVE_SUM_AT + 3] << 24;
    return sum == want;
}

int acg_start_set(int mode, int full_boot, const unsigned char* save)
{
    s_mode = ACG_START_NONE;
    s_pending = 0;
    s_full_boot = 0;
    if (mode == ACG_START_LOAD) {
        if (!acg_start_save_ok(save, ACG_SAVE_BYTES)) return 0;
        memcpy(s_save, save, ACG_SAVE_BYTES);
    } else if (mode != ACG_START_NEW && mode != ACG_START_TITLE) {
        return mode == ACG_START_NONE;
    }
    s_mode = mode;
    s_pending = mode == ACG_START_LOAD || mode == ACG_START_NEW;
    s_full_boot = mode == ACG_START_TITLE && full_boot;
    return 1;
}

/* ---- helpers ------------------------------------------------------------------ */

static void note(const char* what)
{
    psx_mod_counter_add(what, 1);
    fprintf(stderr, "[acg start] %s\n", what);
}

static uint32_t call(struct CPUState* cpu, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2)
{
    return psx_mod_call_guest(cpu, fn, cpu->gpr[31], a0, a1, a2, 0);
}

static uint32_t le32(const uint8_t* p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void write_bytes(uint32_t dst, const uint8_t* src, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) psx_mod_write_byte(dst + i, src[i]);
}

static int code_ok(void)
{
    for (size_t i = 0; i < sizeof k_code / sizeof k_code[0]; i++)
        if (psx_mod_read_word(k_code[i].at) != k_code[i].word) return 0;
    return 1;
}

/* The menu loads most things through a disc queue; wait until it is empty,
 * as the game does before it relies on them (boot-loads.md rule 3). */
static void wait_disc(struct CPUState* cpu, const char* cap_note)
{
    uint32_t spins = 0;
    while ((call(cpu, G_DISC_BUSY, 0, 0, 0) & 0xFFFFu) != 0u)
        if (++spins >= DISC_WAIT_MAX) { note(cap_note); return; }
}

/* ---- the hub background ------------------------------------------------------- */

/* Every hub open puts its background tile (MENU_TIM entry 0's 16x64 4-bit
 * block at VRAM (416,0)) and its 16 colours ((368,225)) back from two copies
 * in RAM. The game fills those copies from VRAM after the title opens; a
 * direct start skips that, so they are filled here from the disc's own data
 * (reading VRAM back was found unreliable in the recomp: black hub).
 * Read once per program run. */
#define TILE_BYTES 2048u          /* 16 VRAM words x 64 rows */
#define TILE_CLUT_BYTES 32u
static uint8_t s_tile[TILE_BYTES], s_tile_clut[TILE_CLUT_BYTES];
static int s_tile_state = -1;     /* -1 not read, 0 failed, 1 ok */

static int read_hub_tile(void)
{
    uint32_t size = 0;
    if (!psx_mod_read_disc_file("GG/MS/MENU_TIM.T", NULL, 0, &size) || size < 2048u) return 0;
    uint8_t* f = (uint8_t*)malloc(size);
    if (!f) return 0;
    int got_pix = 0, got_clut = 0;
    uint32_t got = 0;
    if (psx_mod_read_disc_file("GG/MS/MENU_TIM.T", f, size, &got) && got == size) {
        /* Archive header: u16 entry count, then each entry's first sector. */
        const uint32_t a = (uint32_t)f[2] | (uint32_t)f[3] << 8;
        const uint32_t b = (uint32_t)f[4] | (uint32_t)f[5] << 8;
        if (b > a && (uint64_t)b * 2048u <= size) {
            const uint8_t* e = f + a * 2048u;
            const uint32_t n = (b - a) * 2048u;
            uint32_t o = 0;
            /* One TIM after another: id 0x10, flags, [CLUT block], image block;
             * each block: u32 length, u16 x, y, w, h, then the data. */
            while (o + 8u <= n && le32(e + o) == 0x10u) {
                const uint32_t flags = le32(e + o + 4u);
                o += 8u;
                int ok = 1;
                for (int part = (flags & 8u) ? 0 : 1; part < 2 && ok; part++) {
                    if (o + 12u > n) { ok = 0; break; }
                    const uint32_t len = le32(e + o);
                    if (len < 12u || o + len > n) { ok = 0; break; }
                    const uint32_t x = (uint32_t)e[o + 4] | (uint32_t)e[o + 5] << 8;
                    const uint32_t y = (uint32_t)e[o + 6] | (uint32_t)e[o + 7] << 8;
                    const uint32_t w = (uint32_t)e[o + 8] | (uint32_t)e[o + 9] << 8;
                    const uint32_t h = (uint32_t)e[o + 10] | (uint32_t)e[o + 11] << 8;
                    if (part == 1 && x == 416u && y == 0u && w == 16u && h == 64u &&
                        len - 12u >= TILE_BYTES) {
                        memcpy(s_tile, e + o + 12u, TILE_BYTES);
                        got_pix = 1;
                    }
                    if (part == 0 && x == 368u && y == 225u && w == 16u && h == 1u &&
                        len - 12u >= TILE_CLUT_BYTES) {
                        memcpy(s_tile_clut, e + o + 12u, TILE_CLUT_BYTES);
                        got_clut = 1;
                    }
                    o += (len + 3u) & ~3u;
                }
                if (!ok) break;
            }
        }
    }
    free(f);
    return got_pix && got_clut;
}

/* Fills the hub's two RAM copies. Falls back to reading VRAM, as the title
 * does, if the disc data can't be had. */
static void hub_tile(struct CPUState* cpu)
{
    if (s_tile_state < 0) s_tile_state = read_hub_tile();
    if (s_tile_state > 0) {
        write_bytes(G_HUB_TILE_COPY, s_tile, TILE_BYTES);
        write_bytes(G_HUB_CLUT_COPY, s_tile_clut, TILE_CLUT_BYTES);
        return;
    }
    note("acg.start.hub_tile_from_vram");
    call(cpu, G_STORE_IMAGE, G_HUB_TILE_RECT, G_HUB_TILE_COPY, 0);
    call(cpu, G_STORE_IMAGE, G_HUB_CLUT_RECT, G_HUB_CLUT_COPY, 0);
    call(cpu, G_DRAW_SYNC, 0, 0, 0);
}

/* ---- the roads ---------------------------------------------------------------- */

/* §3: what the title's SCENARIO row does before NEW / LOAD. */
static void scenario(struct CPUState* cpu)
{
    psx_mod_write_byte(G_SCORE, 255);
    psx_mod_write_byte(G_MODE, 0);
    call(cpu, G_SCENARIO_AC, 0, 0, 0);
    call(cpu, G_ARENA_RESET, 0, 0, 0);
    call(cpu, G_VS_RESET, 0, 0, 0);
    call(cpu, G_SETTINGS_DEF, 0, 0, 0);
    call(cpu, G_SETTINGS_APPLY, 0, 0, 0);
    for (uint32_t i = 0; i < 8u; i++) psx_mod_write_byte(G_PLUS_FLAGS + i, 0);
    call(cpu, G_KILL_GROUP, 2, 0, 0);
    call(cpu, G_PICTURE, ARCH_MENU_TIM, 0, 0);
    call(cpu, G_PICTURE, ARCH_MENU_TIM, 1, 0);
    call(cpu, G_PICTURE, ARCH_MENU_TIM, 4, 0);
    hub_tile(cpu);
}

/* LOAD GAME: §3, then §4a steps 2-4. */
static int load_game(struct CPUState* cpu)
{
    scenario(cpu);
    /* LOAD's unpack. */
    for (size_t k = 0; k < sizeof k_unpack / sizeof k_unpack[0]; k++)
        write_bytes(k_unpack[k].dst, s_save + k_unpack[k].from, k_unpack[k].bytes);
    /* The emblem and its colours go to VRAM by DMA, which only reaches main
     * RAM, so they go through a menu heap buffer as LOAD's own card buffer
     * does (boot-loads.md rule 4). */
    const uint32_t buf = call(cpu, G_MENU_ALLOC, ACG_SAVE_BYTES, 0, 0);
    if (buf >= 0x80000000u && buf + ACG_SAVE_BYTES <= 0x80200000u) {
        for (uint32_t i = 0; i < ACG_SAVE_BYTES; i += 4u) psx_mod_write_word(buf + i, le32(s_save + i));
        call(cpu, G_LOAD_IMAGE, G_EMBLEM_RECT, buf + SAVE_EMBLEM, 0);
        call(cpu, G_LOAD_IMAGE, G_EMBLEM_CLUT, buf + SAVE_CLUT, 0);
        call(cpu, G_DRAW_SYNC, 0, 0, 0);
        call(cpu, G_MENU_FREE, buf, 0, 0);
    } else {
        note("acg.start.load_no_emblem_buffer");
    }
    for (size_t i = 0; i < sizeof k_after_load / sizeof k_after_load[0]; i++)
        call(cpu, k_after_load[i], 0, 0, 0);
    psx_mod_write_byte(G_LOADED_FLAG, 1);

    /* The card screen's finish (0x8005CE68), as when opened from the title. */
    for (uint32_t i = 0; i < 256u; i++)
        psx_mod_write_byte(G_OWNED_COPY + i, psx_mod_read_byte(G_OWNED + i));
    call(cpu, G_PART_LISTS, 0, 0, 0);
    call(cpu, G_SLOT_DEFAULTS, 0, 0, 0);
    call(cpu, G_EMPTY_SLOTS, 1, 0, 0);
    call(cpu, G_AC_TEXTURES, 0, 0, 0);
    /* The AC's textures must be in VRAM before the block below moves and
     * before the hub opens (the hub never loads them itself). */
    wait_disc(cpu, "acg.start.load_disc_wait_cap");
    const uint8_t bl = psx_mod_read_byte(G_GARAGE + 6u), br = psx_mod_read_byte(G_GARAGE + 7u);
    if ((bl != 0xFFu && bl >= 52u) || (br != 0xFFu && br >= 52u)) {
        call(cpu, G_MOVE_IMAGE, G_WEAPON_RECT, 768, 448);
        call(cpu, G_MOVE_IMAGE, G_WEAPON_CLUT, 0, 493);
        call(cpu, G_MOVE_IMAGE, G_WEAPON_CLUT, 0, 494);
        call(cpu, G_DRAW_SYNC, 0, 0, 0);
    }
    /* LOAD's callback: the hub. The background copies once more right before
     * it reads them, in case anything since wrote over them. */
    hub_tile(cpu);
    call(cpu, G_HUB_OPEN, 0, 0, 0);
    note("acg.start.load_hub");
    return 1;
}

/* NEW GAME: §3, then §4b. ✕ on NEW GAME queues the AC's textures and
 * MENU_TIM 7, then the title's task runs 0x800AC820 (windows) and 0x800ACA0C
 * (credits, setup, name entry, then the hub). A task like the title's is
 * made here and started at 0x800AC820, so the game runs that chain itself. */
static int new_game(struct CPUState* cpu)
{
    scenario(cpu);
    call(cpu, G_AC_TEXTURES, 0, 0, 0);
    call(cpu, G_TIM_QUEUE, ARCH_MENU_TIM, 7, 0);
    const uint32_t t = call(cpu, G_NEW_TASK, 0, 0, 0);
    if (t < 0x80000000u || t >= 0x80200000u || t == G_NO_TASK) {
        /* SCENARIO is already half set, so the title would be wrong too:
         * report it and leave the menu as it is. */
        note("acg.start.new_no_task");
        return 1;
    }
    psx_mod_write_word(t + 0x74u, G_TITLE_OPEN);     /* as the title's own task */
    psx_mod_write_word(t + 0x78u, G_NEWGAME_STATE);
    psx_mod_write_word(t + 0x7Cu, G_NULL_DRAW);
    psx_mod_write_word(t + 0x70u, 0);
    psx_mod_write_byte(t + 2u, 0);
    psx_mod_write_byte(t + 3u, 0);
    psx_mod_write_byte(t + 5u, 0);
    psx_mod_write_byte(t + 0xA3u, 1);
    note("acg.start.new_name_entry");
    return 1;
}

/* ---- the hook ----------------------------------------------------------------- */

/* The title is about to open. Returning 0 lets it open (the argument may have
 * been changed to skip the logos); returning 1 means a direct start ran in
 * its place and the menu carries on from there. */
static int title_open(struct CPUState* cpu)
{
    if (s_mode == ACG_START_NONE || psx_mod_netplay_is_active()) return 0;
    if (s_pending) {
        s_pending = 0;                  /* once: a later title is the player's */
        if (!code_ok()) {
            note("acg.start.code_mismatch");
        } else {
            /* What opening the title clears (the menu entry clears both after
             * this call too). */
            psx_mod_write_byte(G_TO_TITLE, 0);
            psx_mod_write_byte(G_NO_LOGOS, 0);
            if (s_mode == ACG_START_LOAD ? load_game(cpu) : new_game(cpu)) return 1;
        }
    }
    if (!s_full_boot) cpu->gpr[4] = 1;  /* the title's own "skip logos and intro" */
    return 0;
}

PSX_MOD_CONSTRUCTOR(acg_quick_start_register)
{
    static const uint32_t guard[1] = { G_TITLE_WORD };
    /* Credit 0: a launcher choice, not something the PS1 would spend time on.
     * The game routines it calls charge their own time as they run. */
    if (func_override_add_guarded("acg:start", G_TITLE_OPEN, title_open, guard, 1, 0) != FO_OK)
        fprintf(stderr, "[acg start] could not register the title hook\n");
}

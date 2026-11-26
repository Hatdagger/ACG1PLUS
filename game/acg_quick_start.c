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
 * Addresses per disc: game/acg_addrs_us.h (US disc 1, SLUS-01030) and
 * game/acg_addrs_jp.h (Japanese disc 1, SLPS-01855), picked by ACG_REGION_JP.
 * While a disc's table isn't complete (ACG_START_ALL 0, acg_start.h) nothing is
 * hooked: every start boots the normal way and only LAUNCH GAME is accepted.
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

#if ACG_REGION_JP
#include "acg_addrs_jp.h"   /* Japanese disc 1 (SLPS-01855) */
#else
#include "acg_addrs_us.h"   /* US disc 1 (SLUS-01030) */
#endif

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
#if !ACG_START_ALL
    /* This disc's addresses aren't all found yet: LAUNCH GAME (a normal boot)
     * or nothing; a stray LOAD / NEW says no. */
    (void)full_boot;
    (void)save;
    return mode == ACG_START_NONE || mode == ACG_START_TITLE;
#endif
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
#if !ACG_START_ALL
    return;   /* table not complete for this disc: no hook, normal boot */
#endif
    static const uint32_t guard[1] = { G_TITLE_WORD };
    /* Credit 0: a launcher choice, not something the PS1 would spend time on.
     * The game routines it calls charge their own time as they run. */
    if (func_override_add_guarded("acg:start", G_TITLE_OPEN, title_open, guard, 1, 0) != FO_OK)
        fprintf(stderr, "[acg start] could not register the title hook\n");
}

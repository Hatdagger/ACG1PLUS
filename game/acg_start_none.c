/* START GAME stand-in: every start boots the normal way.
 *
 * Used by the Project Phantasma program (since 2026-11-26) until its own
 * quick-start hooks are written (ladder 0.3): the Master of Arena version
 * (reference/moa-build/acg_quick_start_moa_us.c) replays MoA US code at MoA
 * addresses, which mean nothing on Phantasma. The launcher's START GAME page
 * offers only LAUNCH GAME meanwhile (launcher/moa_menu.inc, ACG_QUICK_START).
 *
 * The save check below is Master of Arena's save format and is not used while
 * LOAD GAME is off; it gets Phantasma's layout with the quick starts.
 */
#include "acg_start.h"

#define SAVE_VERSION   3672u
#define SAVE_SUM_AT    384u
#define SAVE_SUM_FROM  432u
#define SAVE_SUM_BYTES (3672u - 432u)

int acg_start_save_ok(const unsigned char* save, unsigned bytes)
{
    if (!save || bytes < ACG_SAVE_BYTES) return 0;
    if (save[0] != 'S' || save[1] != 'C' || save[SAVE_VERSION] != 1u) return 0;
    unsigned long sum = 0;
    for (unsigned i = 0; i < SAVE_SUM_BYTES; i++)
        sum = (sum + (unsigned long)(unsigned char)~save[SAVE_SUM_FROM + i]) & 0xFFFFFFFFul;
    const unsigned long want = (unsigned long)save[SAVE_SUM_AT] |
                               (unsigned long)save[SAVE_SUM_AT + 1] << 8 |
                               (unsigned long)save[SAVE_SUM_AT + 2] << 16 |
                               (unsigned long)save[SAVE_SUM_AT + 3] << 24;
    return sum == want;
}

/* Every choice boots normally. 1 for the choices the launcher offers
 * (LAUNCH GAME, nothing), 0 for the ones it doesn't, so a stray call says no. */
int acg_start_set(int mode, int full_boot, const unsigned char* save)
{
    (void)full_boot;
    (void)save;
    return mode == ACG_START_NONE || mode == ACG_START_TITLE;
}

/* START GAME stand-in for the Japanese disc program (ACG_REGION=JP).
 *
 * game/acg_quick_start.c replays the US game's own boot, title and LOAD code
 * at US disc addresses. Those addresses have moved on the Japanese disc
 * (SLPS-01855), so until they are found there this program boots every start
 * the normal way: logos, intro and title screen. The launcher's START GAME
 * page offers only LAUNCH GAME in this program (launcher/moa_menu.inc,
 * ACG_REGION_JP).
 *
 * The save check stays: it reads the memory card block, not game code.
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

/* Every choice boots normally. 1 for the choices the JP launcher offers
 * (LAUNCH GAME, nothing), 0 for the ones it doesn't, so a stray call says no. */
int acg_start_set(int mode, int full_boot, const unsigned char* save)
{
    (void)full_boot;
    (void)save;
    return mode == ACG_START_NONE || mode == ACG_START_TITLE;
}

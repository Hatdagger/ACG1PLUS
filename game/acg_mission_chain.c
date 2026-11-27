/* Missions that start the next one straight away, and the AC1 test switch.
 *
 * Phantasma's missions engine kept AC1's chain: after a won mission 48, its main loop (0x8004A68C) sets the next
 * mission to 49 and returns 1, and the engine starts 49 without going back to the menu (check at 0x8004AA48).
 * No Phantasma mission is 48, so it never fires. AC1 needs it for "Destroy Floating Mines" -> "Destruction of
 * Ravens" (AC1 48 -> 49 = engine 98 -> 99), and the Master of Arena import wants "Enter Factory" -> "Final
 * Mission" (38 -> 39). Instead of patching the two numbers in the check, a table says which mission leads to
 * which (import-outlook.md §8):
 *
 *   - just before the results screen (0x80088E30, the last call before the check), a won mission listed in the
 *     table is shown to the check as 48, so the game's own chain runs: next = 49, start now;
 *   - when the engine starts again (0x8004DAD4), a 49 that came from a listed mission becomes the table's next.
 * Nothing between those two points reads the number (FDAT 202 reads player block +0x25D only in the check and
 * in the start-up; the boot program only after the engine has returned).
 *
 * The AC1 test switch (content/ac1/test-slot.txt, made by `ac1import.py --test-slot N`): every Phantasma mission
 * started from the menu (not the garage test, 19) runs AC1 slot N instead, until AC1 has its own mission list. Remove the file to stop.
 *
 * Project Phantasma USA addresses; every hook checks its code words first. Stands aside during netplay.
 */
#include "acg_mission_files.h"

#include "cpu_state.h"
#include "func_override.h"
#include "mod_netplay.h"
#include "mod_plugins.h"

#include <stdio.h>

#define G_RESULTS       0x80088E30u   /* results screen; called at 0x8004A9F4 */
#define RA_RESULTS      0x8004A9FCu
#define G_STARTUP       0x8004DAD4u   /* missions engine start-up: next number -> M+0, loads the files */
#define G_CHECK         0x8004AA48u   /* lbu v1, 0x25d(a0); addiu v0, zero, 48 */
#define W_CHECK0        0x9083025Du
#define W_CHECK1        0x24020030u
#define LOST_OR_QUIT    0x300u        /* M+0xC bits that take the other ends of the main loop (no chain) */

#define JAL(t) (0x0C000000u | (((t) >> 2) & 0x03FFFFFFu))

static const struct { uint8_t from, to; } s_chain[] = {
    { 98, 99 },   /* AC1 "Destroy Floating Mines" -> "Destruction of Ravens" */
};

static int chain_next(uint32_t n)
{
    for (size_t i = 0; i < sizeof s_chain / sizeof s_chain[0]; i++)
        if (s_chain[i].from == n) return s_chain[i].to;
    return -1;
}

static int check_resident(void)
{
    return psx_mod_read_word(G_CHECK) == W_CHECK0 && psx_mod_read_word(G_CHECK + 4u) == W_CHECK1
        && psx_mod_read_word(RA_RESULTS - 8u) == JAL(G_RESULTS) && acg_missions_engine_resident();
}

static int results(struct CPUState* cpu)
{
    if (cpu->gpr[31] != RA_RESULTS || psx_mod_netplay_is_active() || !check_resident()) return 0;
    const uint32_t next = psx_mod_read_byte(ACG_PP_NEXT_MISSION);
    if (chain_next(next) >= 0 && next == psx_mod_read_byte(ACG_PP_MISSION_NUMBER)
        && (psx_mod_read_word(ACG_PP_MISSION_FLAGS) & LOST_OR_QUIT) == 0) {
        psx_mod_write_byte(ACG_PP_NEXT_MISSION, 48);
        fprintf(stderr, "[acg missions] mission %u won: chaining to %d\n", next, chain_next(next));
    }
    return 0;   /* the results screen runs as usual */
}

static int startup(struct CPUState* cpu)
{
    (void)cpu;
    if (psx_mod_netplay_is_active() || !acg_missions_engine_resident()) return 0;
    const uint32_t next = psx_mod_read_byte(ACG_PP_NEXT_MISSION);
    const int to = chain_next(psx_mod_read_byte(ACG_PP_MISSION_NUMBER));   /* M+0 still holds the last one */
    if (next == 49 && to >= 0) {
        psx_mod_write_byte(ACG_PP_NEXT_MISSION, (uint8_t)to);
        return 0;
    }
    const int slot = acg_ac1_test_slot();
    if (slot >= 0 && acg_mission_source(next) == ACG_SRC_PHANTASMA && next != 19) {   /* 19 = garage test */
        fprintf(stderr, "[acg missions] test switch: Phantasma mission %u -> AC1 slot %d\n", next, slot);
        psx_mod_write_byte(ACG_PP_NEXT_MISSION, (uint8_t)(ACG_AC1_FIRST + (uint32_t)slot));
    }
    return 0;   /* the start-up runs as usual, with the new number */
}

PSX_MOD_CONSTRUCTOR(acg_mission_chain_register)
{
    static const uint32_t g_results[2] = { 0x27BDFFA0u, 0x3C028009u };
    static const uint32_t g_startup[2] = { 0x27BDFFD0u, 0x24040001u };
    /* Both only change a byte and let the game's routine run (credit unused on a decline). */
    if (func_override_add_guarded("acg:missions:results", G_RESULTS, results, g_results, 2, 0) != FO_OK)
        fprintf(stderr, "[acg missions] could not register the results hook\n");
    if (func_override_add_guarded("acg:missions:startup", G_STARTUP, startup, g_startup, 2, 0) != FO_OK)
        fprintf(stderr, "[acg missions] could not register the start-up hook\n");
}

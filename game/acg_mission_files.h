/* Missions from other games on Phantasma's missions engine: shared pieces.
 *
 * Phantasma's missions engine runs one mission at a time, picked by a byte, the engine number (mission state
 * record M+0, 0x80097A18). Numbers say where the mission comes from:
 *     0 .. 19   Phantasma's own (the disc)
 *    20 .. 39   Master of Arena (missions-import-pp.md §5; not built yet: these load from the disc as before)
 *    50 .. 119  Armored Core 1: AC1 engine number + 50 (import-outlook.md §8)
 * While a mission from another game runs, its files come from that game's pack in the content folder, made on
 * the player's PC from their own disc (tools/ac1import/). Nothing here is game data.
 *
 * Project Phantasma USA (SLUS-00670) addresses. Every hook first checks the code words around it, so another
 * disc or another module in memory leaves the game alone.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum AcgMissionSource { ACG_SRC_PHANTASMA = 0, ACG_SRC_MOA = 1, ACG_SRC_AC1 = 2 };

#define ACG_AC1_FIRST 50u
#define ACG_AC1_END   120u

#define ACG_PP_MISSION_NUMBER 0x80097A18u   /* M+0: engine number of the mission that runs (byte) */
#define ACG_PP_MISSION_FLAGS  0x80097A24u   /* M+0xC */
#define ACG_PP_NEXT_MISSION   0x80042745u   /* player block 1 +0x25D: number the next start-up takes (byte) */

int acg_mission_source(uint32_t number);

/* The missions engine (Phantasma FDAT 202) is what's in memory now. */
int acg_missions_engine_resident(void);

/* Folder of the content packs, ending in a separator ("" if unknown):
 *   $ACG1PLUS_CONTENT_DIR, else <game folder>/RecompData/content/ when the program sits in RecompData/<os>/,
 *   else <program's folder>/content/. */
const char* acg_content_dir(void);

/* AC1 pack ready (pack.txt read, files there). */
int acg_ac1_pack_ready(void);

/* AC1 test switch: the AC1 slot to run in place of any Phantasma mission (content/ac1/test-slot.txt),
 * or -1. Test builds only; goes away when AC1 gets its own mission list. */
int acg_ac1_test_slot(void);

#ifdef __cplusplus
}
#endif

/* The launcher's START GAME choice, handed to the game side.
 *
 * The launcher and the game run in one program, so the choice is passed as
 * plain data: the launcher calls acg_start_set() right before it starts the
 * game, and game/acg_quick_start.c acts on it when the game would open its
 * title screen.
 *
 *   ACG_START_LOAD   LOAD GAME: the save given goes into memory and the hub
 *                    opens (no logos, title, NEW/LOAD menu or card screens)
 *   ACG_START_NEW    NEW GAME: straight to the game's name entry
 *   ACG_START_TITLE  LAUNCH GAME: the title screen; the logos and intro movie
 *                    are skipped unless full_boot is set
 *   ACG_START_NONE   the game boots as it always has
 *
 * Offline only: nothing happens while netplay is active.
 */
#ifndef ACG_START_H
#define ACG_START_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    ACG_START_NONE = 0,
    ACG_START_LOAD = 1,
    ACG_START_NEW = 2,
    ACG_START_TITLE = 3
};

/* What the game's LOAD reads from a save: the first 3,712 bytes of its block. */
#define ACG_SAVE_BYTES 3712u

/* Set the next start. `save` (ACG_SAVE_BYTES, copied) is needed for
 * ACG_START_LOAD and ignored otherwise. full_boot only matters for
 * ACG_START_TITLE. Returns 0 (and sets ACG_START_NONE) if a LOAD has no save
 * or the save doesn't check out. */
int acg_start_set(int mode, int full_boot, const unsigned char* save);

/* Checks a save block the way the game's LOAD does: the "SC" card header,
 * the version byte, and the checksum. 1 = the game would load it. */
int acg_start_save_ok(const unsigned char* save, unsigned bytes);

#ifdef __cplusplus
}
#endif

#endif

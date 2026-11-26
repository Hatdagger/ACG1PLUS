/* Portable install: everything the game and the launcher write stays inside the
 * game folder, so the folder can live on a USB drive and be copied to another
 * PC or the Steam Deck (owner decision 2026-11-26; docs/building.md).
 *
 * game/acg_portable.c runs before the game's main() and points the framework's
 * caches into the folder. This header gives the launcher its own folder. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* The launcher's small files (menu-sounds.txt, full-boot.txt, the updater's
 * GitHub token), as a UTF-8 path ending in a separator:
 *   <game folder>/RecompData/launcher/   in the portable layout
 *   <program's folder>/launcher/         when the program runs anywhere else
 * The folder is created if missing. Empty string if the program's own path
 * cannot be read. */
const char* acg_portable_launcher_dir(void);

#ifdef __cplusplus
}
#endif

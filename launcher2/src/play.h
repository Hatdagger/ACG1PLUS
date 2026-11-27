/* START GAME (ladder 0.2.3): starting the game that "Preparing game" built in
 * RecompData/<os>/, and waiting for it to close.
 *
 * The game is started straight into the game (its own in-game launcher skipped
 * with --no-launcher), with the player's disc (--disc) and the shared memory
 * cards in "Memory Card/" (--memcard-dir), the same folder for the Windows and
 * the Steam Deck builds. ACG1PLUS_LAUNCHER=1 tells the game that this launcher
 * started it, so Esc (or holding SELECT + START) closes the game and the
 * launcher comes back (game/acg_escape.c). Everything the game prints goes to
 * RecompData/launcher/logs/game-last.log (the run before: game-previous.log). */
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace play {

namespace fs = std::filesystem;

/* FULL BOOT: show the PlayStation start-up screen before the game (off: it is
 * skipped). Saved in launcher.ini (full_boot=0/1). Skipping the game's own logos
 * and intro movie needs game-side hooks (ladder 0.3). */
bool full_boot();
void set_full_boot(bool on);

/* Tests: extra words for the game's command line (--test-game-args a,b,c). */
void set_test_args(const std::vector<std::string>& args);

/* Start the game with this disc. False (and *error in plain words) when it
 * could not be started. */
bool start(const fs::path& disc_cue, std::string* error);
bool running();
/* Stop the game (and everything it started). */
void stop();

struct Result {
    int         code = 0;          /* the game's exit code */
    double      seconds = 0.0;     /* how long it ran */
    std::string log_file;
    std::vector<std::string> tail; /* its last lines, for the error page */
};
/* True once, when the game has closed since start(). */
bool ended(Result* out);

} /* namespace play */

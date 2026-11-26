/* Armored Core Gen 1: update check and self-update from the project's GitHub
 * releases, and (dev builds only) from the newest GitHub Actions build. Nothing
 * here runs by itself: the launcher calls check() or download_and_apply() when
 * the player asks.
 *
 * An update replaces only the program's own files. Memory cards, saves, mods,
 * disc images and the player's settings are never read, moved or overwritten.
 *
 * Brought over from the AC1Recomp fork (games/master-of-arena/moa_update.*),
 * adapted to this repo's releases and folder; the dev channel is new.
 */
#pragma once
#include <cstdint>
#include <string>

namespace moa_update {

enum class State {
    Idle,          /* nothing asked yet */
    Checking,
    UpToDate,
    Available,     /* a newer build exists */
    Downloading,
    Applying,
    Done,          /* installed; the launcher restarts the game by itself */
    Failed,
};

enum class Channel { Release, Dev };

struct Snapshot {
    State       state = State::Idle;
    Channel     channel = Channel::Release;   /* what the last check looked at */
    std::string current;      /* this build's version, e.g. "0.1.0" */
    std::string current_dev;  /* dev build installed by the updater, "" if none */
    std::string latest;       /* newest found: a version, or "BUILD #12 (abc1234)" */
    std::string message;      /* last result, one line */
    uint64_t    done = 0;     /* bytes downloaded */
    uint64_t    total = 0;    /* bytes expected, 0 when unknown */
};

const char* current_version();
/* True when this build can replace itself; otherwise why not. */
bool supported(std::string* why);
Snapshot snapshot();
void check();                 /* asks GitHub which release is newest */
void download_and_apply();    /* only after a check found one */
/* After an update (state Done): starts the game again through the starter and
 * returns true; the caller then closes the launcher. False when it can't (the
 * message then says to restart by hand). */
bool restart();

/* ---- dev channel (compiled out of public releases) ------------------------ */
bool dev_available();         /* false in public builds */
bool has_token();
bool set_token(const std::string& token);   /* stored outside the game folder */
void forget_token();
void check_dev();             /* the newest successful Actions build of main */

} /* namespace moa_update */

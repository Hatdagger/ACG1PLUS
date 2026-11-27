/* Finding and checking the player's Project Phantasma USA disc image.
 *
 * Looks in Disc/, beside the launcher, at the disc chosen last time, then on
 * other drives (Windows: the other drive letters; Linux / Deck: /run/media,
 * /media, /mnt), two folders deep. Each .cue's data track is checked here, before
 * any download: its size against game.toml's known_sizes, then its MD5 against
 * known_md5 (both baked into the launcher from game.toml). The build checks it
 * again with the framework's verify-disc. */
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace disc {

namespace fs = std::filesystem;

enum class Check { Waiting, Checking, Good, Bad };

struct Candidate {
    fs::path    cue;
    fs::path    bin;
    uint64_t    size = 0;
    bool        inside = false;    /* already in the game folder */
    Check       check = Check::Waiting;
    float       progress = 0.0f;   /* while Checking */
    std::string why;               /* when Bad, in plain words */
};

struct Snapshot {
    bool scanning = false;         /* still looking or checking */
    std::string looking_in;
    std::vector<Candidate> found;
};

/* Look everywhere (background thread). */
void scan(const fs::path& root, const fs::path& last_choice);
/* Check one file the player picked (background thread; it joins the list). */
void check_one(const fs::path& cue, const fs::path& root);
Snapshot snapshot();
void stop();

/* The test-only extra MD5 (--test-disc-md5), for a test image. */
void accept_md5(const std::string& md5);

/* Copy the .cue and its .bin into DEST (background thread). */
void copy_start(const Candidate& c, const fs::path& dest);
/* -1 copying (frac set), 0 failed (error set), 1 done (new_cue set). */
int copy_status(float* frac, std::string* error, fs::path* new_cue);

std::string display_name(const fs::path& cue);

} /* namespace disc */

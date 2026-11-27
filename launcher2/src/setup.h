/* The "Preparing game" work (ladder 0.2.2): the steps of scripts/linux/build.sh
 * and scripts/windows/build.ps1, run by the launcher on a background thread,
 * every program it needs started as a child (curl, the pack's Python with
 * psxrecomp_cli.py generate / rebuild). Everything stays in the game folder.
 *
 * Each step leaves a marker when it is done, so a stopped build (cancel, crash,
 * power off) picks up where it stopped; downloads resume. */
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace setup {

namespace fs = std::filesystem;

enum Stage {
    S_FOLDER = 0,    /* Checking the game folder */
    S_TOOLS_DL,      /* Downloading the tools */
    S_TOOLS_UNPACK,  /* Unpacking the tools */
    S_SOURCE,        /* Getting the game's source */
    S_DISC,          /* Checking your disc */
    S_TRANSLATE,     /* Translating the game from your disc */
    S_NATIVE,        /* Building mission and arena code */
    S_COMPILE,       /* Compiling the game */
    S_PLACE,         /* Putting the game in place */
    S_CHECK,         /* Final check */
    S_COUNT
};
const char* stage_name(int s);

enum class Status { Idle, Running, Cancelling, Cancelled, Failed, Done };

struct Snapshot {
    Status      status = Status::Idle;
    int         stage = -1;                 /* the one running (or that stopped) */
    float       frac[S_COUNT] = {};         /* 0..1 within each step; -1 = no measure */
    bool        done[S_COUNT] = {};
    bool        skipped[S_COUNT] = {};      /* already done before, nothing to do */
    std::string detail;                      /* "312 of 833 MB", "packed 41 of 67" ... */
    double      elapsed = 0.0;               /* seconds since start */
    double      eta = -1.0;                  /* seconds left, -1 = can't tell yet */
    std::string error_title, error_text;     /* when Failed */
    std::vector<std::string> log;            /* the last lines */
    std::string log_file;
};

/* Folder layout (docs/building.md), from the launcher's own folder. */
struct Paths {
    fs::path root, data, build, src, tools, pack, temp, logs, game, launcher, state;
};
const Paths& paths();
void init_paths(const fs::path& root);

/* Test / developer options (command line). */
struct Options {
    fs::path    source_zip;          /* --source-zip: use this zip, any checksum */
    std::vector<std::string> extra_md5;   /* --test-disc-md5 */
    bool        force_translate = false;
};
void set_options(const Options& o);
const Options& options();

/* The source zip this launcher builds (baked in by the release workflow). */
std::string source_version();
bool        source_known();

void start(const fs::path& disc_cue);
void cancel();
Snapshot snapshot();
/* Joins the worker when it has ended (call from the UI now and then). */
void reap();

/* Is a game built in RecompData/<os>/ (and from which source)? */
bool game_built(std::string* info);

} /* namespace setup */

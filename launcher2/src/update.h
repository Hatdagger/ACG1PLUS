/* UPDATES (ladder 0.2.4): newer launchers from the repo's GitHub Releases, a
 * hidden TEST channel for the owner (the test-* pre-releases), and the
 * launcher replacing itself.
 *
 * A release's launcher and its source zip belong together (launcher2/release.sh
 * bakes the zip's checksum into the launcher), so an update is one thing: the
 * new launcher replaces this one and starts, and then rebuilds the game from
 * its own source. The old game stays in RecompData/<os>/ and keeps working
 * until the new one has been built and put in place; if the build stops or
 * fails, nothing changes.
 *
 * The work runs on a background thread with curl (as the first run does):
 *   - the list of releases: api.github.com/repos/<repo>/releases (no sign-in;
 *     GitHub allows 60 of these an hour from one address),
 *   - each release's manifest.json (launcher version, every file's SHA-256),
 *   - the new launcher, checked against the manifest, then asked for its
 *     version (it must start here and say the version the manifest says)
 *     before it replaces this one.
 *
 * Everything stays in the game folder: RecompData/launcher/updates/ (the last
 * list, downloads, the launcher that was replaced, after-update.ini) and the
 * settings in launcher.ini (check_updates, test_unlocked, channel). */
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace update {

namespace fs = std::filesystem;

enum class State { Idle, Checking, Checked, Failed, Downloading, Installed };

struct Release {
    std::string tag, title, date;       /* date: YYYY-MM-DD */
    bool        test = false;           /* a test-* pre-release */
    std::string manifest_url;
    /* From its manifest.json (have_manifest). */
    bool        have_manifest = false;
    std::string version, summary, commit;
    std::string launcher_url, launcher_sha256;
    uint64_t    launcher_size = 0;
    /* Compared with this launcher. */
    bool        installed = false;      /* its launcher is this very file */
    bool        newer = false;          /* a higher version than this one */
    bool        too_old = false;        /* before 0.2.4: it couldn't update back */
};

struct Snapshot {
    State       state = State::Idle;
    std::string message;                /* plain words for the page */
    bool        bad = false;            /* message is a problem */
    std::vector<Release> releases;      /* the chosen channel's, newest first */
    int         best = -1;              /* RELEASES: the one to offer, or -1 */
    uint64_t    done = 0, total = 0;    /* while downloading */
    std::string checked;                /* "20:41" when the list was read */
    std::string target;                 /* the tag being installed */
};

/* Where the launcher's own file is and how it was started (for the restart). */
void init(const std::vector<std::string>& args);
/* Tests: read the list of releases from this address instead of GitHub's
 * (file:// works; --test-releases-url). */
void set_releases_url(const std::string& url);

void check();                 /* look for updates now (background) */
void install(int index);      /* install snapshot().releases[index] (background) */
Snapshot snapshot();
bool busy();

/* After install(): true once, then main.cpp closes and starts the new launcher. */
bool take_restart();
void restart();               /* main.cpp, after everything is shut down */
void shutdown();              /* the launcher is closing: stop and wait */

/* The launcher was just updated: the version it replaced (once), or "". */
std::string take_updated_from();

/* launcher.ini settings. */
bool check_at_start();
void set_check_at_start(bool on);
bool test_unlocked();         /* the TEST channel row is shown */
void set_test_unlocked(bool on);
bool test_channel();          /* TEST chosen (only while unlocked) */
void set_test_channel(bool on);

/* "1.2.10" > "1.2.9": compares the numbers one by one. */
int compare_versions(const std::string& a, const std::string& b);

} /* namespace update */

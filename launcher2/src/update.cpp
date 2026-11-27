/* UPDATES (ladder 0.2.4). See update.h for what it does and why one update is
 * "new launcher, then rebuild the game from its source".
 *
 * Reconsidered from the old in-game updater (launcher/moa_update.cpp): that one
 * downloaded GitHub Actions builds with the player's own GitHub token and
 * replaced the game's program files while the game was running. Here the repo
 * is public and has no game in it, so no token is needed (Releases only), and
 * what is replaced is only this small launcher; the game is built on this PC
 * as in the first run, and only put in place once it has built. */
#include "update.h"

#include "hash.h"
#include "ini.h"
#include "json.h"
#include "platform.h"
#include "setup.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <thread>

#ifndef ACG_LAUNCHER_VERSION
#define ACG_LAUNCHER_VERSION "0.0.0"
#endif
#ifndef ACG_REPO
#define ACG_REPO "Hatdagger/ACG1PLUS"
#endif

namespace update {

namespace {

/* The first launcher that has UPDATES: a test build older than this one could
 * be installed, but couldn't update back by itself. */
const char* kFirstWithUpdates = "0.2.4";

#if defined(_WIN32)
const char* kSelfName = "ACG1PLUS.exe";
#else
const char* kSelfName = "ACG1PLUS";
#endif

std::mutex g_mu;
Snapshot g_snap;
std::thread g_worker;
std::atomic<bool> g_running{false};
std::atomic<bool> g_restart{false};
std::atomic<bool> g_stop{false};     /* the launcher is closing */
std::chrono::steady_clock::time_point g_installed_at;  /* the page shows it for a moment */
fs::path g_self;                 /* this launcher's file, found at start */
std::vector<std::string> g_args; /* how it was started (for the restart) */
std::string g_self_sha;          /* worked out once, on the worker thread */
std::string g_releases_url;      /* tests only */

fs::path ini_file() { return setup::paths().launcher / "launcher.ini"; }
fs::path dir() { return setup::paths().launcher / "updates"; }

std::string setting(const char* key, const char* def) {
    const auto kv = ini::load(ini_file());
    auto it = kv.find(key);
    return it == kv.end() ? std::string(def) : it->second;
}

void set_msg(const std::string& m, bool bad) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_snap.message = m;
    g_snap.bad = bad;
}

std::string read_file(const fs::path& p) {
    FILE* f = plat::open_file(p, "rb");
    if (!f) return std::string();
    std::string s;
    char b[8192];
    size_t n;
    while ((n = std::fread(b, 1, sizeof b, f)) > 0) s.append(b, n);
    std::fclose(f);
    return s;
}

std::string sha256_file(const fs::path& p) {
    FILE* f = plat::open_file(p, "rb");
    if (!f) return std::string();
    hash::Sha256 h;
    std::vector<char> buf(1 << 20);
    size_t n;
    while ((n = std::fread(buf.data(), 1, buf.size(), f)) > 0) h.update(buf.data(), n);
    std::fclose(f);
    return h.hex();
}

uint64_t fsize(const fs::path& p) {
    std::error_code ec;
    const auto s = fs::file_size(p, ec);
    return ec ? 0 : (uint64_t)s;
}

struct Failed { std::string text; };

/* One curl download into DEST. Small files (lists, manifests) get a time limit;
 * the launcher itself doesn't, and its progress is the file's size. Returns
 * the HTTP status (0 for file:// in tests). */
int fetch(const std::string& url, const fs::path& dest, bool small, uint64_t expect = 0) {
    const std::string curl = plat::curl_program();
    if (curl.empty()) throw Failed{ "This system has no curl program, which the launcher uses to download. "
                                    "Windows 10 and 11 and SteamOS come with it." };
    std::error_code ec;
    fs::create_directories(dest.parent_path(), ec);
    fs::remove(dest, ec);
    std::vector<std::string> args = { curl, "-L", "-sS", "--connect-timeout", "20", "-o", plat::u8str(dest),
                                      "-w", "%{http_code}", "-A", "ACG1PLUS-launcher/" ACG_LAUNCHER_VERSION };
    if (small) { args.push_back("--max-time"); args.push_back("60"); }
    else { args.push_back("--retry"); args.push_back("3"); }
    if (url.find("api.github.com") != std::string::npos) {
        args.push_back("-H");
        args.push_back("Accept: application/vnd.github+json");
    }
    args.push_back(url);
    std::string out, err;
    plat::Process p;
    std::string why;
    if (!p.start(args, {}, dest.parent_path(), [&](int st, const std::string& l) {
            if (st == 1) out += l; else err = l;
        }, &why))
        throw Failed{ "Couldn't start curl: " + why };
    int code = 0;
    while (!p.poll(&code)) {
        if (g_stop) { p.kill_tree(); p.wait(); throw Failed{ "Stopped." }; }
        if (expect) {
            std::lock_guard<std::mutex> lk(g_mu);
            g_snap.done = fsize(dest);
            g_snap.total = expect;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
    if (code != 0)
        throw Failed{ "Couldn't reach GitHub. Check the internet connection and try again." +
                      (err.empty() ? std::string() : "\n(curl: " + err + ")") };
    return std::atoi(out.c_str());
}

std::string date_of(const std::string& iso) { return iso.size() >= 10 ? iso.substr(0, 10) : iso; }

/* GitHub's answer when it isn't a list: usually the hourly limit. */
std::string github_problem(int http, const std::string& body) {
    json::Value v;
    std::string msg;
    if (json::parse(body, &v)) msg = v["message"].str();
    if (http == 403 || http == 429 || msg.find("rate limit") != std::string::npos)
        return "GitHub says too many update checks came from this internet connection in the last hour. "
               "Try again later.";
    if (http == 404) return "The releases page wasn't found on GitHub (" ACG_REPO ").";
    return "GitHub answered with an error (" + std::to_string(http) + (msg.empty() ? "" : ": " + msg) + ").";
}

void read_manifest(Release& r, int index) {
    const fs::path f = dir() / ("manifest-" + std::to_string(index) + ".json");
    const int http = fetch(r.manifest_url, f, true);
    if (http != 0 && http != 200) return;
    json::Value m;
    if (!json::parse(read_file(f), &m) || !m.is_object()) return;
    r.version = m["launcher_version"].str();
    r.summary = m["summary"].str();
    r.commit = m["commit"].str();
    const json::Value& file = m["files"][kSelfName];
    r.launcher_url = file["url"].str();
    r.launcher_sha256 = file["sha256"].str();
    r.launcher_size = (uint64_t)file["size"].num();
    r.have_manifest = !r.version.empty() && !r.launcher_url.empty() && r.launcher_sha256.size() == 64;
    if (!r.have_manifest) return;
    r.installed = r.launcher_sha256 == g_self_sha;
    r.newer = compare_versions(r.version, ACG_LAUNCHER_VERSION) > 0;
    r.too_old = compare_versions(r.version, kFirstWithUpdates) < 0;
}

void do_check() {
    if (g_self_sha.empty() && !g_self.empty()) g_self_sha = sha256_file(g_self);
    const bool test = test_channel();
    const std::string url = !g_releases_url.empty() ? g_releases_url
                          : "https://api.github.com/repos/" ACG_REPO "/releases?per_page=40";
    const fs::path list = dir() / "releases.json";
    const int http = fetch(url, list, true);
    const std::string body = read_file(list);
    if (http != 0 && http != 200) throw Failed{ github_problem(http, body) };
    json::Value all;
    if (!json::parse(body, &all) || !all.is_array()) throw Failed{ "GitHub's list of releases couldn't be read." };

    std::vector<Release> found;
    for (const json::Value& r : all.items) {
        if (r["draft"].truthy()) continue;
        const std::string tag = r["tag_name"].str();
        const bool pre = r["prerelease"].truthy();
        const bool is_test = pre && tag.rfind("test-", 0) == 0;
        if (test ? !is_test : pre) continue;   /* RELEASES: real releases only; TEST: test-* only */
        Release rel;
        rel.tag = tag;
        rel.title = r["name"].str();
        rel.date = date_of(r["published_at"].str());
        rel.test = is_test;
        for (const json::Value& a : r["assets"].items)
            if (a["name"].str() == "manifest.json") rel.manifest_url = a["browser_download_url"].str();
        if (rel.manifest_url.empty()) continue;   /* 0.2.1 had none */
        found.push_back(rel);
        if (found.size() >= (test ? 6u : 3u)) break;   /* GitHub lists newest first */
    }
    for (size_t i = 0; i < found.size(); ++i) read_manifest(found[i], (int)i);
    found.erase(std::remove_if(found.begin(), found.end(), [](const Release& r) { return !r.have_manifest; }),
                found.end());

    int best = -1;
    if (!test)
        for (size_t i = 0; i < found.size(); ++i)
            if (found[i].newer && (best < 0 || compare_versions(found[i].version, found[best].version) > 0))
                best = (int)i;

    std::string msg;
    if (test) {
        msg = found.empty() ? "No test builds on GitHub." : "Choose a test build to install.";
    } else if (best >= 0) {
        msg = "Version " + found[best].version + " is ready to install" +
              (found[best].summary.empty() ? "." : ": " + found[best].summary);
    } else if (found.empty()) {
        msg = "There is no release on GitHub yet. You have test launcher " ACG_LAUNCHER_VERSION ".";
    } else {
        msg = "You have the newest version.";
    }
    char hm[16];
    std::time_t t = std::time(nullptr);
    std::strftime(hm, sizeof hm, "%H:%M", std::localtime(&t));
    std::lock_guard<std::mutex> lk(g_mu);
    g_snap.releases = found;
    g_snap.best = best;
    g_snap.checked = hm;
    g_snap.message = msg;
    g_snap.bad = false;
    g_snap.state = State::Checked;
}

void do_install(Release r) {
    if (g_self.empty()) throw Failed{ "The launcher couldn't find its own file, so it can't replace it." };
    if (!r.have_manifest) throw Failed{ "That release has no launcher for this system." };
    if (r.too_old)
        throw Failed{ "Launcher " + r.version + " is from before UPDATES, so it couldn't update back by itself. "
                      "To try it, download it by hand into a new folder." };
    const fs::path dl = dir() / "download";
    const fs::path part = dl / (std::string(kSelfName) + ".part");
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_snap.state = State::Downloading;
        g_snap.done = 0;
        g_snap.total = r.launcher_size;
        g_snap.target = r.tag;
        g_snap.message = "Downloading launcher " + r.version + " ...";
        g_snap.bad = false;
    }
    const int http = fetch(r.launcher_url, part, false, r.launcher_size);
    if (http != 0 && http != 200) throw Failed{ "The download failed (" + std::to_string(http) + ")." };
    set_msg("Checking the download ...", false);
    const std::string got = sha256_file(part);
    if (got != r.launcher_sha256 || (r.launcher_size && fsize(part) != r.launcher_size)) {
        std::error_code ec;
        fs::remove(part, ec);
        throw Failed{ "The download didn't match the release's checksum, so it wasn't used. Try again; if it "
                      "keeps happening, the release was changed after it was listed (check again)." };
    }
    plat::make_executable(part);

    /* Can the new one start here? It must say the version its manifest says. */
    set_msg("Checking that the new launcher starts ...", false);
    const fs::path vf = dl / "version.txt";
    std::error_code ec;
    fs::remove(vf, ec);
    {
        plat::Process p;
        std::string why;
        if (!p.start({ plat::u8str(part), "--version-file", plat::u8str(vf) }, {}, dl,
                     [](int, const std::string&) {}, &why))
            throw Failed{ "The new launcher couldn't be started here: " + why };
        int code = -1;
        for (int i = 0; i < 300 && !p.poll(&code); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!p.poll(&code)) { p.kill_tree(); p.wait(); }
    }
    std::string said = read_file(vf);
    while (!said.empty() && (said.back() == '\n' || said.back() == '\r')) said.pop_back();
    if (said != r.version)
        throw Failed{ "The new launcher didn't start properly here (it said \"" + said + "\", expected \"" +
                      r.version + "\"), so this one was kept." };

    /* Swap: this launcher to updates/previous/, the new one into its place. */
    set_msg("Replacing the launcher ...", false);
    const fs::path prev_dir = dir() / "previous";
    fs::create_directories(prev_dir, ec);
    const fs::path prev = prev_dir / g_self.filename();
    fs::remove(prev, ec);
    std::string err;
    if (!plat::rename_retry(g_self, prev, &err)) throw Failed{ "Couldn't move the old launcher aside: " + err };
    if (!plat::rename_retry(part, g_self, &err)) {
        std::string back;
        plat::rename_retry(prev, g_self, &back);
        throw Failed{ "Couldn't put the new launcher in place (the old one is back): " + err };
    }
    ini::save(dir() / "after-update.ini", { { "from", ACG_LAUNCHER_VERSION }, { "to", r.version }, { "tag", r.tag } });
    std::lock_guard<std::mutex> lk(g_mu);
    g_snap.state = State::Installed;
    g_snap.message = "Launcher " + r.version + " is installed. Starting it ...";
    g_snap.bad = false;
    g_installed_at = std::chrono::steady_clock::now();
    g_restart = true;
}

template <class F>
void run_worker(F f) {
    if (g_running) return;
    if (g_worker.joinable()) g_worker.join();
    g_running = true;
    g_worker = std::thread([f] {
        try {
            f();
        } catch (const Failed& e) {
            std::lock_guard<std::mutex> lk(g_mu);
            g_snap.state = State::Failed;
            g_snap.message = e.text;
            g_snap.bad = true;
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lk(g_mu);
            g_snap.state = State::Failed;
            g_snap.message = std::string("Something went wrong: ") + e.what();
            g_snap.bad = true;
        }
        g_running = false;
    });
}

} /* namespace */

void init(const std::vector<std::string>& args) {
    g_self = plat::self_exe();
    g_args = args;
    /* Leftovers of an earlier download. */
    std::error_code ec;
    fs::remove_all(dir() / "download", ec);
}

void set_releases_url(const std::string& url) { g_releases_url = url; }

void check() {
    {
        std::lock_guard<std::mutex> lk(g_mu);
        if (g_running) return;
        g_snap.state = State::Checking;
        g_snap.message = "Looking for updates ...";
        g_snap.bad = false;
    }
    run_worker([] { do_check(); });
}

void install(int index) {
    Release r;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        if (g_running || index < 0 || index >= (int)g_snap.releases.size()) return;
        r = g_snap.releases[index];
        g_snap.state = State::Downloading;
        g_snap.target = r.tag;
    }
    run_worker([r] { do_install(r); });
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lk(g_mu);
    return g_snap;
}

bool busy() { return g_running; }

bool take_restart() {
    if (g_running || !g_restart) return false;
    if (std::chrono::steady_clock::now() - g_installed_at < std::chrono::milliseconds(1500)) return false;
    if (g_worker.joinable()) g_worker.join();
    return true;
}

void shutdown() {
    g_stop = true;
    if (g_worker.joinable()) g_worker.join();
}

void restart() {
    if (g_worker.joinable()) g_worker.join();
    if (!plat::restart_self(g_self, g_args))
        std::fprintf(stderr, "Couldn't start the new launcher %s; start it again by hand.\n", plat::u8str(g_self).c_str());
}

std::string take_updated_from() {
    const fs::path f = dir() / "after-update.ini";
    const auto kv = ini::load(f);
    std::error_code ec;
    fs::remove(f, ec);
    auto it = kv.find("from");
    return it == kv.end() ? std::string() : it->second;
}

bool check_at_start() { return setting("check_updates", "1") != "0"; }
void set_check_at_start(bool on) { ini::set(ini_file(), "check_updates", on ? "1" : "0"); }
bool test_unlocked() { return setting("test_unlocked", "0") == "1"; }
void set_test_unlocked(bool on) {
    ini::set(ini_file(), "test_unlocked", on ? "1" : "");
    if (!on) set_test_channel(false);
}
bool test_channel() { return test_unlocked() && setting("channel", "") == "test"; }
void set_test_channel(bool on) {
    ini::set(ini_file(), "channel", on ? "test" : "");
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_running) return;
    g_snap.releases.clear();   /* the other channel's list means nothing here */
    g_snap.best = -1;
    g_snap.state = State::Idle;
    g_snap.message.clear();
}

int compare_versions(const std::string& a, const std::string& b) {
    size_t i = 0, j = 0;
    while (i < a.size() || j < b.size()) {
        unsigned long x = 0, y = 0;
        while (i < a.size() && a[i] >= '0' && a[i] <= '9') x = x * 10 + (unsigned long)(a[i++] - '0');
        while (j < b.size() && b[j] >= '0' && b[j] <= '9') y = y * 10 + (unsigned long)(b[j++] - '0');
        if (x != y) return x < y ? -1 : 1;
        while (i < a.size() && !(a[i] >= '0' && a[i] <= '9')) ++i;
        while (j < b.size() && !(b[j] >= '0' && b[j] <= '9')) ++j;
    }
    return 0;
}

} /* namespace update */

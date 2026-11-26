/* Armored Core Gen 1: update check and self-update. See moa_update.h.
 *
 * Network access goes through the system's own curl (Windows 10+ ships one in
 * System32), run without a window; the program carries no TLS code.
 *
 * Release channel: the release list comes from the GitHub API, the files from
 * the release page, and a download is accepted only when its SHA-256 matches
 * the release's SHA256SUMS.txt.
 *
 * Dev channel (not in public builds): the newest successful "Release builds"
 * run on main, read with the player's own GitHub token (fine-grained, this repo
 * only, read-only Actions + Contents). The token is kept outside the game
 * folder, handed to curl through a header file that is deleted straight after
 * (never on a command line other programs can read), and never sent anywhere
 * but api.github.com: the artifact's storage link is fetched first without
 * following it, then downloaded without the token.
 *
 * Installing (Windows): the zip is unpacked into Update/stage in the game's
 * folder and its files are moved over the old ones; a file in use (the running
 * program) is renamed, not deleted, and swept away on the next check. The
 * player's settings, saves, memory cards, mods and disc images are skipped.
 *
 * Folder layouts: a Windows release is laid out for the player, with the game
 * in RecompData/ under the folder the player sees (Armored Core Gen 1.exe,
 * Memory Card/, Mods/ beside it; scripts/package_prebuilt_release.sh). When
 * this program runs from a folder called RecompData, the folder above it is
 * the one updated, from a zip with that same layout. Older flat builds update
 * from flat zips. A zip whose layout doesn't match the install is refused
 * with a note to unzip that one by hand.
 */
#include "moa_update.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

extern "C" {
#include "psx_sha256.h"
}

#ifdef _WIN32
#  include <windows.h>
#else
#  include <spawn.h>
#  include <sys/stat.h>
#  include <sys/wait.h>
#  include <fcntl.h>
#  include <unistd.h>
extern char** environ;
#endif

#ifndef MOA_VERSION
#  define MOA_VERSION "dev"
#endif
#ifndef MOA_DEV_UPDATES
#  define MOA_DEV_UPDATES 0
#endif

namespace fs = std::filesystem;

namespace moa_update {
namespace {

const char* const kRepo = "Hatdagger/Armored-Core-Gen-1";
const char* const kWorkflow = "release.yml";
const char* const kSums = "SHA256SUMS.txt";
const char* const kDevStamp = "dev-build.txt";   /* beside the game: the Actions run installed */
#ifdef _WIN32
const char* const kPlatform = "windows-x64";
#else
const char* const kPlatform = "linux-x64";
#endif

struct Asset { std::string name, url; uint64_t size = 0; };

std::mutex g_mu;
Snapshot   g_snap;
Asset      g_asset, g_sums;       /* what the last check chose */
std::string g_dev_run;            /* run id the last dev check chose */
std::string g_done_label;         /* what the last update installed */
std::atomic<bool> g_busy{false};

void set(State s, const std::string& msg) {
    std::lock_guard<std::mutex> l(g_mu);
    g_snap.state = s;
    g_snap.message = msg;
}

/* ---- versions ------------------------------------------------------------- */

/* Any number of parts: this project's fixes add parts (0.3.2.1.1). */
std::vector<int> parse_version(const std::string& s) {
    std::vector<int> v;
    size_t i = 0;
    while (i < s.size() && !std::isdigit((unsigned char)s[i])) ++i;
    while (i < s.size() && std::isdigit((unsigned char)s[i])) {
        v.push_back(std::atoi(s.c_str() + i));
        while (i < s.size() && std::isdigit((unsigned char)s[i])) ++i;
        if (i < s.size() && s[i] == '.') ++i; else break;
    }
    return v;
}

int compare(const std::string& a, const std::string& b) {
    std::vector<int> x = parse_version(a), y = parse_version(b);
    const size_t n = std::max(x.size(), y.size());
    x.resize(n, 0); y.resize(n, 0);
    for (size_t i = 0; i < n; ++i)
        if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    return 0;
}

/* ---- small JSON reading ----------------------------------------------------
 * Only a few fields are needed, so the reply is flattened (whitespace outside
 * strings removed) and searched for "key":value. */

std::string flatten(const std::string& j) {
    std::string out;
    out.reserve(j.size());
    bool in_str = false, esc = false;
    for (char c : j) {
        if (in_str) {
            out.push_back(c);
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') in_str = false;
        } else if (c == '"') {
            in_str = true;
            out.push_back(c);
        } else if (!std::isspace((unsigned char)c)) {
            out.push_back(c);
        }
    }
    return out;
}

/* Position just after "key": at or after `from`, npos when absent before `to`. */
size_t key_at(const std::string& j, const std::string& key, size_t from, size_t to = std::string::npos) {
    const std::string k = "\"" + key + "\":";
    const size_t p = j.find(k, from);
    if (p == std::string::npos || (to != std::string::npos && p >= to)) return std::string::npos;
    return p + k.size();
}

std::string str_at(const std::string& j, size_t v) {
    if (v == std::string::npos || v >= j.size() || j[v] != '"') return "";
    const size_t e = j.find('"', v + 1);
    return e == std::string::npos ? "" : j.substr(v + 1, e - v - 1);
}

std::string num_at(const std::string& j, size_t v) {
    std::string out;
    while (v != std::string::npos && v < j.size() && std::isdigit((unsigned char)j[v])) out.push_back(j[v++]);
    return out;
}

/* ---- running curl --------------------------------------------------------- */

bool safe_url(const std::string& u, const char* prefix) {
    if (u.compare(0, std::strlen(prefix), prefix) != 0) return false;
    for (char c : u)
        if (!(std::isalnum((unsigned char)c) || std::strchr(".-_/:%+?=&~", c))) return false;
    return true;
}

std::string release_prefix() {
    return std::string("https://github.com/") + kRepo + "/releases/download/";
}

/* Returns curl's exit code, -1 when curl could not be started. */
int run_curl(const std::vector<std::string>& args) {
#ifdef _WIN32
    wchar_t sys[MAX_PATH] = {0};
    GetSystemDirectoryW(sys, MAX_PATH);
    std::wstring exe = std::wstring(sys) + L"\\curl.exe";
    std::wstring cmd = L"\"" + exe + L"\"";
    for (const std::string& a : args) {
        cmd += L" \"";
        cmd += fs::u8path(a).wstring();
        cmd += L"\"";
    }
    STARTUPINFOW si; PROCESS_INFORMATION pi;
    std::memset(&si, 0, sizeof si); si.cb = sizeof si;
    std::memset(&pi, 0, sizeof pi);
    std::vector<wchar_t> buf(cmd.begin(), cmd.end()); buf.push_back(0);
    if (!CreateProcessW(exe.c_str(), buf.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return (int)code;
#else
    std::vector<std::string> av = { "curl" };
    av.insert(av.end(), args.begin(), args.end());
    std::vector<char*> argv;
    for (std::string& a : av) argv.push_back(a.data());
    argv.push_back(nullptr);
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, 1, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);
    pid_t pid = 0;
    const int rc = posix_spawnp(&pid, "curl", &fa, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&fa);
    if (rc != 0) return -1;
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}

std::vector<std::string> curl_args(const std::string& url, const fs::path& out, bool api,
                                   const fs::path& auth_headers = fs::path(), bool follow = true) {
    std::vector<std::string> a = { follow ? "-fsSL" : "-fsS", "--connect-timeout", "20", "--max-time", "1800",
                                   "--retry", "2", "-A", "Armored-Core-Gen-1-Updater" };
#ifdef _WIN32
    a.push_back("--ssl-no-revoke");
#endif
    if (api) {
        a.push_back("-H"); a.push_back("Accept: application/vnd.github+json");
        a.push_back("-H"); a.push_back("X-GitHub-Api-Version: 2022-11-28");
    }
    if (!auth_headers.empty()) { a.push_back("-H"); a.push_back("@" + auth_headers.u8string()); }
    a.push_back("-o"); a.push_back(out.u8string());
    a.push_back(url);
    return a;
}

std::string curl_failure(int rc) {
    if (rc == -1) return "CURL IS NOT INSTALLED ON THIS SYSTEM";
    if (rc == 6 || rc == 7 || rc == 28 || rc == 35 || rc == 56) return "COULD NOT REACH GITHUB. CHECK THE CONNECTION";
    if (rc == 22) return "GITHUB REFUSED THE REQUEST (RATE LIMIT, TOKEN OR NO SUCH FILE)";
    return "THE DOWNLOAD FAILED (CURL ERROR " + std::to_string(rc) + ")";
}

std::string read_all(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

fs::path scratch_dir() {
    std::error_code ec;
    fs::path d = fs::temp_directory_path(ec) / "acg-update";
    fs::create_directories(d, ec);
    return d;
}

/* ---- the release list ------------------------------------------------------ */

struct Release { std::string tag; bool draft = false; std::vector<Asset> assets; };

std::vector<Release> parse_releases(const std::string& raw) {
    const std::string j = flatten(raw);
    std::vector<Release> out;
    std::vector<size_t> tags;
    for (size_t p = key_at(j, "tag_name", 0); p != std::string::npos; p = key_at(j, "tag_name", p)) tags.push_back(p);
    for (size_t i = 0; i < tags.size(); ++i) {
        const size_t end = i + 1 < tags.size() ? tags[i + 1] : j.size();
        Release r;
        r.tag = str_at(j, tags[i]);
        const size_t d = key_at(j, "draft", tags[i], end);
        r.draft = d != std::string::npos && j.compare(d, 4, "true") == 0;
        size_t prev = tags[i];
        for (size_t p = key_at(j, "browser_download_url", tags[i], end); p != std::string::npos;
             p = key_at(j, "browser_download_url", p, end)) {
            Asset a;
            a.url = str_at(j, p);
            a.name = a.url.substr(a.url.rfind('/') + 1);
            size_t sp = std::string::npos;   /* the asset's "size", the last one before its URL */
            for (size_t q = key_at(j, "size", prev, p); q != std::string::npos; q = key_at(j, "size", q, p)) sp = q;
            if (sp != std::string::npos) a.size = std::strtoull(num_at(j, sp).c_str(), nullptr, 10);
            prev = p;
            r.assets.push_back(a);
        }
        out.push_back(r);
    }
    return out;
}

/* ---- where this build lives ------------------------------------------------ */

fs::path exe_path() {
#ifdef _WIN32
    wchar_t buf[32768];
    const DWORD n = GetModuleFileNameW(nullptr, buf, 32768);
    if (!n || n >= 32768) return {};
    return fs::path(buf);
#else
    std::error_code ec;
    return fs::read_symlink("/proc/self/exe", ec);
#endif
}

/* The folder this program is in (the game's own files). */
const char* const kDataDir = "RecompData";
/* The Japanese disc's program: its own folder inside the game folder, with its
 * own game.toml and player files (CMakeLists.txt ACG_REGION). */
const char* const kJapanDir = "Japan";

/* The game folder: where the program is, or one level up for the Japanese
 * program in RecompData/Japan/ (or <game>/Japan/ in a flat build). */
fs::path game_root() {
    const fs::path dir = exe_path().parent_path();
    if (dir.filename() == kJapanDir && dir.has_parent_path() &&
        fs::exists(dir.parent_path() / "regional-launch.toml"))
        return dir.parent_path();
    return dir;
}

/* True when the game sits in RecompData/ under the player's folder. */
bool nested_install() {
    const fs::path data = game_root();
    return !data.empty() && data.filename() == kDataDir && data.has_parent_path();
}

/* The folder an update applies to: the player's folder above RecompData/, or
 * the game's own folder in a flat build. */
fs::path install_root() { return nested_install() ? game_root().parent_path() : game_root(); }

std::string installed_dev_run() {
    std::string s = read_all(game_root() / kDevStamp);
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
    return s;
}

/* ---- checking a download --------------------------------------------------- */

std::string sha256_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return "";
    psx_sha256_ctx c;
    psx_sha256_init(&c);
    std::vector<char> buf(1 << 20);
    while (f) {
        f.read(buf.data(), (std::streamsize)buf.size());
        if (f.gcount() > 0) psx_sha256_update(&c, (const uint8_t*)buf.data(), (size_t)f.gcount());
    }
    uint8_t h[32];
    psx_sha256_final(&c, h);
    char hex[65];
    for (int i = 0; i < 32; ++i) std::snprintf(hex + 2 * i, 3, "%02x", h[i]);
    return hex;
}

/* The hash the release lists for a file name; empty when it is not listed. */
std::string listed_hash(const std::string& sums, const std::string& name) {
    size_t p = 0;
    while (p < sums.size()) {
        size_t q = sums.find('\n', p);
        if (q == std::string::npos) q = sums.size();
        std::string line = sums.substr(p, q - p);
        p = q + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        const size_t sp = line.find_first_of(" \t");
        if (sp != 64) continue;
        size_t n = sp;
        while (n < line.size() && (line[n] == ' ' || line[n] == '\t' || line[n] == '*')) ++n;
        std::string listed = line.substr(n);
        listed = listed.substr(listed.find_last_of("/\\") + 1);
        if (listed == name) {
            std::string h = line.substr(0, 64);
            for (char& c : h) c = (char)std::tolower((unsigned char)c);
            return h;
        }
    }
    return "";
}

bool same_content(const fs::path& a, const fs::path& b) {
    std::error_code ec;
    if (fs::file_size(a, ec) != fs::file_size(b, ec)) return false;
    std::ifstream fa(a, std::ios::binary), fb(b, std::ios::binary);
    if (!fa || !fb) return false;
    std::vector<char> x(1 << 20), y(1 << 20);
    while (fa && fb) {
        fa.read(x.data(), (std::streamsize)x.size());
        fb.read(y.data(), (std::streamsize)y.size());
        if (fa.gcount() != fb.gcount() || std::memcmp(x.data(), y.data(), (size_t)fa.gcount()) != 0) return false;
    }
    return true;
}

#ifdef _WIN32
/* ---- Windows: unpacking and moving the new files over the old --------------- */

const char* const kOldSuffix = ".acg-old";

void sweep_old(const fs::path& root) {
    std::error_code ec;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
        const fs::path& p = it->path();
        if (p.extension() == kOldSuffix) { std::error_code r; fs::remove(p, r); }
    }
}

/* Windows 10+ tar.exe (bsdtar) reads zips. */
bool unzip(const fs::path& zip, const fs::path& dest) {
    std::error_code ec;
    fs::remove_all(dest, ec);
    fs::create_directories(dest, ec);
    wchar_t sys[MAX_PATH] = {0};
    GetSystemDirectoryW(sys, MAX_PATH);
    std::wstring tar = std::wstring(sys) + L"\\tar.exe";
    std::wstring cmd = L"\"" + tar + L"\" -xf \"" + zip.wstring() + L"\" -C \"" + dest.wstring() + L"\"";
    STARTUPINFOW si; PROCESS_INFORMATION pi;
    std::memset(&si, 0, sizeof si); si.cb = sizeof si; std::memset(&pi, 0, sizeof pi);
    std::vector<wchar_t> buf(cmd.begin(), cmd.end()); buf.push_back(0);
    DWORD code = 1;
    if (CreateProcessW(tar.c_str(), buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
    return code == 0;
}

/* Files that belong to the player or to this machine, relative to the game
 * folder. Names follow the framework's own list of personal files
 * (psxrecomp/tools/package_title_release.py, PERSONAL_NAMES). */
bool protected_path(const std::string& rel) {
    static const std::set<std::string> user = {
        "settings.toml", "disc.cfg", "bios.cfg", "rom.cfg", "keybinds.ini", "input.ini",
        "launcher.ini", "launcher-window.ini", "config.ini", "overlay_captures.json",
        "diagnostics-location.txt", kDevStamp,
        /* the framework's note of which disc version was last used */
        "europe-selection.toml" };
    if (user.count(rel)) return true;
    /* The Japanese program's folder holds the same kinds of player files. */
    const std::string jp = std::string(kJapanDir) + "/";
    if (rel.size() > jp.size() && rel.compare(0, jp.size(), jp) == 0)
        return protected_path(rel.substr(jp.size()));
    /* mods/bundled holds the mods each release ships: replace and add them. */
    if (rel.compare(0, 13, "mods/bundled/") == 0) return false;
    for (const char* d : { "saves/", "mods/", "netplay/", "cache/", "diagnostics/", "Update/",
                           "disc/", "discs/", "Disc/", "Discs/", "iso/", "roms/", "games/" })
        if (rel.compare(0, std::strlen(d), d) == 0) return true;
    std::string low = rel;
    for (char& c : low) c = (char)std::tolower((unsigned char)c);
    for (const char* ext : { ".mcd", ".mcr", ".cue", ".chd", ".iso", ".img", ".psxmod" })
        if (low.size() > std::strlen(ext) && low.compare(low.size() - std::strlen(ext), std::strlen(ext), ext) == 0)
            return true;
    /* .bin: disc images; the release's own BIOS lives in bios/ and is updated. */
    if (low.size() > 4 && low.compare(low.size() - 4, 4, ".bin") == 0 && low.compare(0, 5, "bios/") != 0)
        return true;
    return false;
}

/* The same, for a path relative to the player's folder in a RecompData/
 * install: the game's files are judged as above; at the top, Memory Card/ and
 * Mods/ are the player's, and so are disc images and their folders. */
bool protected_install_path(const std::string& rel, bool nested) {
    if (!nested) return protected_path(rel);
    const std::string data = std::string(kDataDir) + "/";
    if (rel.compare(0, data.size(), data) == 0) return protected_path(rel.substr(data.size()));
    for (const char* d : { "Memory Card/", "Mods/" })
        if (rel.compare(0, std::strlen(d), d) == 0) return true;
    return protected_path(rel);
}

/* Unpacks a release zip and moves its files over the game's. */
bool apply_zip(const fs::path& zip, const fs::path& root, std::string& err) {
    std::error_code ec;
    sweep_old(root);
    const fs::path stage = root / "Update" / "stage";
    const fs::path exe_name = fs::u8path(exe_path().filename().u8string());
    if (!unzip(zip, stage)) {
        fs::remove_all(root / "Update", ec);
        err = "THE UPDATE COULD NOT BE UNPACKED";
        return false;
    }
    /* Which layout the zip has, and whether it is the one installed here. */
    const fs::path data = stage / kDataDir;
    const bool zip_nested = fs::exists(data / exe_name, ec) && fs::exists(data / "game.toml", ec);
    const bool zip_flat = fs::exists(stage / exe_name, ec) && fs::exists(stage / "game.toml", ec);
    const bool nested = nested_install();
    if (!zip_nested && !zip_flat) {
        fs::remove_all(root / "Update", ec);
        err = "THE UPDATE COULD NOT BE UNPACKED";
        return false;
    }
    if (zip_nested != nested) {
        fs::remove_all(root / "Update", ec);
        err = zip_nested
            ? "THIS UPDATE CHANGES THE FOLDER LAYOUT. DOWNLOAD IT ONCE BY HAND AND UNZIP IT INTO A NEW FOLDER"
            : "THIS UPDATE USES THE OLD FOLDER LAYOUT AND CANNOT BE INSTALLED HERE";
        return false;
    }

    struct Move { fs::path dest, old; bool had_old; };
    std::vector<Move> done;
    bool ok = true;
    for (fs::recursive_directory_iterator it(stage, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        const std::string rel = it->path().lexically_relative(stage).generic_u8string();
        if (protected_install_path(rel, nested)) continue;
        const fs::path dest = root / fs::u8path(rel);
        if (fs::exists(dest, ec) && same_content(it->path(), dest)) continue;
        fs::create_directories(dest.parent_path(), ec);
        Move mv{ dest, fs::path(dest.u8string() + kOldSuffix), false };
        if (fs::exists(dest, ec)) {
            fs::remove(mv.old, ec);
            fs::rename(dest, mv.old, ec);
            if (ec) { ok = false; break; }
            mv.had_old = true;
        }
        fs::rename(it->path(), dest, ec);
        if (ec) {
            if (mv.had_old) fs::rename(mv.old, dest, ec);
            ok = false;
            break;
        }
        done.push_back(mv);
    }
    if (!ok) {   /* put every file back as it was */
        for (auto i = done.rbegin(); i != done.rend(); ++i) {
            fs::remove(i->dest, ec);
            if (i->had_old) fs::rename(i->old, i->dest, ec);
        }
        fs::remove_all(root / "Update", ec);
        err = "A FILE WAS IN USE. NOTHING WAS CHANGED";
        return false;
    }
    fs::remove_all(root / "Update", ec);
    return true;
}
#endif

/* Downloads url to file, keeping the progress figure up to date. */
int download(const std::string& url, const fs::path& file, uint64_t total) {
    std::error_code ec;
    fs::remove(file, ec);
    { std::lock_guard<std::mutex> l(g_mu); g_snap.state = State::Downloading; g_snap.message = "DOWNLOADING"; g_snap.done = 0; g_snap.total = total; }
    std::atomic<bool> finished{false};
    std::thread meter([&] {
        while (!finished) {
            std::error_code e;
            const uint64_t n = fs::exists(file, e) ? (uint64_t)fs::file_size(file, e) : 0;
            { std::lock_guard<std::mutex> l(g_mu); if (g_snap.state == State::Downloading) g_snap.done = n; }
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    });
    const int rc = run_curl(curl_args(url, file, false));
    finished = true;
    meter.join();
    if (rc != 0) fs::remove(file, ec);
    return rc;
}

/* ---- the token (dev channel) ------------------------------------------------ */

fs::path token_file() {
    std::error_code ec;
    fs::path base;
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (appdata && appdata[0]) base = fs::u8path(appdata);
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");
    if (xdg && xdg[0]) base = fs::u8path(xdg);
    else if (home && home[0]) base = fs::u8path(home) / ".config";
#endif
    if (base.empty()) return {};
    return base / "Armored-Core-Gen-1" / "github-token.txt";
}

std::string read_token() {
    const fs::path p = token_file();
    if (p.empty()) return "";
    std::string t = read_all(p);
    while (!t.empty() && std::isspace((unsigned char)t.back())) t.pop_back();
    return t;
}

bool token_chars_ok(const std::string& t) {
    if (t.size() < 20 || t.size() > 255) return false;
    for (char c : t) if (!(std::isalnum((unsigned char)c) || c == '_')) return false;
    return true;
}

/* A header file for curl's -H @file. Deleted by the caller straight after. */
fs::path write_auth_headers() {
    const std::string tok = read_token();
    if (tok.empty()) return {};
    const fs::path p = scratch_dir() / "auth-headers.txt";
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << "Authorization: Bearer " << tok << "\n";
    f.close();
#ifndef _WIN32
    chmod(p.c_str(), 0600);
#endif
    return p;
}

/* GET an API URL with the token; the reply goes to out. */
int api_get(const std::string& url, const fs::path& out) {
    const fs::path h = write_auth_headers();
    if (h.empty()) return 22;
    const int rc = run_curl(curl_args(url, out, true, h));
    std::error_code ec;
    fs::remove(h, ec);
    return rc;
}

/* ---- the jobs ------------------------------------------------------------------ */

void do_check() {
    const fs::path out = scratch_dir() / "releases.json";
    std::error_code ec;
    fs::remove(out, ec);
    const std::string api = std::string("https://api.github.com/repos/") + kRepo + "/releases?per_page=15";
    const int rc = run_curl(curl_args(api, out, true));
    if (rc != 0) { set(State::Failed, curl_failure(rc)); return; }
    const std::vector<Release> rels = parse_releases(read_all(out));
    fs::remove(out, ec);

    const std::string suffix = std::string("-") + kPlatform + ".zip";
    const Release* best = nullptr;
    Asset pick, sums;
    for (const Release& r : rels) {
        if (r.draft || r.tag.empty() || r.tag[0] != 'v') continue;
        Asset a, s;
        for (const Asset& x : r.assets) {
            if (x.name.size() > suffix.size() &&
                x.name.compare(x.name.size() - suffix.size(), std::string::npos, suffix) == 0) a = x;
            if (x.name == kSums) s = x;
        }
        if (a.url.empty() || s.url.empty()) continue;
        if (!best || compare(r.tag, best->tag) > 0) { best = &r; pick = a; sums = s; }
    }
    if (!best) { set(State::Failed, "NO RELEASE FOR THIS SYSTEM WAS FOUND"); return; }
    const std::string latest = best->tag.substr(1);
    std::lock_guard<std::mutex> l(g_mu);
    g_snap.channel = Channel::Release;
    g_snap.latest = latest;
    g_asset = pick; g_sums = sums;
    g_snap.total = pick.size;
    const std::string cur = MOA_VERSION;
    if (cur == "dev" || compare(latest, cur) > 0 || !installed_dev_run().empty()) {
        g_snap.state = State::Available;
        g_snap.message = "VERSION " + latest + " IS AVAILABLE";
    } else {
        g_snap.state = State::UpToDate;
        g_snap.message = "YOU HAVE THE LATEST VERSION";
    }
}

void finish_apply(const fs::path& zip, const fs::path& dir, const std::string& label,
                  const std::string& dev_run) {
#ifdef _WIN32
    set(State::Applying, "INSTALLING");
    std::string err;
    const bool ok = apply_zip(zip, install_root(), err);
    std::error_code r;
    fs::remove_all(dir, r);
    if (!ok) { set(State::Failed, err); return; }
    /* Remember which dev build is installed; a release clears the note. The
     * note sits beside the program, in RecompData/ when there is one. */
    const fs::path stamp = game_root() / kDevStamp;
    if (dev_run.empty()) fs::remove(stamp, r);
    else { std::ofstream f(stamp, std::ios::trunc); f << dev_run << "\n"; }
    { std::lock_guard<std::mutex> l(g_mu); g_done_label = label; }
    set(State::Done, "UPDATED TO " + label + ". RESTARTING");
#else
    (void)zip; (void)dir; (void)label; (void)dev_run;
    set(State::Failed, "UPDATING IS ONLY AVAILABLE ON WINDOWS FOR NOW");
#endif
}

void do_apply_release() {
    Asset asset, sums;
    std::string latest;
    { std::lock_guard<std::mutex> l(g_mu); asset = g_asset; sums = g_sums; latest = g_snap.latest; }
    const std::string prefix = release_prefix();
    if (!safe_url(asset.url, prefix.c_str()) || !safe_url(sums.url, prefix.c_str())) {
        set(State::Failed, "THE RELEASE POINTS SOMEWHERE UNEXPECTED");
        return;
    }
    std::error_code ec;
    const fs::path dir = install_root() / "Update";
    fs::create_directories(dir, ec);
    const fs::path sumfile = dir / kSums;
    fs::remove(sumfile, ec);
    int rc = run_curl(curl_args(sums.url, sumfile, false));
    if (rc != 0) { fs::remove_all(dir, ec); set(State::Failed, curl_failure(rc)); return; }
    const std::string want = listed_hash(read_all(sumfile), asset.name);
    fs::remove(sumfile, ec);
    if (want.empty()) { fs::remove_all(dir, ec); set(State::Failed, "THE RELEASE HAS NO CHECKSUM FOR THIS FILE"); return; }

    const fs::path file = dir / asset.name;
    rc = download(asset.url, file, asset.size);
    if (rc != 0) { fs::remove_all(dir, ec); set(State::Failed, curl_failure(rc)); return; }
    set(State::Applying, "CHECKING THE DOWNLOAD");
    if (sha256_file(file) != want) {
        fs::remove_all(dir, ec);
        set(State::Failed, "THE DOWNLOAD DID NOT MATCH ITS CHECKSUM. NOTHING WAS CHANGED");
        return;
    }
    finish_apply(file, dir, latest, "");
}

#if MOA_DEV_UPDATES
std::string api_base() { return std::string("https://api.github.com/repos/") + kRepo; }

void do_check_dev() {
    std::error_code ec;
    const fs::path out = scratch_dir() / "runs.json";
    fs::remove(out, ec);
    int rc = api_get(api_base() + "/actions/workflows/" + kWorkflow +
                     "/runs?branch=main&status=success&per_page=1", out);
    if (rc != 0) { fs::remove(out, ec); set(State::Failed, rc == 22 ? "GITHUB REFUSED THE TOKEN, OR IT CANNOT SEE ACTIONS" : curl_failure(rc)); return; }
    const std::string runs = flatten(read_all(out));
    fs::remove(out, ec);

    /* The newest successful run of main: "workflow_runs":[{"id":N,... and
     * the first run_number / head_sha after it are that run's own. */
    const size_t list = key_at(runs, "workflow_runs", 0);
    if (list == std::string::npos) { set(State::Failed, "GITHUB SENT AN UNEXPECTED REPLY"); return; }
    if (runs.compare(list, 7, "[{\"id\":") != 0) { set(State::Failed, "NO DEV BUILD WAS FOUND"); return; }
    const std::string want = std::string("acg-") + kPlatform;
    {
        const size_t s = list + 1;
        const std::string run_id = num_at(runs, s + 6);
        const std::string number = num_at(runs, key_at(runs, "run_number", s));
        const std::string sha = str_at(runs, key_at(runs, "head_sha", s));
        if (run_id.empty()) { set(State::Failed, "GITHUB SENT AN UNEXPECTED REPLY"); return; }

        const fs::path arts = scratch_dir() / "artifacts.json";
        fs::remove(arts, ec);
        rc = api_get(api_base() + "/actions/runs/" + run_id + "/artifacts", arts);
        if (rc != 0) { fs::remove(arts, ec); set(State::Failed, curl_failure(rc)); return; }
        const std::string a = flatten(read_all(arts));
        fs::remove(arts, ec);
        const size_t name = a.find("\"name\":\"" + want + "\"");
        const size_t obj = name == std::string::npos ? name : a.rfind("{\"id\":", name);
        if (obj == std::string::npos) { set(State::Failed, "THE NEWEST DEV BUILD HAS NOTHING FOR THIS SYSTEM"); return; }
        const size_t obj_end = a.find("\"workflow_run\":", name);
        const size_t exp = key_at(a, "expired", obj, obj_end);
        if (exp != std::string::npos && a.compare(exp, 4, "true") == 0) { set(State::Failed, "THE NEWEST DEV BUILD HAS EXPIRED ON GITHUB"); return; }
        const std::string art_id = num_at(a, obj + 6);
        const std::string size = num_at(a, key_at(a, "size_in_bytes", obj, obj_end));
        if (art_id.empty()) { set(State::Failed, "GITHUB SENT AN UNEXPECTED REPLY"); return; }

        const std::string label = "BUILD #" + number + " (" + sha.substr(0, 7) + ")";
        std::lock_guard<std::mutex> l(g_mu);
        g_snap.channel = Channel::Dev;
        g_snap.latest = label;
        g_snap.total = std::strtoull(size.c_str(), nullptr, 10);
        g_asset = Asset{ want + ".zip", art_id, g_snap.total };
        g_dev_run = run_id;
        if (installed_dev_run() == run_id) {
            g_snap.state = State::UpToDate;
            g_snap.message = "YOU HAVE THE NEWEST DEV BUILD";
        } else {
            g_snap.state = State::Available;
            g_snap.message = label + " IS AVAILABLE";
        }
    }
}

void do_apply_dev() {
    Asset asset;
    std::string label, run_id;
    { std::lock_guard<std::mutex> l(g_mu); asset = g_asset; label = g_snap.latest; run_id = g_dev_run; }
    std::error_code ec;
    const fs::path dir = install_root() / "Update";
    fs::create_directories(dir, ec);

    /* The API answers with a redirect to short-lived storage. Ask without
     * following it, so the token goes to api.github.com only. */
    const fs::path head = dir / "redirect.txt";
    const fs::path h = write_auth_headers();
    if (h.empty()) { set(State::Failed, "NO GITHUB TOKEN IS SET"); return; }
    std::vector<std::string> args = curl_args(api_base() + "/actions/artifacts/" + asset.url + "/zip",
#ifdef _WIN32
                                              fs::path("NUL"),
#else
                                              fs::path("/dev/null"),
#endif
                                              true, h, false);
    args.insert(args.begin(), { "-D", head.u8string() });
    int rc = run_curl(args);
    fs::remove(h, ec);
    std::string location;
    {
        const std::string hdr = read_all(head);
        fs::remove(head, ec);
        size_t p = 0;
        while (p < hdr.size()) {
            size_t q = hdr.find('\n', p);
            if (q == std::string::npos) q = hdr.size();
            std::string line = hdr.substr(p, q - p);
            p = q + 1;
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            std::string low = line.substr(0, 9);
            for (char& c : low) c = (char)std::tolower((unsigned char)c);
            if (low == "location:") { location = line.substr(9); break; }
        }
        while (!location.empty() && location[0] == ' ') location.erase(0, 1);
    }
    if (location.empty() || location.compare(0, 8, "https://") != 0) {
        fs::remove_all(dir, ec);
        set(State::Failed, rc != 0 ? curl_failure(rc) : "GITHUB DID NOT HAND OVER THE BUILD");
        return;
    }

    const fs::path outer = dir / "artifact.zip";
    rc = download(location, outer, asset.size);
    if (rc != 0) { fs::remove_all(dir, ec); set(State::Failed, curl_failure(rc)); return; }
#ifdef _WIN32
    /* An Actions artifact is a zip around the game zip. */
    set(State::Applying, "UNPACKING");
    const fs::path inner_dir = dir / "artifact";
    fs::path inner;
    if (unzip(outer, inner_dir))
        for (fs::directory_iterator it(inner_dir, ec), end; !ec && it != end; it.increment(ec))
            if (it->path().extension() == ".zip") { inner = it->path(); break; }
    if (inner.empty()) { fs::remove_all(dir, ec); set(State::Failed, "THE BUILD COULD NOT BE UNPACKED"); return; }
    finish_apply(inner, dir, label, run_id);
#else
    finish_apply(outer, dir, label, run_id);
#endif
}
#endif /* MOA_DEV_UPDATES */

void do_apply() {
    std::string why;
    if (!supported(&why)) { set(State::Failed, why); return; }
    Channel ch;
    { std::lock_guard<std::mutex> l(g_mu); ch = g_snap.channel; }
#if MOA_DEV_UPDATES
    if (ch == Channel::Dev) { do_apply_dev(); return; }
#else
    (void)ch;
#endif
    do_apply_release();
}

void start(void (*job)()) {
    bool expected = false;
    if (!g_busy.compare_exchange_strong(expected, true)) return;
    std::thread([job] { job(); g_busy = false; }).detach();
}

} /* namespace */

const char* current_version() { return MOA_VERSION; }

bool supported(std::string* why) {
    const char* reason = nullptr;
#ifdef _WIN32
    std::error_code ec;
    if (game_root().empty() || !fs::exists(game_root() / "game.toml", ec))
        reason = "UPDATING NEEDS THE GAME'S OWN FOLDER (GAME.TOML BESIDE THE PROGRAM)";
#else
    reason = "UPDATING IS ONLY AVAILABLE ON WINDOWS FOR NOW";
#endif
    if (why) *why = reason ? reason : "";
    return reason == nullptr;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> l(g_mu);
    Snapshot s = g_snap;
    s.current = MOA_VERSION;
    s.current_dev = installed_dev_run();
    return s;
}

void check() {
    {
        std::lock_guard<std::mutex> l(g_mu);
        if (g_busy) return;
        g_snap.state = State::Checking;
        g_snap.channel = Channel::Release;
        g_snap.message = "LOOKING FOR UPDATES";
    }
#ifdef _WIN32
    sweep_old(install_root());
#endif
    start(do_check);
}

void download_and_apply() {
    {
        std::lock_guard<std::mutex> l(g_mu);
        if (g_busy || g_snap.state != State::Available) return;
        g_snap.state = State::Downloading;      /* at once, so a second press finds it busy */
        g_snap.message = "DOWNLOADING";
        g_snap.done = 0;
    }
    start(do_apply);
}

/* Starts the game again after an update, through the starter the player
 * opens ("Armored Core Gen 1.exe" above RecompData/), so it comes back with
 * the same memory card folder and picks the last disc (US or Japanese) as a
 * normal start does. The starter is told this program's id and waits for it
 * to close first, so the two never run at once. The caller then closes the
 * launcher. Without a starter (a flat build) the player restarts by hand. */
bool restart() {
#ifdef _WIN32
    std::string label;
    {
        std::lock_guard<std::mutex> l(g_mu);
        if (g_snap.state != State::Done) return false;
        label = g_done_label;
    }
    std::error_code ec;
    const fs::path root = install_root();
    const fs::path starter = root / fs::path(L"Armored Core Gen 1.exe");
    bool ok = nested_install() && fs::exists(starter, ec);
    if (ok) {
        const std::wstring cmd = L"\"" + starter.wstring() + L"\" --after-update " +
                                 std::to_wstring((unsigned long)GetCurrentProcessId());
        std::vector<wchar_t> buf(cmd.begin(), cmd.end());
        buf.push_back(0);
        STARTUPINFOW si; PROCESS_INFORMATION pi;
        std::memset(&si, 0, sizeof si); si.cb = sizeof si; std::memset(&pi, 0, sizeof pi);
        ok = CreateProcessW(starter.c_str(), buf.data(), nullptr, nullptr, FALSE, 0, nullptr,
                            root.c_str(), &si, &pi) != 0;
        if (ok) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
    }
    if (!ok) set(State::Done, "UPDATED TO " + label + ". CLOSE THE GAME AND START IT AGAIN");
    return ok;
#else
    return false;
#endif
}

bool dev_available() { return MOA_DEV_UPDATES != 0; }

bool has_token() { return MOA_DEV_UPDATES && !read_token().empty(); }

bool set_token(const std::string& raw) {
#if MOA_DEV_UPDATES
    std::string t;
    for (char c : raw) if (!std::isspace((unsigned char)c)) t.push_back(c);
    if (!token_chars_ok(t)) return false;
    const fs::path p = token_file();
    if (p.empty()) return false;
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << t << "\n";
    f.close();
#ifndef _WIN32
    chmod(p.c_str(), 0600);
#endif
    return (bool)f;
#else
    (void)raw;
    return false;
#endif
}

void forget_token() {
    const fs::path p = token_file();
    std::error_code ec;
    if (!p.empty()) fs::remove(p, ec);
}

void check_dev() {
#if MOA_DEV_UPDATES
    {
        std::lock_guard<std::mutex> l(g_mu);
        if (g_busy) return;
        g_snap.state = State::Checking;
        g_snap.channel = Channel::Dev;
        g_snap.message = "LOOKING FOR DEV BUILDS";
    }
#ifdef _WIN32
    sweep_old(install_root());
#endif
    start(do_check_dev);
#endif
}

} /* namespace moa_update */

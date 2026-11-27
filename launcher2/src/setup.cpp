/* The "Preparing game" steps. Kept in the same order and with the same
 * settings as scripts/linux/build.sh and scripts/windows/build.ps1 (PRs #11,
 * #17), which stay in the repo as developer tools. What changed on the way:
 *   - No git: the source comes as acg1plus-source-<version>.zip from the
 *     release (tools/release/make_source_zip.py), checked against the SHA-256
 *     baked into this launcher, with the framework patches already applied and
 *     the library sources inside (so the build downloads nothing:
 *     PSX_DEPS_OFFLINE=1).
 *   - The pack's env.sh (bash) is not sourced: the same variables are set here
 *     for each child program.
 *   - Markers in RecompData/build/state/ let a stopped build carry on.
 *   - The launcher is ACG1PLUS(.exe) at the top of the folder now, so the old
 *     starter exe is not copied there and no Start/Build scripts are written.
 */
#include "setup.h"

#include "hash.h"
#include "ini.h"
#include "platform.h"
#include "unzip.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <map>
#include <mutex>
#include <regex>
#include <thread>

#ifndef ACG_LAUNCHER_VERSION
#define ACG_LAUNCHER_VERSION "0.0.0"
#endif
/* Baked in by the release workflow (launcher2/release.sh); empty in a local build. */
#ifndef ACG_SOURCE_URL
#define ACG_SOURCE_URL ""
#endif
#ifndef ACG_SOURCE_SHA256
#define ACG_SOURCE_SHA256 ""
#endif
#ifndef ACG_SOURCE_SIZE
#define ACG_SOURCE_SIZE 0
#endif
#ifndef ACG_SOURCE_VERSION
#define ACG_SOURCE_VERSION ""
#endif

namespace setup {

namespace {

/* ---- pins (the same as the scripts) ------------------------------------------- */
const char* kPackVersion = "v1.0.14";
#if defined(_WIN32)
const char* kOs = "windows";
const char* kPackUrl = "https://github.com/RetroPortingToolKit/RetroPorting-Toolchains/releases/download/v1.0.14/cmake-clang-v1-windows-x64.zip";
const char* kPackSha = "28da9742385e7ff875b3d9311e8ed89dbdc84f27b6ecba2bc0d0acc11f6d2b4d";
const uint64_t kPackSize = 209497009ull;
const uint64_t kPackUnpacked = 800ull << 20;
const uint64_t kBuildNeed = 3ull << 30;     /* source, translated C, build files, the game */
const char* kExe = "ACG1PLUS_Game.exe";
#else
const char* kOs = "linux";
const char* kPackUrl = "https://github.com/RetroPortingToolKit/RetroPorting-Toolchains/releases/download/v1.0.14/cmake-clang-v1-linux-x64.zip";
const char* kPackSha = "597c8d343a3cf02ba6f6b2ae7cf6fe2fef125dde8feff62a144e8dd3da3d484e";
const uint64_t kPackSize = 833435449ull;
const uint64_t kPackUnpacked = 2600ull << 20;
const uint64_t kBuildNeed = 1800ull << 20;
const char* kExe = "ACG1PLUS_Game";
#endif
const char* kExeBase = "ACG1PLUS_Game";

const char* kNames[S_COUNT] = {
    "Checking the game folder",
    "Downloading the tools",
    "Unpacking the tools",
    "Getting the game's source",
    "Checking your disc",
    "Translating the game from your disc",
    "Building mission and arena code",
    "Compiling the game",
    "Putting the game in place",
    "Final check",
};
const char* kKeys[S_COUNT] = { "folder", "tools_dl", "tools_unpack", "source", "disc",
                               "translate", "native", "compile", "place", "check" };
/* First-time guesses, seconds (replaced by this PC's own times after a build). */
const double kGuess[S_COUNT] = { 5, 240, 120, 30, 20, 900, 600, 1800, 30, 5 };

Paths g_paths;
Options g_opts;

/* ---- shared state --------------------------------------------------------------- */
std::mutex g_mu;
Snapshot g_snap;
std::deque<std::string> g_log;
std::thread g_worker;
std::atomic<bool> g_cancel{false};
std::atomic<bool> g_running{false};
FILE* g_logf = nullptr;
double g_t0 = 0.0, g_stage_t0 = 0.0, g_t_end = 0.0;
std::map<std::string, std::string> g_times;   /* build-times.ini */
uint64_t g_dl_bytes0 = 0;                     /* for the download speed */
std::wstring g_user_path;                     /* Windows: user PATH before */
bool g_have_user_path = false;
std::vector<fs::path> g_watch_new;            /* Linux: home folders that must stay absent */
std::atomic<bool> g_pack_ready{false};       /* for the estimate */
std::mutex g_game_mu;                         /* the two below: the game vs the swap */
bool g_game_open = false;
bool g_swapping = false;

double now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

std::string stamp() {
    std::time_t t = std::time(nullptr);
    char b[32];
    std::strftime(b, sizeof b, "%Y-%m-%d_%H-%M-%S", std::localtime(&t));
    return b;
}

void log(const std::string& line) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_log.push_back(line);
    while (g_log.size() > 400) g_log.pop_front();
    if (g_logf) { std::fprintf(g_logf, "%s\n", line.c_str()); std::fflush(g_logf); }
}

void set_frac(int s, float f) { std::lock_guard<std::mutex> lk(g_mu); g_snap.frac[s] = f; }
void set_detail(const std::string& d) { std::lock_guard<std::mutex> lk(g_mu); g_snap.detail = d; }

struct Fail {
    std::string title, text;
};
[[noreturn]] void fail(const std::string& title, const std::string& text) { throw Fail{ title, text }; }
struct Cancelled {};
void check_cancel() { if (g_cancel) throw Cancelled{}; }

std::string mb(uint64_t b) { return std::to_string((unsigned long long)((b + (1 << 19)) >> 20)) + " MB"; }
std::string gb(uint64_t b) {
    char s[32];
    std::snprintf(s, sizeof s, "%.1f GB", (double)b / (1024.0 * 1024.0 * 1024.0));
    return s;
}

std::string read_text(const fs::path& p) {
    FILE* f = plat::open_file(p, "rb");
    if (!f) return std::string();
    std::string s;
    char b[4096];
    size_t n;
    while ((n = std::fread(b, 1, sizeof b, f)) > 0) s.append(b, n);
    std::fclose(f);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    return s;
}
bool write_text(const fs::path& p, const std::string& s) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    FILE* f = plat::open_file(p, "wb");
    if (!f) return false;
    const bool ok = std::fwrite(s.data(), 1, s.size(), f) == s.size();
    return std::fclose(f) == 0 && ok;
}

std::string sha256_of(const fs::path& p, int stage, uint64_t size_hint) {
    FILE* f = plat::open_file(p, "rb");
    if (!f) return std::string();
    hash::Sha256 h;
    std::vector<char> buf(1 << 20);
    size_t n;
    uint64_t done = 0;
    while ((n = std::fread(buf.data(), 1, buf.size(), f)) > 0) {
        h.update(buf.data(), n);
        done += n;
        if (stage >= 0 && size_hint) set_detail("checking the download: " + std::to_string(done * 100 / size_hint) + "%");
        if (g_cancel) { std::fclose(f); throw Cancelled{}; }
    }
    std::fclose(f);
    return h.hex();
}

uint64_t fsize(const fs::path& p) {
    std::error_code ec;
    const auto s = fs::file_size(p, ec);
    return ec ? 0 : (uint64_t)s;
}

/* ---- running child programs ---------------------------------------------------- */

using Env = std::vector<std::pair<std::string, std::string>>;

/* The environment every build program gets: every tool's caches, temp files
 * and toolchain lookups inside the folder (build.sh / build.ps1 "keep every
 * tool inside the folder"), plus the pack's own settings (its env.sh). */
Env build_env() {
    const Paths& P = g_paths;
    const std::string temp = plat::u8str(P.temp), pack = plat::u8str(P.pack), tools = plat::u8str(P.tools);
    Env e = {
        { "TEMP", temp }, { "TMP", temp }, { "TMPDIR", temp },
        { "CCACHE_DIR", plat::u8str(P.tools / "ccache") },
        { "RETCOMM_TOOLCHAIN_DIR", pack },
        { "PSXRECOMP_TOOLCHAIN_DIR", pack },
        { "RETCOMM_TOOLCHAIN_CACHE", plat::u8str(P.tools / "cache") },
        { "RETCOMM_DATA_HOME", plat::u8str(P.tools / "retcomm") },
        { "RETCOMM_TOOLCHAIN_SKIP_UPDATE", "1" },
        { "PSXRECOMP_TOOLCHAIN_READONLY", "1" },
        { "PYTHONNOUSERSITE", "1" }, { "PIP_NO_INDEX", "1" },
        { "PYTHONHOME", "" }, { "PYTHONPATH", "" },
        { "PYTHONUTF8", "1" }, { "PYTHONUNBUFFERED", "1" },
        { "PSXRECOMP_PROJECT_ROOT", plat::u8str(P.src) },
    };
    (void)tools;
    /* The source zip carries every library source: the build must not download. */
    const auto src = ini::load(P.src / "SOURCE.txt");
    auto it = src.find("offline");
    if (it != src.end() && it->second == "1") e.push_back({ "PSX_DEPS_OFFLINE", "1" });
#if defined(_WIN32)
    std::string path = plat::u8str(P.pack / "bin") + ";" + plat::u8str(P.pack / "python");
    if (const char* old = std::getenv("PATH")) { path += ";"; path += old; }
    e.push_back({ "PATH", path });
#else
    const fs::path cache = P.temp / "xdg";
    e.push_back({ "XDG_CACHE_HOME", plat::u8str(cache / "cache") });
    e.push_back({ "XDG_CONFIG_HOME", plat::u8str(cache / "config") });
    e.push_back({ "XDG_DATA_HOME", plat::u8str(cache / "data") });
    e.push_back({ "XDG_STATE_HOME", plat::u8str(cache / "state") });
    /* PATH: the gcc stand-in first (see write_gcc_shim), then the pack. */
    std::string path = plat::u8str(P.tools / "shim") + ":" + pack + "/bin:" + pack + "/python/bin";
    if (const char* old = std::getenv("PATH")) { path += ":"; path += old; }
    e.push_back({ "PATH", path });
    /* What the pack's env.sh sets. */
    e.push_back({ "CC", pack + "/bin/clang" });
    e.push_back({ "CXX", pack + "/bin/clang++" });
    e.push_back({ "AR", pack + "/bin/llvm-ar" });
    e.push_back({ "RANLIB", pack + "/bin/llvm-ranlib" });
    std::string ld = pack + "/lib";
    if (const char* old = std::getenv("LD_LIBRARY_PATH")) if (*old) { ld += ":"; ld += old; }
    e.push_back({ "LD_LIBRARY_PATH", ld });
    e.push_back({ "LDFLAGS", "-fuse-ld=lld --rtlib=compiler-rt" });
    e.push_back({ "RETCOMM_PYTHON", pack + "/python/bin/python3" });
    std::error_code ec;
    if (fs::is_directory(P.pack / "sysroot" / "usr" / "include", ec)) e.push_back({ "CMAKE_SYSROOT", pack + "/sysroot" });
    if (fs::exists(P.pack / "deps/lib/cmake/SDL3/SDL3Config.cmake", ec) ||
        fs::exists(P.pack / "deps/lib/cmake/SDL3/SDL3-config.cmake", ec))
        e.push_back({ "SDL3_DIR", pack + "/deps/lib/cmake/SDL3" });
    if (fs::exists(P.pack / "deps/include/zlib.h", ec)) e.push_back({ "ZLIB_ROOT", pack + "/deps" });
    /* The same git settings as the scripts, in case anything runs git. */
    e.push_back({ "GIT_CONFIG_NOSYSTEM", "1" });
    e.push_back({ "GIT_CONFIG_COUNT", "2" });
    e.push_back({ "GIT_CONFIG_KEY_0", "safe.directory" });
    e.push_back({ "GIT_CONFIG_VALUE_0", "*" });
    e.push_back({ "GIT_CONFIG_KEY_1", "core.fileMode" });
    e.push_back({ "GIT_CONFIG_VALUE_1", "false" });
#endif
    return e;
}

fs::path python() {
#if defined(_WIN32)
    return g_paths.pack / "python" / "python.exe";
#else
    return g_paths.pack / "python" / "bin" / "python3";
#endif
}

/* Windows: anything that changed the user PATH while a build program ran is
 * put back (the framework's toolchain installer can write it; never reached). */
void check_user_path() {
#if defined(_WIN32)
    if (!g_have_user_path) return;
    std::wstring now_path;
    if (plat::user_path_get(&now_path) && now_path != g_user_path) {
        log("Something changed your user PATH during the build. Putting it back.");
        plat::user_path_set(g_user_path);
    }
#endif
}

/* Runs a program to the end. on_line gets every line (stdout = 1, stderr = 2);
 * every_second runs about once a second while it works. Returns its exit code. */
int run(const std::vector<std::string>& args, const Env& env, const fs::path& cwd,
        const std::function<void(int, const std::string&)>& on_line,
        const std::function<void()>& every_second = nullptr) {
    std::string cmd;
    for (const auto& a : args) { if (!cmd.empty()) cmd += ' '; cmd += a; }
    log("$ " + cmd);
    plat::Process p;
    std::string err;
    if (!p.start(args, env, cwd, on_line, &err)) {
        log(err);
        return -1;
    }
    double last = now();
    int code = 0;
    while (!p.poll(&code)) {
        if (g_cancel) {
            log("Stopping " + args[0] + " ...");
            p.kill_tree();
            p.wait();
            check_user_path();
            throw Cancelled{};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (every_second && now() - last >= 1.0) { last = now(); every_second(); }
    }
    check_user_path();
    if (code != 0) log("(exit code " + std::to_string(code) + ")");
    return code;
}

/* ---- download with resume -------------------------------------------------------- */

void download(int stage, const std::string& url, const fs::path& dest, const std::string& sha, uint64_t size,
              const char* what) {
    const fs::path part = dest.string() + ".part";
    std::error_code ec;
    if (fs::exists(dest, ec)) {
        if (sha256_of(dest, stage, size) == sha) return;
        fs::remove(dest, ec);
    }
    const std::string curl = plat::curl_program();
    if (curl.empty())
        fail("No download program", "This system has no curl program, which the launcher uses to download. "
             "Windows 10 and 11 and SteamOS come with it.");
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (size && fsize(part) < size) {
            log(std::string("Downloading ") + what + ": " + url);
            g_dl_bytes0 = fsize(part);
            std::string last_err;
            auto on_line = [&](int, const std::string& l) { last_err = l; log("curl: " + l); };
            auto tick = [&] {
                const uint64_t have = fsize(part);
                set_frac(stage, size ? (float)((double)have / size) : -1.0f);
                set_detail(mb(have) + " of " + mb(size));
            };
            const int code = run({ curl, "-L", "--fail", "-sS", "--retry", "5", "--retry-delay", "3",
                                   "--connect-timeout", "30", "-C", "-", "-o", plat::u8str(part), url },
                                 {}, g_paths.build, on_line, tick);
            if (code != 0) {
                /* 33 = the server can't resume: start the file again once. */
                if (code == 33 && attempt == 0) { fs::remove(part, ec); continue; }
                fail("Download failed",
                     std::string("Couldn't download ") + what + ". Check the internet connection, then RETRY: "
                     "it carries on where it stopped." + (last_err.empty() ? "" : "\n\ncurl: " + last_err));
            }
        }
        set_frac(stage, 1.0f);
        const std::string got = sha256_of(part, stage, size);
        if (got == sha) {
            fs::rename(part, dest, ec);
            if (ec) fail("Download failed", "Couldn't rename " + plat::u8str(part) + ": " + ec.message());
            return;
        }
        log("Checksum mismatch: got " + got + ", want " + sha);
        fs::remove(part, ec);
    }
    fail("Download damaged", std::string("The download of ") + what + " didn't match its checksum, twice. "
         "RETRY downloads it again; if this keeps happening, the file on the server may have changed.");
}

/* ---- steps ------------------------------------------------------------------------- */

bool pack_ready();

void step_folder() {
    const Paths& P = g_paths;
    std::error_code ec;
    for (const fs::path& d : { P.data, P.build, P.tools, P.temp, P.logs, P.state, P.root / "Disc",
                               P.root / "Memory Card", P.root / "Mods" }) {
        fs::create_directories(d, ec);
        if (ec) fail("Can't write here", "Couldn't make the folder " + plat::u8str(d) + ": " + ec.message() +
                     ". Is the drive read-only or full?");
    }
#if !defined(_WIN32)
    const fs::path cache = P.temp / "xdg";
    for (const char* sub : { "cache", "config", "data", "state" }) fs::create_directories(cache / sub, ec);
    std::string why;
    if (!plat::can_run_programs(P.temp, &why))
        fail("Programs can't run from this drive",
             "The compiler and the game both have to run from the game folder, and " + why + ".\n\n"
             "Put the game folder on a drive formatted exFAT and mounted by the Deck itself (Desktop Mode "
             "mounts USB drives at /run/media/deck/...), or on the Deck's own storage.");
    log("Programs can run from this folder.");
    /* Places outside the folder this build must not touch (build.sh's list). */
    const fs::path h = plat::home_dir();
    g_watch_new.clear();
    for (const char* rel : { ".local/share/retcomm", ".local/share/psxrecomp", ".cache/retcomm", ".cache/psxrecomp",
                             ".cache/ccache", ".config/ccache", ".config/ACG1PLUS", ".config/recomp-ui",
                             ".local/share/ACG1PLUS", ".local/share/Armored-Core-Gen-1" }) {
        if (!fs::exists(h / rel, ec)) g_watch_new.push_back(h / rel);
    }
#else
    g_have_user_path = plat::user_path_get(&g_user_path);
#endif
    /* Free space: what is still to come. */
    uint64_t need = kBuildNeed;
    const bool have_pack = pack_ready();
    g_pack_ready = have_pack;
    if (!have_pack) need += kPackSize + kPackUnpacked;
    if (!fs::exists(P.src / "SOURCE.txt", ec)) need += 300ull << 20;
    const uint64_t free_b = plat::free_space(P.root);
    log("Free space: " + gb(free_b) + ", needed about " + gb(need));
    if (free_b && free_b < need)
        fail("Not enough space",
             "Building the game needs about " + gb(need) + " free on this drive; it has " + gb(free_b) +
             ". Free some space (or move the game folder to a bigger drive), then RETRY.");
}

bool pack_ready() {
    const Paths& P = g_paths;
    std::error_code ec;
#if defined(_WIN32)
    const bool files = fs::exists(P.pack / "bin" / "cmake.exe", ec) && fs::exists(P.pack / "python" / "python.exe", ec);
#else
    const bool files = fs::exists(P.pack / "bin" / "cmake", ec) && fs::exists(P.pack / "python" / "bin" / "python3", ec);
#endif
    return files && read_text(P.pack / ".acg-pack-version") == kPackVersion;
}

fs::path pack_zip() {
    return g_paths.tools / (std::string("cmake-clang-v1-") + kOs + "-x64-" + kPackVersion + ".zip");
}

bool step_tools_dl() {
    if (pack_ready()) { log(std::string("Compiler pack ") + kPackVersion + " is already here."); return false; }
    download(S_TOOLS_DL, kPackUrl, pack_zip(), kPackSha, kPackSize, "the compiler pack");
    return true;
}

bool step_tools_unpack() {
    const Paths& P = g_paths;
    if (pack_ready()) return false;
    std::error_code ec;
    const fs::path tmp = P.tools / "toolchain.partial";
    fs::remove_all(tmp, ec);
    log("Unpacking " + plat::u8str(pack_zip()) + " (about " + mb(kPackUnpacked) + ")");
    std::string err;
    const bool ok = unzip::extract_all(pack_zip(), tmp, [&](uint64_t d, uint64_t t) {
        set_frac(S_TOOLS_UNPACK, t ? (float)((double)d / t) : 0.0f);
        set_detail(mb(d) + " of " + mb(t));
        return !g_cancel.load();
    }, &err);
    check_cancel();
    if (!ok) fail("Couldn't unpack the tools", err + "\n\nIs the drive full? RETRY unpacks them again.");
    fs::remove_all(P.pack, ec);
    fs::rename(tmp, P.pack, ec);
    if (ec) fail("Couldn't unpack the tools", "Couldn't move the tools into place: " + ec.message());
    write_text(P.pack / ".acg-pack-version", kPackVersion);
    fs::remove(pack_zip(), ec);
    return true;
}

void check_pack_runs() {
    const Paths& P = g_paths;
#if defined(_WIN32)
    const std::string clang = plat::u8str(P.pack / "bin" / "clang.exe");
#else
    const std::string clang = plat::u8str(P.pack / "bin" / "clang");
#endif
    std::string first;
    const int code = run({ clang, "--version" }, build_env(), P.build,
                         [&](int, const std::string& l) { if (first.empty()) first = l; log(l); });
    if (code != 0) fail("The compiler doesn't run here", "The compiler in the pack didn't start on this system "
                        "(exit code " + std::to_string(code) + "). The log has the details.");
#if !defined(_WIN32)
    /* The framework compiles the disc's code with the first of gcc, cc, clang it
     * finds. A stock Deck has no gcc (so it takes the pack's), a Linux PC may:
     * then two machines would build different games (netplay needs identical
     * builds). A stand-in named gcc, first on PATH for the build, sends those
     * calls to the pack's Clang everywhere (as build.sh does). */
    const fs::path shim = P.tools / "shim" / "gcc";
    write_text(shim, "#!/bin/sh\nexec \"" + clang + "\" \"$@\"\n");
    plat::make_executable(shim);
#endif
}

fs::path find_source_zip(std::string* sha_out) {
    const Paths& P = g_paths;
    if (!g_opts.source_zip.empty()) {
        std::error_code ec;
        if (!fs::is_regular_file(g_opts.source_zip, ec))
            fail("Source zip not found", "--source-zip " + plat::u8str(g_opts.source_zip) + " doesn't exist.");
        log("Using the source zip given on the command line (test): " + plat::u8str(g_opts.source_zip));
        *sha_out = sha256_of(g_opts.source_zip, S_SOURCE, fsize(g_opts.source_zip));
        return g_opts.source_zip;
    }
    if (!source_known())
        fail("No source for this launcher",
             "This launcher was built without a source zip to go with it (a developer build). "
             "Start it with --source-zip FILE, or use a launcher from a release.");
    const fs::path zip = P.build / (std::string("acg1plus-source-") + ACG_SOURCE_VERSION + ".zip");
    download(S_SOURCE, ACG_SOURCE_URL, zip, ACG_SOURCE_SHA256, (uint64_t)ACG_SOURCE_SIZE, "the game's source");
    *sha_out = ACG_SOURCE_SHA256;
    /* 0.2.4: an update brings a new zip; the ones before it aren't needed. */
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(P.build, ec)) {
        const std::string n = plat::u8str(e.path().filename());
        if (e.path() != zip && n.rfind("acg1plus-source-", 0) == 0) fs::remove(e.path(), ec);
    }
    return zip;
}

bool step_source() {
    const Paths& P = g_paths;
    std::error_code ec;
    std::string sha;
    const fs::path zip = find_source_zip(&sha);
    const fs::path marker = P.state / "source.sha256";
    if (read_text(marker) == sha && fs::exists(P.src / "SOURCE.txt", ec) && fs::exists(P.src / "game.toml", ec)) {
        log("The source is already unpacked (" + sha.substr(0, 12) + ").");
        return false;
    }
    /* Unpack beside the old source and swap it in only when complete, keeping
     * what the build made from your disc and the build files (so a new version
     * builds faster). A broken zip leaves the old source as it was. */
    fs::remove(marker, ec);
    const fs::path fresh = P.build / "source.partial";
    fs::remove_all(fresh, ec);
    log("Unpacking the source " + plat::u8str(zip));
    std::string err;
    const bool ok = unzip::extract_all(zip, fresh, [&](uint64_t d, uint64_t t) {
        set_frac(S_SOURCE, t ? (float)((double)d / t) : 0.0f);
        set_detail("unpacking " + mb(d) + " of " + mb(t));
        return !g_cancel.load();
    }, &err);
    if (!ok) fs::remove_all(fresh, ec);
    check_cancel();
    if (!ok) fail("Couldn't unpack the source", err + "\n\nRETRY tries again; the source in use before is unchanged.");
    if (!fs::exists(fresh / "psxrecomp" / "psxrecomp_cli.py", ec) || !fs::exists(fresh / "game.toml", ec)) {
        fs::remove_all(fresh, ec);
        fail("The source is incomplete", "The source zip has no psxrecomp/psxrecomp_cli.py or game.toml.");
    }
    static const char* keep[] = { "generated", "build-release", ".cache", "disc" };
    for (const char* k : keep) {
        if (fs::exists(P.src / k, ec)) {
            fs::remove_all(fresh / k, ec);
            fs::rename(P.src / k, fresh / k, ec);
        }
    }
    fs::remove_all(P.src, ec);
    fs::rename(fresh, P.src, ec);
    if (ec) fail("Couldn't put the source in place", ec.message());
    write_text(marker, sha);
    const auto info = ini::load(P.src / "SOURCE.txt");
    log("Source " + (info.count("version") ? info.at("version") : std::string("?")) + ", commit " +
        (info.count("commit") ? info.at("commit") : std::string("?")));
    return true;
}

/* psxrecomp_cli.py --json-progress: stdout is one JSON object per line. A tiny
 * reader for the few fields used here (event, phase, pct, message, status). */
std::string json_str(const std::string& l, const char* key) {
    const std::string k = std::string("\"") + key + "\":";
    size_t p = l.find(k);
    if (p == std::string::npos) return std::string();
    p += k.size();
    if (p < l.size() && l[p] == '"') {
        std::string out;
        for (size_t i = p + 1; i < l.size(); ++i) {
            if (l[i] == '\\' && i + 1 < l.size()) {
                const char c = l[++i];
                if (c == 'n') out += '\n';
                else if (c == 't') out += '\t';
                else if (c == 'u' && i + 4 < l.size()) { out += '?'; i += 4; }
                else out += c;
            } else if (l[i] == '"') break;
            else out += l[i];
        }
        return out;
    }
    size_t e = l.find_first_of(",}", p);
    return l.substr(p, e == std::string::npos ? std::string::npos : e - p);
}

struct CliResult { std::string error; bool verify_failed = false; };

/* One psxrecomp_cli.py command; JSON events to on_event, text to the log. */
int run_cli(const std::vector<std::string>& cli_args, CliResult* res,
            const std::function<void(const std::string& event, const std::string& line)>& on_event,
            const std::function<void()>& every_second = nullptr) {
    const Paths& P = g_paths;
    std::vector<std::string> args = { plat::u8str(python()), "-u",
                                      plat::u8str(P.src / "psxrecomp" / "psxrecomp_cli.py") };
    args.insert(args.end(), cli_args.begin(), cli_args.end());
    args.push_back("--json-progress");
    return run(args, build_env(), P.src, [&](int stream, const std::string& line) {
        if (stream == 1 && !line.empty() && line[0] == '{') {
            const std::string ev = json_str(line, "event");
            if (ev == "log") return;   /* mirrored on stderr anyway */
            if (ev == "error" && res) {
                res->error = json_str(line, "message");
                res->verify_failed = line.find("\"verify_failed\":true") != std::string::npos;
                log("ERROR: " + res->error);
            } else if (ev == "phase") {
                const std::string m = json_str(line, "message");
                if (!m.empty()) log("-- " + m);
            }
            if (on_event) on_event(ev, line);
            return;
        }
        log(line);
        if (on_event) on_event("text", line);
    }, every_second);
}

fs::path g_disc;

void step_disc() {
    const Paths& P = g_paths;
    std::error_code ec;
    if (!fs::exists(g_disc, ec)) fail("Disc not found", "The disc image " + plat::u8str(g_disc) +
                                      " isn't there any more. Plug its drive back in, or choose the disc again.");
    CliResult r;
    const int code = run_cli({ "verify-disc", "--config", plat::u8str(P.src / "game.toml"), "--project-root",
                               plat::u8str(P.src), "--disc", plat::u8str(g_disc) }, &r, nullptr);
    if (code != 0)
        fail("Your disc didn't pass the check",
             (r.error.empty() ? std::string("verify-disc stopped (exit code ") + std::to_string(code) + ")." : r.error) +
             "\n\nThis build needs Project Phantasma USA (SLUS-00670), a Redump-style .cue + .bin with one data "
             "track of 454,072,416 bytes.");
}

std::string translate_key() {
    const Paths& P = g_paths;
    return read_text(P.state / "source.sha256") + "|" + plat::u8str(g_disc) + "|" + std::to_string(fsize(g_disc));
}

/* Generate: the main program, then (aot/overlays.json) the code the game loads
 * from the disc while playing, both into source/generated/ (never leaves this PC). */
bool step_translate() {
    const Paths& P = g_paths;
    std::error_code ec;
    const fs::path marker = P.state / "translated.txt";
    if (!g_opts.force_translate && read_text(marker) == translate_key() &&
        fs::exists(P.src / "generated" / "SLUS_006.70_dispatch.c", ec)) {
        log("The game was already translated from this disc with this source.");
        std::lock_guard<std::mutex> lk(g_mu);
        g_snap.done[S_NATIVE] = g_snap.skipped[S_NATIVE] = true;
        return false;
    }
    fs::remove(marker, ec);
    std::vector<std::string> args = { "generate", "--config", plat::u8str(P.src / "game.toml"), "--project-root",
                                      plat::u8str(P.src), "--disc", plat::u8str(g_disc) };
    if (g_opts.force_translate) args.push_back("--force-aot-static");
    const std::regex job_line("^\\w+ ([0-9]+)/([0-9]+): (.*)$");
    bool native = false;
    CliResult r;
    const int code = run_cli(args, &r, [&](const std::string& ev, const std::string& line) {
        if (ev == "phase") {
            const std::string ph = json_str(line, "phase");
            const double pct = std::atof(json_str(line, "pct").c_str());
            if (ph == "aot_static" && !native) {
                native = true;
                std::lock_guard<std::mutex> lk(g_mu);
                g_snap.frac[S_TRANSLATE] = 1.0f;
                g_snap.done[S_TRANSLATE] = true;
                g_snap.stage = S_NATIVE;
                g_snap.detail.clear();
                const double t = now();
                g_times[kKeys[S_TRANSLATE]] = std::to_string((int)(t - g_stage_t0));
                g_stage_t0 = t;
            } else if (!native && pct > 0.0) {
                set_frac(S_TRANSLATE, (float)std::min(0.99, pct / 0.7));
                set_detail(ph == "emit" ? "translating the main program" : ph == "bios" ? "the BIOS" :
                           ph == "emitters" ? "building the translator" : ph);
            }
        } else if (ev == "text" && native) {
            std::smatch m;
            if (std::regex_match(line, m, job_line)) {
                const double a = std::atof(m[1].str().c_str()), b = std::atof(m[2].str().c_str());
                if (b > 0) {
                    set_frac(S_NATIVE, (float)(a / b));
                    set_detail("piece " + m[1].str() + " of " + m[2].str());
                }
            } else if (line.rfind("Reused ", 0) == 0) {
                set_detail("already built from this disc");
            } else if (line.rfind("Verified ", 0) == 0) {
                /* "Verified 67 images / 67 recipes": the static build compiles them
                 * in one go without a counter, so the estimate carries the bar. */
                const size_t sp = line.find(' ', 9);
                set_detail("compiling " + line.substr(9, sp == std::string::npos ? std::string::npos : sp - 9) +
                           " pieces of disc code");
            }
        }
    });
    if (code != 0)
        fail(native ? "Building native code stopped" : "Translating the game stopped",
             (r.error.empty() ? std::string("psxrecomp generate stopped (exit code ") + std::to_string(code) + ")."
                              : r.error) + "\n\nSHOW LOG has the details. RETRY carries on.");
    if (!native) {   /* the framework found nothing to pre-compile */
        std::lock_guard<std::mutex> lk(g_mu);
        g_snap.done[S_NATIVE] = g_snap.skipped[S_NATIVE] = true;
    }
    /* Generate copies the disc into source/disc/ the first time; only the boot
     * program it extracted is needed again (next builds read your own copy). */
    for (const auto& e : fs::directory_iterator(P.src / "disc", ec)) {
        std::string ext = plat::u8str(e.path().extension());
        for (char& c : ext) c = (char)std::tolower((unsigned char)c);
        if ((ext == ".bin" || ext == ".cue") && e.path() != g_disc) fs::remove(e.path(), ec);
    }
    write_text(marker, translate_key());
    return true;
}

size_t count_lines(const fs::path& p) {
    FILE* f = plat::open_file(p, "rb");
    if (!f) return 0;
    size_t n = 0;
    char b[65536];
    size_t got;
    while ((got = std::fread(b, 1, sizeof b, f)) > 0) n += (size_t)std::count(b, b + got, '\n');
    std::fclose(f);
    return n;
}

/* Finished Ninja steps logged after the first SKIP lines: distinct command
 * hashes (the last field), since a step with several outputs logs a line each. */
size_t count_new_steps(const fs::path& p, size_t skip) {
    FILE* f = plat::open_file(p, "rb");
    if (!f) return 0;
    std::vector<std::string> hashes;
    std::string line;
    size_t n = 0;
    int c;
    while ((c = std::fgetc(f)) != EOF) {
        if (c != '\n') { line += (char)c; continue; }
        if (n++ >= skip && !line.empty() && line[0] != '#') {
            const size_t t = line.rfind('\t');
            if (t != std::string::npos) hashes.push_back(line.substr(t + 1));
        }
        line.clear();
    }
    std::fclose(f);
    std::sort(hashes.begin(), hashes.end());
    return (size_t)(std::unique(hashes.begin(), hashes.end()) - hashes.begin());
}

void step_compile() {
    const Paths& P = g_paths;
    const fs::path out = P.src / "build-release";
    const fs::path ninja_log = out / ".ninja_log";
    std::error_code ec;
    const size_t log_lines0 = count_lines(ninja_log);
    const auto log_time0 = fs::exists(ninja_log, ec) ? fs::last_write_time(ninja_log, ec) : fs::file_time_type::min();
    size_t total = 0;
    std::vector<std::string> args = {
        "rebuild", "--config", plat::u8str(P.src / "game.toml"), "--project-root", plat::u8str(P.src),
        "--build-dir", "build-release", "--target", "psx-runtime", "--exe-basename", kExeBase,
        "--no-toolchain-download", "--no-pgo",
        "--cmake-extra=-DPSX_SDL_BACKEND=SDL3", "--cmake-extra=-DPSX_PGXP_VARIANT=OFF",
        "--cmake-extra=-DPSX_RELEASE_CHANNEL=playtest",
    };
#if !defined(_WIN32)
    /* OpenSSL and FreeType off, so every Linux machine builds what a stock Deck
     * builds from the pack alone (build.sh explains). */
    args.push_back("--cmake-extra=-DCMAKE_DISABLE_FIND_PACKAGE_OpenSSL=ON");
    args.push_back("--cmake-extra=-DCMAKE_DISABLE_FIND_PACKAGE_Freetype=ON");
    const std::string ninja = plat::u8str(P.pack / "bin" / "ninja");
#else
    const std::string ninja = plat::u8str(P.pack / "bin" / "ninja.exe");
#endif
    /* The framework only passes the compiler's output on at the end, so the bar
     * comes from Ninja's own log of finished steps (.ninja_log: one line per
     * output file, so steps are counted by their command hash), against the
     * number of steps it has (ninja -t commands). The total is counted again
     * every 20 seconds: the build adds the disc code's files part way through. */
    double last_count = -100.0;
    auto tick = [&] {
        std::error_code e2;
        if (!fs::exists(ninja_log, e2) || fs::last_write_time(ninja_log, e2) <= log_time0) {
            set_detail("setting up the build");
            return;
        }
        if (now() - last_count >= 20.0) {
            last_count = now();
            plat::Process np;
            std::string err;
            size_t n = 0;
            if (np.start({ ninja, "-C", plat::u8str(out), "-t", "commands", "psx-runtime" }, build_env(), out,
                         [&](int st, const std::string&) { if (st == 1) ++n; }, &err) && np.wait() == 0 && n > total) {
                if (total == 0 || n != total) log("Compile steps: " + std::to_string(n));
                total = n;
            }
        }
        const size_t done = count_new_steps(ninja_log, log_lines0);
        if (total) {
            set_frac(S_COMPILE, std::min(0.99f, (float)done / (float)total));
            set_detail(std::to_string(std::min(done, total)) + " of " + std::to_string(total) + " steps");
        } else {
            set_detail(std::to_string(done) + " steps");
        }
    };
    CliResult r;
    const int code = run_cli(args, &r, nullptr, tick);
    if (code != 0)
        fail("Compiling stopped",
             (r.error.empty() ? std::string("psxrecomp rebuild stopped (exit code ") + std::to_string(code) + ")."
                              : r.error) + "\n\nSHOW LOG has the details. RETRY carries on.");
    if (!fs::exists(out / kExe, ec)) fail("Compiling stopped", std::string("The build finished but ") + kExe + " is missing.");
}

/* Make TO an exact copy of FROM (stale files removed), leaving KEEP alone. */
void mirror(const fs::path& from, const fs::path& to, const char* keep = nullptr) {
    std::error_code ec;
    if (!fs::is_directory(from, ec)) return;
    fs::create_directories(to, ec);
    for (const auto& e : fs::directory_iterator(to, ec)) {
        if (keep && e.path().filename() == keep) continue;
        fs::remove_all(e.path(), ec);
    }
    for (const auto& e : fs::directory_iterator(from, ec)) {
        if (keep && e.path().filename() == keep) continue;
        fs::copy(e.path(), to / e.path().filename(),
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        if (ec) fail("Couldn't put the game in place", "Couldn't copy " + plat::u8str(e.path()) + ": " + ec.message());
    }
}

/* 0.2.4: the new game is put together in RecompData/<os>.incoming/ first and
 * swapped in only when it is complete, so a full drive or a failed copy leaves
 * the game that was there (the one an update replaces) as it was. The swap is
 * a few renames. The player's own files in RecompData/<os>/ (settings.toml,
 * saves, caches, keyboard profiles, mods/installed) are not in the incoming
 * folder and stay where they are. */
void step_place() {
    const Paths& P = g_paths;
    const fs::path out = P.src / "build-release";
    const fs::path in = P.data / (std::string(kOs) + ".incoming");
    const fs::path old = P.data / (std::string(kOs) + ".replaced");
    std::error_code ec;
    fs::remove_all(in, ec);
    fs::remove_all(old, ec);
    fs::create_directories(in, ec);
    if (ec) fail("Couldn't put the game in place", "Couldn't make " + plat::u8str(in) + ": " + ec.message());
    fs::copy_file(out / kExe, in / kExe, fs::copy_options::overwrite_existing, ec);
    if (ec) fail("Couldn't put the game in place", std::string("Couldn't copy ") + kExe + ": " + ec.message() +
                 "\n\nIs the drive full? The game you had before is unchanged.");
    plat::make_executable(in / kExe);
#if defined(_WIN32)
    for (const auto& e : fs::directory_iterator(out, ec)) {
        std::string ext = plat::u8str(e.path().extension());
        for (char& c : ext) c = (char)std::tolower((unsigned char)c);
        if (ext == ".dll") fs::copy_file(e.path(), in / e.path().filename(), fs::copy_options::overwrite_existing, ec);
    }
#endif
    for (const char* f : { "game.toml", "game_options.toml", "psx_game_version.txt" })
        if (fs::exists(out / f, ec)) fs::copy_file(out / f, in / f, fs::copy_options::overwrite_existing, ec);
    /* Folders the runtime reads beside the program. mods/installed holds the
     * player's installed mods and is never replaced. */
    set_frac(S_PLACE, 0.2f);
    mirror(out / "assets", in / "assets");
    mirror(out / "bios", in / "bios");
    mirror(out / "mods", in / "mods", "installed");
    mirror(out / "overlay_toolchain", in / "overlay_toolchain");
    mirror(P.src / "psxrecomp" / "runtime" / "licenses", in / "licenses");
    set_frac(S_PLACE, 0.7f);
    /* Portable: the background compiler is named relative to overlay_toolchain/. */
#if defined(_WIN32)
    const char* rel = "..\\..\\build\\tools-windows\\toolchain\\bin\\clang.exe";
#else
    const char* rel = "../../build/tools-linux/toolchain/bin/clang";
#endif
    if (fs::is_directory(in / "overlay_toolchain", ec)) {
        write_text(in / "overlay_toolchain" / "compiler.txt", std::string(rel) + "\n");
        log(std::string("Background compiler: ") + rel + " (relative, so the folder can be moved)");
    }
    const auto info = ini::load(P.src / "SOURCE.txt");
    std::string bi = "built=" + stamp() + "\nlauncher=" ACG_LAUNCHER_VERSION "\n";
    for (const char* k : { "version", "commit" }) if (info.count(k)) bi += std::string("source_") + k + "=" + info.at(k) + "\n";
    /* Which source zip, so a newer launcher knows the game needs rebuilding. */
    bi += "source_sha256=" + read_text(P.state / "source.sha256") + "\n";
    if (!write_text(in / "build-info.txt", bi)) fail("Couldn't put the game in place", "Couldn't write build-info.txt.");

    /* 0.2.5.1: the game that is there may be open (it can be played while the
     * new one builds): wait until it is closed, and keep it closed during the
     * swap. */
    bool said = false;
    for (;;) {
        {
            std::lock_guard<std::mutex> lk(g_game_mu);
            if (!g_game_open) { g_swapping = true; break; }
        }
        if (!said) {
            said = true;
            log("The new game is ready. Waiting for the game that is open to close...");
            std::lock_guard<std::mutex> lk(g_mu);
            g_snap.waiting_for_game = true;
            g_snap.detail = "waiting for the game to close";
        }
        if (g_cancel) throw Cancelled{};
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_snap.waiting_for_game = false;
        g_snap.detail = "swapping in the new game";
    }
    struct EndSwap { ~EndSwap() { std::lock_guard<std::mutex> lk(g_game_mu); g_swapping = false; } } end_swap;

    /* The swap: each of the new game's files and folders replaces the old one
     * (moved aside to <os>.replaced first, and back if a rename fails). */
    fs::create_directories(P.game, ec);
    fs::create_directories(old, ec);
    std::vector<fs::path> names, moved;   /* moved: names already swapped */
    for (const auto& e : fs::directory_iterator(in, ec)) names.push_back(e.path().filename());
    for (const fs::path& name : names) {
        const fs::path src = in / name;
        const fs::path dst = P.game / name;
        std::string err;
        if (name == "mods" && fs::is_directory(dst / "installed", ec)) {
            if (!plat::rename_retry(dst / "installed", src / "installed", &err))
                fail("Couldn't put the game in place", err + "\n\nIs the game still running? Close it and RETRY.");
        }
        if (fs::exists(dst, ec) && !plat::rename_retry(dst, old / name, &err)) {
            for (const auto& m : moved) {   /* put back what was swapped */
                std::string e2;
                fs::remove_all(P.game / m, ec);
                plat::rename_retry(old / m, P.game / m, &e2);
            }
            fail("Couldn't put the game in place", err + "\n\nIs the game still running? Close it and RETRY. "
                 "The game you had before is unchanged.");
        }
        if (!plat::rename_retry(src, dst, &err)) {
            plat::rename_retry(old / name, dst, &err);
            fail("Couldn't put the game in place", err);
        }
        moved.push_back(name);
    }
    fs::remove_all(in, ec);
    fs::remove_all(old, ec);
}

void step_check() {
    const Paths& P = g_paths;
    std::error_code ec;
    std::vector<std::string> problems;
    if (!fs::exists(P.game / kExe, ec)) problems.push_back(std::string(kExe) + " is missing");
    if (!fs::exists(P.game / "bios" / "openbios.bin", ec)) problems.push_back("bios/openbios.bin is missing");
    for (const auto& w : g_watch_new)
        if (fs::exists(w, ec)) problems.push_back("something wrote " + plat::u8str(w) + " during the build (it must stay inside the game folder)");
    check_user_path();
    if (!problems.empty()) {
        std::string t;
        for (const auto& p : problems) t += "- " + p + "\n";
        fail("The final check found a problem", t);
    }
}

/* ---- the worker -------------------------------------------------------------------- */

void begin_stage(int s) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_snap.stage = s;
    g_snap.detail.clear();
    g_snap.frac[s] = 0.0f;
    g_stage_t0 = now();
    if (g_logf) std::fprintf(g_logf, "\n==> %d/%d  %s\n", s + 1, (int)S_COUNT, kNames[s]);
    g_log.push_back(std::string("==> ") + kNames[s]);
}

void end_stage(int s, bool did_work) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_snap.done[s] = true;
    g_snap.frac[s] = 1.0f;
    if (!did_work) g_snap.skipped[s] = true;
    /* Only real work teaches the estimate. */
    else g_times[kKeys[s]] = std::to_string((int)(now() - g_stage_t0));
    g_snap.detail.clear();
}

void worker() {
    const Paths& P = g_paths;
    try {
        for (int s = 0; s < S_COUNT; ++s) {
            check_cancel();
            {
                std::lock_guard<std::mutex> lk(g_mu);
                if (g_snap.done[s]) continue;   /* done as part of the step before */
            }
            begin_stage(s);
            bool work = true;
            switch (s) {
                case S_FOLDER: step_folder(); break;
                case S_TOOLS_DL: work = step_tools_dl(); break;
                case S_TOOLS_UNPACK: work = step_tools_unpack(); check_pack_runs(); break;
                case S_SOURCE: work = step_source(); break;
                case S_DISC: step_disc(); break;
                case S_TRANSLATE: work = step_translate(); s = std::max(s, (int)snapshot().stage); break;
                case S_COMPILE: step_compile(); break;
                case S_PLACE: step_place(); break;
                case S_CHECK: step_check(); break;
                default: break;
            }
            end_stage(s, work);
        }
        {
            std::lock_guard<std::mutex> lk(g_mu);
            g_snap.status = Status::Done;
            g_snap.stage = S_COUNT;
            g_times["total_last"] = std::to_string((int)(now() - g_t0));
        }
        log("Done in about " + std::to_string(std::max(1, (int)((now() - g_t0) / 60.0 + 0.5))) + " min.");
        log("Game folder: " + plat::u8str(P.root));
    } catch (const Fail& f) {
        log("STOPPED: " + f.title + ": " + f.text);
        std::lock_guard<std::mutex> lk(g_mu);
        g_snap.status = Status::Failed;
        g_snap.error_title = f.title;
        g_snap.error_text = f.text;
    } catch (const Cancelled&) {
        log("Cancelled. Starting again carries on from this step.");
        std::lock_guard<std::mutex> lk(g_mu);
        g_snap.status = Status::Cancelled;
    } catch (const std::exception& e) {
        log(std::string("STOPPED: ") + e.what());
        std::lock_guard<std::mutex> lk(g_mu);
        g_snap.status = Status::Failed;
        g_snap.error_title = "Something went wrong";
        g_snap.error_text = e.what();
    }
    ini::save(P.launcher / "build-times.ini", g_times);
    std::lock_guard<std::mutex> lk(g_mu);
    g_t_end = now();
    if (g_logf) { std::fclose(g_logf); g_logf = nullptr; }
    g_running = false;
}

} /* namespace */

const char* stage_name(int s) { return s >= 0 && s < S_COUNT ? kNames[s] : ""; }

const Paths& paths() { return g_paths; }

void init_paths(const fs::path& root) {
    Paths& P = g_paths;
    P.root = root;
    P.data = root / "RecompData";
    P.build = P.data / "build";
    P.src = P.build / "source";
    P.tools = P.build / (std::string("tools-") + kOs);
    P.pack = P.tools / "toolchain";
    P.temp = P.build / "temp";
    P.logs = P.build / "logs";
    P.state = P.build / "state";
    P.game = P.data / kOs;
    P.launcher = P.data / "launcher";
}

void set_options(const Options& o) { g_opts = o; }
const Options& options() { return g_opts; }

std::string source_version() { return ACG_SOURCE_VERSION; }
bool source_known() { return std::strlen(ACG_SOURCE_URL) > 0 && std::strlen(ACG_SOURCE_SHA256) == 64; }

void start(const fs::path& disc_cue) {
    reap();
    if (g_running) return;
    const Paths& P = g_paths;
    std::error_code ec;
    fs::create_directories(P.logs, ec);
    g_disc = disc_cue;
    g_cancel = false;
    g_times = ini::load(P.launcher / "build-times.ini");
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_snap = Snapshot();
        g_snap.status = Status::Running;
        g_log.clear();
        const fs::path lf = P.logs / ("launcher-" + std::string(kOs) + "-" + stamp() + ".txt");
        g_logf = plat::open_file(lf, "wb");
        g_snap.log_file = plat::u8str(lf);
    }
    log("ACG1PLUS launcher " ACG_LAUNCHER_VERSION ", preparing the game in " + plat::u8str(P.root));
    log("Disc: " + plat::u8str(disc_cue));
    g_t0 = now();
    g_running = true;
    g_worker = std::thread(worker);
}

void cancel() {
    if (!g_running) return;
    g_cancel = true;
    std::lock_guard<std::mutex> lk(g_mu);
    g_snap.status = Status::Cancelling;
}

void reap() {
    if (!g_running && g_worker.joinable()) g_worker.join();
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lk(g_mu);
    Snapshot s = g_snap;
    s.log.assign(g_log.end() - (long)std::min<size_t>(g_log.size(), 60), g_log.end());
    if (g_t0 > 0.0) s.elapsed = (g_running ? now() : g_t_end) - g_t0;
    if (s.status == Status::Running || s.status == Status::Cancelling) {
        const double t = now();
        /* Estimate: this PC's own times when known, else first-time guesses. */
        auto est = [&](int st) {
            auto it = g_times.find(kKeys[st]);
            return it != g_times.end() ? std::max(1.0, std::atof(it->second.c_str())) : kGuess[st];
        };
        double left = 0.0;
        bool unknown = false;
        for (int st = std::max(0, s.stage); st < S_COUNT; ++st) {
            if (s.done[st]) continue;
            if ((st == S_TOOLS_DL || st == S_TOOLS_UNPACK) && g_pack_ready) continue;
            if (st == s.stage) {
                const double in = t - g_stage_t0;
                /* Steps that report nothing while they work: the bar follows the
                 * usual time (never quite full until the step ends). */
                if ((st == S_NATIVE || st == S_DISC) && s.frac[st] <= 0.0f && in > 2.0)
                    s.frac[st] = (float)std::min(0.95, in / est(st));
                const float f = st == S_NATIVE || st == S_DISC ? 0.0f : s.frac[st];
                if (st == S_TOOLS_DL || (st == S_SOURCE && f < 1.0f && f > 0.0f)) {
                    /* downloads: by speed */
                    const uint64_t got = (uint64_t)((double)f * (double)(st == S_TOOLS_DL ? kPackSize : (uint64_t)ACG_SOURCE_SIZE));
                    const double speed = in > 3.0 && got > g_dl_bytes0 ? (got - g_dl_bytes0) / in : 0.0;
                    const uint64_t total = st == S_TOOLS_DL ? kPackSize : (uint64_t)ACG_SOURCE_SIZE;
                    if (speed > 0) left += (total - std::min(total, got)) / speed;
                    else unknown = true;
                } else if (f > 0.05f && in > 20.0) {
                    left += std::max(0.0, in / f - in);
                } else {
                    left += std::max(est(st) - in, est(st) * 0.1);
                }
            } else {
                left += est(st);
            }
        }
        s.eta = unknown && left < 60.0 ? -1.0 : left;
    }
    return s;
}

bool game_built(std::string* info) {
    const Paths& P = g_paths;
    std::error_code ec;
    if (!fs::exists(P.game / kExe, ec) || !fs::exists(P.game / "build-info.txt", ec)) return false;
    if (info) {
        const auto kv = ini::load(P.game / "build-info.txt");
        std::string s;
        if (kv.count("source_version")) s = kv.at("source_version");
        if (kv.count("built")) s += (s.empty() ? "" : ", ") + kv.at("built").substr(0, 10);
        *info = s;
    }
    return true;
}

bool game_outdated(std::string* built_version) {
    if (!source_known()) return false;   /* a developer build: nothing to compare */
    const Paths& P = g_paths;
    std::error_code ec;
    if (!fs::exists(P.game / "build-info.txt", ec)) return false;
    const auto kv = ini::load(P.game / "build-info.txt");
    if (built_version) *built_version = kv.count("source_version") ? kv.at("source_version") : std::string();
    auto it = kv.find("source_sha256");
    /* Built by 0.2.2 / 0.2.3 (no source_sha256): from an older source too. */
    return it == kv.end() || it->second != ACG_SOURCE_SHA256;
}

bool begin_game() {
    std::lock_guard<std::mutex> lk(g_game_mu);
    if (g_swapping) return false;
    g_game_open = true;
    return true;
}

void end_game() {
    std::lock_guard<std::mutex> lk(g_game_mu);
    g_game_open = false;
}

} /* namespace setup */

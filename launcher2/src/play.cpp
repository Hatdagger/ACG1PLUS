#include "play.h"

#include "ini.h"
#include "platform.h"
#include "setup.h"

#include <SDL3/SDL.h>

#include <chrono>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <utility>

namespace play {

namespace {

#if defined(_WIN32)
const char* kGameExe = "ACG1PLUS_Game.exe";
#else
const char* kGameExe = "ACG1PLUS_Game";
#endif
const size_t kTail = 40;

const setup::Paths& P() { return setup::paths(); }
fs::path ini_file() { return P().launcher / "launcher.ini"; }

std::vector<std::string> g_test_args;
std::unique_ptr<plat::Process> g_proc;
std::chrono::steady_clock::time_point g_started;
bool g_reported = true;

std::mutex g_mu;          /* the two below are written by the output threads */
FILE* g_log = nullptr;
std::deque<std::string> g_tail;

void on_line(int stream, const std::string& line) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_log) {
        std::fprintf(g_log, "%s%s\n", stream == 2 ? "! " : "", line.c_str());
        std::fflush(g_log);
    }
    g_tail.push_back(line);
    while (g_tail.size() > kTail) g_tail.pop_front();
}

fs::path log_path() { return P().launcher / "logs" / "game-last.log"; }

/* game.toml says nothing about the window or the volume (those are the
 * player's own), so the first start would be a 1920x1440 window at the
 * framework's default volume. The game's own launcher used to write these on
 * its first run; it is skipped now, so this one does: borderless full screen,
 * full volume. Only when the game has no settings.toml yet: anything the player
 * changed later stays. */
void first_start_settings() {
    const fs::path f = P().game / "settings.toml";
    std::error_code ec;
    if (fs::exists(f, ec)) return;
    if (FILE* out = plat::open_file(f, "wb")) {
        std::fputs("# Written by the ACG1PLUS launcher on the first start (full screen, full volume).\n"
                   "settings_format = 2\n\n"
                   "[video]\n"
                   "fullscreen        = 1\n\n"
                   "[audio]\n"
                   "volume = 100\n", out);
        std::fclose(out);
    }
}

} /* namespace */

bool full_boot() {
    const auto kv = ini::load(ini_file());
    auto it = kv.find("full_boot");
    return it != kv.end() && it->second == "1";
}

void set_full_boot(bool on) { ini::set(ini_file(), "full_boot", on ? "1" : "0"); }

void set_test_args(const std::vector<std::string>& args) { g_test_args = args; }

bool running() { return g_proc && !g_reported; }

bool start(const fs::path& disc_cue, std::string* error) {
    if (running()) { if (error) *error = "The game is already running."; return false; }
    const fs::path exe = P().game / kGameExe;
    const fs::path cards = P().root / "Memory Card";
    std::error_code ec;
    if (!fs::exists(exe, ec)) {
        if (error) *error = "The game isn't built yet (" + plat::u8str(exe) + " is missing).";
        return false;
    }
    if (disc_cue.empty() || !fs::exists(disc_cue, ec)) {
        if (error) *error = "The disc image isn't there any more: " + plat::u8str(disc_cue);
        return false;
    }
    fs::create_directories(cards, ec);
    fs::create_directories(log_path().parent_path(), ec);
    first_start_settings();
#if !defined(_WIN32)
    plat::make_executable(exe);
#endif

    std::vector<std::string> args = { plat::u8str(exe), "--no-launcher",
                                      "--disc", plat::u8str(disc_cue),
                                      "--memcard-dir", plat::u8str(cards) };
    for (const auto& a : g_test_args) args.push_back(a);

    std::vector<std::pair<std::string, std::string>> env = {
        { "ACG1PLUS_LAUNCHER", "1" },
        /* PlayStation start-up screen: shown with FULL BOOT, skipped without. */
        { "PSX_BIOS_HLE_KEEP_INTRO", full_boot() ? "1" : "0" },
    };
#if !defined(_WIN32)
    /* The launcher points Mesa's shader cache into its own folder (main.cpp);
     * the game keeps its own, so don't hand the launcher's on. */
    if (SDL_getenv("MESA_SHADER_CACHE_DIR")) env.push_back({ "MESA_SHADER_CACHE_DIR", "" });
#endif

    {
        std::lock_guard<std::mutex> lk(g_mu);
        if (g_log) { std::fclose(g_log); g_log = nullptr; }
        fs::rename(log_path(), log_path().parent_path() / "game-previous.log", ec);
        g_log = plat::open_file(log_path(), "wb");
        g_tail.clear();
        if (g_log) {
            std::fprintf(g_log, "# started by the ACG1PLUS launcher:");
            for (const auto& a : args) std::fprintf(g_log, " \"%s\"", a.c_str());
            std::fprintf(g_log, "\n# FULL BOOT %s\n", full_boot() ? "on" : "off");
            std::fflush(g_log);
        }
    }

    auto proc = std::make_unique<plat::Process>();
    std::string why;
    if (!proc->start(args, env, P().game, on_line, &why)) {
        if (error) *error = "The game could not be started: " + why;
        std::lock_guard<std::mutex> lk(g_mu);
        if (g_log) { std::fprintf(g_log, "# could not start: %s\n", why.c_str()); std::fclose(g_log); g_log = nullptr; }
        return false;
    }
    g_proc = std::move(proc);
    g_started = std::chrono::steady_clock::now();
    g_reported = false;
    return true;
}

void stop() {
    if (running()) g_proc->kill_tree();
}

bool ended(Result* out) {
    if (!g_proc || g_reported) return false;
    int code = 0;
    if (!g_proc->poll(&code)) return false;
    g_reported = true;
    Result r;
    r.code = code;
    r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_started).count();
    r.log_file = plat::u8str(log_path());
    {
        std::lock_guard<std::mutex> lk(g_mu);
        r.tail.assign(g_tail.begin(), g_tail.end());
        if (g_log) {
            std::fprintf(g_log, "# the game closed: exit code %d after %.0f s\n", code, r.seconds);
            std::fclose(g_log);
            g_log = nullptr;
        }
    }
    g_proc.reset();
    if (out) *out = std::move(r);
    return true;
}

} /* namespace play */

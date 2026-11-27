/* ACG1PLUS launcher (launcher2), ladder step 0.2.3: START GAME.
 *
 * A program of its own with no game code in it (docs/launcher-plan.md): one
 * window (1280x800, or full screen), the Armored Core system menu (menu.cpp),
 * controller first with mouse and keyboard as a fallback. 0.2.2 adds the first
 * run (firstrun.cpp): finding the disc (disc.cpp) and the "Preparing game"
 * screen that builds the game from it (setup.cpp). 0.2.3 starts the built game
 * (play.cpp): the launcher's window is hidden while it runs and comes back when
 * it closes. 0.2.4 adds UPDATES (update.cpp): newer launchers from GitHub
 * Releases (and a hidden TEST channel), the launcher replacing itself and
 * starting again, then rebuilding the game. Later: settings (0.2.5), mods (0.2.6).
 *
 * SDL3 for the window, controllers and input; Dear ImGui (the copy in
 * recomp-ui) with its SDL3 + OpenGL 3 backends for drawing. Both are linked
 * in, and the menu font is built into the program, so the launcher is a single
 * file.
 *
 * Portable: everything it writes is inside its own folder: RecompData/launcher/
 * (launcher.ini: window / full screen and the disc chosen; build-times.ini: how
 * long each build step took here, for the estimate) and, while preparing the
 * game, RecompData/build/ and Disc/ (docs/building.md).
 *
 * Options:
 *   --fullscreen / --windowed   start that way (not saved)
 *   --version                   print the version and stop
 *   --version-file FILE         write only the version into FILE and stop (the
 *                               old launcher asks a new one this before an update)
 *   --no-update-check           don't look for updates at start this time
 *   --test-releases-url URL     (tests) read the list of releases from URL
 *                               (file:// works) instead of GitHub's
 *   --test-keys a,b,c           (tests) simulated presses: up, down, left,
 *                               right, confirm, back, x, y; waitN pauses N seconds
 *   --test-shot FILE.bmp        (tests) save a picture of the window after the
 *                               presses, then close
 *   --no-first-run              don't open the first run by itself
 *   --source-zip FILE           (developers / tests) build from this source zip
 *                               instead of the release's (any checksum)
 *   --force-translate           translate the game again even if nothing changed
 *   --test-disc-md5 HEX         (tests) also accept a disc image with this MD5
 *                               (a test image from tools/build/packs_to_test_image.py)
 *   --test-auto                 (tests) go through the first run by itself
 *   --test-exit-when-done       (tests) close when the game is ready or the build stops
 *   --test-shots DIR            (tests) save a picture of every new page and step
 *                               into DIR, and one every 2 minutes while preparing
 *   --test-game-args a,b,c      (tests) extra words for the game's command line
 *                               (for example --headless)
 *   --test-no-hide              (tests) keep the launcher's window up while the
 *                               game runs (to see the "running" page)
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "disc.h"
#include "firstrun.h"
#include "ini.h"
#include "menu.h"
#include "platform.h"
#include "play.h"
#include "setup.h"
#include "ui.h"
#include "update.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef ACG_LAUNCHER_VERSION
#define ACG_LAUNCHER_VERSION "0.0.0"
#endif

/* The menu fonts, built into the program (fonts.c, made by CMake from
 * recomp-ui's LatoLatin, SIL Open Font License 1.1). */
extern "C" {
extern const unsigned char acg_font_regular[];
extern const unsigned int  acg_font_regular_size;
extern const unsigned char acg_font_bold[];
extern const unsigned int  acg_font_bold_size;
}

namespace {

namespace fs = std::filesystem;

/* ---- where things are ------------------------------------------------------- */

fs::path program_dir() {
    const char* base = SDL_GetBasePath();   /* owned by SDL, ends with a separator */
    fs::path p = base ? plat::u8path(base) : fs::current_path();
    /* No trailing separator, so paths shown to the player look plain. */
    if (!p.has_filename() && p.has_parent_path() && p != p.root_path()) p = p.parent_path();
    return p;
}

fs::path settings_file() {
    return program_dir() / "RecompData" / "launcher" / "launcher.ini";
}

bool env_set(const char* name) {
    const char* v = SDL_getenv(name);
    return v && v[0] && std::strcmp(v, "0") != 0;
}

/* Steam's Game Mode on the Deck sets SteamDeck=1; Big Picture / Game Mode
 * anywhere sets SteamGamepadUI. */
bool steam_game_mode() { return env_set("SteamDeck") || env_set("SteamGamepadUI"); }

const char* system_name() {
#if defined(_WIN32)
    return "WINDOWS";
#else
    return env_set("SteamDeck") ? "STEAM DECK" : "LINUX";
#endif
}

/* launcher.ini: "fullscreen=0/1". -1 = no saved choice. */
int load_fullscreen_choice() {
    const auto kv = ini::load(settings_file());
    auto it = kv.find("fullscreen");
    if (it == kv.end()) return -1;
    return it->second == "1" ? 1 : 0;
}

void save_fullscreen_choice(bool on) { ini::set(settings_file(), "fullscreen", on ? "1" : "0"); }

/* ---- fonts -------------------------------------------------------------------
 * ImGui 1.91 draws a font at another size by stretching its baked bitmap, so
 * each size the menu uses is baked at the size it appears on screen, and the
 * set is baked again when the window's scale changes. */
const float kSizes[] = { 10.0f, 11.0f, 13.0f, 19.0f };
const int   kSizeCount = (int)(sizeof kSizes / sizeof kSizes[0]);
ImFont* g_regular[kSizeCount] = {};
ImFont* g_bold[kSizeCount] = {};
float   g_font_scale = 0.0f;

ImFont* pick_font(float size, bool bold) {
    int best = 0;
    for (int i = 1; i < kSizeCount; ++i)
        if (std::fabs(kSizes[i] - size) < std::fabs(kSizes[best] - size)) best = i;
    return bold ? g_bold[best] : g_regular[best];
}

void bake_fonts(float pixels_per_unit) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    static const ImWchar kRanges[] = { 0x0020, 0x00FF, 0x2010, 0x2027, 0 };
    for (int i = 0; i < kSizeCount; ++i) {
        const float px = std::max(6.0f, std::round(kSizes[i] * pixels_per_unit));
        ImFontConfig cfg;
        cfg.FontDataOwnedByAtlas = false;   /* the data lives in the program */
        cfg.OversampleH = 2;
        cfg.OversampleV = 1;
        cfg.PixelSnapH = true;
        g_regular[i] = io.Fonts->AddFontFromMemoryTTF((void*)acg_font_regular,
                                                      (int)acg_font_regular_size, px, &cfg, kRanges);
        ImFontConfig bcfg = cfg;
        g_bold[i] = io.Fonts->AddFontFromMemoryTTF((void*)acg_font_bold,
                                                   (int)acg_font_bold_size, px, &bcfg, kRanges);
    }
    io.FontDefault = g_regular[2];
    ImGui_ImplOpenGL3_DestroyFontsTexture();
    ImGui_ImplOpenGL3_CreateFontsTexture();
    g_font_scale = pixels_per_unit;
}

/* ---- tests -------------------------------------------------------------------- */

std::vector<std::string> split_commas(const char* s) {
    std::vector<std::string> out;
    std::string cur;
    for (; s && *s; ++s) {
        if (*s == ',') { if (!cur.empty()) out.push_back(cur); cur.clear(); }
        else cur += *s;
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

bool save_shot(SDL_Window* win, const char* path) {
    typedef void (*ReadPixelsFn)(int, int, int, int, unsigned, unsigned, void*);
    auto read_pixels = (ReadPixelsFn)SDL_GL_GetProcAddress("glReadPixels");
    if (!read_pixels) return false;
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(win, &w, &h);
    std::vector<unsigned char> px((size_t)w * h * 4), flipped(px.size());
    read_pixels(0, 0, w, h, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, px.data());
    for (int y = 0; y < h; ++y)
        std::memcpy(&flipped[(size_t)y * w * 4], &px[(size_t)(h - 1 - y) * w * 4], (size_t)w * 4);
    SDL_Surface* s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, flipped.data(), w * 4);
    if (!s) return false;
    const bool ok = SDL_SaveBMP(s, path);
    SDL_DestroySurface(s);
    return ok;
}

} /* namespace */

int main(int argc, char** argv) {
    int force_fullscreen = -1;
    const char* test_shot = nullptr;
    const char* test_shots = nullptr;
    bool no_first_run = false, test_auto = false, test_exit = false, test_no_hide = false;
    std::vector<std::string> test_keys;
    setup::Options opts;
    bool no_update_check = false;
    /* For the restart after an update: the same options, without the test
     * presses and picture (they were for this run). */
    std::vector<std::string> all_args;
    for (int i = 1; i < argc; ++i) {
        if ((!std::strcmp(argv[i], "--test-keys") || !std::strcmp(argv[i], "--test-shot")) && i + 1 < argc) { ++i; continue; }
        all_args.push_back(argv[i]);
    }
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (!std::strcmp(a, "--version-file") && i + 1 < argc) {
            /* Before anything else (no window): the updater's "does it start here" check. */
            FILE* f = plat::open_file(plat::u8path(argv[i + 1]), "wb");
            if (!f) return 1;
            std::fprintf(f, "%s\n", ACG_LAUNCHER_VERSION);
            return std::fclose(f) == 0 ? 0 : 1;
        }
        if (!std::strcmp(a, "--version")) {
            std::printf("ACG1PLUS launcher %s (source %s)\n", ACG_LAUNCHER_VERSION,
                        setup::source_known() ? setup::source_version().c_str() : "none: developer build");
            return 0;
        }
        else if (!std::strcmp(a, "--fullscreen")) force_fullscreen = 1;
        else if (!std::strcmp(a, "--windowed")) force_fullscreen = 0;
        else if (!std::strcmp(a, "--test-shot") && i + 1 < argc) test_shot = argv[++i];
        else if (!std::strcmp(a, "--test-keys") && i + 1 < argc) test_keys = split_commas(argv[++i]);
        else if (!std::strcmp(a, "--test-shots") && i + 1 < argc) test_shots = argv[++i];
        else if (!std::strcmp(a, "--no-first-run")) no_first_run = true;
        else if (!std::strcmp(a, "--test-auto")) test_auto = true;
        else if (!std::strcmp(a, "--test-exit-when-done")) test_exit = true;
        else if (!std::strcmp(a, "--force-translate")) opts.force_translate = true;
        else if (!std::strcmp(a, "--source-zip") && i + 1 < argc) opts.source_zip = fs::absolute(plat::u8path(argv[++i]));
        else if (!std::strcmp(a, "--test-disc-md5") && i + 1 < argc) disc::accept_md5(argv[++i]);
        else if (!std::strcmp(a, "--test-game-args") && i + 1 < argc) play::set_test_args(split_commas(argv[++i]));
        else if (!std::strcmp(a, "--test-no-hide")) test_no_hide = true;
        else if (!std::strcmp(a, "--no-update-check")) no_update_check = true;
        else if (!std::strcmp(a, "--test-releases-url") && i + 1 < argc) update::set_releases_url(argv[++i]);
    }
    setup::init_paths(program_dir());
    setup::set_options(opts);
#if !defined(_WIN32)
    /* Portable: Mesa (the Deck's and most Linux graphics drivers) keeps a shader
     * cache in ~/.cache unless told otherwise; keep the launcher's in its folder
     * (only if nobody, e.g. Steam, has chosen a place already). */
    if (!SDL_getenv("MESA_SHADER_CACHE_DIR")) {
        const std::string c = plat::u8str(setup::paths().launcher / "cache" / "mesa");
        SDL_setenv_unsafe("MESA_SHADER_CACHE_DIR", c.c_str(), 1);
    }
#endif
    firstrun::set_test_auto(test_auto, test_exit);

    SDL_SetAppMetadata("ACG1PLUS", ACG_LAUNCHER_VERSION, "io.github.hatdagger.acg1plus");
    /* The Deck's own controller and Steam's virtual one both come through as
     * gamepads; keep reading them when the window isn't focused (Steam's
     * overlay takes focus for a moment). */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "ACG1PLUS", SDL_GetError(), nullptr);
        return 1;
    }

    /* OpenGL 3.2 core; if the driver won't, whatever it offers (the ImGui
     * backend copes with GL 2.x and up). */
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);

    bool fullscreen = steam_game_mode();
    if (const int saved = load_fullscreen_choice(); saved >= 0) fullscreen = saved == 1;
    if (force_fullscreen >= 0) fullscreen = force_fullscreen == 1;

    /* The window: 1280x800 (the Deck's screen), or 90% of a smaller screen in
     * the same shape. Full screen starts at the screen's own size. */
    int win_w = 1280, win_h = 800;
    if (const SDL_DisplayMode* dm = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay())) {
        if (fullscreen) { win_w = dm->w; win_h = dm->h; }
        else {
            const float fit = std::min(1.0f, std::min(dm->w * 0.9f / win_w, dm->h * 0.9f / win_h));
            if (fit < 1.0f) { win_w = (int)(win_w * fit); win_h = (int)(win_h * fit); }
        }
    }

    const SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN |
                                  (fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
    SDL_Window* win = SDL_CreateWindow("ACG1PLUS", win_w, win_h, flags);
    SDL_GLContext gl = win ? SDL_GL_CreateContext(win) : nullptr;
    const char* glsl = "#version 150";
    if (win && !gl) {
        SDL_DestroyWindow(win);
        SDL_GL_ResetAttributes();
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        win = SDL_CreateWindow("ACG1PLUS", win_w, win_h, flags);
        gl = win ? SDL_GL_CreateContext(win) : nullptr;
        glsl = "#version 120";
    }
    if (!win || !gl) {
        std::string msg = "Could not open the launcher window (OpenGL): ";
        msg += SDL_GetError();
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "ACG1PLUS", msg.c_str(), nullptr);
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(win, 640, 400);
    SDL_GL_MakeCurrent(win, gl);
    SDL_GL_SetSwapInterval(1);
    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(win);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   /* portable: ImGui writes no files */
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   /* lets the backend feed gamepad keys */
    ImGui_ImplSDL3_InitForOpenGL(win, gl);
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoAll);
    ImGui_ImplOpenGL3_Init(glsl);

    /* After an update: the UPDATES page says so (and offers UPDATE THE GAME). */
    update::init(all_args);
    const std::string updated_from = update::take_updated_from();
    if (!updated_from.empty()) menu::open_updates(updated_from);
    if (!no_update_check && update::check_at_start()) update::check();

    /* First start (no game built yet): straight into the first run. */
    if (!no_first_run && !setup::game_built(nullptr)) firstrun::open();
    bool restart_after = false;

    std::string controller;
    bool running = true;
    std::string shot_key_seen;
    int shot_due = -1, shot_count = 0;
    double next_prep_shot = 0.0;
    bool cursor_hidden = false;
    int frame = 0;
    size_t next_key = 0;
    /* Test presses: one every 10 frames from frame 40; "waitN" pauses N seconds.
     * The --test-shot picture is taken 30 frames after the last one. */
    int key_frame = 40;
    double key_wait_until = 0.0;
    int shot_frame = -1;
    bool hidden_for_game = false;

    while (running) {
        /* The game has closed: the launcher comes back. What was pressed to
         * close the game is not taken as a choice here. */
        play::Result game_result;
        if (play::ended(&game_result)) {
            if (hidden_for_game) {
                SDL_ShowWindow(win);
                SDL_RaiseWindow(win);
                hidden_for_game = false;
            }
            SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
            io.ClearInputKeys();
            io.ClearInputMouse();
            ui::hold_input(20);
            key_frame = std::max(key_frame, frame + 30);
            menu::game_closed(game_result);
        }
        /* While the game runs with the launcher hidden: nothing is drawn and
         * nothing the controller does reaches the menu (the launcher keeps
         * reading controllers in the background, see above). */
        if (hidden_for_game) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev))
                if (ev.type == SDL_EVENT_QUIT) running = false;
            if (!running) break;
            SDL_Delay(50);
            continue;
        }

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL3_ProcessEvent(&ev);
            switch (ev.type) {
                case SDL_EVENT_QUIT: running = false; break;
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    if (ev.window.windowID == SDL_GetWindowID(win)) running = false;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (!ev.key.repeat && (ev.key.key == SDLK_F11 ||
                        (ev.key.key == SDLK_RETURN && (ev.key.mod & SDL_KMOD_ALT)))) {
                        fullscreen = !fullscreen;
                        SDL_SetWindowFullscreen(win, fullscreen);
                        save_fullscreen_choice(fullscreen);
                    }
                    break;
                /* Controller in use: hide the mouse pointer; mouse moved: show it. */
                case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                    if (ev.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && std::abs(ev.gaxis.value) < 16000) break;
                    if (!cursor_hidden) { SDL_HideCursor(); cursor_hidden = true; }
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (cursor_hidden) { SDL_ShowCursor(); cursor_hidden = false; }
                    break;
                default: break;
            }
        }
        if (SDL_GetWindowFlags(win) & SDL_WINDOW_MINIMIZED) { SDL_Delay(50); continue; }

        /* First connected controller's name, for STATUS. */
        controller.clear();
        int pads = 0;
        if (SDL_JoystickID* ids = SDL_GetGamepads(&pads)) {
            if (pads > 0) if (const char* n = SDL_GetGamepadNameForID(ids[0])) controller = n;
            SDL_free(ids);
        }

        int w = 0, h = 0, pw = 0, ph = 0;
        SDL_GetWindowSize(win, &w, &h);
        SDL_GetWindowSizeInPixels(win, &pw, &ph);
        const float density = w > 0 ? (float)pw / (float)w : 1.0f;
        const float want = menu::scale_for(w, h) * density;
        if (std::fabs(want - g_font_scale) > 0.01f) bake_fonts(want);

        if (next_key < test_keys.size() && frame >= key_frame && SDL_GetTicks() / 1000.0 >= key_wait_until) {
            const std::string& k = test_keys[next_key++];
            if (k.rfind("wait", 0) == 0) key_wait_until = SDL_GetTicks() / 1000.0 + std::atof(k.c_str() + 4);
            else menu::inject(k.c_str());
            key_frame = frame + 10;
        }
        if (test_shot && shot_frame < 0 && next_key >= test_keys.size() && SDL_GetTicks() / 1000.0 >= key_wait_until)
            shot_frame = std::max(frame, key_frame) + 30;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        menu::Host host;
        host.version = ACG_LAUNCHER_VERSION;
        host.system = system_name();
        host.controller = controller.c_str();
        host.fullscreen = fullscreen;
        host.width = pw;
        host.height = ph;
        /* The menu asks for sizes in design units; fonts are baked in pixels
         * for the current scale, so its size argument is the design size. */
        host.font = [](float size, bool bold) { return pick_font(size, bold); };
        menu::Requests req;
        menu::draw(host, req);
        if (req.quit) running = false;
        /* A new launcher is in place: close this one and start it. */
        if (update::take_restart()) { restart_after = true; running = false; }
        if (req.game_started && !test_no_hide) hidden_for_game = true;
        if (req.toggle_fullscreen) {
            fullscreen = !fullscreen;
            SDL_SetWindowFullscreen(win, fullscreen);
            save_fullscreen_choice(fullscreen);
        }

        ImGui::Render();
        {
            typedef void (*ViewportFn)(int, int, int, int);
            typedef void (*ClearColorFn)(float, float, float, float);
            typedef void (*ClearFn)(unsigned);
            static auto vpf = (ViewportFn)SDL_GL_GetProcAddress("glViewport");
            static auto ccf = (ClearColorFn)SDL_GL_GetProcAddress("glClearColor");
            static auto clf = (ClearFn)SDL_GL_GetProcAddress("glClear");
            if (vpf) vpf(0, 0, pw, ph);
            if (ccf) ccf(6 / 255.0f, 9 / 255.0f, 13 / 255.0f, 1.0f);
            if (clf) clf(0x00004000 /* GL_COLOR_BUFFER_BIT */);
        }
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (frame == shot_frame) {
            if (!save_shot(win, test_shot)) std::fprintf(stderr, "could not save %s\n", test_shot);
            running = false;
        }
        /* --test-shots: a picture of every new page / step (a few frames after it
         * appears, so it has drawn), and one every 2 minutes while preparing. */
        if (test_shots) {
            const std::string k = firstrun::active() ? firstrun::shot_key() : "menu" + std::to_string(menu::page());
            if (k != shot_key_seen) { shot_key_seen = k; shot_due = frame + 12; }
            const double t = ImGui::GetTime();
            if (firstrun::preparing() && t >= next_prep_shot) {
                if (next_prep_shot > 0.0 && shot_due < 0) shot_due = frame;
                next_prep_shot = t + 120.0;
            }
            if (frame == shot_due) {
                char name[256];
                std::error_code fe;
                do   /* after an update's restart, the new launcher carries on numbering */
                    std::snprintf(name, sizeof name, "%s/%03d-%s.bmp", test_shots, shot_count++, k.c_str());
                while (fs::exists(plat::u8path(name), fe));
                if (!save_shot(win, name)) std::fprintf(stderr, "could not save %s\n", name);
                shot_due = -1;
            }
        }
        SDL_GL_SwapWindow(win);
        ++frame;
        /* Hidden after this frame was shown (the "running" page), so a window
         * that can't be hidden still shows something sensible. */
        if (hidden_for_game) SDL_HideWindow(win);
    }

    /* Closing the launcher while the game runs (only possible where its window
     * can't be hidden, or when the system asks it to close): the game too. */
    if (play::running()) {
        play::stop();
        for (int i = 0; i < 100 && !play::ended(nullptr); ++i) SDL_Delay(50);
    }

    /* Closing while preparing: stop the build (everything it started), so it
     * carries on from that step next time. */
    setup::cancel();
    for (;;) {
        const setup::Status st = setup::snapshot().status;
        if (st != setup::Status::Running && st != setup::Status::Cancelling) break;
        SDL_Delay(50);
    }
    setup::reap();
    disc::stop();
    if (!restart_after) update::shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(win);
    SDL_Quit();
    if (restart_after) update::restart();   /* Linux: doesn't come back unless it failed */
    return 0;
}

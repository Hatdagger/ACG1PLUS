/* ACG1PLUS launcher (launcher2), ladder step 0.2.1: the shell.
 *
 * A program of its own with no game code in it (docs/launcher-plan.md): one
 * window (1280x800, or full screen), the Armored Core system menu (menu.cpp),
 * controller first with mouse and keyboard as a fallback. Later steps add the
 * disc finder and the "Preparing game" build screen (0.2.2), starting the game
 * (0.2.3), updates (0.2.4), settings (0.2.5) and mods (0.2.6).
 *
 * SDL3 for the window, controllers and input; Dear ImGui (the copy in
 * recomp-ui) with its SDL3 + OpenGL 3 backends for drawing. Both are linked
 * in, and the menu font is built into the program, so the launcher is a single
 * file.
 *
 * Portable: the only thing it writes is RecompData/launcher/launcher.ini
 * beside itself (the window / full screen choice), and only when that choice
 * changes.
 *
 * Options:
 *   --fullscreen / --windowed   start that way (not saved)
 *   --version                   print the version and stop
 *   --test-keys a,b,c           (tests) simulated presses: up, down, left,
 *                               right, confirm, back
 *   --test-shot FILE.bmp        (tests) save a picture of the window after the
 *                               presses, then close
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "menu.h"

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
    return base ? fs::u8path(base) : fs::current_path();
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
    std::ifstream f(settings_file());
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("fullscreen=", 0) == 0) return line.size() > 11 && line[11] == '1' ? 1 : 0;
    }
    return -1;
}

void save_fullscreen_choice(bool on) {
    std::error_code ec;
    const fs::path p = settings_file();
    fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::trunc);
    if (f) f << "fullscreen=" << (on ? 1 : 0) << "\n";
}

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
    std::vector<std::string> test_keys;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (!std::strcmp(a, "--version")) { std::printf("ACG1PLUS launcher %s\n", ACG_LAUNCHER_VERSION); return 0; }
        else if (!std::strcmp(a, "--fullscreen")) force_fullscreen = 1;
        else if (!std::strcmp(a, "--windowed")) force_fullscreen = 0;
        else if (!std::strcmp(a, "--test-shot") && i + 1 < argc) test_shot = argv[++i];
        else if (!std::strcmp(a, "--test-keys") && i + 1 < argc) test_keys = split_commas(argv[++i]);
    }

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

    std::string controller;
    bool running = true;
    bool cursor_hidden = false;
    int frame = 0;
    size_t next_key = 0;
    int shot_frame = test_shot ? 40 + 10 * (int)test_keys.size() + 30 : -1;

    while (running) {
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

        if (next_key < test_keys.size() && frame >= 40 + 10 * (int)next_key)
            menu::inject(test_keys[next_key++].c_str());

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
        SDL_GL_SwapWindow(win);
        ++frame;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

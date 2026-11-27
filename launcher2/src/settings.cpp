/* SETTINGS (ladder 0.2.5). See settings.h for where everything is saved.
 *
 * The rows, their words and their order follow the owner's own settings pages
 * in launcher/moa_menu.inc (the launcher's allowed exception to "don't copy old
 * code"). What was reconsidered on the way:
 *   - The old pages lived inside the game and changed values through the
 *     framework's launcher model and mod provider. This launcher is its own
 *     program, so each row names the settings.toml line it owns and writes it
 *     straight to the file, keeping every other line.
 *   - Allowed values of the framework's player settings come from
 *     player_settings.h (built in), and are checked with the framework's own
 *     player_setting_valid before they are offered.
 *   - A row with no saved value shows the value the game will really use:
 *     game.toml's (built in, or the copy beside the built game), else the
 *     framework's default (written in the row).
 *   - Dropped: SKIP BIOS INTRO (START GAME's FULL BOOT does that now), SKIP
 *     THIS MENU (the game's own menu is skipped always), STOCK SETTINGS PAGE
 *     (no in-game menu to open), ASPECT choices (Phantasma has no widescreen
 *     support yet: shown, fixed at 4:3), MSAA ANTIALIASING (the PlayStation
 *     build never offered it) and MENU SOUNDS (they need the disc's sound
 *     files read: a later step).
 *   - BUTTON LAYOUT is one page for player 1: the controller button and the
 *     keyboard key of each PlayStation button side by side. */
#include "settings.h"

#include "kvfile.h"
#include "platform.h"
#include "setup.h"

#include "player_settings.h"   /* psxrecomp/recompiler/src: the framework's player settings */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

/* game.toml, built in (CMake: cmake/embed_text.cmake). */
extern "C" {
extern const unsigned char acg_game_toml[];
extern const unsigned int  acg_game_toml_size;
}

namespace settings {

using namespace ui;
namespace fs = std::filesystem;

namespace {

/* ---- files ------------------------------------------------------------------- */

fs::path settings_path() { return setup::paths().game / "settings.toml"; }
fs::path input_path()    { return setup::paths().game / "input.ini"; }
fs::path keys_path()     { return setup::paths().game / "keybinds.ini"; }

std::vector<std::string> split(const char* list) {
    std::vector<std::string> out;
    std::string w;
    for (const char* p = list ? list : ""; ; ++p) {
        if (*p == ',' || !*p) {
            size_t a = 0, b = w.size();
            while (a < b && w[a] == ' ') ++a;
            while (b > a && w[b - 1] == ' ') --b;
            if (b > a) out.push_back(w.substr(a, b - a));
            w.clear();
            if (!*p) break;
        } else w += *p;
    }
    return out;
}

/* ---- the rows ---------------------------------------------------------------- */

enum Tab { TAB_DISPLAY = 0, TAB_PICTURE, TAB_SYSTEM, TAB_ADVANCED, TAB__COUNT };
const char* const kTabNames[TAB__COUNT] = { "DISPLAY", "PICTURE", "SOUND / SYSTEM", "ADVANCED" };

enum Kind {
    K_CHOICE,    /* one of values (LEFT / RIGHT walk them) */
    K_FIXED,     /* shown, can't change here */
    K_BUTTONS,   /* opens BUTTON LAYOUT */
    K_RESTORE,   /* puts every row back */
};

struct Row {
    int         tab;
    const char* label;
    const char* info;
    const char* section;
    const char* key;
    Kind        kind;
    const char* values;     /* as written in the file, comma separated */
    const char* labels;     /* shown for each value (same count); null: the values */
    bool        quoted;     /* a TOML string */
    const char* fallback;   /* the framework's default, when no file sets it */
    const char* needs;      /* "section.key=value": shown only then */
    bool        core;       /* a settings.toml key of the framework's own (not in player_settings.h) */
};

#define ONOFF "false,true", "OFF,ON", false

const Row kRows[] = {
    /* DISPLAY */
    { TAB_DISPLAY, "ASPECT", "The shape of the picture. Project Phantasma is shown in its original "
      "4:3 shape; wider shapes need the game's widescreen support, which comes later.",
      "video", "aspect_ratio", K_FIXED, nullptr, nullptr, true, "4:3", nullptr, true },
    { TAB_DISPLAY, "FULLSCREEN", "A window, borderless full screen (quick to switch away from) or "
      "exclusive full screen.",
      "video", "fullscreen", K_CHOICE, "0,1,2", "WINDOW,BORDERLESS,EXCLUSIVE", false, "0", nullptr, true },
    { TAB_DISPLAY, "WINDOW SIZE", "Size of the game's window when FULLSCREEN is set to WINDOW.",
      "video", "window_width", K_CHOICE, "960,1280,1600,1920", "960 x 720,1280 x 960,1600 x 1200,1920 x 1440",
      false, "960", "video.fullscreen=0", true },
    { TAB_DISPLAY, "RENDERER", "How the picture is drawn. OpenGL is the fast, sharp one; software is "
      "the fallback for unusual graphics cards.",
      "video", "renderer", K_CHOICE, "opengl,software", "OPENGL,SOFTWARE", true, "opengl", nullptr, true },
    { TAB_DISPLAY, "INTERNAL RESOLUTION", "How finely the 3D picture is drawn. Higher is sharper and needs "
      "a faster graphics card. NATIVE is the PlayStation's own; MATCH SCREEN follows your display.",
      "video", "internal_resolution", K_CHOICE, "native,720p,1080p,1440p,4k,5k,8k,display",
      "NATIVE,720P,1080P,1440P,4K,5K,8K,MATCH SCREEN", true, "4x", nullptr, false },
    { TAB_DISPLAY, "AUTOMATIC RESOLUTION", "Lowers the internal resolution for a moment when the graphics "
      "card falls behind, so the frame rate holds.",
      "video", "dynamic_resolution", K_CHOICE, ONOFF, "false", nullptr, false },
    { TAB_DISPLAY, "VSYNC", "Matches the picture to the display to avoid tearing. ADAPTIVE lets a late "
      "frame through instead of waiting.",
      "video", "vsync", K_CHOICE, "on,immediate,adaptive", "ON,OFF,ADAPTIVE", true, "on", nullptr, true },
    { TAB_DISPLAY, "SMOOTH MOTION", "Draws extra frames between the game's own, so movement looks "
      "smoother on high refresh displays.",
      "video", "frame_interpolation", K_CHOICE, ONOFF, "false", nullptr, true },
    { TAB_DISPLAY, "SMOOTH MOTION RATE", "How many frames a second smooth motion aims for. DISPLAY "
      "REFRESH follows your screen.",
      "video", "frame_interpolation_fps", K_CHOICE, "0,90,120,144,165,240",
      "DISPLAY REFRESH,90 FPS,120 FPS,144 FPS,165 FPS,240 FPS", false, "0", "video.frame_interpolation=true", true },

    /* PICTURE */
    { TAB_PICTURE, "TEXTURE FILTER", "Smooths textures. NEAREST keeps the original sharp pixels; STABLE "
      "and XBR smooth them in different ways.",
      "video", "texture_filtering", K_CHOICE, "nearest,bilinear,stable,xbr", "NEAREST,BILINEAR,STABLE,XBR",
      true, "nearest", nullptr, false },
    { TAB_PICTURE, "DITHERING", "The PS1's fine dot pattern. OFF is clean at high resolution; ON is the "
      "original look; FINE keeps it but smaller.",
      "video", "dithering", K_CHOICE, "off,on,fine", "OFF,ON,FINE", true, "on", nullptr, false },
    { TAB_PICTURE, "STEADY POLYGONS", "Stops the PS1's shaking, wobbling 3D (PGXP vertex correction). "
      "It made the game run poorly before: try it, and turn it off if the game slows down.",
      "video", "geometry_correction", K_CHOICE, ONOFF, "false", nullptr, false },
    { TAB_PICTURE, "STEADY TEXTURES", "Stops textures warping across walls and floors "
      "(perspective-correct texturing). Needs STEADY POLYGONS.",
      "video", "perspective_texturing", K_CHOICE, ONOFF, "false", "video.geometry_correction=true", false },
    { TAB_PICTURE, "MOVIE FILTER", "How the intro and ending movies are scaled up.",
      "video", "fmv_filter", K_CHOICE, "nearest,bilinear,sharp,bicubic", "NEAREST,BILINEAR,SHARP,BICUBIC",
      true, "nearest", nullptr, false },
    { TAB_PICTURE, "SCREEN MODEL", "Imitates an old TV: CRT, composite cable or Trinitron. RAW is the "
      "clean picture.",
      "video", "crt_filter", K_CHOICE, "raw,crt,composite,trinitron", "RAW,CRT,COMPOSITE,TRINITRON",
      true, "raw", nullptr, false },
    { TAB_PICTURE, "SCANLINES", "Dark lines between picture lines, as on a TV.",
      "video", "scanlines", K_CHOICE, ONOFF, "false", nullptr, false },
    { TAB_PICTURE, "SCANLINE STRENGTH", "How dark the scanlines are.",
      "video", "scanline_strength", K_CHOICE, "0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1",
      "10%,20%,30%,40%,50%,60%,70%,80%,90%,100%", false, "0.5", "video.scanlines=true", false },

    /* SOUND / SYSTEM */
    { TAB_SYSTEM, "VOLUME", "Overall sound level of the game.",
      "audio", "volume", K_CHOICE, "0,5,10,15,20,25,30,35,40,45,50,55,60,65,70,75,80,85,90,95,100", nullptr,
      false, "100", nullptr, true },
    { TAB_SYSTEM, "HIGH-QUALITY SOUND", "A cleaner mix of the PS1 sound chip.",
      "audio", "spu_hq", K_CHOICE, ONOFF, "false", nullptr, false },
    { TAB_SYSTEM, "BUTTON LAYOUT", "Change which controller button and which keyboard key does what in "
      "the game.",
      nullptr, nullptr, K_BUTTONS, nullptr, nullptr, false, nullptr, nullptr, true },
    { TAB_SYSTEM, "REWIND", "Keeps the last few seconds so the Rewind hotkey can step back. Costs some "
      "speed.",
      "video", "rewind", K_CHOICE, ONOFF, "false", nullptr, false },
    { TAB_SYSTEM, "RESTORE DEFAULTS", "Put every setting on these pages back to how it was first "
      "installed (full screen, full volume, the game's own look). The button layout has its own.",
      nullptr, nullptr, K_RESTORE, nullptr, nullptr, false, nullptr, nullptr, true },

    /* ADVANCED */
    { TAB_ADVANCED, "ANISOTROPIC FILTER", "Keeps floor and wall textures sharp at a slant.",
      "video", "anisotropic_filtering", K_CHOICE, "1,2,4,8,16", "OFF,2X,4X,8X,16X", false, "1", nullptr, false },
    { TAB_ADVANCED, "TEXTURE MIPMAPS", "Calms shimmering on far-off textures.",
      "video", "texture_lod", K_CHOICE, "off,mipmap", "OFF,ON", true, "off", nullptr, false },
    { TAB_ADVANCED, "EDGE SMOOTHING", "A cheap smoothing pass over the finished picture (FXAA).",
      "video", "antialiasing_mode", K_CHOICE, "off,fxaa,fxaa_hq", "OFF,FXAA,FXAA HIGH", true, "off", nullptr, false },
    { TAB_ADVANCED, "SMOOTH SCALING", "Blends pixels when the picture is scaled to the window.",
      "video", "antialiasing", K_CHOICE, ONOFF, "true", nullptr, false },
    { TAB_ADVANCED, "BLOOM", "Glow around bright lights and explosions. 0 is off.",
      "video", "bloom", K_CHOICE, "0,0.25,0.5,0.75,1,1.5,2", "OFF,0.25,0.5,0.75,1,1.5,2", false, "0", nullptr, false },
    { TAB_ADVANCED, "ACCURATE BLENDING", "Draws see-through effects exactly as the PS1 did. Leave on "
      "unless a graphics card struggles.",
      "video", "accurate_blending", K_CHOICE, ONOFF, "true", nullptr, false },
    { TAB_ADVANCED, "MOVIE COLOR SMOOTHING", "Softens blocky colour edges in the movies.",
      "video", "fmv_chroma_smoothing", K_CHOICE, ONOFF, "false", nullptr, false },
    { TAB_ADVANCED, "SUPERSAMPLING", "Draws above the internal resolution and scales down. Very costly.",
      "video", "supersample", K_CHOICE, "1,1.5,2,3,4", "OFF,1.5X,2X,3X,4X", false, "1", nullptr, false },
    { TAB_ADVANCED, "LOWEST AUTO RESOLUTION", "The floor AUTOMATIC RESOLUTION never drops below.",
      "video", "dynamic_resolution_min", K_CHOICE, "native,720p,1080p,1440p", "NATIVE,720P,1080P,1440P",
      true, "720p", "video.dynamic_resolution=true", false },
    { TAB_ADVANCED, "SMOOTH MOTION METHOD", "REDRAW makes real in-between frames; REPROJECTION is "
      "cheaper but can smear.",
      "video", "frame_generation_method", K_CHOICE, "redraw,reprojection", "REDRAW,REPROJECTION", true,
      "redraw", "video.frame_interpolation=true", false },
    { TAB_ADVANCED, "TEXTURE BATCHING", "Speeds up drawing of repeating textures. Turn off if textures "
      "look wrong.",
      "video", "texture_window_batching", K_CHOICE, ONOFF, "false", nullptr, false },
    { TAB_ADVANCED, "PGXP CPU MODE", "Steadier geometry for more of the game's maths. Slower; try it if "
      "polygons still shake.",
      "video", "pgxp_cpu_mode", K_CHOICE, ONOFF, "false", "video.geometry_correction=true", false },
    { TAB_ADVANCED, "PGXP PRECISE CULLING", "Fewer gaps at polygon edges. Never used in netplay.",
      "video", "pgxp_culling", K_CHOICE, ONOFF, "false", "video.geometry_correction=true", false },
    { TAB_ADVANCED, "PGXP COLOR CORRECTION", "Corrects colour on steadied polygons.",
      "video", "pgxp_color_correction", K_CHOICE, ONOFF, "false", "video.geometry_correction=true", false },
    { TAB_ADVANCED, "PGXP SEAM FIX", "Closes thin cracks between polygons.",
      "video", "pgxp_seam", K_CHOICE, "off,fine,wide", "OFF,FINE,WIDE", true, "off",
      "video.geometry_correction=true", false },
    { TAB_ADVANCED, "PGXP DEPTH BUFFER", "Sorts overlapping polygons by real depth.",
      "video", "pgxp_depth_buffer", K_CHOICE, ONOFF, "false", "video.geometry_correction=true", false },
    { TAB_ADVANCED, "AUDIO BUFFER (MS)", "Lower reacts faster; raise it if sound crackles.",
      "audio", "buffer_ms", K_CHOICE, "30,40,50,60,80,100,150,180,200,300,500", nullptr, false, "180",
      nullptr, false },
    { TAB_ADVANCED, "ANALOG ANTI-DEADZONE", "Pushes small stick movements past the game's own dead zone. "
      "0 is off.",
      "controller", "anti_deadzone", K_CHOICE, "0,1000,2000,3000,4000,6000,8000", "OFF,1000,2000,3000,4000,6000,8000",
      false, "0", nullptr, false },
    { TAB_ADVANCED, "SELECT SHORTCUTS", "SELECT plus a button reaches the game window's own functions on "
      "pads without spare buttons.",
      "controller", "select_shortcuts", K_CHOICE, ONOFF, "true", nullptr, false },
    { TAB_ADVANCED, "LOW LATENCY INPUT", "Reads the pad again just before each frame is shown.",
      "video", "low_latency_input", K_CHOICE, ONOFF, "true", nullptr, true },
    { TAB_ADVANCED, "REWIND BUFFER", "How far back rewind can go.",
      "video", "rewind_depth", K_CHOICE, "50,100,150,200", nullptr, false, "50", "video.rewind=true", false },
    { TAB_ADVANCED, "REWIND INTERVAL", "How often rewind saves a step (in frames). Fewer is finer and "
      "slower.",
      "video", "rewind_interval", K_CHOICE, "1,4,8,12,15", nullptr, false, "15", "video.rewind=true", false },
};
#undef ONOFF
const int kRowCount = (int)(sizeof kRows / sizeof kRows[0]);

/* What is offered for a row once the framework's rules are applied. */
struct Offer {
    bool shown = false;
    std::vector<std::string> values, labels;
};

/* ---- state ------------------------------------------------------------------- */

struct State {
    bool open = false;
    bool controls = false;          /* BUTTON LAYOUT page */
    int  tab = TAB_DISPLAY;
    int  cursor[TAB__COUNT] = {};
    int  top[TAB__COUNT] = {};
    double restore_armed = 0.0;     /* RESTORE DEFAULTS: second press within 3 s */
    kv::File file;                  /* settings.toml */
    kv::File game_toml;             /* game.toml: the game's own defaults */
    std::vector<Offer> offers;
    bool offers_ready = false;
    /* BUTTON LAYOUT */
    int  c_row = 0, c_col = 0;
    int  capture = -1;              /* column being set (0 controller, 1 keyboard), -1 none */
    double capture_until = 0.0;
    double ctrl_reset_armed = 0.0;
    kv::File input, keys;
};
State S;

const PSXRecompV4::PlayerSettingSpec* spec_of(const Row& r) {
    if (!r.section || !r.key) return nullptr;
    for (const auto& s : PSXRecompV4::kExtraPlayerSettings)
        if (!std::strcmp(s.section, r.section) && !std::strcmp(s.key, r.key)) return &s;
    return nullptr;
}

void build_offers() {
    S.offers.assign(kRowCount, Offer());
    for (int i = 0; i < kRowCount; ++i) {
        const Row& r = kRows[i];
        Offer& o = S.offers[i];
        if (r.kind != K_CHOICE) { o.shown = true; continue; }
        const PSXRecompV4::PlayerSettingSpec* spec = spec_of(r);
        /* A framework player setting the framework no longer has: hidden, so
         * nothing the game doesn't read is written. */
        if (!r.core && !spec) continue;
        const auto vals = split(r.values);
        const auto labs = r.labels ? split(r.labels) : std::vector<std::string>();
        for (size_t v = 0; v < vals.size(); ++v) {
            if (spec && !PSXRecompV4::player_setting_valid(*spec, vals[v])) continue;
            o.values.push_back(vals[v]);
            o.labels.push_back(v < labs.size() ? labs[v] : upper(vals[v]));
        }
        o.shown = !o.values.empty();
    }
    S.offers_ready = true;
}

void reload() {
    S.file.load(settings_path());
    const fs::path installed = setup::paths().game / "game.toml";
    std::error_code ec;
    if (!fs::exists(installed, ec) || !S.game_toml.load(installed))
        S.game_toml.load_text(std::string((const char*)acg_game_toml, acg_game_toml_size));
    if (!S.offers_ready) build_offers();
}

/* The value the game will use, as plain text (quotes taken off), and whether
 * the player's settings.toml sets it. */
std::string effective(const char* section, const char* key, const char* fallback, bool* saved = nullptr) {
    std::string raw;
    if (saved) *saved = false;
    if (S.file.get(section, key, &raw)) { if (saved) *saved = true; return kv::unquote(raw); }
    if (S.game_toml.get(section, key, &raw)) return kv::unquote(raw);
    return fallback ? fallback : "";
}

const Row* row_by_id(const std::string& id) {
    for (const Row& r : kRows)
        if (r.section && r.key && id == std::string(r.section) + "." + r.key) return &r;
    return nullptr;
}

bool needs_met(const Row& r) {
    if (!r.needs) return true;
    const std::string n = r.needs;
    const size_t eq = n.find('=');
    const Row* dep = row_by_id(n.substr(0, eq));
    const char* fb = dep ? dep->fallback : "";
    const size_t dot = n.find('.');
    const std::string sec = n.substr(0, dot), key = n.substr(dot + 1, eq - dot - 1);
    return effective(sec.c_str(), key.c_str(), fb) == n.substr(eq + 1);
}

bool same_value(const std::string& a, const std::string& b) {
    if (a == b) return true;
    char* ea = nullptr; char* eb = nullptr;
    const double x = std::strtod(a.c_str(), &ea), y = std::strtod(b.c_str(), &eb);
    return ea != a.c_str() && !*ea && eb != b.c_str() && !*eb && std::fabs(x - y) < 1e-9;
}

int value_index(int i, const std::string& v) {
    const Offer& o = S.offers[i];
    for (size_t k = 0; k < o.values.size(); ++k) if (same_value(o.values[k], v)) return (int)k;
    return -1;
}

std::string shown_value(int i) {
    const Row& r = kRows[i];
    if (r.kind == K_FIXED) return upper(effective(r.section, r.key, r.fallback));
    if (r.kind == K_BUTTONS || r.kind == K_RESTORE) return ">";
    const std::string v = effective(r.section, r.key, r.fallback);
    const int k = value_index(i, v);
    return k >= 0 ? S.offers[i].labels[k] : upper(v);
}

void save_settings() {
    S.file.set_top("settings_format", "2");
    if (!S.file.save()) note("COULDN'T SAVE settings.toml");
}

void step(int i, int dir) {
    const Row& r = kRows[i];
    const Offer& o = S.offers[i];
    if (r.kind != K_CHOICE || o.values.empty()) return;
    const int n = (int)o.values.size();
    const std::string v = effective(r.section, r.key, r.fallback);
    const int k = value_index(i, v);
    int next;
    /* VOLUME stops at 0 and 100 (a wrap from 100 to 0 would be a surprise); the rest go round. */
    const bool clamp = !std::strcmp(r.key, "volume");
    if (k >= 0) next = clamp ? std::clamp(k + (dir < 0 ? -1 : 1), 0, n - 1)
                             : ((k + (dir < 0 ? -1 : 1)) % n + n) % n;
    else {
        /* A value from the file that isn't one of the stops: the next stop up
         * or down from it (or the first / last for a word). */
        char* end = nullptr;
        const double x = std::strtod(v.c_str(), &end);
        next = dir > 0 ? 0 : n - 1;
        if (end != v.c_str() && !*end) {
            if (dir > 0) { for (int j = n - 1; j >= 0; --j) if (std::strtod(o.values[j].c_str(), nullptr) > x) next = j; }
            else         { for (int j = 0; j < n; ++j) if (std::strtod(o.values[j].c_str(), nullptr) < x) next = j; }
        }
    }
    S.file.set(r.section, r.key, r.quoted ? kv::quote(o.values[next]) : o.values[next]);
    save_settings();
}

/* The first start's own lines (play.cpp used to write them). */
const char kFirstStart[] =
    "# Written by the ACG1PLUS launcher: the game's own settings (SETTINGS in the launcher).\n"
    "# The launcher changes only the lines it has a row for; anything else here is kept.\n"
    "settings_format = 2\n\n"
    "[video]\n"
    "fullscreen = 1\n\n"
    "[audio]\n"
    "volume = 100\n";

void restore_defaults() {
    for (const Row& r : kRows)
        if (r.kind == K_CHOICE && r.section && r.key) S.file.erase(r.section, r.key);
    S.file.set("video", "fullscreen", "1");
    S.file.set("audio", "volume", "100");
    save_settings();
    note("SETTINGS RESTORED");
}

/* ---- the tabs ---------------------------------------------------------------- */

int tab_strip(const Gfx& g, float x, float y, float w) {
    const float tw = w / TAB__COUNT;
    int clicked = -1;
    for (int t = 0; t < TAB__COUNT; ++t) {
        const float tx = x + tw * t;
        const ImVec2 a = at(g, tx + 2.0f, y), b = at(g, tx + tw - 2.0f, y + 20.0f);
        const bool sel = t == S.tab;
        g.dl->AddRectFilled(a, b, sel ? IM_COL32(240, 160, 32, 60) : kPanel);
        g.dl->AddRect(a, b, sel ? kHi : kLine);
        const float lw = text_width(g, 11.0f, kTabNames[t]);
        text(g, tx + (tw - lw) * 0.5f, y + 4.0f, 11.0f, sel ? kBright : kDim, kTabNames[t]);
        if (ImGui::IsMouseHoveringRect(a, b, false) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) clicked = t;
    }
    return clicked;
}

const int kVisible = 10;

void draw_tabs(const Gfx& g, const Input& in) {
    if (in.tab_prev) S.tab = (S.tab + TAB__COUNT - 1) % TAB__COUNT;
    if (in.tab_next) S.tab = (S.tab + 1) % TAB__COUNT;

    header(g, "SETTINGS", "CHANGES APPLY THE NEXT TIME THE GAME STARTS");
    const int clicked_tab = tab_strip(g, 24.0f, 62.0f, 592.0f);
    if (clicked_tab >= 0) S.tab = clicked_tab;

    std::vector<int> rows;
    for (int i = 0; i < kRowCount; ++i)
        if (kRows[i].tab == S.tab && S.offers[i].shown && needs_met(kRows[i])) rows.push_back(i);
    const int count = (int)rows.size();
    int& cur = S.cursor[S.tab];
    int& top = S.top[S.tab];
    if (count > 0) {
        if (in.up)   cur = (cur + count - 1) % count;
        if (in.down) cur = (cur + 1) % count;
        cur = std::clamp(cur, 0, count - 1);
    } else cur = 0;
    if (cur < top) top = cur;
    if (cur >= top + kVisible) top = cur - kVisible + 1;
    top = std::clamp(top, 0, std::max(0, count - kVisible));

    panel(g, 24.0f, 96.0f, 592.0f, 8.0f + kRowH * kVisible, kTabNames[S.tab]);
    int clicked = -1;
    for (int v = 0; v < kVisible && top + v < count; ++v) {
        const int idx = top + v;
        const int i = rows[idx];
        const Row& r = kRows[i];
        bool saved = false;
        if (r.section && r.key) effective(r.section, r.key, r.fallback, &saved);
        const std::string val = shown_value(i);
        /* A value the player chose is bright; the game's default is dim. */
        const ImU32 vc = r.kind == K_FIXED ? kOff : saved ? kHi : kText;
        bool hovered = false;
        if (row(g, 25.0f, 100.0f + kRowH * v, 590.0f, idx == cur, r.kind != K_FIXED, r.label, val.c_str(), vc,
                &hovered))
            clicked = idx;
        if (hovered && mouse_moved()) cur = idx;
    }
    /* More rows above / below. */
    if (top > 0) text(g, 600.0f, 98.0f, 10.0f, kDim, "^");
    if (top + kVisible < count) text(g, 600.0f, 96.0f + kRowH * kVisible - 4.0f, 10.0f, kDim, "v");

    panel(g, 24.0f, 384.0f, 592.0f, 52.0f, "INFO");
    if (count > 0) {
        const Row& r = kRows[rows[cur]];
        bool saved = false;
        if (r.section && r.key) effective(r.section, r.key, r.fallback, &saved);
        std::string info = r.info;
        if (r.kind == K_CHOICE && !saved) info += "  (Game default.)";
        text(g, 38.0f, 396.0f, 10.0f, kText, info.c_str(), 566.0f);
    }
    footer(g, "LEFT / RIGHT  CHANGE      L1 / R1 (Q / E)  TAB      A  SELECT      B / ESC  BACK");

    if (in.back) { S.open = false; clear_note(); return; }
    if (count == 0) return;
    if (in.left) step(rows[cur], -1);
    if (in.right) step(rows[cur], +1);
    if (clicked >= 0) cur = clicked;
    if (!in.confirm && clicked < 0) return;
    const int ci = rows[cur];
    const Row& cr = kRows[ci];
    switch (cr.kind) {
        case K_CHOICE: step(ci, +1); break;
        case K_BUTTONS:
            S.controls = true;
            S.input.load(input_path());
            S.keys.load(keys_path());
            S.capture = -1;
            {
                int pads = 0;
                SDL_JoystickID* ids = SDL_GetGamepads(&pads);
                if (ids) SDL_free(ids);
                S.c_col = pads > 0 ? 0 : 1;   /* the keyboard column first when no controller is there */
            }
            clear_note();
            break;
        case K_RESTORE:
            if (ImGui::GetTime() < S.restore_armed) { restore_defaults(); S.restore_armed = 0.0; }
            else { S.restore_armed = ImGui::GetTime() + 3.0; note("PRESS A AGAIN TO RESTORE EVERY SETTING"); }
            break;
        default: break;
    }
}

/* ---- BUTTON LAYOUT ------------------------------------------------------------ */

struct Btn { const char* ini; const char* label; const char* pad; const char* key; };
/* The game's own defaults (psxrecomp runtime/src/main.cpp default_input_ini_text
 * and psx_keybinds.c PSXKB_PLAYER_DEFAULTS), shown and restored by DEFAULT. */
const Btn kBtns[] = {
    { "up",       "UP",       "dpup",          "Up" },
    { "down",     "DOWN",     "dpdown",        "Down" },
    { "left",     "LEFT",     "dpleft",        "Left" },
    { "right",    "RIGHT",    "dpright",       "Right" },
    { "cross",    "CROSS",    "a",             "X" },
    { "circle",   "CIRCLE",   "b",             "S" },
    { "square",   "SQUARE",   "x",             "Z" },
    { "triangle", "TRIANGLE", "y",             "A" },
    { "l1",       "L1",       "leftshoulder",  "Q" },
    { "r1",       "R1",       "rightshoulder", "W" },
    { "l2",       "L2",       "lefttrigger",   "E" },
    { "r2",       "R2",       "righttrigger",  "R" },
    { "l3",       "L3",       "leftstick",     "T" },
    { "r3",       "R3",       "rightstick",    "Y" },
    { "start",    "START",    "start",         "Return" },
    { "select",   "SELECT",   "back",          "Right Shift" },
};
const int kBtnCount = (int)(sizeof kBtns / sizeof kBtns[0]);
const int kCtrlRows = kBtnCount + 1;   /* + ALL BACK TO DEFAULT */

std::string pad_word(const std::string& s) {
    static const struct { const char* n; const char* w; } k[] = {
        { "a", "A" }, { "b", "B" }, { "x", "X" }, { "y", "Y" },
        { "back", "VIEW / SELECT" }, { "start", "MENU / START" }, { "guide", "GUIDE" },
        { "leftstick", "LEFT STICK PRESS" }, { "rightstick", "RIGHT STICK PRESS" },
        { "leftshoulder", "LB / L1" }, { "rightshoulder", "RB / R1" },
        { "lefttrigger", "LT / L2" }, { "righttrigger", "RT / R2" },
        { "lefttrigger+", "LT / L2" }, { "righttrigger+", "RT / R2" },
        { "dpup", "D-PAD UP" }, { "dpdown", "D-PAD DOWN" }, { "dpleft", "D-PAD LEFT" }, { "dpright", "D-PAD RIGHT" },
        { "leftx-", "LEFT STICK LEFT" }, { "leftx+", "LEFT STICK RIGHT" },
        { "lefty-", "LEFT STICK UP" }, { "lefty+", "LEFT STICK DOWN" },
        { "rightx-", "RIGHT STICK LEFT" }, { "rightx+", "RIGHT STICK RIGHT" },
        { "righty-", "RIGHT STICK UP" }, { "righty+", "RIGHT STICK DOWN" },
    };
    std::string out;
    for (const std::string& part : split(s.c_str())) {
        std::string w = upper(part);
        for (const auto& e : k) if (part == e.n) { w = e.w; break; }
        if (!out.empty()) out += ", ";
        out += w;
    }
    return out.empty() ? "NONE" : out;
}

std::string pad_of(int b) {
    std::string raw;
    if (S.input.get("mapping", kBtns[b].ini, &raw)) return raw;
    return kBtns[b].pad;
}
std::string key_of(int b) {
    std::string raw;
    if (S.keys.get("player1", kBtns[b].ini, &raw)) return raw;
    return kBtns[b].key;
}
/* keybinds.ini allows a second key after a comma: kept when the first is changed. */
std::string key_alt(int b) {
    const std::string raw = key_of(b);
    const size_t c = raw.find(',');
    return c == std::string::npos ? "" : raw.substr(c);
}

void set_pad(int b, const std::string& source) {
    S.input.set("mapping", kBtns[b].ini, source);
    if (!S.input.save()) note("COULDN'T SAVE input.ini");
}
void set_key(int b, const std::string& name) {
    S.keys.set("player1", kBtns[b].ini, name + key_alt(b));
    if (!S.keys.save()) note("COULDN'T SAVE keybinds.ini");
}

void controls_default_all() {
    for (int b = 0; b < kBtnCount; ++b) {
        S.input.set("mapping", kBtns[b].ini, kBtns[b].pad);
        S.keys.set("player1", kBtns[b].ini, kBtns[b].key);
    }
    const bool ok = S.input.save() && S.keys.save();
    note(ok ? "BUTTON LAYOUT BACK TO DEFAULT" : "COULDN'T SAVE THE BUTTON LAYOUT");
}

void start_capture(int col) {
    S.capture = col;
    S.capture_until = ImGui::GetTime() + 6.0;
    clear_note();
}

void end_capture() {
    S.capture = -1;
    /* What was just pressed is not a choice on this page. */
    hold_input(10);
}

void draw_controls(const Gfx& g, const Input& in) {
    if (S.capture >= 0 && ImGui::GetTime() > S.capture_until) { end_capture(); note("NOTHING PRESSED: NOT CHANGED"); }
    const bool capturing = S.capture >= 0;
    if (!capturing) {
        if (in.up)   S.c_row = (S.c_row + kCtrlRows - 1) % kCtrlRows;
        if (in.down) S.c_row = (S.c_row + 1) % kCtrlRows;
        if (in.left || in.right) S.c_col = 1 - S.c_col;
    }

    header(g, "BUTTON LAYOUT", "PLAYER 1   CHANGES APPLY THE NEXT TIME THE GAME STARTS");
    panel(g, 24.0f, 66.0f, 592.0f, 352.0f, "PLAYER 1");
    const float x_pad = 220.0f, x_key = 424.0f, col_w = 186.0f, rh = 19.0f, y0 = 92.0f;
    text(g, 54.0f, 76.0f, 10.0f, kDim, "PLAYSTATION BUTTON");
    text(g, x_pad + 8.0f, 76.0f, 10.0f, S.c_col == 0 ? kHi : kDim, "CONTROLLER");
    text(g, x_key + 8.0f, 76.0f, 10.0f, S.c_col == 1 ? kHi : kDim, "KEYBOARD");
    int click_row = -1, click_col = -1;
    const bool blink = std::fmod(ImGui::GetTime(), 0.8) < 0.5;
    for (int r = 0; r < kCtrlRows; ++r) {
        const float y = y0 + rh * r;
        const bool sel = r == S.c_row;
        if (sel) {
            g.dl->AddRectFilledMultiColor(at(g, 25.0f, y), at(g, 615.0f, y + rh), IM_COL32(240, 160, 32, 40),
                                          IM_COL32(240, 160, 32, 0), IM_COL32(240, 160, 32, 0), IM_COL32(240, 160, 32, 40));
            g.dl->AddTriangleFilled(at(g, 36.0f, y + 5.0f), at(g, 44.0f, y + 9.5f), at(g, 36.0f, y + 14.0f), kHi);
        }
        if (r == kBtnCount) {
            text(g, 54.0f, y + 3.0f, 12.0f, sel ? kBright : kText, "ALL BACK TO DEFAULT");
            const ImVec2 a = at(g, 25.0f, y), b = at(g, 615.0f, y + rh);
            if (ImGui::IsMouseHoveringRect(a, b, false)) {
                if (mouse_moved() && !capturing) S.c_row = r;
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) { click_row = r; click_col = S.c_col; }
            }
            continue;
        }
        text(g, 54.0f, y + 3.0f, 12.0f, sel ? kBright : kText, kBtns[r].label);
        for (int c = 0; c < 2; ++c) {
            const float x = c == 0 ? x_pad : x_key;
            const ImVec2 a = at(g, x, y + 1.0f), b = at(g, x + col_w, y + rh - 1.0f);
            const bool cell = sel && c == S.c_col;
            if (cell) g.dl->AddRect(a, b, kHi);
            std::string v;
            ImU32 color = kText;
            if (cell && capturing && S.capture == c) {
                v = blink ? (c == 0 ? "PRESS A BUTTON..." : "PRESS A KEY...") : "";
                color = kHi;
            } else if (c == 0) {
                const std::string raw = pad_of(r);
                v = pad_word(raw);
                if (raw != kBtns[r].pad) color = kHi;
            } else {
                const std::string raw = key_of(r);
                v = upper(raw);
                if (raw != kBtns[r].key) color = kHi;
            }
            text(g, x + 8.0f, y + 4.0f, 10.0f, color, fit(g, 10.0f, v, col_w - 14.0f).c_str());
            if (!capturing && ImGui::IsMouseHoveringRect(a, b, false)) {
                if (mouse_moved()) { S.c_row = r; S.c_col = c; }
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) { click_row = r; click_col = c; }
            }
        }
    }
    const char* info = capturing
        ? (S.capture == 0 ? "Press the controller button (or pull the trigger, or push the stick) for this "
                            "PlayStation button. Esc to leave it as it is."
                          : "Press the key for this PlayStation button. Esc to leave it as it is (Esc closes "
                            "the game, so it can't be used).")
        : S.c_row == kBtnCount ? "Every button and key back to the game's own layout. Press A twice."
        : "A: set this one.  Y: back to default.  LEFT / RIGHT: controller or keyboard. Highlighted values "
          "are your own.";
    text(g, 38.0f, 426.0f, 10.0f, capturing ? kHi : kDim, info, 566.0f);
    footer(g, capturing ? "WAITING FOR A PRESS..." : "A  SET      Y  DEFAULT      LEFT / RIGHT  COLUMN      B / ESC  BACK");

    if (capturing) return;
    if (in.back) { S.controls = false; clear_note(); return; }
    if (click_row >= 0) { S.c_row = click_row; S.c_col = click_col; }
    if (S.c_row == kBtnCount) {
        if (in.confirm || click_row == kBtnCount) {
            if (ImGui::GetTime() < S.ctrl_reset_armed) { controls_default_all(); S.ctrl_reset_armed = 0.0; }
            else { S.ctrl_reset_armed = ImGui::GetTime() + 3.0; note("PRESS A AGAIN TO RESET EVERY BUTTON"); }
        }
        return;
    }
    if (in.y) {
        if (S.c_col == 0) set_pad(S.c_row, kBtns[S.c_row].pad);
        else set_key(S.c_row, kBtns[S.c_row].key);
        return;
    }
    if (in.confirm || click_row >= 0) start_capture(S.c_col);
}

} /* namespace */

void ensure_file() {
    std::error_code ec;
    const fs::path f = settings_path();
    if (fs::exists(f, ec)) return;
    fs::create_directories(f.parent_path(), ec);
    if (FILE* out = plat::open_file(f, "wb")) {
        std::fputs(kFirstStart, out);
        std::fclose(out);
    }
}

void open() {
    ensure_file();
    reload();
    S.open = true;
    S.controls = false;
    S.capture = -1;
    clear_note();
}

bool active() { return S.open; }

void draw(const Gfx& g, const Input& in) {
    if (S.controls) draw_controls(g, in);
    else draw_tabs(g, in);
}

bool capture_event(const SDL_Event& ev) {
    if (!S.open || !S.controls || S.capture < 0) return false;
    /* Releases always go on: the menu must see the button that started this
     * one let go, or it would think it is still held. */
    if (ev.type == SDL_EVENT_KEY_UP || ev.type == SDL_EVENT_GAMEPAD_BUTTON_UP ||
        ev.type == SDL_EVENT_MOUSE_BUTTON_UP)
        return false;
    const int b = S.c_row;
    if (b < 0 || b >= kBtnCount) { S.capture = -1; return false; }
    /* Esc, or a right click: leave it as it is. */
    if ((ev.type == SDL_EVENT_KEY_DOWN && ev.key.scancode == SDL_SCANCODE_ESCAPE) ||
        (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.button == SDL_BUTTON_RIGHT)) {
        end_capture();
        note("NOT CHANGED");
        return true;
    }
    if (S.capture == 0) {
        std::string source;
        if (ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            switch ((SDL_GamepadButton)ev.gbutton.button) {
                case SDL_GAMEPAD_BUTTON_SOUTH: source = "a"; break;
                case SDL_GAMEPAD_BUTTON_EAST: source = "b"; break;
                case SDL_GAMEPAD_BUTTON_WEST: source = "x"; break;
                case SDL_GAMEPAD_BUTTON_NORTH: source = "y"; break;
                case SDL_GAMEPAD_BUTTON_BACK: source = "back"; break;
                case SDL_GAMEPAD_BUTTON_START: source = "start"; break;
                case SDL_GAMEPAD_BUTTON_GUIDE: source = "guide"; break;
                case SDL_GAMEPAD_BUTTON_LEFT_STICK: source = "leftstick"; break;
                case SDL_GAMEPAD_BUTTON_RIGHT_STICK: source = "rightstick"; break;
                case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: source = "leftshoulder"; break;
                case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: source = "rightshoulder"; break;
                case SDL_GAMEPAD_BUTTON_DPAD_UP: source = "dpup"; break;
                case SDL_GAMEPAD_BUTTON_DPAD_DOWN: source = "dpdown"; break;
                case SDL_GAMEPAD_BUTTON_DPAD_LEFT: source = "dpleft"; break;
                case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: source = "dpright"; break;
                default:
                    /* Back paddles and the like: the game can't read them. */
                    note("THE GAME CAN'T USE THAT BUTTON. TRY ANOTHER");
                    return true;
            }
        } else if (ev.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && std::abs((int)ev.gaxis.value) > 20000) {
            const bool plus = ev.gaxis.value > 0;
            switch ((SDL_GamepadAxis)ev.gaxis.axis) {
                case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: if (plus) source = "lefttrigger"; break;
                case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: if (plus) source = "righttrigger"; break;
                case SDL_GAMEPAD_AXIS_LEFTX: source = plus ? "leftx+" : "leftx-"; break;
                case SDL_GAMEPAD_AXIS_LEFTY: source = plus ? "lefty+" : "lefty-"; break;
                case SDL_GAMEPAD_AXIS_RIGHTX: source = plus ? "rightx+" : "rightx-"; break;
                case SDL_GAMEPAD_AXIS_RIGHTY: source = plus ? "righty+" : "righty-"; break;
                default: break;
            }
        }
        /* Not a press the controller column takes (a key, a small stick move): not passed on either. */
        if (source.empty()) return ev.type == SDL_EVENT_KEY_DOWN || ev.type == SDL_EVENT_GAMEPAD_AXIS_MOTION;
        set_pad(b, source);
        end_capture();
        return true;
    }
    /* Keyboard. */
    if (ev.type == SDL_EVENT_KEY_DOWN) {
        if (ev.key.repeat) return true;
        const char* name = SDL_GetScancodeName(ev.key.scancode);
        if (!name || !name[0]) { note("THAT KEY HAS NO NAME. TRY ANOTHER"); return true; }
        set_key(b, name);
        end_capture();
        return true;
    }
    /* Controller presses while a key is awaited: not passed on. */
    return ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || ev.type == SDL_EVENT_TEXT_INPUT;
}

bool capture_event_pending() { return S.open && S.controls && (S.capture >= 0 || holding()); }

std::string shot_key() {
    if (!S.open) return "";
    if (S.controls) return S.capture >= 0 ? "buttons-capture" : "buttons";
    static const char* k[TAB__COUNT] = { "display", "picture", "system", "advanced" };
    return std::string("settings-") + k[S.tab];
}

} /* namespace settings */

/* ACG1PLUS launcher (launcher2): the Armored Core system menu.
 *
 * The look and layout are ported from launcher/moa_menu.inc (the owner's own
 * design, the allowed exception to "don't copy old code"): the dark ground with
 * its grid, captioned panels, the amber cursor row, the STATUS and INFO panels.
 * What changed on the way:
 *   - It no longer sits inside the framework's launcher, so nothing here talks
 *     to a launcher model; the program around it (main.cpp) passes what it
 *     knows in a Host.
 *   - Fonts are baked at the size they are drawn at (ImGui 1.91 scales a baked
 *     font as a bitmap, which went soft at 1280x800 and above).
 *   - 0.2.2: the drawing helpers moved to ui.cpp, START GAME opens the first
 *     run (firstrun.cpp) until the game is built, and STATUS shows the disc
 *     and the game. SETTINGS, MODS and UPDATES say which step brings them.
 *     VERSUS and ROGUE ARENA are marked as coming, with the version numbers of
 *     the current ladder (docs/versioning.md).
 *   - 0.2.3: START GAME starts the built game (play.cpp). Its page has LAUNCH
 *     GAME and FULL BOOT, as the old START GAME page had; LOAD GAME and NEW
 *     GAME need game-side hooks at Phantasma's addresses and come with 0.3.
 *     START on the controller from the main menu starts the game at once.
 */
#include "menu.h"

#include "firstrun.h"
#include "play.h"
#include "setup.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include "platform.h"

namespace menu {

using namespace ui;

namespace {

enum Page { PAGE_MAIN = 0, PAGE_VERSUS, PAGE_START, PAGE_RUNNING, PAGE_CLOSED, PAGE__COUNT };

struct State {
    Page page = PAGE_MAIN;
    int  cursor[PAGE__COUNT] = {0};
    bool launch_after_disc = false;   /* the disc page was opened to start the game */
    /* PAGE_CLOSED: why the game couldn't start, or how it closed. */
    std::string closed_title, closed_text;
    play::Result closed;
};
State g_state;

void move_cursor(Page page, int count, const Input& in) {
    int& c = g_state.cursor[page];
    if (count <= 0) { c = 0; return; }
    if (in.up)   c = (c + count - 1) % count;
    if (in.down) c = (c + 1) % count;
    c = std::clamp(c, 0, count - 1);
}

/* ---- the main page ----------------------------------------------------------- */

enum MainEntry { MAIN_START = 0, MAIN_VERSUS, MAIN_ROGUE, MAIN_SETTINGS, MAIN_MODS, MAIN_UPDATES, MAIN_EXIT };

struct Entry {
    MainEntry   id;
    const char* label;
    const char* tag;      /* shown at the right of the row ("COMING"), or null */
    const char* when;     /* the step that brings it, for the note; null = works now */
    const char* info;
};

const Entry kMain[] = {
    { MAIN_START, "START GAME", nullptr, nullptr,
      "Play Project Phantasma. The first time it finds your disc and builds the game from it "
      "(PREPARING GAME). START on the controller starts the game straight away." },
    { MAIN_VERSUS, "VERSUS", "COMING", nullptr,
      "Two-player and computer-player fights: local link on two monitors, network, "
      "player vs AI and AI vs AI. Not ready yet; see what's coming." },
    { MAIN_ROGUE, "ROGUE ARENA", "COMING", "0.8",
      "A roguelike run: a random AC, a ladder of opponents from every arena, prizes "
      "along the way, and one loss ends it. Coming in version 0.8." },
    { MAIN_SETTINGS, "SETTINGS", nullptr, "0.2.5",
      "Display, picture, sound and controls: every settings page of the game's own menu "
      "moves here in 0.2.5. Until then: F11 or ALT+ENTER (or the controller's VIEW / "
      "SELECT button) switches between a window and full screen." },
    { MAIN_MODS, "MODS", nullptr, "0.2.6",
      "Switch installed mods on or off, and install new ones from the Mods folder. "
      "Coming in 0.2.6." },
    { MAIN_UPDATES, "UPDATES", nullptr, "0.2.4",
      "Look for a newer version and install it when you choose: the game is rebuilt from "
      "your disc and the old one stays playable until the new one is ready. Coming in 0.2.4." },
    { MAIN_EXIT, "EXIT", nullptr, nullptr, "Close the launcher." },
};
const int kMainCount = (int)(sizeof kMain / sizeof kMain[0]);

void not_ready(const char* when) {
    char buf[120];
    std::snprintf(buf, sizeof buf, "NOT IN THIS TEST YET. COMING IN %s", when);
    note(buf);
}

void show_closed(const std::string& title, const std::string& text) {
    g_state.closed_title = title;
    g_state.closed_text = text;
    g_state.closed = play::Result();
    g_state.cursor[PAGE_CLOSED] = 0;
    g_state.page = PAGE_CLOSED;
}

/* Start the game with the disc chosen last time; if it isn't there any more,
 * the disc page first (and then this again). */
void launch(Requests& req) {
    clear_note();
    const std::filesystem::path cue = firstrun::chosen_disc();
    if (cue.empty()) {
        g_state.launch_after_disc = true;
        firstrun::find_disc();
        return;
    }
    std::string err;
    if (play::start(cue, &err)) {
        req.game_started = true;
        g_state.page = PAGE_RUNNING;
        g_state.cursor[PAGE_RUNNING] = 0;
    } else {
        show_closed("THE GAME COULD NOT START", err);
    }
}

void draw_main(const Gfx& g, const Input& in, Requests& req) {
    move_cursor(PAGE_MAIN, kMainCount, in);
    int& cur = g_state.cursor[PAGE_MAIN];
    std::string built_info;
    const bool built = setup::game_built(&built_info);

    header(g, "PROJECT PHANTASMA", "SYSTEM MENU");
    panel(g, 24.0f, 76.0f, 270.0f, 340.0f, "SELECT");
    int activate = -1;
    for (int i = 0; i < kMainCount; ++i) {
        bool hovered = false;
        const char* tag = kMain[i].tag;
        if (kMain[i].id == MAIN_START && !built) tag = "BUILD FIRST";
        if (row(g, 25.0f, 90.0f + kRowH * i, 268.0f, i == cur, true, kMain[i].label, tag, kDim, &hovered))
            activate = i;
        if (hovered && mouse_moved()) cur = i;
    }
    if (in.confirm) activate = cur;
    const bool start_now = in.start && built && setup::snapshot().status != setup::Status::Running;
    if (in.start && !start_now) activate = MAIN_START;

    /* Status. */
    panel(g, 310.0f, 76.0f, 306.0f, 150.0f, "STATUS");
    {
        const Host& h = *g.host;
        float y = 92.0f;
        char buf[160];
        bool disc_ok = false;
        const std::string d = firstrun::disc_status(&disc_ok);
        text(g, 324.0f, y, 11.0f, kDim, "DISC");
        text(g, 424.0f, y, 11.0f, disc_ok ? kText : kWarn, fit(g, 11.0f, d, 180.0f).c_str());
        y += 19.0f;
        text(g, 324.0f, y, 11.0f, kDim, "GAME");
        const setup::Snapshot s = setup::snapshot();
        if (s.status == setup::Status::Running) text(g, 424.0f, y, 11.0f, kHi, "PREPARING...");
        else if (built) text(g, 424.0f, y, 11.0f, kOn, ("BUILT  " + upper(built_info)).c_str());
        else text(g, 424.0f, y, 11.0f, kWarn, "NOT BUILT YET");
        y += 19.0f;
        text(g, 324.0f, y, 11.0f, kDim, "CONTROLLER");
        const std::string pad = h.controller && h.controller[0] ? upper(h.controller)
                                                                 : std::string("NONE (KEYBOARD AND MOUSE)");
        text(g, 424.0f, y, 11.0f, h.controller && h.controller[0] ? kText : kDim, fit(g, 11.0f, pad, 180.0f).c_str());
        y += 19.0f;
        text(g, 324.0f, y, 11.0f, kDim, "DISPLAY");
        std::snprintf(buf, sizeof buf, "%s  %dx%d", h.fullscreen ? "FULL SCREEN" : "WINDOW", h.width, h.height);
        text(g, 424.0f, y, 11.0f, kText, buf);
        y += 19.0f;
        text(g, 324.0f, y, 11.0f, kDim, "SYSTEM");
        text(g, 424.0f, y, 11.0f, kText, h.system ? h.system : "");
        y += 19.0f;
        text(g, 324.0f, y, 11.0f, kDim, "LAUNCHER");
        std::snprintf(buf, sizeof buf, "%s  TEST", h.version ? h.version : "");
        text(g, 424.0f, y, 11.0f, kHi, buf);
    }

    panel(g, 310.0f, 242.0f, 306.0f, 174.0f, "INFO");
    text(g, 324.0f, 258.0f, 11.0f, kHi, kMain[cur].label);
    text(g, 324.0f, 280.0f, 11.0f, kText, kMain[cur].info, 278.0f);

    footer(g, "A / ENTER  SELECT      D-PAD  MOVE      VIEW / F11  FULL SCREEN");

    if (start_now) { cur = MAIN_START; launch(req); return; }
    if (activate < 0) return;
    cur = activate;
    const Entry& e = kMain[activate];
    switch (e.id) {
        case MAIN_START:
            if (!built || setup::snapshot().status == setup::Status::Running) firstrun::open();
            else { g_state.page = PAGE_START; g_state.cursor[PAGE_START] = 0; clear_note(); }
            break;
        case MAIN_VERSUS:
            g_state.page = PAGE_VERSUS;
            clear_note();
            break;
        case MAIN_EXIT:
            req.quit = true;
            break;
        default:
            not_ready(e.when);
            break;
    }
}

/* ---- the VERSUS page ------------------------------------------------------- */

struct ComingMode { const char* label; const char* when; const char* info; };

const ComingMode kVersusModes[] = {
    { "LOCAL LINK", "0.5",
      "Two players on one PC, one monitor each: two copies of the game joined the way "
      "the PlayStation's link cable joined two consoles. Each player sets up their own "
      "AC. Coming in version 0.5." },
    { "NETWORK", "0.6",
      "Versus over your home network (LAN) or straight to an address a friend gives you "
      "(Direct IP). Coming in version 0.6." },
    { "PLAYER VS AI", "0.7",
      "Fight a computer-flown AC in the versus arenas. Coming in version 0.7." },
    { "AI VS AI", "0.7",
      "Watch two computer-flown ACs fight each other. Coming in version 0.7." },
};
const int kVersusCount = (int)(sizeof kVersusModes / sizeof kVersusModes[0]);

void draw_versus(const Gfx& g, const Input& in) {
    move_cursor(PAGE_VERSUS, kVersusCount, in);
    int& cur = g_state.cursor[PAGE_VERSUS];
    header(g, "VERSUS", "PROJECT PHANTASMA");
    panel(g, 24.0f, 76.0f, 592.0f, 124.0f, "SELECT");
    int activate = -1;
    for (int i = 0; i < kVersusCount; ++i) {
        char when[24];
        std::snprintf(when, sizeof when, "COMING IN %s", kVersusModes[i].when);
        bool hovered = false;
        if (row(g, 25.0f, 90.0f + (kRowH + 2.0f) * i, 590.0f, i == cur, false,
                kVersusModes[i].label, when, kOff, &hovered))
            activate = i;
        if (hovered && mouse_moved()) cur = i;
    }
    panel(g, 24.0f, 218.0f, 592.0f, 120.0f, "INFO");
    text(g, 38.0f, 234.0f, 11.0f, kHi, kVersusModes[cur].label);
    text(g, 38.0f, 256.0f, 11.0f, kText, kVersusModes[cur].info, 560.0f);
    footer(g, "A / ENTER  SELECT      D-PAD  MOVE      B / ESC  BACK");

    if (in.back) {
        g_state.page = PAGE_MAIN;
        clear_note();
        return;
    }
    if (in.confirm || in.start) activate = cur;
    if (activate < 0) return;
    cur = activate;
    char buf[80];
    std::snprintf(buf, sizeof buf, "NOT READY YET. COMING IN VERSION %s", kVersusModes[activate].when);
    note(buf);
}

/* ---- the START GAME page ---------------------------------------------------- */

enum StartEntry { START_LAUNCH = 0, START_FULL_BOOT, START_LOAD, START_NEW, START__COUNT };

void draw_start(const Gfx& g, const Input& in, Requests& req) {
    move_cursor(PAGE_START, START__COUNT, in);
    int& cur = g_state.cursor[PAGE_START];
    const bool fb = play::full_boot();
    header(g, "START GAME", "PROJECT PHANTASMA");
    panel(g, 24.0f, 76.0f, 270.0f, 124.0f, "SELECT");
    struct R { const char* label; const char* value; ImU32 vc; bool enabled; };
    const R rows[START__COUNT] = {
        { "LAUNCH GAME", nullptr, kDim, true },
        { "FULL BOOT", fb ? "ON" : "OFF", fb ? kOn : kOff, true },
        { "LOAD GAME", "COMING IN 0.3", kOff, false },
        { "NEW GAME", "COMING IN 0.3", kOff, false },
    };
    int activate = -1;
    for (int i = 0; i < START__COUNT; ++i) {
        bool hovered = false;
        if (row(g, 25.0f, 90.0f + kRowH * i, 268.0f, i == cur, rows[i].enabled, rows[i].label, rows[i].value,
                rows[i].vc, &hovered))
            activate = i;
        if (hovered && mouse_moved()) cur = i;
    }

    panel(g, 310.0f, 76.0f, 306.0f, 124.0f, "DISC");
    {
        bool ok = false;
        const std::string d = firstrun::disc_status(&ok);
        text(g, 324.0f, 92.0f, 11.0f, ok ? kText : kWarn, fit(g, 11.0f, d, 278.0f).c_str());
        const std::string where = ok ? plat::u8str(firstrun::chosen_disc().parent_path())
                                     : std::string("START GAME will ask for it.");
        text(g, 324.0f, 110.0f, 10.0f, kDim, fit(g, 10.0f, where, 278.0f).c_str());
        text(g, 324.0f, 140.0f, 10.0f, kDim, "MEMORY CARDS");
        text(g, 324.0f, 156.0f, 10.0f, kText,
             fit(g, 10.0f, plat::u8str(setup::paths().root / "Memory Card"), 278.0f).c_str());
    }

    static const char* kInfo[START__COUNT] = {
        "Start the game at its title screen. To come back here, press Esc, or hold SELECT and START "
        "together for two seconds. Progress since your last save in the game is not kept.",
        "ON: the PlayStation start-up screen plays before the game, as on the console. OFF: it is "
        "skipped. (Skipping the game's own logos and intro movie comes with 0.3.)",
        "Pick your data on memory card 1 and start straight in the garage, without the title and "
        "memory card screens. Coming in version 0.3.",
        "Start a new game straight away, without the title screen. Coming in version 0.3.",
    };
    panel(g, 24.0f, 216.0f, 592.0f, 120.0f, "INFO");
    text(g, 38.0f, 232.0f, 11.0f, kHi, rows[cur].label);
    text(g, 38.0f, 254.0f, 11.0f, kText, kInfo[cur], 560.0f);
    footer(g, "A / ENTER  SELECT      D-PAD  MOVE      START  LAUNCH      B / ESC  BACK");

    if (in.back) { g_state.page = PAGE_MAIN; clear_note(); return; }
    if (cur == START_FULL_BOOT && (in.left || in.right)) { play::set_full_boot(!fb); return; }
    if (in.start) { launch(req); return; }
    if (in.confirm) activate = cur;
    if (activate < 0) return;
    cur = activate;
    switch (activate) {
        case START_LAUNCH:    launch(req); break;
        case START_FULL_BOOT: play::set_full_boot(!fb); break;
        default:              note("NOT READY YET. COMING IN VERSION 0.3"); break;
    }
}

/* ---- while the game runs ----------------------------------------------------
 * The launcher's window is hidden while the game runs (main.cpp), so this is
 * seen only for a moment, or where a window can't be hidden. */
void draw_running(const Gfx& g, const Input& in) {
    header(g, "START GAME", "THE GAME IS RUNNING");
    panel(g, 24.0f, 76.0f, 592.0f, 120.0f, "PLAYING");
    text(g, 38.0f, 92.0f, 13.0f, kOn, "THE GAME IS RUNNING", 0.0f, true);
    text(g, 38.0f, 116.0f, 11.0f, kText,
         "This menu comes back when the game closes: press Esc in the game, or hold SELECT and START "
         "together for two seconds.", 560.0f);
    panel(g, 24.0f, 212.0f, 270.0f, 40.0f, "SELECT");
    bool hovered = false;
    const bool clicked = row(g, 25.0f, 224.0f, 268.0f, true, true, "STOP THE GAME", nullptr, kDim, &hovered);
    footer(g, "A / ENTER  STOP THE GAME");
    if (clicked || in.confirm) { play::stop(); note("STOPPING THE GAME..."); }
}

/* ---- the game couldn't start, or closed with an error ----------------------- */
void draw_closed(const Gfx& g, const Input& in, Requests& req) {
    int& cur = g_state.cursor[PAGE_CLOSED];
    move_cursor(PAGE_CLOSED, 2, in);
    header(g, "START GAME", "PROBLEM");
    panel(g, 24.0f, 76.0f, 592.0f, 236.0f, "WHAT HAPPENED");
    text(g, 38.0f, 92.0f, 13.0f, kWarn, g_state.closed_title.c_str(), 0.0f, true);
    text(g, 38.0f, 114.0f, 11.0f, kText, g_state.closed_text.c_str(), 560.0f);
    const auto& tail = g_state.closed.tail;
    if (!tail.empty()) {
        text(g, 38.0f, 162.0f, 10.0f, kDim, "THE GAME'S LAST LINES");
        const int n = std::min((int)tail.size(), 9);
        for (int i = 0; i < n; ++i)
            text(g, 38.0f, 176.0f + 14.0f * i, 10.0f, kText,
                 fit(g, 10.0f, tail[tail.size() - n + i], 560.0f).c_str());
    }
    static const char* labels[2] = { "START AGAIN", "BACK" };
    panel(g, 24.0f, 322.0f, 270.0f, 64.0f, "SELECT");
    int activate = -1;
    for (int i = 0; i < 2; ++i) {
        bool hovered = false;
        if (row(g, 25.0f, 334.0f + kRowH * i, 268.0f, i == cur, true, labels[i], nullptr, kDim, &hovered))
            activate = i;
        if (hovered && mouse_moved()) cur = i;
    }
    if (!g_state.closed.log_file.empty()) {
        panel(g, 310.0f, 322.0f, 306.0f, 64.0f, "LOG FILE");
        text(g, 324.0f, 338.0f, 10.0f, kDim, fit(g, 10.0f, g_state.closed.log_file, 278.0f).c_str());
    }
    footer(g, "A / ENTER  SELECT      D-PAD  MOVE      B / ESC  BACK");
    if (in.back) { g_state.page = PAGE_START; clear_note(); return; }
    if (in.confirm) activate = cur;
    if (activate == 0) launch(req);
    else if (activate == 1) { g_state.page = PAGE_START; clear_note(); }
}

} /* namespace */

int page() { return (int)g_state.page; }

void game_closed(const play::Result& r) {
    if (r.code == 0) {
        /* A normal close (Esc, SELECT + START, the window's close button):
         * back to the START GAME page. */
        if (g_state.page == PAGE_RUNNING) g_state.page = PAGE_START;
        return;
    }
    char t[200];
    if (r.seconds < 10.0)
        std::snprintf(t, sizeof t, "It closed %.0f seconds after it started, with error code %d. "
                      "The lines below and the log file may say why.", r.seconds, r.code);
    else
        std::snprintf(t, sizeof t, "It closed after %d min %02d s with error code %d. "
                      "The lines below and the log file may say why.",
                      (int)r.seconds / 60, (int)r.seconds % 60, r.code);
    show_closed("THE GAME CLOSED WITH AN ERROR", t);
    g_state.closed = r;
}

float scale_for(int width, int height) {
    const float s = std::min((float)width / kW, (float)height / kH);
    return s > 0.0f ? s : 1.0f;
}

void inject(const char* what) { ui::inject(what); }

void draw(const Host& host, Requests& req) {
    const Gfx g = ui::begin(host);
    const Input in = read_input();
    if (!holding() && ImGui::IsKeyPressed(ImGuiKey_GamepadBack, false)) req.toggle_fullscreen = true;
    /* The disc page was opened by START GAME (the disc had moved), or START
     * GAME was chosen on the "ready" page: carry on starting the game. */
    if (firstrun::take_disc_ready() && g_state.launch_after_disc) { g_state.launch_after_disc = false; launch(req); }
    if (firstrun::take_start_now()) launch(req);
    if (firstrun::active()) { firstrun::draw(g, in, req); return; }
    g_state.launch_after_disc = false;
    if (play::running() && g_state.page != PAGE_RUNNING) g_state.page = PAGE_RUNNING;
    switch (g_state.page) {
        case PAGE_VERSUS:  draw_versus(g, in); break;
        case PAGE_START:   draw_start(g, in, req); break;
        case PAGE_RUNNING: draw_running(g, in); break;
        case PAGE_CLOSED:  draw_closed(g, in, req); break;
        default:           draw_main(g, in, req); break;
    }
}

} /* namespace menu */

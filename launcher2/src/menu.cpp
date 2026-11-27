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
 *     and the game. Starting the game comes in 0.2.3; SETTINGS, MODS and
 *     UPDATES say which step brings them. VERSUS and ROGUE ARENA are marked as
 *     coming, with the version numbers of the current ladder (docs/versioning.md).
 */
#include "menu.h"

#include "firstrun.h"
#include "setup.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace menu {

using namespace ui;

namespace {

enum Page { PAGE_MAIN = 0, PAGE_VERSUS, PAGE__COUNT };

struct State {
    Page page = PAGE_MAIN;
    int  cursor[PAGE__COUNT] = {0};
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
    { MAIN_START, "START GAME", nullptr, "0.2.3",
      "The first time: find your Project Phantasma disc and build the game from it (PREPARING GAME). "
      "Once it is built, starting it from here comes with launcher 0.2.3." },
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
    if (in.start) activate = MAIN_START;

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

    if (activate < 0) return;
    cur = activate;
    const Entry& e = kMain[activate];
    switch (e.id) {
        case MAIN_START:
            if (!built || setup::snapshot().status == setup::Status::Running) firstrun::open();
            else note("THE GAME IS BUILT. STARTING IT COMES IN 0.2.3");
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

} /* namespace */

float scale_for(int width, int height) {
    const float s = std::min((float)width / kW, (float)height / kH);
    return s > 0.0f ? s : 1.0f;
}

void inject(const char* what) { ui::inject(what); }

void draw(const Host& host, Requests& req) {
    const Gfx g = ui::begin(host);
    const Input in = read_input();
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadBack, false)) req.toggle_fullscreen = true;
    if (firstrun::active()) { firstrun::draw(g, in, req); return; }
    switch (g_state.page) {
        case PAGE_VERSUS: draw_versus(g, in); break;
        default:          draw_main(g, in, req); break;
    }
}

} /* namespace menu */

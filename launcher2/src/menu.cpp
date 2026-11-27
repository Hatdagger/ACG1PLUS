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
 *   - 0.2.4: the UPDATES page (update.cpp does the work). The old page's
 *     GitHub token rows are gone (the repo is public); the hidden TEST
 *     channel is shown by pressing Y five times on the page.
 *   - 0.2.5: SETTINGS opens the settings pages (settings.cpp).
 */
#include "menu.h"

#include "firstrun.h"
#include "play.h"
#include "settings.h"
#include "setup.h"
#include "update.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "platform.h"

namespace menu {

using namespace ui;

namespace {

enum Page { PAGE_MAIN = 0, PAGE_VERSUS, PAGE_START, PAGE_RUNNING, PAGE_CLOSED, PAGE_UPDATES, PAGE__COUNT };

struct State {
    Page page = PAGE_MAIN;
    int  cursor[PAGE__COUNT] = {0};
    bool launch_after_disc = false;   /* the disc page was opened to start the game */
    /* PAGE_CLOSED: why the game couldn't start, or how it closed. */
    std::string closed_title, closed_text;
    play::Result closed;
    /* PAGE_UPDATES */
    int  confirm = -1;                /* asking before installing releases[confirm] */
    int  confirm_cursor = 0;
    bool confirm_rebuild = false;     /* asking before UPDATE THE GAME */
    int  y_presses = 0;               /* five Y presses show / hide the TEST channel */
    double y_last = 0.0;
    std::string updated_from;         /* the launcher was just updated from this version */
    bool focus_rebuild = false;       /* put the cursor on UPDATE THE GAME once */
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
    { MAIN_SETTINGS, "SETTINGS", nullptr, nullptr,
      "The game's display, picture, sound and system settings, and its button layout. "
      "Changes apply the next time the game starts. (This menu's own window: F11, "
      "ALT+ENTER or the controller's VIEW / SELECT button.)" },
    { MAIN_MODS, "MODS", nullptr, "0.2.6",
      "Switch installed mods on or off, and install new ones from the Mods folder. "
      "Coming in 0.2.6." },
    { MAIN_UPDATES, "UPDATES", nullptr, nullptr,
      "Look for a newer version and install it when you choose. The launcher replaces itself, then "
      "builds the game again from your disc; the game you have stays playable until the new one is ready." },
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
    /* 0.2.5.1: a new game being swapped in (a few seconds) can't be started from. */
    if (!setup::begin_game()) { note("THE NEW GAME IS BEING PUT IN PLACE. TRY AGAIN IN A MOMENT"); return; }
    std::string err;
    if (play::start(cue, &err)) {
        req.game_started = true;
        g_state.page = PAGE_RUNNING;
        g_state.cursor[PAGE_RUNNING] = 0;
    } else {
        setup::end_game();
        show_closed("THE GAME COULD NOT START", err);
    }
}

void draw_main(const Gfx& g, const Input& in, Requests& req) {
    move_cursor(PAGE_MAIN, kMainCount, in);
    int& cur = g_state.cursor[PAGE_MAIN];
    std::string built_info;
    const bool built = setup::game_built(&built_info);
    const bool outdated = built && setup::game_outdated(nullptr);

    header(g, "PROJECT PHANTASMA", "SYSTEM MENU");
    panel(g, 24.0f, 76.0f, 270.0f, 340.0f, "SELECT");
    int activate = -1;
    for (int i = 0; i < kMainCount; ++i) {
        bool hovered = false;
        const char* tag = kMain[i].tag;
        ImU32 tag_color = kDim;
        if (kMain[i].id == MAIN_START && !built) tag = "BUILD FIRST";
        if (kMain[i].id == MAIN_UPDATES) {
            const update::Snapshot u = update::snapshot();
            if (outdated) { tag = "REBUILD GAME"; tag_color = kHi; }
            else if (u.best >= 0) { tag = "NEW"; tag_color = kHi; }
        }
        if (row(g, 25.0f, 90.0f + kRowH * i, 268.0f, i == cur, true, kMain[i].label, tag, tag_color, &hovered))
            activate = i;
        if (hovered && mouse_moved()) cur = i;
    }
    if (in.confirm) activate = cur;
    /* 0.2.5.1: the game that is there can be played while a new one builds. */
    const bool start_now = in.start && built;
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
        if (s.status == setup::Status::Running)
            text(g, 424.0f, y, 11.0f, kHi, built ? "BUILDING THE NEW ONE..." : "PREPARING...");
        else if (built && outdated) text(g, 424.0f, y, 11.0f, kHi, ("BUILT  " + upper(built_info) + "  (OLDER)").c_str());
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
        /* What the update check found, in one line. */
        const update::Snapshot u = update::snapshot();
        y += 19.0f;
        if (u.best >= 0) {
            const update::Release& r = u.releases[u.best];
            text(g, 324.0f, y, 11.0f, kDim, "UPDATE");
            text(g, 424.0f, y, 11.0f, kHi, (r.version + " READY TO INSTALL").c_str());
        }
    }

    panel(g, 310.0f, 242.0f, 306.0f, 174.0f, "INFO");
    text(g, 324.0f, 258.0f, 11.0f, kHi, kMain[cur].label);
    text(g, 324.0f, 280.0f, 11.0f, kText, kMain[cur].info, 278.0f);
    if (kMain[cur].id == MAIN_UPDATES) {
        /* What's new, in one line (the release's WHATSNEW.txt). */
        const update::Snapshot u = update::snapshot();
        if (u.best >= 0) {
            const update::Release& r = u.releases[u.best];
            const std::string t = "NEW IN " + r.version + (r.summary.empty() ? "" : ": " + r.summary);
            text(g, 324.0f, 350.0f, 11.0f, kHi, t.c_str(), 278.0f);
        } else if (outdated) {
            text(g, 324.0f, 350.0f, 11.0f, kHi, "The game was built by an older launcher: UPDATE THE GAME builds it again.", 278.0f);
        }
    }

    footer(g, "A / ENTER  SELECT      D-PAD  MOVE      VIEW / F11  FULL SCREEN");

    if (start_now) { cur = MAIN_START; launch(req); return; }
    if (activate < 0) return;
    cur = activate;
    const Entry& e = kMain[activate];
    switch (e.id) {
        case MAIN_START:
            if (!built) firstrun::open();
            else { g_state.page = PAGE_START; g_state.cursor[PAGE_START] = 0; clear_note(); }
            break;
        case MAIN_VERSUS:
            g_state.page = PAGE_VERSUS;
            clear_note();
            break;
        case MAIN_UPDATES:
            g_state.page = PAGE_UPDATES;
            g_state.cursor[PAGE_UPDATES] = 0;
            g_state.confirm = -1;
            g_state.confirm_rebuild = false;
            clear_note();
            if (update::snapshot().state == update::State::Idle) update::check();
            break;
        case MAIN_SETTINGS:
            settings::open();
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

/* ---- the UPDATES page (0.2.4) ----------------------------------------------
 * CHECK FOR UPDATES, the release to install (or, on the TEST channel, the
 * newest test builds), UPDATE THE GAME when the game was built by an older
 * launcher, CHECK AT START, and CHANNEL once the TEST channel is shown. */

enum UpdAct { U_CHECK, U_INSTALL, U_REBUILD, U_AT_START, U_CHANNEL };
struct UpdRow { UpdAct act; int release; std::string label, value; ImU32 vc; bool enabled; std::string info; };

void draw_confirm(const Gfx& g, const Input& in, const char* title, const std::string& body, const char* yes,
                  bool* answered, bool* accepted) {
    *answered = *accepted = false;
    g.dl->AddRectFilled(at(g, 0.0f, 0.0f), at(g, kW, kH), IM_COL32(0, 0, 0, 150));
    g.dl->AddRectFilled(at(g, 110.0f, 130.0f), at(g, 530.0f, 330.0f), kBg);   /* nothing shows through */
    panel(g, 110.0f, 130.0f, 420.0f, 200.0f, title);
    text(g, 124.0f, 148.0f, 11.0f, kText, body.c_str(), 392.0f);
    const char* labels[2] = { yes, "CANCEL" };
    int& c = g_state.confirm_cursor;
    if (in.up || in.down) c = 1 - c;
    int act = -1;
    for (int i = 0; i < 2; ++i) {
        bool hov = false;
        if (row(g, 111.0f, 268.0f + kRowH * i, 418.0f, c == i, true, labels[i], nullptr, kDim, &hov)) act = i;
        if (hov && mouse_moved()) c = i;
    }
    if (in.confirm) act = c;
    if (in.back) act = 1;
    if (act >= 0) { *answered = true; *accepted = act == 0; }
}

void draw_updates(const Gfx& g, const Input& in, Requests& req) {
    (void)req;
    const update::Snapshot u = update::snapshot();
    const bool test = update::test_channel();
    const bool working = update::busy();
    const setup::Status bs = setup::snapshot().status;
    const bool building = bs == setup::Status::Running || bs == setup::Status::Cancelling;
    std::string built_from;
    const bool built = setup::game_built(nullptr);
    const bool outdated = built && setup::game_outdated(&built_from);

    std::vector<UpdRow> rows;
    rows.push_back({ U_CHECK, -1, "CHECK FOR UPDATES",
                     u.state == update::State::Checking ? "CHECKING..." : u.checked.empty() ? "" : "LAST " + u.checked,
                     kDim, !working,
                     "Ask GitHub for the newest " + std::string(test ? "test builds" : "release") +
                     ". Only a small list is read; nothing is installed until you choose it." });
    if (test) {
        for (int i = 0; i < (int)u.releases.size(); ++i) {
            const update::Release& r = u.releases[i];
            std::string label = r.tag.rfind("test-", 0) == 0 ? r.tag.substr(5) : r.tag;
            std::string value = r.installed ? "INSTALLED" : r.too_old ? "TOO OLD" : r.date;
            const ImU32 vc = r.installed ? kOn : r.too_old ? kOff : kDim;
            std::string info = "Test build " + r.tag + " (launcher " + r.version + ", " + r.date + ")" +
                               (r.summary.empty() ? "." : ": " + r.summary + ".") +
                               (r.installed ? " This is the launcher you are using."
                                : r.too_old ? " It is from before UPDATES, so it couldn't update back by itself."
                                : " Choose it to install it in place of this launcher.");
            rows.push_back({ U_INSTALL, i, upper(label), value, vc, !working && !building && !r.installed && !r.too_old, info });
        }
    } else {
        if (u.best >= 0) {
            const update::Release& r = u.releases[u.best];
            rows.push_back({ U_INSTALL, u.best, "INSTALL " + r.version, r.date, kHi, !working && !building,
                             "Version " + r.version + (r.summary.empty() ? "." : ": " + r.summary + ".") +
                             " The launcher replaces itself and starts again, then builds the game from your disc." });
        } else {
            rows.push_back({ U_INSTALL, -1, "INSTALL", "NOTHING NEWER", kOff, false,
                             "Nothing to install. CHECK FOR UPDATES looks again." });
        }
    }
    if (outdated)
        rows.push_back({ U_REBUILD, -1, "UPDATE THE GAME", "REBUILD", kHi, !working && !building,
                         "The game was built by an older launcher (source " +
                         (built_from.empty() ? std::string("?") : built_from) + "). Build it again from your disc "
                         "with this launcher's source. It takes a while (the time is shown); the game you have "
                         "stays playable until the new one is ready, and if the build stops nothing changes." });
    const bool at_start = update::check_at_start();
    rows.push_back({ U_AT_START, -1, "CHECK AT START", at_start ? "ON" : "OFF", at_start ? kOn : kOff, true,
                     "ON: the launcher looks for an update each time it starts and says so on the main menu. "
                     "OFF: only when you choose CHECK FOR UPDATES." });
    if (update::test_unlocked())
        rows.push_back({ U_CHANNEL, -1, "CHANNEL", test ? "TEST" : "RELEASES", test ? kHi : kText, !working,
                         "RELEASES: approved versions only. TEST: test builds of changes before they are merged "
                         "(the owner's). Press Y five times here to hide this row again." });

    const int n = (int)rows.size();
    const bool modal = g_state.confirm >= 0 || g_state.confirm_rebuild;
    if (!modal) move_cursor(PAGE_UPDATES, n, in);
    int& cur = g_state.cursor[PAGE_UPDATES];
    if (g_state.focus_rebuild) {
        g_state.focus_rebuild = false;
        for (int i = 0; i < n; ++i) if (rows[i].act == U_REBUILD) cur = i;
    }
    cur = std::clamp(cur, 0, n - 1);

    header(g, "UPDATES", test ? "TEST CHANNEL" : "RELEASES");
    const float sel_h = 14.0f + kRowH * n + 6.0f;
    panel(g, 24.0f, 76.0f, 300.0f, sel_h, "SELECT");
    int activate = -1;
    for (int i = 0; i < n; ++i) {
        bool hov = false;
        if (row(g, 25.0f, 90.0f + kRowH * i, 298.0f, i == cur, rows[i].enabled, rows[i].label.c_str(),
                rows[i].value.empty() ? nullptr : rows[i].value.c_str(), rows[i].vc, &hov) && !modal)
            activate = i;
        if (hov && mouse_moved() && !modal) cur = i;
    }

    /* Versions. */
    panel(g, 340.0f, 76.0f, 276.0f, 120.0f, "VERSION");
    {
        float y = 92.0f;
        text(g, 354.0f, y, 11.0f, kDim, "LAUNCHER");
        text(g, 440.0f, y, 11.0f, kText, (std::string(g.host->version) + "  TEST").c_str());
        y += 19.0f;
        text(g, 354.0f, y, 11.0f, kDim, "GAME");
        if (!built) text(g, 440.0f, y, 11.0f, kWarn, "NOT BUILT YET");
        else if (outdated) text(g, 440.0f, y, 11.0f, kHi, ("SOURCE " + upper(built_from.empty() ? "?" : built_from) + "  (OLDER)").c_str());
        else if (setup::source_known()) text(g, 440.0f, y, 11.0f, kOn, ("SOURCE " + upper(setup::source_version())).c_str());
        else text(g, 440.0f, y, 11.0f, kOn, "BUILT (DEVELOPER LAUNCHER)");
        y += 19.0f;
        text(g, 354.0f, y, 11.0f, kDim, "NEWEST");
        std::string newest = "NOT CHECKED YET";
        if (u.state == update::State::Checking) newest = "CHECKING...";
        else if (!u.checked.empty()) {
            if (test) newest = u.releases.empty() ? "NO TEST BUILDS" : upper(u.releases[0].tag);
            else if (u.best >= 0) newest = u.releases[u.best].version + "  " + u.releases[u.best].date;
            else newest = u.releases.empty() ? "NO RELEASE YET" : "YOU HAVE IT";
        }
        text(g, 440.0f, y, 11.0f, u.best >= 0 && !test ? kHi : kText, fit(g, 11.0f, newest, 166.0f).c_str());
        y += 19.0f;
        text(g, 354.0f, y, 11.0f, kDim, "CHANNEL");
        text(g, 440.0f, y, 11.0f, test ? kHi : kText, test ? "TEST" : "RELEASES");
    }

    /* What is happening, then what the row does. */
    const float iy = std::max(212.0f, 76.0f + sel_h + 16.0f);
    panel(g, 24.0f, iy, 592.0f, 416.0f - iy, "INFO");
    {
        float y = iy + 16.0f;
        if (!g_state.updated_from.empty()) {
            const std::string t = "LAUNCHER UPDATED FROM " + g_state.updated_from + " TO " + g.host->version + "." +
                                  (outdated ? " NOW UPDATE THE GAME." : "");
            text(g, 38.0f, y, 11.0f, kOn, t.c_str(), 564.0f, true);
            y += 19.0f;
        }
        if (!u.message.empty()) {
            const ImU32 c = u.bad ? kWarn : u.state == update::State::Checked && u.best >= 0 ? kHi : kText;
            text(g, 38.0f, y, 11.0f, c, u.message.c_str(), 564.0f);
            y += 32.0f;
        }
        if (u.state == update::State::Downloading && u.total) {
            const float f = std::min(1.0f, (float)((double)u.done / (double)u.total));
            bar(g, 38.0f, y, 564.0f, 10.0f, f, kHi);
            char b[64];
            std::snprintf(b, sizeof b, "%.1f OF %.1f MB", u.done / 1048576.0, u.total / 1048576.0);
            text(g, 38.0f, y + 14.0f, 10.0f, kDim, b);
            y += 32.0f;
        }
        if (building) {
            text(g, 38.0f, y, 11.0f, kHi, "THE GAME IS BEING PREPARED. INSTALLING WAITS UNTIL IT HAS FINISHED.", 564.0f);
            y += 19.0f;
        }
        if (y < 416.0f - 40.0f) text(g, 38.0f, y + 4.0f, 11.0f, kDim, rows[cur].info.c_str(), 564.0f);
    }
    footer(g, "A / ENTER  SELECT      D-PAD  MOVE      B / ESC  BACK");

    /* Asking first. */
    if (g_state.confirm >= 0) {
        if (g_state.confirm >= (int)u.releases.size()) { g_state.confirm = -1; return; }
        const update::Release& r = u.releases[g_state.confirm];
        const std::string body = "Install launcher " + r.version + " (" + r.tag + ")?\n\n" +
            "The launcher replaces itself and starts again. Then UPDATE THE GAME builds the game from your disc "
            "with the new source; until that has finished, the game you have now stays playable.\n\n"
            "Memory cards, settings and your disc are not touched.";
        bool answered, yes;
        draw_confirm(g, in, "INSTALL", body, "INSTALL", &answered, &yes);
        if (answered) {
            const int idx = g_state.confirm;
            g_state.confirm = -1;
            if (yes) update::install(idx);
        }
        return;
    }
    if (g_state.confirm_rebuild) {
        bool answered, yes;
        draw_confirm(g, in, "UPDATE THE GAME",
                     "Build the game again from your disc with this launcher's source?\n\n"
                     "It runs like the first build (often faster: the tools are already here). The game you have "
                     "stays until the new one has been built; if the build stops or fails, nothing changes.",
                     "BUILD IT NOW", &answered, &yes);
        if (answered) {
            g_state.confirm_rebuild = false;
            if (yes) { g_state.updated_from.clear(); firstrun::rebuild(); }
        }
        return;
    }

    /* Five Y presses in a row (within 2 s of each other): show / hide TEST. */
    if (in.y) {
        const double t = ImGui::GetTime();
        g_state.y_presses = t - g_state.y_last < 2.0 ? g_state.y_presses + 1 : 1;
        g_state.y_last = t;
        if (g_state.y_presses >= 5) {
            g_state.y_presses = 0;
            const bool on = !update::test_unlocked();
            update::set_test_unlocked(on);
            note(on ? "TEST CHANNEL SHOWN" : "TEST CHANNEL HIDDEN");
        }
    }
    if (in.back) { g_state.page = PAGE_MAIN; g_state.updated_from.clear(); clear_note(); return; }
    const UpdRow& r = rows[cur];
    if ((in.left || in.right) && r.enabled) {
        if (r.act == U_AT_START) { update::set_check_at_start(!at_start); return; }
        if (r.act == U_CHANNEL) { update::set_test_channel(!test); update::check(); return; }
    }
    if (in.confirm) activate = cur;
    if (activate < 0) return;
    cur = activate;
    const UpdRow& a = rows[activate];
    if (!a.enabled) {
        if (building && (a.act == U_INSTALL || a.act == U_REBUILD)) note("WAIT UNTIL THE GAME HAS BEEN PREPARED");
        return;
    }
    switch (a.act) {
        case U_CHECK:    update::check(); break;
        case U_INSTALL:  g_state.confirm = a.release; g_state.confirm_cursor = 0; break;
        case U_REBUILD:  g_state.confirm_rebuild = true; g_state.confirm_cursor = 0; break;
        case U_AT_START: update::set_check_at_start(!at_start); break;
        case U_CHANNEL:  update::set_test_channel(!test); update::check(); break;
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

void open_updates(const std::string& updated_from) {
    g_state.page = PAGE_UPDATES;
    g_state.cursor[PAGE_UPDATES] = 0;
    g_state.updated_from = updated_from;
    g_state.focus_rebuild = !updated_from.empty();
    g_state.confirm = -1;
    g_state.confirm_rebuild = false;
}

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
    /* VIEW / SELECT: this menu's window or full screen (not while a button is being set). */
    if (!holding() && !settings::capture_event_pending() && ImGui::IsKeyPressed(ImGuiKey_GamepadBack, false))
        req.toggle_fullscreen = true;
    /* The disc page was opened by START GAME (the disc had moved), or START
     * GAME was chosen on the "ready" page: carry on starting the game. */
    if (firstrun::take_disc_ready() && g_state.launch_after_disc) { g_state.launch_after_disc = false; launch(req); }
    if (firstrun::take_start_now()) launch(req);
    if (firstrun::active()) { firstrun::draw(g, in, req); return; }
    firstrun::take_ready_note();
    if (settings::active() && !play::running()) { settings::draw(g, in); return; }
    g_state.launch_after_disc = false;
    if (play::running() && g_state.page != PAGE_RUNNING) g_state.page = PAGE_RUNNING;
    switch (g_state.page) {
        case PAGE_VERSUS:  draw_versus(g, in); break;
        case PAGE_START:   draw_start(g, in, req); break;
        case PAGE_RUNNING: draw_running(g, in); break;
        case PAGE_CLOSED:  draw_closed(g, in, req); break;
        case PAGE_UPDATES: draw_updates(g, in, req); break;
        default:           draw_main(g, in, req); break;
    }
}

} /* namespace menu */

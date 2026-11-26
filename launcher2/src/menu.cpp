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
 *   - Step 0.2.1 is only the shell: START GAME, SETTINGS, MODS and UPDATES say
 *     which step brings them. VERSUS and ROGUE ARENA are marked as coming,
 *     with the version numbers of the current ladder (docs/versioning.md):
 *     Link Versus moved from 0.4 to 0.5 when the ladder was rewritten.
 */
#include "menu.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace menu {

namespace {

/* Design grid. */
const float kW = 640.0f, kH = 480.0f;
const float kRowH = 26.0f;

const ImU32 kBg     = IM_COL32(6, 9, 13, 255);
const ImU32 kGrid   = IM_COL32(26, 43, 51, 110);
const ImU32 kPanel  = IM_COL32(11, 18, 24, 225);
const ImU32 kLine   = IM_COL32(47, 74, 85, 255);
const ImU32 kText   = IM_COL32(185, 212, 207, 255);
const ImU32 kBright = IM_COL32(255, 244, 220, 255);
const ImU32 kTitle  = IM_COL32(223, 238, 234, 255);
const ImU32 kDim    = IM_COL32(95, 127, 128, 255);
const ImU32 kHi     = IM_COL32(240, 160, 32, 255);
const ImU32 kOn     = IM_COL32(95, 224, 138, 255);
const ImU32 kOff    = IM_COL32(61, 84, 88, 255);
const ImU32 kWarn   = IM_COL32(226, 91, 58, 255);

enum Page { PAGE_MAIN = 0, PAGE_VERSUS, PAGE__COUNT };

struct State {
    Page   page = PAGE_MAIN;
    int    cursor[PAGE__COUNT] = {0};
    char   note[200] = {0};
    double note_until = 0.0;
    /* Simulated presses for tests (inject()). */
    bool   fake_up = false, fake_down = false, fake_left = false, fake_right = false;
    bool   fake_confirm = false, fake_back = false;
};
State g_state;

struct Gfx {
    ImDrawList* dl;
    const Host* host;
    float       s;      /* window pixels per design unit */
    ImVec2      o;      /* window position of design (0,0) */
};

ImVec2 at(const Gfx& g, float x, float y) {
    return ImVec2(g.o.x + x * g.s, g.o.y + y * g.s);
}

ImFont* font_of(const Gfx& g, float size, bool bold) {
    ImFont* f = g.host->font ? g.host->font(size, bold) : nullptr;
    return f ? f : ImGui::GetFont();
}

float text_width(const Gfx& g, float size, const char* s, bool bold = false) {
    return font_of(g, size, bold)->CalcTextSizeA(size * g.s, FLT_MAX, 0.0f, s).x / g.s;
}

void text(const Gfx& g, float x, float y, float size, ImU32 color,
          const char* s, float wrap = 0.0f, bool bold = false) {
    /* Whole pixels: a baked font drawn on a half pixel blurs. */
    ImVec2 p = at(g, x, y);
    p.x = std::floor(p.x + 0.5f);
    p.y = std::floor(p.y + 0.5f);
    g.dl->AddText(font_of(g, size, bold), size * g.s, p, color, s, nullptr,
                  wrap > 0.0f ? wrap * g.s : 0.0f);
}

void text_right(const Gfx& g, float right, float y, float size, ImU32 color,
                const char* s) {
    text(g, right - text_width(g, size, s), y, size, color, s);
}

void note(const char* s) {
    std::snprintf(g_state.note, sizeof g_state.note, "%s", s ? s : "");
    g_state.note_until = ImGui::GetTime() + 4.0;
}

void panel(const Gfx& g, float x, float y, float w, float h, const char* caption) {
    g.dl->AddRectFilled(at(g, x, y), at(g, x + w, y + h), kPanel);
    g.dl->AddRect(at(g, x, y), at(g, x + w, y + h), kLine);
    if (caption && caption[0]) {
        const float cw = text_width(g, 10.0f, caption);
        g.dl->AddRectFilled(at(g, x + 10.0f, y - 6.0f), at(g, x + 22.0f + cw, y + 6.0f), kBg);
        text(g, x + 16.0f, y - 6.0f, 10.0f, kDim, caption);
    }
}

void header(const Gfx& g, const char* title, const char* sub) {
    text(g, 24.0f, 22.0f, 19.0f, kTitle, title, 0.0f, true);
    text_right(g, kW - 24.0f, 32.0f, 10.0f, kDim, sub);
    g.dl->AddLine(at(g, 24.0f, 52.0f), at(g, kW - 24.0f, 52.0f), kLine);
}

void footer(const Gfx& g, const char* hints) {
    g.dl->AddLine(at(g, 24.0f, kH - 36.0f), at(g, kW - 24.0f, kH - 36.0f), kLine);
    text(g, 24.0f, kH - 28.0f, 10.0f, kDim, hints);
    if (g_state.note[0] && ImGui::GetTime() < g_state.note_until)
        text_right(g, kW - 24.0f, kH - 28.0f, 10.0f, kHi, g_state.note);
}

/* One selectable row. Returns true when it was clicked with the mouse;
 * *hovered_out says whether the mouse is over it. */
bool row(const Gfx& g, float x, float y, float w, bool selected, bool enabled,
         const char* label, const char* value, ImU32 value_color, bool* hovered_out) {
    const ImVec2 a = at(g, x, y), b = at(g, x + w, y + kRowH);
    const bool hovered = ImGui::IsMouseHoveringRect(a, b, false);
    if (hovered_out) *hovered_out = hovered;
    if (selected) {
        g.dl->AddRectFilledMultiColor(a, b, IM_COL32(240, 160, 32, 70), IM_COL32(240, 160, 32, 0),
                                      IM_COL32(240, 160, 32, 0), IM_COL32(240, 160, 32, 70));
        g.dl->AddTriangleFilled(at(g, x + 12.0f, y + 8.0f), at(g, x + 21.0f, y + 13.0f),
                                at(g, x + 12.0f, y + 18.0f), kHi);
    }
    const ImU32 color = !enabled ? (selected ? kDim : kOff) : selected ? kBright : kText;
    text(g, x + 30.0f, y + 6.0f, 13.0f, color, label);
    if (value && value[0]) text_right(g, x + w - 14.0f, y + 6.0f, 10.0f, value_color, value);
    return hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
}

bool mouse_moved() {
    const ImVec2 d = ImGui::GetIO().MouseDelta;
    return d.x != 0.0f || d.y != 0.0f;
}

/* ---- input ---------------------------------------------------------------- */

struct Input { bool up, down, left, right, confirm, back, start; };

bool key(ImGuiKey k, bool repeat) { return ImGui::IsKeyPressed(k, repeat); }

Input read_input() {
    Input in = {};
    in.up    = key(ImGuiKey_UpArrow, true) || key(ImGuiKey_GamepadDpadUp, true) ||
               key(ImGuiKey_GamepadLStickUp, true) || key(ImGuiKey_W, true);
    in.down  = key(ImGuiKey_DownArrow, true) || key(ImGuiKey_GamepadDpadDown, true) ||
               key(ImGuiKey_GamepadLStickDown, true) || key(ImGuiKey_S, true);
    in.left  = key(ImGuiKey_LeftArrow, true) || key(ImGuiKey_GamepadDpadLeft, true) ||
               key(ImGuiKey_GamepadLStickLeft, true);
    in.right = key(ImGuiKey_RightArrow, true) || key(ImGuiKey_GamepadDpadRight, true) ||
               key(ImGuiKey_GamepadLStickRight, true);
    /* Enter with Alt held is the full-screen key, not a choice. */
    const bool alt = ImGui::GetIO().KeyAlt;
    in.confirm = (!alt && (key(ImGuiKey_Enter, false) || key(ImGuiKey_KeypadEnter, false))) ||
                 key(ImGuiKey_Space, false) || key(ImGuiKey_GamepadFaceDown, false);
    in.back  = key(ImGuiKey_Escape, false) || key(ImGuiKey_Backspace, false) ||
               key(ImGuiKey_GamepadFaceRight, false);
    in.start = key(ImGuiKey_GamepadStart, false);

    in.up |= g_state.fake_up;       in.down |= g_state.fake_down;
    in.left |= g_state.fake_left;   in.right |= g_state.fake_right;
    in.confirm |= g_state.fake_confirm;
    in.back |= g_state.fake_back;
    g_state.fake_up = g_state.fake_down = g_state.fake_left = g_state.fake_right = false;
    g_state.fake_confirm = g_state.fake_back = false;
    return in;
}

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
    { MAIN_START, "START GAME", nullptr, "0.2.2 AND 0.2.3",
      "Build the game from your own Project Phantasma disc the first time, then play. "
      "This test launcher can't do that yet: finding the disc and building come in "
      "0.2.2, starting the game in 0.2.3." },
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

std::string upper(const char* s) {
    std::string out(s ? s : "");
    for (char& c : out) c = (char)std::toupper((unsigned char)c);
    return out;
}

void draw_main(const Gfx& g, const Input& in, Requests& req) {
    move_cursor(PAGE_MAIN, kMainCount, in);
    int& cur = g_state.cursor[PAGE_MAIN];

    header(g, "PROJECT PHANTASMA", "SYSTEM MENU");
    panel(g, 24.0f, 76.0f, 270.0f, 340.0f, "SELECT");
    int activate = -1;
    for (int i = 0; i < kMainCount; ++i) {
        bool hovered = false;
        if (row(g, 25.0f, 90.0f + kRowH * i, 268.0f, i == cur, true, kMain[i].label,
                kMain[i].tag, kDim, &hovered))
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
        text(g, 324.0f, y, 11.0f, kDim, "DISC");
        text(g, 424.0f, y, 11.0f, kWarn, "NOT LOOKED FOR YET (0.2.2)");
        y += 19.0f;
        text(g, 324.0f, y, 11.0f, kDim, "GAME");
        text(g, 424.0f, y, 11.0f, kWarn, "NOT BUILT YET (0.2.2)");
        y += 19.0f;
        text(g, 324.0f, y, 11.0f, kDim, "CONTROLLER");
        const std::string pad = h.controller && h.controller[0] ? upper(h.controller)
                                                                 : std::string("NONE (KEYBOARD AND MOUSE)");
        text(g, 424.0f, y, 11.0f, h.controller && h.controller[0] ? kText : kDim, pad.c_str());
        y += 19.0f;
        text(g, 324.0f, y, 11.0f, kDim, "DISPLAY");
        std::snprintf(buf, sizeof buf, "%s  %dx%d", h.fullscreen ? "FULL SCREEN" : "WINDOW",
                      h.width, h.height);
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
        case MAIN_VERSUS:
            g_state.page = PAGE_VERSUS;
            g_state.note[0] = '\0';
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

    const bool right_click = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    if (in.back || right_click) {
        g_state.page = PAGE_MAIN;
        g_state.note[0] = '\0';
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

void inject(const char* what) {
    if (!what) return;
    if (!std::strcmp(what, "up")) g_state.fake_up = true;
    else if (!std::strcmp(what, "down")) g_state.fake_down = true;
    else if (!std::strcmp(what, "left")) g_state.fake_left = true;
    else if (!std::strcmp(what, "right")) g_state.fake_right = true;
    else if (!std::strcmp(what, "confirm")) g_state.fake_confirm = true;
    else if (!std::strcmp(what, "back")) g_state.fake_back = true;
}

void draw(const Host& host, Requests& req) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    Gfx g;
    g.dl = ImGui::GetBackgroundDrawList();
    g.host = &host;
    g.s = scale_for((int)vp->Size.x, (int)vp->Size.y);
    g.o = ImVec2(std::floor(vp->Pos.x + (vp->Size.x - kW * g.s) * 0.5f),
                 std::floor(vp->Pos.y + (vp->Size.y - kH * g.s) * 0.5f));

    /* Ground and grid across the whole window, not just the design area. */
    const ImVec2 a = vp->Pos, b = ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y);
    g.dl->AddRectFilled(a, b, kBg);
    const float step = 32.0f * g.s;
    for (float x = std::fmod(g.o.x, step); x < b.x; x += step)
        g.dl->AddLine(ImVec2(x, a.y), ImVec2(x, b.y), kGrid);
    for (float y = std::fmod(g.o.y, step); y < b.y; y += step)
        g.dl->AddLine(ImVec2(a.x, y), ImVec2(b.x, y), kGrid);

    const Input in = read_input();
    if (key(ImGuiKey_GamepadBack, false)) req.toggle_fullscreen = true;
    switch (g_state.page) {
        case PAGE_VERSUS: draw_versus(g, in); break;
        default:          draw_main(g, in, req); break;
    }
}

} /* namespace menu */

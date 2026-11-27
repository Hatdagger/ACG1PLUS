#include "ui.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ui {

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

namespace {
char   g_note[200] = {0};
double g_note_until = 0.0;
bool   g_fake[9] = {};   /* up down left right confirm back start x y */

ImFont* font_of(const Gfx& g, float size, bool bold) {
    ImFont* f = g.host->font ? g.host->font(size, bold) : nullptr;
    return f ? f : ImGui::GetFont();
}
bool key(ImGuiKey k, bool repeat) { return ImGui::IsKeyPressed(k, repeat); }
} /* namespace */

Gfx begin(const Host& host) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    Gfx g;
    g.dl = ImGui::GetBackgroundDrawList();
    g.host = &host;
    g.s = std::min(vp->Size.x / kW, vp->Size.y / kH);
    if (g.s <= 0.0f) g.s = 1.0f;
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
    return g;
}

ImVec2 at(const Gfx& g, float x, float y) { return ImVec2(g.o.x + x * g.s, g.o.y + y * g.s); }

float text_width(const Gfx& g, float size, const char* s, bool bold) {
    return font_of(g, size, bold)->CalcTextSizeA(size * g.s, FLT_MAX, 0.0f, s).x / g.s;
}

void text(const Gfx& g, float x, float y, float size, ImU32 color, const char* s, float wrap, bool bold) {
    /* Whole pixels: a baked font drawn on a half pixel blurs. */
    ImVec2 p = at(g, x, y);
    p.x = std::floor(p.x + 0.5f);
    p.y = std::floor(p.y + 0.5f);
    g.dl->AddText(font_of(g, size, bold), size * g.s, p, color, s, nullptr, wrap > 0.0f ? wrap * g.s : 0.0f);
}

void text_right(const Gfx& g, float right, float y, float size, ImU32 color, const char* s) {
    text(g, right - text_width(g, size, s), y, size, color, s);
}

std::string fit(const Gfx& g, float size, const std::string& s, float width) {
    if (text_width(g, size, s.c_str()) <= width) return s;
    /* Keep the end (the file name) and as much of the start as fits. */
    const size_t keep_end = std::min<size_t>(s.size(), 28);
    const std::string tail = s.substr(s.size() - keep_end);
    for (size_t front = s.size() - keep_end; front > 0; front = front > 4 ? front - 4 : 0) {
        const std::string t = s.substr(0, front) + "..." + tail;
        if (text_width(g, size, t.c_str()) <= width) return t;
    }
    return "..." + tail;
}

void note(const char* s) {
    std::snprintf(g_note, sizeof g_note, "%s", s ? s : "");
    g_note_until = ImGui::GetTime() + 4.0;
}
void clear_note() { g_note[0] = '\0'; }

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
    if (g_note[0] && ImGui::GetTime() < g_note_until)
        text_right(g, kW - 24.0f, kH - 28.0f, 10.0f, kHi, g_note);
}

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

void bar(const Gfx& g, float x, float y, float w, float h, float frac, ImU32 color) {
    frac = std::clamp(frac, 0.0f, 1.0f);
    g.dl->AddRectFilled(at(g, x, y), at(g, x + w, y + h), IM_COL32(20, 32, 40, 255));
    if (frac > 0.0f) g.dl->AddRectFilled(at(g, x, y), at(g, x + w * frac, y + h), color);
    g.dl->AddRect(at(g, x, y), at(g, x + w, y + h), kLine);
}

bool mouse_moved() {
    const ImVec2 d = ImGui::GetIO().MouseDelta;
    return d.x != 0.0f || d.y != 0.0f;
}

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
               key(ImGuiKey_GamepadFaceRight, false) || ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    in.start = key(ImGuiKey_GamepadStart, false);
    in.x     = key(ImGuiKey_GamepadFaceLeft, false) || key(ImGuiKey_Tab, false);
    in.y     = key(ImGuiKey_GamepadFaceUp, false) || key(ImGuiKey_L, false);

    bool* f = g_fake;
    in.up |= f[0]; in.down |= f[1]; in.left |= f[2]; in.right |= f[3];
    in.confirm |= f[4]; in.back |= f[5]; in.start |= f[6]; in.x |= f[7]; in.y |= f[8];
    for (bool& b : g_fake) b = false;
    return in;
}

void inject(const char* what) {
    if (!what) return;
    static const char* names[9] = { "up", "down", "left", "right", "confirm", "back", "start", "x", "y" };
    for (int i = 0; i < 9; ++i) if (!std::strcmp(what, names[i])) g_fake[i] = true;
}

std::string upper(const std::string& s) {
    std::string out(s);
    for (char& c : out) c = (char)std::toupper((unsigned char)c);
    return out;
}

} /* namespace ui */

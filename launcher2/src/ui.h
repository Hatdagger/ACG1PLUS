/* The Armored Core look shared by every launcher page: the dark ground and grid,
 * captioned panels, the amber cursor row, header and footer, and input
 * (controller first, keyboard and mouse as a fallback). Moved out of menu.cpp
 * in 0.2.2 so the first-run pages (firstrun.cpp) draw the same way.
 *
 * Everything is drawn on a 640x480 design grid scaled to the window. */
#pragma once

#include "imgui.h"

#include <string>

namespace ui {

/* What the pages need from the program around them. */
struct Host {
    const char* version;          /* launcher version, e.g. "0.2.2" */
    const char* system;           /* "WINDOWS", "STEAM DECK", "LINUX" */
    const char* controller;       /* first controller's name, or "" */
    bool        fullscreen;
    int         width, height;    /* window size in pixels */
    ImFont*   (*font)(float design_size, bool bold);
};

/* Asked of the program after a frame. */
struct Requests {
    bool quit = false;
    bool toggle_fullscreen = false;
};

extern const float kW, kH, kRowH;
extern const ImU32 kBg, kGrid, kPanel, kLine, kText, kBright, kTitle, kDim, kHi, kOn, kOff, kWarn;

struct Gfx {
    ImDrawList* dl;
    const Host* host;
    float       s;      /* window pixels per design unit */
    ImVec2      o;      /* window position of design (0,0) */
};

/* Sets up the frame and draws the ground and grid. */
Gfx begin(const Host& host);

ImVec2 at(const Gfx& g, float x, float y);
float  text_width(const Gfx& g, float size, const char* s, bool bold = false);
void   text(const Gfx& g, float x, float y, float size, ImU32 color, const char* s,
            float wrap = 0.0f, bool bold = false);
void   text_right(const Gfx& g, float right, float y, float size, ImU32 color, const char* s);
/* Text cut to fit a width, with "..." in the middle (long file paths). */
std::string fit(const Gfx& g, float size, const std::string& s, float width);
void   panel(const Gfx& g, float x, float y, float w, float h, const char* caption);
void   header(const Gfx& g, const char* title, const char* sub);
void   footer(const Gfx& g, const char* hints);
/* One selectable row; true when clicked with the mouse. */
bool   row(const Gfx& g, float x, float y, float w, bool selected, bool enabled,
           const char* label, const char* value, ImU32 value_color, bool* hovered_out);
void   bar(const Gfx& g, float x, float y, float w, float h, float frac, ImU32 color);
bool   mouse_moved();
/* A short message at the bottom right for a few seconds. */
void   note(const char* s);
void   clear_note();

struct Input { bool up, down, left, right, confirm, back, start, x, y; };
Input read_input();
/* Simulated presses for tests: up, down, left, right, confirm, back, x, y. */
void inject(const char* what);

std::string upper(const std::string& s);

} /* namespace ui */

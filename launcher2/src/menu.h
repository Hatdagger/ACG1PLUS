/* ACG1PLUS launcher (launcher2): the Armored Core system menu.
 *
 * Draws one page at a time straight onto ImGui's background draw list, on a
 * 640x480 design grid scaled to the window (as launcher/moa_menu.inc did), and
 * reads input through ImGui's key state: D-pad / stick / arrows to move, A /
 * Enter to choose, B / Esc to go back. Mouse: hover to point, click to choose.
 */
#pragma once

#include "imgui.h"

namespace menu {

/* What the menu needs from the program around it. */
struct Host {
    const char* version;          /* launcher version, e.g. "0.2.1" */
    const char* system;           /* "WINDOWS", "STEAM DECK", "LINUX" */
    const char* controller;       /* first controller's name, or "" */
    bool        fullscreen;
    int         width, height;    /* window size in pixels */
    /* Fonts baked for the current scale; font(size) picks the nearest. */
    ImFont*   (*font)(float design_size, bool bold);
};

/* Asked of the program after a frame. */
struct Requests {
    bool quit = false;
    bool toggle_fullscreen = false;
};

/* Pixels per design unit for a window of this size (fonts are baked from it). */
float scale_for(int width, int height);

/* Draw one frame of the menu. */
void draw(const Host& host, Requests& req);

/* Feed simulated presses (tests): "down", "up", "left", "right", "confirm",
 * "back". Applied on the next draw(). */
void inject(const char* what);

} /* namespace menu */

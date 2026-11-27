/* ACG1PLUS launcher (launcher2): the Armored Core system menu.
 *
 * Draws one page at a time straight onto ImGui's background draw list, on a
 * 640x480 design grid scaled to the window (as launcher/moa_menu.inc did), and
 * reads input through ImGui's key state: D-pad / stick / arrows to move, A /
 * Enter to choose, B / Esc to go back. Mouse: hover to point, click to choose.
 * The drawing helpers live in ui.h; the first-run pages in firstrun.h.
 */
#pragma once

#include "ui.h"

namespace menu {

using Host = ui::Host;
using Requests = ui::Requests;

/* Pixels per design unit for a window of this size (fonts are baked from it). */
float scale_for(int width, int height);

/* Draw one frame (the menu, or the first-run pages when they are open). */
void draw(const Host& host, Requests& req);

/* Feed simulated presses (tests): "down", "up", "left", "right", "confirm",
 * "back", "x", "y". Applied on the next draw(). */
void inject(const char* what);

} /* namespace menu */

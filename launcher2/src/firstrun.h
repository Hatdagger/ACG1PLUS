/* The first-run pages (ladder 0.2.2): welcome, finding the disc (or picking it
 * with a controller-friendly file browser), copying it into Disc/, and the
 * "Preparing game" screen with its steps, progress, time left, cancel / carry
 * on, log and retry. */
#pragma once

#include "ui.h"

#include <string>

namespace firstrun {

void open();                 /* show the first-run pages */
bool active();
void draw(const ui::Gfx& g, const ui::Input& in, ui::Requests& req);

/* Tests: go through by itself with the first good disc (copied in when
 * outside), and close the launcher when the game is ready (or the build stops). */
void set_test_auto(bool on, bool exit_when_done);
/* Changes whenever the page, the step or the outcome changes (test screenshots). */
std::string shot_key();
bool preparing();

/* For the main menu's STATUS panel. */
std::string disc_status(bool* ok);

} /* namespace firstrun */

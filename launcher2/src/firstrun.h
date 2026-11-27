/* The first-run pages (ladder 0.2.2): welcome, finding the disc (or picking it
 * with a controller-friendly file browser), copying it into Disc/, and the
 * "Preparing game" screen with its steps, progress, time left, cancel / carry
 * on, log and retry. */
#pragma once

#include "ui.h"

#include <filesystem>
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

/* 0.2.3 START GAME. The disc chosen last time, if it is still there (empty if
 * not). find_disc(): the disc page on its own, to choose the disc again;
 * take_disc_ready() is true once after one was chosen there. take_start_now()
 * is true once after START GAME was chosen on the "ready" page. */
std::filesystem::path chosen_disc();
void find_disc();
bool take_disc_ready();
bool take_start_now();

/* 0.2.4 UPDATES: build the game again (from this launcher's source) with the
 * disc chosen last time, or the disc page first if it has moved. The game that
 * is there now stays until the new one is in place. */
void rebuild();

/* For the main menu's STATUS panel. */
std::string disc_status(bool* ok);

} /* namespace firstrun */

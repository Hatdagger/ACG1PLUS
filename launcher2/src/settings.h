/* SETTINGS (ladder 0.2.5): every page of the game's old in-game settings menu
 * (launcher/moa_menu.inc: DISPLAY, PICTURE, SOUND / SYSTEM, ADVANCED, and the
 * BUTTON LAYOUT page), in the launcher.
 *
 * Where they are saved, beside the game in RecompData/<os>/ (so the Windows
 * and the Steam Deck builds each keep their own, which suits two different
 * screens):
 *   settings.toml   display, picture, sound and system rows (the file the game
 *                   reads at start; psxrecomp config_loader.cpp UserSettings)
 *   input.ini       the controller's buttons ([mapping], read by the game)
 *   keybinds.ini    the keyboard's keys ([player1], read by the game)
 * Only the lines the launcher changes are touched (kvfile.h): anything else in
 * those files stays as it was. Changes apply the next time the game starts.
 *
 * Rows that are framework player settings take their allowed values from
 * psxrecomp/recompiler/src/player_settings.h (built in), so a value the game
 * would refuse is never offered, and a row whose setting the framework drops
 * is hidden instead of written. "Game default" values come from game.toml,
 * also built in. */
#pragma once

#include "ui.h"

#include <SDL3/SDL.h>

#include <string>

namespace settings {

/* The game's settings.toml with what a first start should have (borderless
 * full screen, full volume), when it doesn't exist yet. Called before the game
 * starts and before the settings pages write anything. */
void ensure_file();

/* Open the SETTINGS pages; active() until the player backs out. */
void open();
bool active();
void draw(const ui::Gfx& g, const ui::Input& in);

/* While a button or key is being set: main.cpp gives every event here first;
 * true = taken (not passed on to the menu or the full-screen keys). */
bool capture_event(const SDL_Event& ev);
/* A button or key is being set (or was this frame): VIEW must not switch the window. */
bool capture_event_pending();

/* Test screenshots: which page / tab is up. */
std::string shot_key();

} /* namespace settings */

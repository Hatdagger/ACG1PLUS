/* Esc during play: back to the launcher.
 *
 * The runtime lets a title register its own in-game menu "provider"
 * (psxrecomp runtime/include/psx_menu_host.h), which sees every SDL event
 * before the game does. This one draws nothing and never opens: it only
 * watches for Esc and asks the runtime to end the session and go back to the
 * launcher (PSX_MENU_EXIT_TO_FRONTEND), the same exit the framework's own
 * in-game menu uses. Progress since the last in-game save is not kept.
 *
 * Registering it also takes the place of recomp-ui's in-game menu (Ctrl+L /
 * F10), which this project doesn't use.
 *
 * Started by the launcher program (launcher2, ACG1PLUS_LAUNCHER=1 in the
 * environment, ladder 0.2.3): Esc closes the game instead (PSX_MENU_EXIT_QUIT),
 * and the launcher, which waits for it, comes back. Holding SELECT and START
 * together for two seconds does the same, for players with only a controller
 * (the Steam Deck in Game Mode has no Esc key).
 *
 * During netplay Esc is left to the runtime, which already leaves the match
 * on Esc. The runtime only attaches a menu provider with the OpenGL renderer
 * (the default); with Vulkan or software, Esc does nothing.
 */
#include "psx_menu_host.h"
#include "psx_sdl.h"
#include "mod_plugins.h"   /* PSX_MOD_CONSTRUCTOR */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const PsxMenuHost* s_host = NULL;
static int s_leaving = 0;
/* 1 = started by launcher2: leaving closes the game. Read once. */
static int s_from_launcher = -1;
/* SELECT + START hold (launcher2 only): which of the two are down, and since when
 * both have been (SDL ticks, 0 = not both). */
static int s_select_down = 0, s_start_down = 0;
static unsigned long long s_both_since = 0;
#define ACG_HOLD_MS 2000ull

static int from_launcher(void)
{
    if (s_from_launcher < 0) {
        const char* e = getenv("ACG1PLUS_LAUNCHER");
        s_from_launcher = (e && e[0] && strcmp(e, "0") != 0) ? 1 : 0;
    }
    return s_from_launcher;
}

static void leave(const char* why)
{
    if (s_leaving || !s_host || !s_host->request_exit) return;
    if (s_host->netplay_active && s_host->netplay_active()) return;
    s_leaving = 1;   /* once; the session is ending */
    if (from_launcher()) {
        fprintf(stdout, "[acg] %s: closing the game (back to the launcher)\n", why);
        fflush(stdout);
        s_host->request_exit(PSX_MENU_EXIT_QUIT);
    } else {
        fprintf(stdout, "[acg] %s: back to the launcher\n", why);
        s_host->request_exit(PSX_MENU_EXIT_TO_FRONTEND);
    }
}

static int esc_attach(void* ctx, void* window, void* gl, const PsxMenuHost* host)
{
    (void)ctx; (void)window; (void)gl;
    s_host = host;
    s_leaving = 0;
    s_select_down = s_start_down = 0;
    s_both_since = 0;
    return host != NULL;
}

static int esc_event(void* ctx, const void* event)
{
    (void)ctx;
    const SDL_Event* ev = (const SDL_Event*)event;
    if (!s_host || !ev) return 0;
#if defined(PSX_SDL3)
    /* The controller hold: watched, never taken (the game still sees both
     * buttons, START is its pause). */
    if (from_launcher() && (ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || ev->type == SDL_EVENT_GAMEPAD_BUTTON_UP)) {
        const int down = ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        if (ev->gbutton.button == SDL_GAMEPAD_BUTTON_BACK) s_select_down = down;
        if (ev->gbutton.button == SDL_GAMEPAD_BUTTON_START) s_start_down = down;
        if (s_select_down && s_start_down) { if (!s_both_since) s_both_since = SDL_GetTicks(); }
        else s_both_since = 0;
        return 0;
    }
    if (ev->type != SDL_EVENT_KEY_DOWN || ev->key.repeat || ev->key.key != SDLK_ESCAPE) return 0;
#else
    if (ev->type != SDL_KEYDOWN || ev->key.repeat || ev->key.keysym.sym != SDLK_ESCAPE) return 0;
#endif
    if (s_host->netplay_active && s_host->netplay_active()) return 0;
    leave("Esc");
    return 1;
}

static int esc_is_open(void* ctx) { (void)ctx; return 0; }
static void esc_set_open(void* ctx, int open) { (void)ctx; (void)open; }
/* Called every frame the game shows (it draws nothing): the place to notice
 * that SELECT + START have been held long enough. */
static void esc_draw(void* ctx, int w, int h)
{
    (void)ctx; (void)w; (void)h;
#if defined(PSX_SDL3)
    if (s_both_since && SDL_GetTicks() - s_both_since >= ACG_HOLD_MS) {
        s_both_since = 0;
        leave("SELECT + START held");
    }
#endif
}
static void esc_detach(void* ctx) { (void)ctx; s_host = NULL; }

static const PsxMenuProvider k_provider = {
    PSX_MENU_ABI_VERSION, "acg-esc", NULL,
    esc_attach, esc_event, esc_is_open, esc_set_open, esc_draw, esc_detach
};

PSX_MOD_CONSTRUCTOR(acg_escape_register)
{
    if (!psx_menu_set_provider(&k_provider))
        fprintf(stderr, "[acg] could not register the Esc handler\n");
}

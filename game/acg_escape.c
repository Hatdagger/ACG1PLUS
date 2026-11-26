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
 * During netplay Esc is left to the runtime, which already leaves the match
 * on Esc. The runtime only attaches a menu provider with the OpenGL renderer
 * (the default); with Vulkan or software, Esc does nothing.
 */
#include "psx_menu_host.h"
#include "psx_sdl.h"
#include "mod_plugins.h"   /* PSX_MOD_CONSTRUCTOR */

#include <stdio.h>

static const PsxMenuHost* s_host = NULL;
static int s_leaving = 0;

static int esc_attach(void* ctx, void* window, void* gl, const PsxMenuHost* host)
{
    (void)ctx; (void)window; (void)gl;
    s_host = host;
    s_leaving = 0;
    return host != NULL;
}

static int esc_event(void* ctx, const void* event)
{
    (void)ctx;
    const SDL_Event* ev = (const SDL_Event*)event;
    if (!s_host || !ev) return 0;
#if defined(PSX_SDL3)
    if (ev->type != SDL_EVENT_KEY_DOWN || ev->key.repeat || ev->key.key != SDLK_ESCAPE) return 0;
#else
    if (ev->type != SDL_KEYDOWN || ev->key.repeat || ev->key.keysym.sym != SDLK_ESCAPE) return 0;
#endif
    if (s_host->netplay_active && s_host->netplay_active()) return 0;
    if (!s_leaving && s_host->request_exit) {
        s_leaving = 1;   /* once; the session is ending */
        fprintf(stdout, "[acg] Esc: back to the launcher\n");
        s_host->request_exit(PSX_MENU_EXIT_TO_FRONTEND);
    }
    return 1;
}

static int esc_is_open(void* ctx) { (void)ctx; return 0; }
static void esc_set_open(void* ctx, int open) { (void)ctx; (void)open; }
static void esc_draw(void* ctx, int w, int h) { (void)ctx; (void)w; (void)h; }
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

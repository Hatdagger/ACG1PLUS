/* Portable install (see acg_portable.h).
 *
 * The framework already keeps game.toml, the BIOS, settings, saves, memory
 * cards and its .cfg files beside the program. A few caches default to the
 * user's profile instead (AppData on Windows, ~/.cache or ~/.local on Linux).
 * Each of them reads a setting from the environment first, so this file sets
 * those settings, for this program only, before main() runs:
 *
 *   PSX_VK_PIPELINE_CACHE_DIR        Vulkan pipeline cache (gpu_vk_renderer.c)
 *   PSX_MEDIA_CACHE_DIR              imported media (mod_runtime.cpp; our patch
 *                                    patches/psxrecomp/0001-media-cache-dir.patch)
 *   PSX_RESIDENT_CACHE               mod resident data (mod_resident.cpp)
 *   RECOMP_UI_KEYBOARD_PROFILES_DIR  keyboard profiles (recomp-ui kb_profiles.c)
 *   RETCOMM_TOOLCHAIN_DIR            the compiler pack in RecompData/build/ (if
 *                                    there), so nothing looks in AppData for one
 *   RETCOMM_TOOLCHAIN_CACHE,         where a compiler pack would be unpacked:
 *   RETCOMM_DATA_HOME                inside the folder, never the shared cache
 *   PSXRECOMP_TOOLCHAIN_READONLY=1   never install or move a compiler pack
 *   RETCOMM_TOOLCHAIN_SKIP_UPDATE=1  never look online for a newer pack
 *   TEMP, TMP (Windows), TMPDIR      temporary files, also for the programs the
 *                                    game starts (the background overlay
 *                                    compiler): <program's folder>/cache/temp
 *
 * A value already set (by the player, or by a test) is left alone, except the
 * temp folder, which every system sets. Caches go
 * under <program's folder>/cache/, so the Windows and the Steam Deck builds,
 * which sit side by side in RecompData/windows and RecompData/linux, keep
 * their own. Folders are created here so nothing fails on a fresh copy.
 *
 * Written fresh for ACG1PLUS (2026-11-26); nothing like it in the old fork.
 */
#include "acg_portable.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <direct.h>
#define ACG_SEP "\\"
#else
#include <sys/stat.h>
#include <unistd.h>
#define ACG_SEP "/"
#endif

#define ACG_PATH_MAX 4096

static char g_exe_dir[ACG_PATH_MAX];     /* UTF-8, no trailing separator */
static char g_launcher_dir[ACG_PATH_MAX];  /* UTF-8, with trailing separator */

static int exe_dir(char* out, size_t cap) {
#ifdef _WIN32
    static wchar_t wide[32768];
    DWORD n = GetModuleFileNameW(NULL, wide, 32767);
    if (!n || n >= 32767) return 0;
    wide[n] = 0;
    wchar_t* slash = wcsrchr(wide, L'\\');
    if (!slash) return 0;
    *slash = 0;
    int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, (int)cap, NULL, NULL);
    return len > 0;
#else
    ssize_t n = readlink("/proc/self/exe", out, cap - 1);
    if (n <= 0 || (size_t)n >= cap - 1) return 0;
    out[n] = 0;
    char* slash = strrchr(out, '/');
    if (!slash) return 0;
    *slash = 0;
    return 1;
#endif
}

static const char* last_part(const char* path) {
    const char* a = strrchr(path, '/');
#ifdef _WIN32
    const char* b = strrchr(path, '\\');
    if (!a || (b && b > a)) a = b;
#endif
    return a ? a + 1 : path;
}

static int same_name(const char* a, const char* b) {
#ifdef _WIN32
    return _stricmp(a, b) == 0;
#else
    return strcmp(a, b) == 0;
#endif
}

/* Make every folder along `path` (UTF-8). */
static void make_dirs(const char* path) {
    char part[ACG_PATH_MAX];
    size_t len = strlen(path);
    if (len >= sizeof part) return;
    memcpy(part, path, len + 1);
    for (size_t i = 1; i <= len; ++i) {
        if (part[i] == '/' || part[i] == '\\' || part[i] == 0) {
            const char keep = part[i];
            part[i] = 0;
            if (part[0] && !(i == 2 && part[1] == ':')) {
#ifdef _WIN32
                wchar_t w[ACG_PATH_MAX];
                if (MultiByteToWideChar(CP_UTF8, 0, part, -1, w, ACG_PATH_MAX) > 0)
                    CreateDirectoryW(w, NULL);
#else
                mkdir(part, 0755);
#endif
            }
            part[i] = keep;
        }
    }
}

static int is_dir(const char* path) {
#ifdef _WIN32
    wchar_t w[ACG_PATH_MAX];
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, w, ACG_PATH_MAX) <= 0) return 0;
    DWORD a = GetFileAttributesW(w);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

/* Set name=value unless the player already set it. */
static void set_default(const char* name, const char* value) {
    const char* have = getenv(name);
    if (have && have[0]) return;
#ifdef _WIN32
    /* The wide call updates the process environment too, so programs the game
     * starts (the overlay compiler) see the same values. */
    wchar_t wn[128], wv[ACG_PATH_MAX];
    if (MultiByteToWideChar(CP_UTF8, 0, name, -1, wn, 128) <= 0) return;
    if (MultiByteToWideChar(CP_UTF8, 0, value, -1, wv, ACG_PATH_MAX) <= 0) return;
    _wputenv_s(wn, wv);
#else
    setenv(name, value, 0);
#endif
}

static void set_always(const char* name, const char* value) {
#ifdef _WIN32
    wchar_t wn[128], wv[ACG_PATH_MAX];
    if (MultiByteToWideChar(CP_UTF8, 0, name, -1, wn, 128) <= 0) return;
    if (MultiByteToWideChar(CP_UTF8, 0, value, -1, wv, ACG_PATH_MAX) <= 0) return;
    _wputenv_s(wn, wv);
#else
    setenv(name, value, 1);
#endif
}

static void set_dir(const char* name, const char* sub) {
    char path[ACG_PATH_MAX];
    int n = snprintf(path, sizeof path, "%s" ACG_SEP "%s", g_exe_dir, sub);
    if (n <= 0 || (size_t)n >= sizeof path) return;
    const char* have = getenv(name);
    if (have && have[0]) return;
    make_dirs(path);
    set_default(name, path);
}

/* <program's folder> is RecompData/<os>/ in the portable layout: the folder
 * above it is RecompData/. */
static int recompdata_dir(char* out, size_t cap) {
    char parent[ACG_PATH_MAX];
    snprintf(parent, sizeof parent, "%s", g_exe_dir);
    char* cut = (char*)last_part(parent);
    if (cut == parent) return 0;
    cut[-1] = 0;   /* drop "/<os>" */
    if (!same_name(last_part(parent), "RecompData")) return 0;
    int n = snprintf(out, cap, "%s", parent);
    return n > 0 && (size_t)n < cap;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
#endif
static void acg_portable_init(void) {
    if (!exe_dir(g_exe_dir, sizeof g_exe_dir)) return;

    set_dir("PSX_VK_PIPELINE_CACHE_DIR", "cache" ACG_SEP "vulkan");
    set_dir("PSX_MEDIA_CACHE_DIR", "cache" ACG_SEP "media");
    set_dir("PSX_RESIDENT_CACHE", "cache" ACG_SEP "resident");
    set_dir("RECOMP_UI_KEYBOARD_PROFILES_DIR", "keyboard-profiles");
    set_dir("RETCOMM_TOOLCHAIN_CACHE", "cache" ACG_SEP "toolchains");
    set_dir("RETCOMM_DATA_HOME", "cache" ACG_SEP "retcomm");
    set_default("PSXRECOMP_TOOLCHAIN_READONLY", "1");
    set_default("RETCOMM_TOOLCHAIN_SKIP_UPDATE", "1");
    {
        char temp[ACG_PATH_MAX];
        int n = snprintf(temp, sizeof temp, "%s" ACG_SEP "cache" ACG_SEP "temp", g_exe_dir);
        if (n > 0 && (size_t)n < sizeof temp) {
            make_dirs(temp);
#ifdef _WIN32
            set_always("TEMP", temp);
            set_always("TMP", temp);
#else
            set_always("TMPDIR", temp);
#endif
        }
    }

    char data[ACG_PATH_MAX], pack[ACG_PATH_MAX];
    if (recompdata_dir(data, sizeof data)) {
#ifdef _WIN32
        const char* os = "windows";
#else
        const char* os = "linux";
#endif
        int n = snprintf(pack, sizeof pack, "%s" ACG_SEP "build" ACG_SEP "tools-%s" ACG_SEP "toolchain",
                         data, os);
        if (n > 0 && (size_t)n < sizeof pack && is_dir(pack))
            set_default("RETCOMM_TOOLCHAIN_DIR", pack);
        n = snprintf(g_launcher_dir, sizeof g_launcher_dir, "%s" ACG_SEP "launcher" ACG_SEP, data);
        if (n <= 0 || (size_t)n >= sizeof g_launcher_dir) g_launcher_dir[0] = 0;
    }
    if (!g_launcher_dir[0]) {
        int n = snprintf(g_launcher_dir, sizeof g_launcher_dir, "%s" ACG_SEP "launcher" ACG_SEP,
                         g_exe_dir);
        if (n <= 0 || (size_t)n >= sizeof g_launcher_dir) g_launcher_dir[0] = 0;
    }
    if (g_launcher_dir[0]) make_dirs(g_launcher_dir);
}

const char* acg_portable_launcher_dir(void) {
    if (!g_exe_dir[0]) acg_portable_init();   /* compilers without constructors */
    return g_launcher_dir;
}

/* Missions from other games on Phantasma's missions engine: files, messages and the per-mission value.
 * See acg_mission_files.h for the numbering. Plan: docs/research/ac1/import-outlook.md §8, docs/ac1-import.md.
 *
 * 1. File loads (Phantasma's loader 0x800171B4(archive, entry, dest), a synchronous read of one archive entry).
 *    The missions engine's start-up loads four files by number: RTIM n (pictures), FDAT 200 (common file),
 *    FDAT 5n (mission code) and FDAT 5n + 4 (map). When the call comes from one of those four places and the
 *    mission is from another game, the file is copied from that game's pack instead (content/<game>/
 *    a<archive>-e<entry>.bin). Keyed by game so the Master of Arena import can use the same hook.
 * 2. Messages. AC1 missions bring AC1's message lines at AC1's own numbers (tools/ac1import/mission_data.py says
 *    why). The engine's own fixed lines are 50 higher on Phantasma (151 combat engaged, 155 out of area, 171
 *    defenses damaged, 149 abort?), so where the engine itself asks for one of those, 50 is taken off.
 * 3. The per-mission value (0x8008108C reads a u16 from a 40-entry table by engine number): for AC1 numbers it
 *    comes from AC1's own 100-entry table instead of reading past the end of Phantasma's.
 *
 * Stands aside during netplay. All state that changes is in guest RAM; the pack list is read once at start.
 */
#if !defined(_WIN32) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE   /* readlink */
#endif
#include "acg_mission_files.h"

#include "cpu_state.h"
#include "func_override.h"
#include "mod_netplay.h"
#include "mod_plugins.h"   /* PSX_MOD_CONSTRUCTOR, psx_mod_read/write_* */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* ---- addresses (Project Phantasma USA) ------------------------------------------------------------------ */

#define G_LOADER        0x800171B4u   /* exe: loader(archive, entry, dest) */
#define G_LOADER_W0     0x27BDFFD8u
#define G_LOADER_W1     0xAFB1001Cu

/* FDAT 202 start-up: the four loads by mission number (return addresses) and the words that prove it */
#define RA_RTIM         0x8004DB68u   /* loader(1, M+0, buffer) */
#define RA_COMMON       0x8004DB88u   /* loader(2, 200, buffer) */
#define RA_CODE         0x8004DD28u   /* loader(2, 5 * M+0, 0x80097A38) */
#define RA_MAP          0x8004DD48u   /* loader(2, 5 * M+0 + 4, buffer) */

#define G_SHOW_MESSAGE  0x80050ED0u   /* show message a2 */
#define G_ASK_MESSAGE   0x80050F0Cu   /* show message a0 and wait for an answer */
#define G_MISSION_VALUE 0x8008108Cu   /* u16 per mission -> 0x8008DE76; resets 256 records */
#define G_VALUE_OUT     0x8008DE76u
#define G_VALUE_RECORDS 0x801E657Cu

#define JAL(t) (0x0C000000u | (((t) >> 2) & 0x03FFFFFFu))

static void note(const char* what) { fprintf(stderr, "[acg missions] %s\n", what); }

/* ---- the engine and the numbers ------------------------------------------------------------------------- */

int acg_mission_source(uint32_t n)
{
    if (n >= 20u && n < 40u) return ACG_SRC_MOA;
    if (n >= ACG_AC1_FIRST && n < ACG_AC1_END) return ACG_SRC_AC1;
    return ACG_SRC_PHANTASMA;
}

int acg_missions_engine_resident(void)
{
    /* lbu M+0 beside the four loads, and their calls to the loader */
    return psx_mod_read_word(0x8004DB60u) == 0x92457A18u && psx_mod_read_word(0x8004DD14u) == 0x92027A18u
        && psx_mod_read_word(0x8004DD2Cu) == 0x92027A18u && psx_mod_read_word(RA_CODE - 8u) == JAL(G_LOADER)
        && psx_mod_read_word(RA_MAP - 8u) == JAL(G_LOADER) && psx_mod_read_word(RA_RTIM - 8u) == JAL(G_LOADER)
        && psx_mod_read_word(RA_COMMON - 8u) == JAL(G_LOADER);
}

static uint32_t mission_number(void) { return psx_mod_read_byte(ACG_PP_MISSION_NUMBER); }

/* ---- content folder and the AC1 pack ---------------------------------------------------------------------- */

static char s_content[1024];
static int s_content_done;

static void strip_to_dir(char* p)
{
    char* s = strrchr(p, '/');
#ifdef _WIN32
    char* b = strrchr(p, '\\');
    if (!s || (b && b > s)) s = b;
#endif
    if (s) s[1] = 0; else p[0] = 0;
}

const char* acg_content_dir(void)
{
    if (s_content_done) return s_content;
    s_content_done = 1;
    const char* env = getenv("ACG1PLUS_CONTENT_DIR");
    if (env && *env) {
        size_t n = strlen(env);
        if (n + 2 < sizeof s_content) {
            memcpy(s_content, env, n + 1);
            if (env[n - 1] != '/' && env[n - 1] != '\\') strcat(s_content, "/");
        }
        return s_content;
    }
    char exe[1024] = {0};
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, exe, (DWORD)sizeof exe - 1);
    if (n == 0 || n >= sizeof exe - 1) return s_content;
#else
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n <= 0) return s_content;
    exe[n] = 0;
#endif
    strip_to_dir(exe);                       /* .../RecompData/windows/ */
    char up[1024];
    snprintf(up, sizeof up, "%s", exe);
    size_t l = strlen(up);
    if (l) up[l - 1] = 0;
    strip_to_dir(up);                        /* .../RecompData/ */
    l = strlen(up);
    if (l >= 11 && (strncmp(up + l - 11, "RecompData/", 11) == 0 || strncmp(up + l - 11, "RecompData\\", 11) == 0))
        snprintf(s_content, sizeof s_content, "%scontent/", up);
    else
        snprintf(s_content, sizeof s_content, "%scontent/", exe);
    return s_content;
}

#define MAX_PACK_FILES 512
static struct { char name[24]; uint32_t size; } s_pack[MAX_PACK_FILES];
static int s_pack_count = -1;
static int s_test_slot = -1;

static FILE* open_in(const char* game, const char* name, const char* mode)
{
    char p[1200];
    snprintf(p, sizeof p, "%s%s/%s", acg_content_dir(), game, name);
    return fopen(p, mode);
}

static void read_ac1_pack(void)
{
    if (s_pack_count >= 0) return;
    s_pack_count = 0;
    FILE* f = open_in("ac1", "pack.txt", "r");
    if (!f) return;
    char line[256];
    if (!fgets(line, sizeof line, f) || strncmp(line, "acg1plus-ac1-pack 1", 19) != 0) {
        note("ac1 pack: pack.txt is not format 1, AC1 missions are off");
        fclose(f);
        return;
    }
    int ok = 1;
    while (fgets(line, sizeof line, f) && s_pack_count < MAX_PACK_FILES) {
        char name[64]; unsigned long size;
        if (sscanf(line, "%63s %lu", name, &size) != 2 || strlen(name) >= sizeof s_pack[0].name) continue;
        FILE* g = open_in("ac1", name, "rb");
        long have = -1;
        if (g) { fseek(g, 0, SEEK_END); have = ftell(g); fclose(g); }
        if (have != (long)size) {
            fprintf(stderr, "[acg missions] ac1 pack: %s is missing or the wrong size\n", name);
            ok = 0;
            continue;
        }
        strcpy(s_pack[s_pack_count].name, name);
        s_pack[s_pack_count].size = (uint32_t)size;
        s_pack_count++;
    }
    fclose(f);
    if (!ok) s_pack_count = 0;
    fprintf(stderr, "[acg missions] ac1 pack: %d files in %sac1/\n", s_pack_count, acg_content_dir());

    FILE* t = open_in("ac1", "test-slot.txt", "r");
    if (t) {
        int slot;
        if (fscanf(t, "%d", &slot) == 1 && slot >= 1 && slot < 70) s_test_slot = slot;
        fclose(t);
        if (s_test_slot >= 0)
            fprintf(stderr, "[acg missions] AC1 test switch: every Phantasma mission runs AC1 slot %d\n", s_test_slot);
    }
}

int acg_ac1_pack_ready(void) { read_ac1_pack(); return s_pack_count > 0; }
int acg_ac1_test_slot(void) { read_ac1_pack(); return s_pack_count > 0 ? s_test_slot : -1; }

static int pack_has(const char* name, uint32_t* size)
{
    for (int i = 0; i < s_pack_count; i++)
        if (strcmp(s_pack[i].name, name) == 0) { *size = s_pack[i].size; return 1; }
    return 0;
}

/* ---- 1. file loads ---------------------------------------------------------------------------------------- */

static int loader(struct CPUState* cpu)
{
    const uint32_t ra = cpu->gpr[31];
    if (ra != RA_RTIM && ra != RA_COMMON && ra != RA_CODE && ra != RA_MAP) return 0;
    if (psx_mod_netplay_is_active() || !acg_missions_engine_resident()) return 0;
    const uint32_t n = mission_number();
    const int src = acg_mission_source(n);
    if (src != ACG_SRC_AC1) return 0;          /* Phantasma's own; Master of Arena's pack comes later */
    const uint32_t arc = cpu->gpr[4] & 0xFFFFu, entry = cpu->gpr[5] & 0xFFFFu, dest = cpu->gpr[6];
    const uint32_t want = ra == RA_RTIM ? n : ra == RA_COMMON ? 200u : ra == RA_CODE ? 5u * n : 5u * n + 4u;
    if ((ra == RA_RTIM ? 1u : 2u) != arc || entry != want) {
        note("loader: arguments not as mapped, leaving the load to the disc");
        return 0;
    }
    char name[24];
    uint32_t size = 0;
    snprintf(name, sizeof name, "a%u-e%03u.bin", arc, entry);
    if (!acg_ac1_pack_ready() || !pack_has(name, &size)) {
        fprintf(stderr, "[acg missions] AC1 mission %u: %s is not in the pack\n", n - ACG_AC1_FIRST, name);
        return 0;
    }
    FILE* f = open_in("ac1", name, "rb");
    if (!f) return 0;
    /* the disc read fills whole 2048-byte sectors; the pack files are already whole sectors */
    uint8_t buf[2048];
    uint32_t at = dest, left = size;
    while (left) {
        size_t k = fread(buf, 1, left < sizeof buf ? left : sizeof buf, f);
        if (k == 0) break;
        size_t i = 0;
        if ((at & 3u) == 0)
            for (; i + 4 <= k; i += 4)
                psx_mod_write_word(at + (uint32_t)i, (uint32_t)buf[i] | (uint32_t)buf[i + 1] << 8
                                   | (uint32_t)buf[i + 2] << 16 | (uint32_t)buf[i + 3] << 24);
        for (; i < k; i++) psx_mod_write_byte(at + (uint32_t)i, buf[i]);
        at += (uint32_t)k; left -= (uint32_t)k;
    }
    fclose(f);
    if (left) {
        fprintf(stderr, "[acg missions] AC1 %s: short read\n", name);
        return 0;                                 /* nothing better to do than let the disc try */
    }
    if (ra == RA_CODE) fprintf(stderr, "[acg missions] AC1 mission %u from the pack\n", n - ACG_AC1_FIRST);
    cpu->gpr[2] = 0;                              /* the loader returns 0 once the read is good */
    return 1;
}

/* ---- 2. messages ------------------------------------------------------------------------------------------ */

/* The engine's own fixed lines: (return address, line asked for). Lines 50 lower on AC1. */
static const struct { uint32_t ra, jal_at; uint16_t line; } s_fixed_show[] = {
    { 0x8004A8FCu, 0x8004A8F4u, 155 },   /* out of area */
    { 0x8004A944u, 0x8004A93Cu, 171 },   /* defenses damaged (AC destroyed) */
    { 0x800887FCu, 0x800887F4u, 151 },   /* combat system engaged */
};
static const struct { uint32_t ra, jal_at; uint16_t line; } s_fixed_ask[] = {
    { 0x80076618u, 0x80076610u, 149 },
    { 0x800881F0u, 0x800881E8u, 149 },
};

static int ac1_running(void)
{
    return !psx_mod_netplay_is_active() && acg_mission_source(mission_number()) == ACG_SRC_AC1
        && acg_missions_engine_resident();
}

static int show_message(struct CPUState* cpu)
{
    for (size_t i = 0; i < sizeof s_fixed_show / sizeof s_fixed_show[0]; i++)
        if (cpu->gpr[31] == s_fixed_show[i].ra && cpu->gpr[6] == s_fixed_show[i].line
            && psx_mod_read_word(s_fixed_show[i].jal_at) == JAL(G_SHOW_MESSAGE) && ac1_running()) {
            cpu->gpr[6] -= 50u;
            break;
        }
    return 0;   /* the game's own routine shows it */
}

static int ask_message(struct CPUState* cpu)
{
    for (size_t i = 0; i < sizeof s_fixed_ask / sizeof s_fixed_ask[0]; i++)
        if (cpu->gpr[31] == s_fixed_ask[i].ra && cpu->gpr[4] == s_fixed_ask[i].line
            && psx_mod_read_word(s_fixed_ask[i].jal_at) == JAL(G_ASK_MESSAGE) && ac1_running()) {
            cpu->gpr[4] -= 50u;
            break;
        }
    return 0;
}

/* ---- 3. per-mission value ------------------------------------------------------------------------------- */

static uint16_t s_values[100];
static int s_values_state;          /* 0 not read, 1 read, -1 missing */

static int mission_value(struct CPUState* cpu)
{
    if (!ac1_running()) return 0;
    if (s_values_state == 0) {
        FILE* f = acg_ac1_pack_ready() ? open_in("ac1", "values.bin", "rb") : NULL;
        uint8_t b[200];
        s_values_state = -1;
        if (f && fread(b, 1, sizeof b, f) == sizeof b) {
            for (int i = 0; i < 100; i++) s_values[i] = (uint16_t)(b[2 * i] | b[2 * i + 1] << 8);
            s_values_state = 1;
        }
        if (f) fclose(f);
        if (s_values_state < 0) note("values.bin missing: AC1 missions use 56");
    }
    const uint32_t k = mission_number() - ACG_AC1_FIRST;
    const uint16_t v = s_values_state > 0 && k < 100u ? s_values[k] : 56u;
    /* what the routine does, with AC1's value */
    psx_mod_write_half(G_VALUE_OUT, v);
    for (uint32_t i = 0; i < 0x100u; i++) psx_mod_write_half(G_VALUE_RECORDS + 6u + i * 0xE0u, 0xFFFFu);
    cpu->gpr[2] = 0x80090000u;
    cpu->gpr[3] = v;
    cpu->gpr[4] = 0;
    cpu->gpr[5] = G_VALUE_RECORDS + 0x100u * 0xE0u;
    cpu->gpr[6] = 0xFFFFFFFFu;
    return 1;
}

PSX_MOD_CONSTRUCTOR(acg_mission_files_register)
{
    static const uint32_t g_loader[2] = { G_LOADER_W0, G_LOADER_W1 };
    static const uint32_t g_show[2] = { 0x27BDFFE8u, 0x3C02800Au };
    static const uint32_t g_ask[2] = { 0x27BDFFE8u, 0x00041040u };
    static const uint32_t g_value[2] = { 0x3C02801Eu, 0x2445657Cu };
    /* Credits: the loader stands in for a disc read (free, like fast loading); the message hooks only change an
     * argument and let the game's routine run; the value routine is ~770 guest instructions on the PS1. */
    if (func_override_add_guarded("acg:missions:loader", G_LOADER, loader, g_loader, 2, 0) != FO_OK)
        note("could not register the file-load hook");
    if (func_override_add_guarded("acg:missions:show", G_SHOW_MESSAGE, show_message, g_show, 2, 0) != FO_OK)
        note("could not register the message hook");
    if (func_override_add_guarded("acg:missions:ask", G_ASK_MESSAGE, ask_message, g_ask, 2, 0) != FO_OK)
        note("could not register the question hook");
    if (func_override_add_guarded("acg:missions:value", G_MISSION_VALUE, mission_value, g_value, 2, 780) != FO_OK)
        note("could not register the mission value hook");
}

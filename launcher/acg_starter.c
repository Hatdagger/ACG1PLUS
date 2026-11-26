/* The program the player starts on Windows: "Armored Core Gen 1.exe".
 *
 * The release folder is laid out for the player:
 *
 *   Armored Core Gen 1.exe   this program
 *   (disc image)             the player's own .cue/.bin, .chd or .iso
 *   Memory Card/             memory cards (card1.mcd, card2.mcd)
 *   Mods/                    drop mod packages (.psxmod) here
 *   RecompData/              the game and everything it needs
 *
 * It makes sure Memory Card/ and Mods/ exist, starts the game in RecompData/
 * with its memory cards pointed at Memory Card/, waits for it and hands back
 * its exit code. Anything after the program name is passed on untouched; if
 * that already names a memory card folder (--memcard-dir), it wins.
 *
 * After an update the game starts this program again with
 * "--after-update <process id>" (launcher/moa_update.cpp, restart()): it waits
 * for that old copy of the game to close (at most 30 seconds), drops those two
 * words and starts the new game as usual.
 *
 * The launcher finds the rest by itself: it sees that it runs from a folder
 * called RecompData and looks one level up for discs and the Mods folder
 * (launcher/moa_menu.inc), and the updater updates the whole folder
 * (launcher/moa_update.cpp).
 */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <wchar.h>

#ifndef ACG_GAME_EXE
#define ACG_GAME_EXE L"Armored_Core_Gen_1.exe"
#endif

#define TITLE L"Armored Core Gen 1"
#define BIG 32768

static void say(const wchar_t* what, const wchar_t* detail) {
    static wchar_t text[2048];
    _snwprintf(text, 2047, L"%ls\n\n%ls", what, detail ? detail : L"");
    text[2047] = 0;
    MessageBoxW(NULL, text, TITLE, MB_OK | MB_ICONERROR);
}

/* root\name, refusing anything that would not fit. */
static int join(wchar_t* out, const wchar_t* root, const wchar_t* name) {
    int n = _snwprintf(out, BIG - 1, L"%ls\\%ls", root, name);
    out[BIG - 1] = 0;
    return n > 0 && n < BIG - 1;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR args, int show) {
    static wchar_t root[BIG], data[BIG], game[BIG], cards[BIG], mods[BIG], command[BIG];
    (void)inst; (void)prev; (void)show;

    if (args && wcsncmp(args, L"--after-update ", 15) == 0) {
        wchar_t* end = NULL;
        const DWORD old_id = (DWORD)wcstoul(args + 15, &end, 10);
        HANDLE old = old_id ? OpenProcess(SYNCHRONIZE, FALSE, old_id) : NULL;
        if (old) {
            WaitForSingleObject(old, 30000);
            CloseHandle(old);
        }
        args = end ? end : args + 15;
        while (*args == L' ') ++args;
    }

    DWORD n = GetModuleFileNameW(NULL, root, BIG - 1);
    wchar_t* slash = (n && n < BIG - 1) ? wcsrchr(root, L'\\') : NULL;
    if (!slash) { say(L"Could not tell which folder this program is in.", NULL); return 1; }
    *slash = 0;

    if (!join(data, root, L"RecompData") || !join(game, data, ACG_GAME_EXE) ||
        !join(cards, root, L"Memory Card") || !join(mods, root, L"Mods")) {
        say(L"The game folder's path is too long. Move the folder somewhere shorter.", root);
        return 1;
    }
    if (GetFileAttributesW(game) == INVALID_FILE_ATTRIBUTES) {
        say(L"The game files are missing. Keep the RecompData folder next to this program.", game);
        return 1;
    }
    /* Made here too, so a copy of the game without them still has somewhere
     * to put a memory card or a mod. */
    CreateDirectoryW(cards, NULL);
    CreateDirectoryW(mods, NULL);

    const int has_args = args && args[0];
    const int own_cards = has_args && wcsstr(args, L"--memcard-dir") != NULL;
    int len;
    if (own_cards)
        len = _snwprintf(command, BIG - 1, L"\"%ls\" %ls", game, args);
    else
        len = _snwprintf(command, BIG - 1, L"\"%ls\" --memcard-dir \"%ls\"%ls%ls", game, cards,
                         has_args ? L" " : L"", has_args ? args : L"");
    command[BIG - 1] = 0;
    if (len <= 0 || len >= BIG - 1) { say(L"The command line is too long.", NULL); return 1; }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof si);
    ZeroMemory(&pi, sizeof pi);
    si.cb = sizeof si;
    if (!CreateProcessW(game, command, NULL, NULL, FALSE, 0, NULL, data, &si, &pi)) {
        say(L"The game could not be started.", game);
        return 1;
    }
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    return (int)code;
}

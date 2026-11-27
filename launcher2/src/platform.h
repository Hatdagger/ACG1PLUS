/* What the launcher needs from Windows and Linux: child programs (the build's
 * Python, curl), drives, free space, and a few file helpers. UTF-8 everywhere;
 * on Windows paths are turned into wide strings at the edge. */
#pragma once

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace plat {

namespace fs = std::filesystem;

fs::path u8path(const std::string& s);
std::string u8str(const fs::path& p);

/* fopen with a UTF-8 path. */
FILE* open_file(const fs::path& p, const char* mode);

/* A child program with its output read line by line on two background threads
 * (stdout = stream 1, stderr = stream 2). Everything it starts belongs to it:
 * kill_tree() stops the whole family (a process group on Linux, a job object on
 * Windows), and closing the launcher stops it too on Windows. */
class Process {
public:
    using LineFn = std::function<void(int stream, const std::string& line)>;
    Process();
    ~Process();
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    /* env: variables to set (or override) on top of the launcher's own. */
    bool start(const std::vector<std::string>& args,
               const std::vector<std::pair<std::string, std::string>>& env,
               const fs::path& cwd, LineFn on_line, std::string* error);
    /* True and *code set once it has ended and its output is read. */
    bool poll(int* code);
    int wait();
    void kill_tree();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct Drive {
    fs::path    root;
    std::string label;   /* "E:  USB DRIVE", "/run/media/deck/SD" ... */
    bool        removable = false;
};
/* Places to look for a disc and to start the file picker from. */
std::vector<Drive> drives();

uint64_t free_space(const fs::path& p);
fs::path home_dir();

/* Linux: can a program in this folder be run (noexec mounts, file systems
 * without a program flag)? Always true on Windows. */
bool can_run_programs(const fs::path& dir, std::string* why);
void make_executable(const fs::path& p);

/* Windows: the user PATH in the registry (the framework's toolchain installer
 * can write it; the launcher puts it back). No-ops elsewhere. */
bool user_path_get(std::wstring* out);
void user_path_set(const std::wstring& value);

/* The curl program: Windows 10/11 ship one in System32, SteamOS and other
 * Linux systems have it on PATH. Empty if none was found. */
std::string curl_program();

/* 0.2.4 self-update. The launcher's own program file (resolved at start). */
fs::path self_exe();
/* Rename FROM to TO, replacing TO; tried again for up to 3 seconds (Windows: an
 * antivirus can hold a file it is scanning for a moment). Works on the running
 * program too: Windows lets a running .exe be renamed, Linux any file. */
bool rename_retry(const fs::path& from, const fs::path& to, std::string* error);
/* Start EXE with ARGS in place of this launcher (after a self-update). Linux:
 * replaces this process (same process id, so Steam's Game Mode keeps tracking
 * it); returns only if that failed. Windows: starts it on its own (not tied to
 * this launcher), then the caller exits. False if it couldn't be started. */
bool restart_self(const fs::path& exe, const std::vector<std::string>& args);

} /* namespace plat */

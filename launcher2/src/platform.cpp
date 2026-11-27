/* Windows and Linux halves of platform.h. */
#include "platform.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#else
#include <csignal>
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace plat {

#if defined(_WIN32)
static std::wstring widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}
static std::string narrow(const std::wstring& w) {
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
fs::path u8path(const std::string& s) { return fs::path(widen(s)); }
std::string u8str(const fs::path& p) { return narrow(p.wstring()); }
FILE* open_file(const fs::path& p, const char* mode) { return _wfopen(p.wstring().c_str(), widen(mode).c_str()); }
#else
fs::path u8path(const std::string& s) { return fs::path(s); }
std::string u8str(const fs::path& p) { return p.string(); }
FILE* open_file(const fs::path& p, const char* mode) { return std::fopen(p.c_str(), mode); }
#endif

/* ---- reading a pipe into lines ------------------------------------------------ */

namespace {
struct LineSplitter {
    std::string pending;
    void feed(const char* data, size_t n, int stream, const Process::LineFn& fn) {
        for (size_t i = 0; i < n; ++i) {
            const char c = data[i];
            /* curl and some tools redraw a line with \r: treat it as an end of line. */
            if (c == '\n' || c == '\r') {
                if (!pending.empty() && fn) fn(stream, pending);
                pending.clear();
            } else {
                pending += c;
            }
        }
    }
    void flush(int stream, const Process::LineFn& fn) {
        if (!pending.empty() && fn) fn(stream, pending);
        pending.clear();
    }
};
} /* namespace */

/* ---- Process ------------------------------------------------------------------- */

#if defined(_WIN32)

struct Process::Impl {
    HANDLE proc = nullptr, job = nullptr;
    HANDLE out_r = nullptr, err_r = nullptr;
    std::thread t_out, t_err;
    std::mutex line_mutex;
    LineFn on_line;
    bool ended = false;
    int code = -1;

    void reader(HANDLE h, int stream) {
        LineSplitter sp;
        char buf[4096];
        DWORD got = 0;
        while (ReadFile(h, buf, sizeof buf, &got, nullptr) && got > 0) {
            std::lock_guard<std::mutex> lk(line_mutex);
            sp.feed(buf, got, stream, on_line);
        }
        std::lock_guard<std::mutex> lk(line_mutex);
        sp.flush(stream, on_line);
    }
    void join() {
        if (t_out.joinable()) t_out.join();
        if (t_err.joinable()) t_err.join();
        if (out_r) { CloseHandle(out_r); out_r = nullptr; }
        if (err_r) { CloseHandle(err_r); err_r = nullptr; }
    }
};

/* Quoting for CreateProcess (the rules the C runtime uses to split it again). */
static std::wstring quote_arg(const std::wstring& a) {
    if (!a.empty() && a.find_first_of(L" \t\"") == std::wstring::npos) return a;
    std::wstring q = L"\"";
    size_t slashes = 0;
    for (wchar_t c : a) {
        if (c == L'\\') { ++slashes; continue; }
        if (c == L'"') { q.append(slashes * 2 + 1, L'\\'); q += L'"'; slashes = 0; continue; }
        q.append(slashes, L'\\');
        slashes = 0;
        q += c;
    }
    q.append(slashes * 2, L'\\');
    q += L'"';
    return q;
}

static bool ieq(const std::wstring& a, const std::wstring& b) {
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
}

bool Process::start(const std::vector<std::string>& args,
                    const std::vector<std::pair<std::string, std::string>>& env,
                    const fs::path& cwd, LineFn on_line, std::string* error) {
    impl_.reset(new Impl);
    impl_->on_line = std::move(on_line);
    SECURITY_ATTRIBUTES sa = { sizeof sa, nullptr, TRUE };
    HANDLE out_w = nullptr, err_w = nullptr;
    if (!CreatePipe(&impl_->out_r, &out_w, &sa, 0) || !CreatePipe(&impl_->err_r, &err_w, &sa, 0)) {
        if (error) *error = "could not make a pipe";
        return false;
    }
    SetHandleInformation(impl_->out_r, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(impl_->err_r, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

    /* The environment: the launcher's own, with the overrides on top
     * (Windows variable names ignore case). */
    std::vector<std::wstring> vars;
    if (wchar_t* block = GetEnvironmentStringsW()) {
        for (wchar_t* p = block; *p; p += wcslen(p) + 1) vars.emplace_back(p);
        FreeEnvironmentStringsW(block);
    }
    for (const auto& kv : env) {
        const std::wstring name = widen(kv.first);
        bool replaced = false;
        for (auto& v : vars) {
            const size_t eq = v.find(L'=', 1);
            if (eq != std::wstring::npos && ieq(v.substr(0, eq), name)) {
                v = name + L"=" + widen(kv.second);
                replaced = true;
            }
        }
        if (!replaced) vars.push_back(name + L"=" + widen(kv.second));
    }
    std::wstring block;
    for (const auto& v : vars) { block += v; block += L'\0'; }
    block += L'\0';

    std::wstring cmd;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) cmd += L' ';
        cmd += quote_arg(widen(args[i]));
    }
    STARTUPINFOW si = {};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul;
    si.hStdOutput = out_w;
    si.hStdError = err_w;
    PROCESS_INFORMATION pi = {};
    impl_->job = CreateJobObjectW(nullptr, nullptr);
    if (impl_->job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION li = {};
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(impl_->job, JobObjectExtendedLimitInformation, &li, sizeof li);
    }
    const std::wstring wcwd = cwd.empty() ? std::wstring() : cwd.wstring();
    const BOOL ok = CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED,
                                   (void*)block.c_str(), wcwd.empty() ? nullptr : wcwd.c_str(), &si, &pi);
    CloseHandle(out_w);
    CloseHandle(err_w);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) {
        if (error) *error = "could not start " + (args.empty() ? std::string("?") : args[0]) +
                            " (error " + std::to_string(GetLastError()) + ")";
        CloseHandle(impl_->out_r); impl_->out_r = nullptr;
        CloseHandle(impl_->err_r); impl_->err_r = nullptr;
        return false;
    }
    if (impl_->job) AssignProcessToJobObject(impl_->job, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    impl_->proc = pi.hProcess;
    Impl* im = impl_.get();
    im->t_out = std::thread([im] { im->reader(im->out_r, 1); });
    im->t_err = std::thread([im] { im->reader(im->err_r, 2); });
    return true;
}

bool Process::poll(int* code) {
    if (!impl_ || !impl_->proc) return true;
    if (!impl_->ended) {
        if (WaitForSingleObject(impl_->proc, 0) != WAIT_OBJECT_0) return false;
        DWORD c = 1;
        GetExitCodeProcess(impl_->proc, &c);
        impl_->code = (int)c;
        /* The pipes close when the last program holding them ends (grandchildren too). */
        if (impl_->job) TerminateJobObject(impl_->job, 1);
        impl_->join();
        impl_->ended = true;
    }
    if (code) *code = impl_->code;
    return true;
}

int Process::wait() {
    int c = -1;
    while (!poll(&c)) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return c;
}

void Process::kill_tree() {
    if (!impl_ || !impl_->proc || impl_->ended) return;
    if (impl_->job) TerminateJobObject(impl_->job, 1);
    else TerminateProcess(impl_->proc, 1);
}

Process::Process() = default;
Process::~Process() {
    if (!impl_) return;
    kill_tree();
    if (impl_->proc) { wait(); CloseHandle(impl_->proc); }
    if (impl_->job) CloseHandle(impl_->job);
}

#else /* Linux */

struct Process::Impl {
    pid_t pid = -1;
    int out_r = -1, err_r = -1;
    std::thread t_out, t_err;
    std::mutex line_mutex;
    LineFn on_line;
    bool ended = false;
    int code = -1;

    void reader(int fd, int stream) {
        LineSplitter sp;
        char buf[4096];
        for (;;) {
            const ssize_t got = read(fd, buf, sizeof buf);
            if (got < 0 && errno == EINTR) continue;
            if (got <= 0) break;
            std::lock_guard<std::mutex> lk(line_mutex);
            sp.feed(buf, (size_t)got, stream, on_line);
        }
        std::lock_guard<std::mutex> lk(line_mutex);
        sp.flush(stream, on_line);
    }
    void join() {
        if (t_out.joinable()) t_out.join();
        if (t_err.joinable()) t_err.join();
        if (out_r >= 0) { close(out_r); out_r = -1; }
        if (err_r >= 0) { close(err_r); err_r = -1; }
    }
};

bool Process::start(const std::vector<std::string>& args,
                    const std::vector<std::pair<std::string, std::string>>& env,
                    const fs::path& cwd, LineFn on_line, std::string* error) {
    impl_.reset(new Impl);
    impl_->on_line = std::move(on_line);
    if (args.empty()) { if (error) *error = "nothing to start"; return false; }
    int out_p[2], err_p[2];
    if (pipe2(out_p, O_CLOEXEC) != 0 || pipe2(err_p, O_CLOEXEC) != 0) {
        if (error) *error = "could not make a pipe";
        return false;
    }
    /* Build everything before fork (only async-signal-safe calls after it). */
    std::vector<std::string> envs;
    for (char** e = environ; e && *e; ++e) {
        const char* eq = std::strchr(*e, '=');
        const std::string name = eq ? std::string(*e, (size_t)(eq - *e)) : std::string(*e);
        bool over = false;
        for (const auto& kv : env) if (kv.first == name) { over = true; break; }
        if (!over) envs.emplace_back(*e);
    }
    for (const auto& kv : env) envs.push_back(kv.first + "=" + kv.second);
    std::vector<char*> envp, argv;
    for (auto& s : envs) envp.push_back(&s[0]);
    envp.push_back(nullptr);
    std::vector<std::string> args_copy = args;
    for (auto& s : args_copy) argv.push_back(&s[0]);
    argv.push_back(nullptr);
    const std::string dir = cwd.string();
    /* execvp with our environment: find the program on the new PATH ourselves. */
    std::string exe = args[0];
    if (exe.find('/') == std::string::npos) {
        std::string path;
        for (const auto& s : envs) if (s.rfind("PATH=", 0) == 0) path = s.substr(5);
        size_t a = 0;
        while (a <= path.size()) {
            size_t b = path.find(':', a);
            if (b == std::string::npos) b = path.size();
            const std::string cand = path.substr(a, b - a) + "/" + exe;
            if (b > a && access(cand.c_str(), X_OK) == 0) { exe = cand; break; }
            a = b + 1;
        }
    }

    const pid_t pid = fork();
    if (pid < 0) {
        if (error) *error = "could not start " + args[0];
        return false;
    }
    if (pid == 0) {
        setpgid(0, 0);
        const int nul = open("/dev/null", O_RDONLY);
        if (nul >= 0) dup2(nul, 0);
        dup2(out_p[1], 1);
        dup2(err_p[1], 2);
        if (!dir.empty() && chdir(dir.c_str()) != 0) _exit(126);
        execve(exe.c_str(), argv.data(), envp.data());
        const char msg[] = "launcher: could not run the program\n";
        (void)!write(2, msg, sizeof msg - 1);
        _exit(127);
    }
    setpgid(pid, pid);
    close(out_p[1]);
    close(err_p[1]);
    impl_->pid = pid;
    impl_->out_r = out_p[0];
    impl_->err_r = err_p[0];
    Impl* im = impl_.get();
    im->t_out = std::thread([im] { im->reader(im->out_r, 1); });
    im->t_err = std::thread([im] { im->reader(im->err_r, 2); });
    return true;
}

bool Process::poll(int* code) {
    if (!impl_ || impl_->pid < 0) return true;
    if (!impl_->ended) {
        int status = 0;
        const pid_t r = waitpid(impl_->pid, &status, WNOHANG);
        if (r == 0) return false;
        impl_->code = r < 0 ? -1 : WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        /* Anything it left running still holds the pipes: stop the family. */
        kill(-impl_->pid, SIGKILL);
        impl_->join();
        impl_->ended = true;
    }
    if (code) *code = impl_->code;
    return true;
}

int Process::wait() {
    int c = -1;
    while (!poll(&c)) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return c;
}

void Process::kill_tree() {
    if (!impl_ || impl_->pid < 0 || impl_->ended) return;
    kill(-impl_->pid, SIGTERM);
    for (int i = 0; i < 30; ++i) {   /* 3 seconds to stop politely */
        siginfo_t info = {};
        /* Has it ended? (looks without collecting it; poll() does that) */
        if (waitid(P_PID, (id_t)impl_->pid, &info, WEXITED | WNOHANG | WNOWAIT) == 0 && info.si_pid != 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    kill(-impl_->pid, SIGKILL);
}

Process::Process() = default;
Process::~Process() {
    if (!impl_ || impl_->pid < 0) return;
    kill_tree();
    wait();
}

#endif

/* ---- drives -------------------------------------------------------------------- */

std::vector<Drive> drives() {
    std::vector<Drive> out;
#if defined(_WIN32)
    const DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (!(mask & (1u << i))) continue;
        wchar_t root[4] = { (wchar_t)(L'A' + i), L':', L'\\', 0 };
        const UINT type = GetDriveTypeW(root);
        if (type == DRIVE_NO_ROOT_DIR || type == DRIVE_UNKNOWN) continue;
        wchar_t name[MAX_PATH + 1] = {};
        /* Don't wake an empty card reader or optical drive with a dialog. */
        const UINT old = SetErrorMode(SEM_FAILCRITICALERRORS);
        const bool ready = GetVolumeInformationW(root, name, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0);
        SetErrorMode(old);
        if (!ready) continue;
        Drive d;
        d.root = fs::path(root);
        d.removable = type == DRIVE_REMOVABLE;
        std::string label = narrow(std::wstring(root, 2));
        label += "  ";
        label += name[0] ? narrow(name) : (type == DRIVE_REMOVABLE ? "USB DRIVE" : type == DRIVE_CDROM ? "DISC DRIVE" : "DRIVE");
        d.label = label;
        out.push_back(d);
    }
#else
    const char* user = std::getenv("USER");
    std::vector<fs::path> bases;
    if (user && *user) bases.push_back(fs::path("/run/media") / user);
    bases.push_back("/run/media");
    bases.push_back("/media");
    bases.push_back("/mnt");
    std::error_code ec;
    for (const auto& b : bases) {
        if (!fs::is_directory(b, ec)) continue;
        for (const auto& e : fs::directory_iterator(b, fs::directory_options::skip_permission_denied, ec)) {
            if (!e.is_directory(ec)) continue;
            /* /run/media holds one folder per user; their drives are inside. */
            if (b == fs::path("/run/media") && user && e.path().filename() == user) continue;
            bool dup = false;
            for (const auto& d : out) if (d.root == e.path()) dup = true;
            if (dup) continue;
            Drive d;
            d.root = e.path();
            d.removable = true;
            d.label = e.path().string();
            out.push_back(d);
        }
    }
    Drive h;
    h.root = home_dir();
    h.label = "HOME  " + h.root.string();
    out.push_back(h);
    Drive r;
    r.root = "/";
    r.label = "/  WHOLE SYSTEM";
    out.push_back(r);
#endif
    return out;
}

uint64_t free_space(const fs::path& p) {
    std::error_code ec;
    const fs::space_info si = fs::space(p, ec);
    return ec ? 0 : (uint64_t)si.available;
}

fs::path home_dir() {
#if defined(_WIN32)
    if (const wchar_t* h = _wgetenv(L"USERPROFILE")) return fs::path(h);
    return fs::path(L"C:\\");
#else
    if (const char* h = std::getenv("HOME")) if (*h) return fs::path(h);
    if (const passwd* pw = getpwuid(getuid())) return fs::path(pw->pw_dir);
    return fs::path("/");
#endif
}

bool can_run_programs(const fs::path& dir, std::string* why) {
#if defined(_WIN32)
    (void)dir; (void)why;
    return true;
#else
    std::error_code ec;
    fs::create_directories(dir, ec);
    const fs::path probe = dir / "acg-probe.sh";
    {
        FILE* f = open_file(probe, "wb");
        if (!f) { if (why) *why = "could not write into " + dir.string(); return false; }
        std::fputs("#!/bin/sh\necho ok\n", f);
        std::fclose(f);
    }
    make_executable(probe);
    Process p;
    std::string got;
    std::string err;
    const bool started = p.start({ probe.string() }, {}, dir,
                                 [&](int s, const std::string& l) { if (s == 1) got += l; }, &err);
    const int code = started ? p.wait() : -1;
    fs::remove(probe, ec);
    if (started && code == 0 && got == "ok") return true;
    if (why) {
        struct statvfs sv = {};
        const bool noexec = statvfs(dir.c_str(), &sv) == 0 && (sv.f_flag & ST_NOEXEC);
        *why = noexec ? "the drive is mounted so that programs on it can't run (noexec)"
                      : "programs on this drive can't be run (its file system may not allow it)";
    }
    return false;
#endif
}

void make_executable(const fs::path& p) {
#if !defined(_WIN32)
    struct stat st = {};
    if (stat(p.c_str(), &st) == 0) chmod(p.c_str(), st.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);
#else
    (void)p;
#endif
}

bool user_path_get(std::wstring* out) {
#if defined(_WIN32)
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_READ, &k) != ERROR_SUCCESS) return false;
    DWORD type = 0, size = 0;
    LONG r = RegQueryValueExW(k, L"Path", nullptr, &type, nullptr, &size);
    if (r != ERROR_SUCCESS) { RegCloseKey(k); out->clear(); return true; }
    std::wstring v(size / sizeof(wchar_t) + 1, L'\0');
    r = RegQueryValueExW(k, L"Path", nullptr, &type, (BYTE*)&v[0], &size);
    RegCloseKey(k);
    if (r != ERROR_SUCCESS) return false;
    v.resize(wcslen(v.c_str()));
    *out = v;
    return true;
#else
    (void)out;
    return false;
#endif
}

void user_path_set(const std::wstring& value) {
#if defined(_WIN32)
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
    RegSetValueExW(k, L"Path", 0, REG_EXPAND_SZ, (const BYTE*)value.c_str(),
                   (DWORD)((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 2000, nullptr);
#else
    (void)value;
#endif
}

std::string curl_program() {
#if defined(_WIN32)
    wchar_t sys[MAX_PATH] = {};
    if (GetSystemDirectoryW(sys, MAX_PATH)) {
        const fs::path c = fs::path(sys) / L"curl.exe";
        std::error_code ec;
        if (fs::is_regular_file(c, ec)) return u8str(c);
    }
    return std::string();
#else
    const char* path = std::getenv("PATH");
    std::string p = path ? path : "/usr/bin:/bin";
    size_t a = 0;
    while (a <= p.size()) {
        size_t b = p.find(':', a);
        if (b == std::string::npos) b = p.size();
        const std::string cand = p.substr(a, b - a) + "/curl";
        if (b > a && access(cand.c_str(), X_OK) == 0) return cand;
        a = b + 1;
    }
    return std::string();
#endif
}

} /* namespace plat */

#include "disc.h"

#include "hash.h"
#include "platform.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <mutex>
#include <thread>

/* From game.toml [prepare_disc], baked in by CMake. */
#ifndef ACG_DISC_SIZES
#define ACG_DISC_SIZES "454072416"
#endif
#ifndef ACG_DISC_MD5
#define ACG_DISC_MD5 "91badb09339ee7e3b6d235609e8da1be"
#endif

namespace disc {

namespace {

std::mutex g_mu;
Snapshot g_snap;
std::thread g_thread;
std::atomic<bool> g_stop{false};
std::vector<std::string> g_extra_md5;

std::thread g_copy;
std::atomic<int> g_copy_state{1};
std::atomic<float> g_copy_frac{0.0f};
std::string g_copy_err;
fs::path g_copy_cue;

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::vector<uint64_t> known_sizes() {
    std::vector<uint64_t> v;
    const char* p = ACG_DISC_SIZES;
    while (*p) {
        char* end = nullptr;
        const unsigned long long x = std::strtoull(p, &end, 10);
        if (end == p) { ++p; continue; }
        v.push_back(x);
        p = end;
    }
    return v;
}

std::vector<std::string> known_md5() {
    std::vector<std::string> v;
    std::string cur;
    for (const char* p = ACG_DISC_MD5;; ++p) {
        if (*p == ',' || *p == ' ' || !*p) { if (!cur.empty()) v.push_back(lower(cur)); cur.clear(); if (!*p) break; }
        else cur += *p;
    }
    for (const auto& m : g_extra_md5) v.push_back(lower(m));
    return v;
}

std::string with_commas(uint64_t n) {
    std::string s = std::to_string((unsigned long long)n);
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ",");
    return s;
}

/* The data track's file: the first FILE line followed by a MODE track. */
bool parse_cue(const fs::path& cue, fs::path* bin, std::string* why) {
    FILE* f = plat::open_file(cue, "rb");
    if (!f) { *why = "can't be read"; return false; }
    std::string text;
    char b[4096];
    size_t n;
    while ((n = std::fread(b, 1, sizeof b, f)) > 0 && text.size() < 65536) text.append(b, n);
    std::fclose(f);
    std::string file;
    int files = 0;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t e = text.find('\n', pos);
        if (e == std::string::npos) e = text.size();
        std::string line = text.substr(pos, e - pos);
        pos = e + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        size_t a = line.find_first_not_of(" \t");
        if (a == std::string::npos) continue;
        line = line.substr(a);
        const std::string up = lower(line.substr(0, 5));
        if (up == "file ") {
            ++files;
            std::string rest = line.substr(5);
            std::string name;
            if (!rest.empty() && rest[0] == '"') {
                const size_t q = rest.find('"', 1);
                name = rest.substr(1, q == std::string::npos ? std::string::npos : q - 1);
            } else {
                const size_t sp = rest.rfind(' ');
                name = rest.substr(0, sp);
            }
            if (file.empty()) file = name;
        }
    }
    if (file.empty()) { *why = "is not a disc image .cue (no FILE line)"; return false; }
    (void)files;
    fs::path p = plat::u8path(file);
    if (p.is_relative()) p = cue.parent_path() / p;
    std::error_code ec;
    if (!fs::is_regular_file(p, ec)) {
        /* Some tools write the name with another case; look for it. */
        const std::string want = lower(plat::u8str(p.filename()));
        for (const auto& e2 : fs::directory_iterator(cue.parent_path(), ec))
            if (lower(plat::u8str(e2.path().filename())) == want) { p = e2.path(); break; }
    }
    if (!fs::is_regular_file(p, ec)) { *why = "its .bin file (" + file + ") is missing"; return false; }
    *bin = p;
    return true;
}

bool inside(const fs::path& p, const fs::path& root) {
    std::error_code ec;
    const fs::path a = fs::weakly_canonical(p, ec), r = fs::weakly_canonical(root, ec);
    const std::string as = plat::u8str(a), rs = plat::u8str(r);
    return !rs.empty() && as.compare(0, rs.size(), rs) == 0;
}

void set_looking(const std::string& s) { std::lock_guard<std::mutex> lk(g_mu); g_snap.looking_in = s; }

void add(const fs::path& cue, const fs::path& root) {
    std::error_code ec;
    const fs::path c = fs::weakly_canonical(cue, ec);
    std::lock_guard<std::mutex> lk(g_mu);
    for (const auto& x : g_snap.found) if (fs::weakly_canonical(x.cue, ec) == c) return;
    Candidate k;
    k.cue = c.empty() ? cue : c;
    k.inside = inside(k.cue, root);
    g_snap.found.push_back(k);
}

void find_in(const fs::path& dir, int depth, const fs::path& root, int* budget) {
    if (g_stop || *budget <= 0) return;
    --*budget;
    std::error_code ec;
    fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec);
    if (ec) return;
    std::vector<fs::path> subs;
    for (; it != fs::directory_iterator(); it.increment(ec)) {
        if (ec || g_stop) break;
        const fs::path p = it->path();
        const std::string name = plat::u8str(p.filename());
        if (name.empty() || name[0] == '.' || name[0] == '$') continue;
        if (it->is_directory(ec)) {
            const std::string l = lower(name);
            /* Not worth the time: system and build folders. */
            if (l == "windows" || l == "program files" || l == "program files (x86)" || l == "programdata" ||
                l == "recompdata" || l == "proc" || l == "sys" || l == "dev" || l == "usr" || l == "node_modules" ||
                l == "appdata" || l == "system volume information")
                continue;
            subs.push_back(p);
        } else if (lower(plat::u8str(p.extension())) == ".cue") {
            add(p, root);
        }
    }
    if (depth > 0)
        for (const auto& s : subs) find_in(s, depth - 1, root, budget);
}

/* Size first (instant), then the MD5 of those of the right size. */
void check_all() {
    const auto sizes = known_sizes();
    const auto md5s = known_md5();
    for (size_t i = 0;; ++i) {
        if (g_stop) return;
        Candidate c;
        {
            std::lock_guard<std::mutex> lk(g_mu);
            if (i >= g_snap.found.size()) return;
            if (g_snap.found[i].check != Check::Waiting) continue;
            g_snap.found[i].check = Check::Checking;
            c = g_snap.found[i];
        }
        std::string why;
        fs::path bin;
        Check result = Check::Bad;
        uint64_t size = 0;
        if (parse_cue(c.cue, &bin, &why)) {
            std::error_code ec;
            size = (uint64_t)fs::file_size(bin, ec);
            if (std::find(sizes.begin(), sizes.end(), size) == sizes.end()) {
                why = "is " + with_commas(size) + " bytes; Project Phantasma USA is " +
                      with_commas(sizes.empty() ? 0 : sizes[0]) + ". Another game, another region, or "
                      "another kind of image (one .bin, Redump style, is needed)";
            } else {
                FILE* f = plat::open_file(bin, "rb");
                if (!f) why = "can't be read";
                else {
                    hash::Md5 h;
                    std::vector<char> buf(1 << 20);
                    size_t n;
                    uint64_t done = 0;
                    while ((n = std::fread(buf.data(), 1, buf.size(), f)) > 0) {
                        if (g_stop) { std::fclose(f); return; }
                        h.update(buf.data(), n);
                        done += n;
                        std::lock_guard<std::mutex> lk(g_mu);
                        g_snap.found[i].progress = (float)((double)done / (double)size);
                    }
                    std::fclose(f);
                    const std::string got = h.hex();
                    if (std::find(md5s.begin(), md5s.end(), got) != md5s.end()) result = Check::Good;
                    else why = "has the right size but different contents (a different or changed dump)";
                }
            }
        }
        std::lock_guard<std::mutex> lk(g_mu);
        Candidate& k = g_snap.found[i];
        k.check = result;
        k.why = why;
        k.bin = bin;
        k.size = size;
        k.progress = 1.0f;
    }
}

void join() {
    g_stop = true;
    if (g_thread.joinable()) g_thread.join();
    g_stop = false;
}

} /* namespace */

void accept_md5(const std::string& md5) { g_extra_md5.push_back(md5); }

void scan(const fs::path& root, const fs::path& last_choice) {
    join();
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_snap = Snapshot();
        g_snap.scanning = true;
    }
    g_thread = std::thread([root, last_choice] {
        std::error_code ec;
        if (!last_choice.empty() && fs::is_regular_file(last_choice, ec)) add(last_choice, root);
        int budget = 400;
        set_looking("Disc folder");
        find_in(root / "Disc", 1, root, &budget);
        set_looking("the game folder");
        find_in(root, 0, root, &budget);
        check_all();   /* what is in the folder first: usually that's it */
        bool good = false;
        {
            std::lock_guard<std::mutex> lk(g_mu);
            for (const auto& c : g_snap.found) if (c.check == Check::Good) good = true;
        }
        if (!good) {
            const fs::path home = plat::home_dir();
            for (const char* sub : { "Downloads", "Desktop", "Documents", "Games" }) {
                if (g_stop) break;
                set_looking(sub);
                int b1 = 120;
                find_in(home / sub, 1, root, &b1);
            }
            for (const auto& d : plat::drives()) {
                if (g_stop) break;
                if (d.root == fs::path("/")) continue;
                set_looking(d.label);
                int b2 = 300;
                find_in(d.root, 2, root, &b2);
            }
            check_all();
        }
        std::lock_guard<std::mutex> lk(g_mu);
        g_snap.scanning = false;
        g_snap.looking_in.clear();
    });
}

void check_one(const fs::path& cue, const fs::path& root) {
    join();
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_snap.scanning = true;
        /* The picked one goes first, checked again. */
        std::error_code ec;
        const fs::path c = fs::weakly_canonical(cue, ec);
        g_snap.found.erase(std::remove_if(g_snap.found.begin(), g_snap.found.end(),
                                          [&](const Candidate& x) { return x.cue == c; }), g_snap.found.end());
        Candidate k;
        k.cue = c.empty() ? cue : c;
        k.inside = inside(k.cue, root);
        g_snap.found.insert(g_snap.found.begin(), k);
    }
    g_thread = std::thread([] {
        check_all();
        std::lock_guard<std::mutex> lk(g_mu);
        g_snap.scanning = false;
    });
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lk(g_mu);
    return g_snap;
}

void stop() {
    join();
    if (g_copy.joinable()) g_copy.join();
}

void copy_start(const Candidate& c, const fs::path& dest) {
    if (g_copy.joinable()) g_copy.join();
    g_copy_state = -1;
    g_copy_frac = 0.0f;
    g_copy_err.clear();
    g_copy = std::thread([c, dest] {
        std::error_code ec;
        fs::create_directories(dest, ec);
        const fs::path to_cue = dest / c.cue.filename(), to_bin = dest / c.bin.filename();
        const uint64_t total = c.size ? c.size : 1;
        bool ok = true;
        if (!(fs::exists(to_bin, ec) && fs::file_size(to_bin, ec) == c.size)) {
            FILE* in = plat::open_file(c.bin, "rb");
            const fs::path part = to_bin.string() + ".part";
            FILE* out = in ? plat::open_file(part, "wb") : nullptr;
            if (!in || !out) { ok = false; g_copy_err = "Couldn't open the files to copy."; }
            std::vector<char> buf(4 << 20);
            uint64_t done = 0;
            size_t n;
            while (ok && (n = std::fread(buf.data(), 1, buf.size(), in)) > 0) {
                if (std::fwrite(buf.data(), 1, n, out) != n) { ok = false; g_copy_err = "Couldn't write the copy (is the drive full?)."; }
                done += n;
                g_copy_frac = (float)((double)done / (double)total);
            }
            if (in) std::fclose(in);
            if (out && std::fclose(out) != 0) { ok = false; g_copy_err = "Couldn't finish the copy (is the drive full?)."; }
            if (ok) fs::rename(part, to_bin, ec);
            else fs::remove(part, ec);
            if (ec) { ok = false; g_copy_err = "Couldn't finish the copy: " + ec.message(); }
        }
        if (ok) {
            fs::copy_file(c.cue, to_cue, fs::copy_options::overwrite_existing, ec);
            if (ec) { ok = false; g_copy_err = "Couldn't copy the .cue: " + ec.message(); }
        }
        g_copy_cue = to_cue;
        g_copy_frac = 1.0f;
        g_copy_state = ok ? 1 : 0;
    });
}

int copy_status(float* frac, std::string* error, fs::path* new_cue) {
    const int s = g_copy_state;
    if (frac) *frac = g_copy_frac;
    if (s == 0 && error) *error = g_copy_err;
    if (s == 1 && new_cue) *new_cue = g_copy_cue;
    return s;
}

std::string display_name(const fs::path& cue) {
    return plat::u8str(cue.filename());
}

} /* namespace disc */

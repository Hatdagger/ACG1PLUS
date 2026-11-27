#include "firstrun.h"

#include "disc.h"
#include "ini.h"
#include "platform.h"
#include "setup.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace firstrun {

namespace fs = std::filesystem;
using namespace ui;

namespace {

enum Page { P_NONE, P_WELCOME, P_FIND, P_COPY_ASK, P_COPYING, P_PICKER, P_PREP, P_STOPPED, P_FAILED, P_DONE };

struct State {
    Page   page = P_NONE;
    int    cursor = 0;
    bool   show_log = false;
    bool   ask_cancel = false;
    int    ask_cursor = 0;
    /* disc */
    disc::Candidate chosen;
    fs::path        disc_cue;          /* what the build uses */
    /* picker */
    fs::path        dir;               /* empty = the list of drives */
    int             pick_cursor = 0, pick_top = 0;
    /* tests */
    bool   test_auto = false, test_exit = false;
    double page_since = 0.0;
    std::string copy_error;
    /* 0.2.3: only finding the disc again (the game is built, the disc moved):
     * once one is chosen it is remembered and START GAME carries on. */
    bool   find_only = false;
    bool   disc_ready = false;
    bool   start_now = false;    /* START GAME chosen on the DONE page */
};
State S;

void go(Page p) {
    S.page = p;
    S.cursor = 0;
    S.page_since = ImGui::GetTime();
    S.ask_cancel = false;
    clear_note();
}

const setup::Paths& P() { return setup::paths(); }
fs::path ini_file() { return P().launcher / "launcher.ini"; }

/* The disc chosen last time (launcher.ini disc=, relative to the folder when inside it). */
fs::path last_disc() {
    const auto kv = ini::load(ini_file());
    auto it = kv.find("disc");
    if (it == kv.end() || it->second.empty()) return fs::path();
    fs::path p = plat::u8path(it->second);
    if (p.is_relative()) p = P().root / p;
    return p;
}
void remember_disc(const fs::path& cue) {
    std::error_code ec;
    const fs::path rel = fs::relative(cue, P().root, ec);
    std::string s = !ec && !rel.empty() && plat::u8str(rel).rfind("..", 0) != 0 ? plat::u8str(rel) : plat::u8str(cue);
    std::replace(s.begin(), s.end(), '\\', '/');
    ini::set(ini_file(), "disc", s);
}

std::string clock_text(double secs) {
    const int t = (int)secs;
    char b[32];
    if (t >= 3600) std::snprintf(b, sizeof b, "%d:%02d:%02d", t / 3600, (t / 60) % 60, t % 60);
    else std::snprintf(b, sizeof b, "%d:%02d", t / 60, t % 60);
    return b;
}

std::string left_text(double eta) {
    if (eta < 0) return "WORKING OUT THE TIME LEFT";
    const int m = (int)std::ceil(eta / 60.0);
    char b[64];
    if (m <= 1) return "LESS THAN A MINUTE LEFT";
    if (m < 60) std::snprintf(b, sizeof b, "ABOUT %d MIN LEFT", m);
    else std::snprintf(b, sizeof b, "ABOUT %d H %02d MIN LEFT", m / 60, m % 60);
    return b;
}

std::string gb_text(uint64_t b) {
    char s[32];
    std::snprintf(s, sizeof s, "%.1f GB", (double)b / (1024.0 * 1024.0 * 1024.0));
    return s;
}

void start_build(const fs::path& cue) {
    S.disc_cue = cue;
    remember_disc(cue);
    if (S.find_only) {
        S.find_only = false;
        S.disc_ready = true;
        disc::stop();
        go(P_NONE);
        return;
    }
    setup::start(cue);
    S.show_log = false;
    go(P_PREP);
}

/* Pick a candidate: in the folder → build; outside → ask about copying. */
void use_candidate(const disc::Candidate& c) {
    S.chosen = c;
    if (c.inside) start_build(c.cue);
    else go(P_COPY_ASK);
}

/* ---- pages ------------------------------------------------------------------ */

int menu_rows(const Gfx& g, float x, float y, float w, const char* const* labels, int count, int* cursor,
              const Input& in, const char* const* values = nullptr, float step = kRowH) {
    if (in.up) *cursor = (*cursor + count - 1) % count;
    if (in.down) *cursor = (*cursor + 1) % count;
    int activate = -1;
    for (int i = 0; i < count; ++i) {
        bool hov = false;
        if (row(g, x, y + step * i, w, i == *cursor, true, labels[i], values ? values[i] : nullptr, kDim, &hov))
            activate = i;
        if (hov && mouse_moved()) *cursor = i;
    }
    if (in.confirm) activate = *cursor;
    return activate;
}

void draw_welcome(const Gfx& g, const Input& in, Requests&) {
    header(g, "PREPARING GAME", "FIRST RUN");
    panel(g, 24.0f, 76.0f, 592.0f, 132.0f, "WHAT HAPPENS NOW");
#if defined(_WIN32)
    const char* tools = "about 210 MB";
#else
    const char* tools = "about 830 MB";
#endif
    char buf[900];
    std::snprintf(buf, sizeof buf,
                  "The game is built on this PC from your own Project Phantasma disc (USA, SLUS-00670). Nothing "
                  "of the game is downloaded: the launcher downloads its tools (%s) and this project's source "
                  "(about 70 MB), then translates and compiles the game from your disc.\n\n"
                  "The first time can take an hour or more. You can leave it running; if it is stopped, it "
                  "carries on where it left off. Everything stays in this folder, so it can be copied or moved "
                  "as one (a USB drive works too).", tools);
    text(g, 38.0f, 94.0f, 11.0f, kText, buf, 564.0f);

    panel(g, 24.0f, 226.0f, 300.0f, 76.0f, "SELECT");
    static const char* rows[] = { "FIND MY DISC", "BACK TO THE MENU" };
    const int a = menu_rows(g, 25.0f, 238.0f, 298.0f, rows, 2, &S.cursor, in);

    panel(g, 340.0f, 226.0f, 276.0f, 76.0f, "GAME FOLDER");
    const std::string root = fit(g, 10.0f, plat::u8str(P().root), 250.0f);
    text(g, 354.0f, 242.0f, 10.0f, kText, root.c_str());
    const std::string fr = "FREE SPACE  " + gb_text(plat::free_space(P().root));
    text(g, 354.0f, 262.0f, 10.0f, kDim, fr.c_str());
    if (!setup::source_known() && setup::options().source_zip.empty())
        text(g, 354.0f, 280.0f, 10.0f, kWarn, "DEVELOPER BUILD: NEEDS --source-zip");

    footer(g, "A / ENTER  SELECT      D-PAD  MOVE      B / ESC  BACK");
    if (S.test_auto && ImGui::GetTime() - S.page_since > 1.0) { disc::scan(P().root, last_disc()); go(P_FIND); return; }
    if (in.back || a == 1) { go(P_NONE); return; }
    if (a == 0) { disc::scan(P().root, last_disc()); go(P_FIND); }
}

void draw_find(const Gfx& g, const Input& in, Requests&) {
    const disc::Snapshot ds = disc::snapshot();
    header(g, "YOUR DISC", "PROJECT PHANTASMA  USA  SLUS-00670");
    panel(g, 24.0f, 76.0f, 592.0f, 186.0f, "FOUND");
    const int shown = std::min((int)ds.found.size(), 5);
    std::vector<std::string> labels, values;
    for (int i = 0; i < shown; ++i) {
        const auto& c = ds.found[i];
        labels.push_back(fit(g, 13.0f, disc::display_name(c.cue), 380.0f));
        char v[64];
        switch (c.check) {
            case disc::Check::Waiting:  std::snprintf(v, sizeof v, "WAITING"); break;
            case disc::Check::Checking: std::snprintf(v, sizeof v, "CHECKING %d%%", (int)(c.progress * 100)); break;
            case disc::Check::Good:     std::snprintf(v, sizeof v, c.inside ? "OK" : "OK  (OUTSIDE THE FOLDER)"); break;
            case disc::Check::Bad:      std::snprintf(v, sizeof v, "NOT THIS DISC"); break;
        }
        values.push_back(v);
    }
    std::vector<std::string> extra = { "BROWSE FOR THE DISC", "LOOK AGAIN", "BACK" };
    const int count = shown + 3;
    if (S.cursor >= count) S.cursor = count - 1;
    if (in.up) S.cursor = (S.cursor + count - 1) % count;
    if (in.down) S.cursor = (S.cursor + 1) % count;
    int activate = -1;
    for (int i = 0; i < shown; ++i) {
        bool hov = false;
        const auto& c = ds.found[i];
        const ImU32 vc = c.check == disc::Check::Good ? kOn : c.check == disc::Check::Bad ? kWarn : kHi;
        if (row(g, 25.0f, 88.0f + kRowH * i, 590.0f, S.cursor == i, c.check == disc::Check::Good,
                labels[i].c_str(), values[i].c_str(), vc, &hov))
            activate = i;
        if (hov && mouse_moved()) S.cursor = i;
    }
    if (shown == 0) {
        text(g, 54.0f, 96.0f, 11.0f, kDim, ds.scanning ? "LOOKING..." : "NO .CUE FILE FOUND.");
    }
    if (ds.scanning) {
        const std::string l = "LOOKING IN " + upper(ds.looking_in);
        text_right(g, 604.0f, 240.0f, 10.0f, kDim, fit(g, 10.0f, l, 560.0f).c_str());
    }
    panel(g, 24.0f, 278.0f, 260.0f, 90.0f, "SELECT");
    for (int i = 0; i < 3; ++i) {
        bool hov = false;
        if (row(g, 25.0f, 288.0f + kRowH * i, 258.0f, S.cursor == shown + i, true, extra[i].c_str(), nullptr, kDim, &hov))
            activate = shown + i;
        if (hov && mouse_moved()) S.cursor = shown + i;
    }
    panel(g, 300.0f, 278.0f, 316.0f, 90.0f, "INFO");
    if (S.cursor < shown) {
        const auto& c = ds.found[S.cursor];
        text(g, 314.0f, 290.0f, 10.0f, kDim, fit(g, 10.0f, plat::u8str(c.cue.parent_path()), 290.0f).c_str());
        const char* t = c.check == disc::Check::Good ? (c.inside ? "This is your disc. A to use it."
                                                                 : "This is your disc. It is outside the game folder: A to use it (you can copy it in).")
                      : c.check == disc::Check::Bad ? nullptr : "Checking it is the right disc...";
        std::string why = c.check == disc::Check::Bad ? "This file " + c.why + "." : std::string(t ? t : "");
        text(g, 314.0f, 306.0f, 10.0f, c.check == disc::Check::Bad ? kWarn : kText, why.c_str(), 290.0f);
    } else {
        const std::string where = "Put your .cue + .bin in " + plat::u8str(P().root / "Disc") +
                                  ", or browse for them on another drive.";
        text(g, 314.0f, 290.0f, 10.0f, kText, where.c_str(), 290.0f);
    }
    footer(g, "A / ENTER  SELECT      D-PAD  MOVE      B / ESC  BACK");

    if (S.test_auto && !ds.scanning) {
        for (const auto& c : ds.found) if (c.check == disc::Check::Good) { use_candidate(c); return; }
    }
    if (in.confirm) activate = S.cursor;
    const Page back_to = S.find_only ? P_NONE : P_WELCOME;
    if (in.back) { disc::stop(); S.find_only = false; go(back_to); return; }
    if (activate < 0) return;
    S.cursor = activate;
    if (activate < shown) {
        if (ds.found[activate].check == disc::Check::Good) use_candidate(ds.found[activate]);
        else note(ds.found[activate].check == disc::Check::Bad ? "THAT ISN'T THE RIGHT DISC" : "STILL CHECKING");
        return;
    }
    switch (activate - shown) {
        case 0: S.dir.clear(); S.pick_cursor = 0; S.pick_top = 0; go(P_PICKER); break;
        case 1: disc::scan(P().root, last_disc()); break;
        default: disc::stop(); S.find_only = false; go(back_to); break;
    }
}

void draw_copy_ask(const Gfx& g, const Input& in, Requests&) {
    header(g, "YOUR DISC", "OUTSIDE THE GAME FOLDER");
    panel(g, 24.0f, 76.0f, 592.0f, 150.0f, "COPY IT IN?");
    const std::string where = fit(g, 11.0f, plat::u8str(S.chosen.cue), 560.0f);
    text(g, 38.0f, 92.0f, 11.0f, kHi, where.c_str());
    char buf[600];
    std::snprintf(buf, sizeof buf,
                  "Copy the disc into the game's Disc folder? Then the folder holds everything it needs and "
                  "can be moved or copied as one. It uses %d MB (free here: %s).\n\n"
                  "Using it where it is works too, but that drive has to be there whenever the game is "
                  "rebuilt (updates).",
                  (int)(S.chosen.size >> 20), gb_text(plat::free_space(P().root)).c_str());
    text(g, 38.0f, 116.0f, 11.0f, kText, buf, 564.0f);
    panel(g, 24.0f, 244.0f, 592.0f, 92.0f, "SELECT");
    static const char* rows[] = { "COPY INTO THE DISC FOLDER  (RECOMMENDED)", "USE IT WHERE IT IS", "BACK" };
    int a = menu_rows(g, 25.0f, 256.0f, 590.0f, rows, 3, &S.cursor, in);
    footer(g, "A / ENTER  SELECT      D-PAD  MOVE      B / ESC  BACK");
    if (S.test_auto && ImGui::GetTime() - S.page_since > 1.0) a = 0;
    if (in.back || a == 2) { go(P_FIND); return; }
    if (a == 0) { S.copy_error.clear(); disc::copy_start(S.chosen, P().root / "Disc"); go(P_COPYING); }
    if (a == 1) start_build(S.chosen.cue);
}

void draw_copying(const Gfx& g, const Input& in, Requests&) {
    (void)in;
    header(g, "YOUR DISC", "COPYING");
    panel(g, 24.0f, 76.0f, 592.0f, 110.0f, "COPYING INTO THE DISC FOLDER");
    float f = 0.0f;
    std::string err;
    fs::path cue;
    const int st = disc::copy_status(&f, &err, &cue);
    text(g, 38.0f, 94.0f, 11.0f, kText, fit(g, 11.0f, plat::u8str(S.chosen.bin.filename()), 560.0f).c_str());
    bar(g, 38.0f, 120.0f, 564.0f, 12.0f, f, kHi);
    char b[32];
    std::snprintf(b, sizeof b, "%d%%", (int)(f * 100));
    text_right(g, 602.0f, 142.0f, 10.0f, kDim, b);
    if (st == 0) {
        text(g, 38.0f, 160.0f, 10.0f, kWarn, (err + "  B: BACK").c_str());
        if (in.back) go(P_COPY_ASK);
    }
    footer(g, "");
    if (st == 1) start_build(cue);
}

/* ---- file picker ----------------------------------------------------------------- */

struct Entry { std::string label; fs::path path; int kind; /* 0 up, 1 folder, 2 cue, 3 drive */ };

std::vector<Entry> list_dir() {
    std::vector<Entry> out;
    if (S.dir.empty()) {
        out.push_back({ "THE GAME FOLDER", P().root, 3 });
        for (const auto& d : plat::drives()) out.push_back({ d.label, d.root, 3 });
        return out;
    }
    out.push_back({ "..  UP", S.dir.parent_path() == S.dir ? fs::path() : S.dir.parent_path(), 0 });
    std::vector<Entry> dirs, cues;
    std::error_code ec;
    for (fs::directory_iterator it(S.dir, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::directory_iterator(); it.increment(ec)) {
        const std::string n = plat::u8str(it->path().filename());
        if (n.empty() || n[0] == '.' || n[0] == '$') continue;
        if (it->is_directory(ec)) dirs.push_back({ n + "/", it->path(), 1 });
        else {
            std::string ext = plat::u8str(it->path().extension());
            for (char& c : ext) c = (char)std::tolower((unsigned char)c);
            if (ext == ".cue") cues.push_back({ n, it->path(), 2 });
        }
        if (dirs.size() + cues.size() > 2000) break;
    }
    auto by_name = [](const Entry& a, const Entry& b) { return upper(a.label) < upper(b.label); };
    std::sort(dirs.begin(), dirs.end(), by_name);
    std::sort(cues.begin(), cues.end(), by_name);
    /* .cue files first: that's what is being looked for. */
    out.insert(out.end(), cues.begin(), cues.end());
    out.insert(out.end(), dirs.begin(), dirs.end());
    return out;
}

void draw_picker(const Gfx& g, const Input& in, Requests&) {
    static fs::path listed_for = fs::path("<none>");
    static std::vector<Entry> entries;
    if (listed_for != S.dir || entries.empty()) {
        entries = list_dir();
        listed_for = S.dir;
        /* A folder with a disc image in it: start on the first one. */
        if (!S.dir.empty() && entries.size() > 1 && entries[1].kind == 2) S.pick_cursor = 1;
    }
    const std::string where = S.dir.empty() ? std::string("DRIVES") : plat::u8str(S.dir);
    header(g, "CHOOSE THE DISC", fit(g, 10.0f, upper(where), 360.0f).c_str());
    panel(g, 24.0f, 76.0f, 592.0f, 320.0f, "PICK THE .CUE FILE");
    const int count = (int)entries.size();
    const int visible = 11;
    int& cur = S.pick_cursor;
    if (in.up) cur = (cur + count - 1) % count;
    if (in.down) cur = (cur + 1) % count;
    if (in.left) cur = std::max(0, cur - visible);
    if (in.right) cur = std::min(count - 1, cur + visible);
    cur = std::clamp(cur, 0, std::max(0, count - 1));
    if (cur < S.pick_top) S.pick_top = cur;
    if (cur >= S.pick_top + visible) S.pick_top = cur - visible + 1;
    int activate = -1;
    for (int i = S.pick_top; i < std::min(count, S.pick_top + visible); ++i) {
        const Entry& e = entries[i];
        bool hov = false;
        const char* tag = e.kind == 2 ? "DISC IMAGE" : e.kind == 1 ? "FOLDER" : nullptr;
        if (row(g, 25.0f, 88.0f + kRowH * (i - S.pick_top), 590.0f, i == cur, true,
                fit(g, 13.0f, e.label, 440.0f).c_str(), tag, e.kind == 2 ? kOn : kDim, &hov))
            activate = i;
        if (hov && mouse_moved()) cur = i;
    }
    if (count > visible) {
        char b[48];
        std::snprintf(b, sizeof b, "%d-%d OF %d", S.pick_top + 1, std::min(count, S.pick_top + visible), count);
        text_right(g, 604.0f, 380.0f, 10.0f, kDim, b);
    }
    if (count <= 1 && !S.dir.empty()) text(g, 54.0f, 120.0f, 11.0f, kDim, "NO .CUE FILES OR FOLDERS HERE.");
    footer(g, "A  OPEN / CHOOSE      B  UP      LEFT / RIGHT  PAGE");
    if (in.confirm && count) activate = cur;
    if (in.back) {
        if (S.dir.empty()) { go(P_FIND); return; }
        const fs::path parent = S.dir.parent_path();
        S.dir = parent == S.dir || S.dir == P().root ? fs::path() : parent;
        S.pick_cursor = 0; S.pick_top = 0;
        return;
    }
    if (activate < 0) return;
    const Entry e = entries[activate];
    if (e.kind == 2) {
        disc::check_one(e.path, P().root);
        go(P_FIND);
        return;
    }
    S.dir = e.path;
    S.pick_cursor = 0; S.pick_top = 0;
}

/* ---- preparing ---------------------------------------------------------------------- */

void draw_log(const Gfx& g, const setup::Snapshot& s, float x, float y, float w, float h) {
    panel(g, x, y, w, h, "LOG");
    const int lines = (int)((h - 16.0f) / 12.5f);
    const int n = (int)s.log.size();
    for (int i = 0; i < lines && i < n; ++i) {
        const std::string& l = s.log[(size_t)(n - std::min(n, lines) + i)];
        text(g, x + 10.0f, y + 10.0f + 12.5f * i, 10.0f, l.rfind("==>", 0) == 0 ? kHi : kText,
             fit(g, 10.0f, l, w - 20.0f).c_str());
    }
}

float overall(const setup::Snapshot& s) {
    /* Each step weighted by its usual share of the time. */
    static const float w[setup::S_COUNT] = { 0.5f, 8, 4, 1, 1, 30, 20, 34, 1, 0.5f };
    float tot = 0, got = 0;
    for (int i = 0; i < setup::S_COUNT; ++i) {
        tot += w[i];
        if (s.done[i]) got += w[i];
        else if (i == s.stage && s.frac[i] > 0) got += w[i] * s.frac[i];
    }
    return tot > 0 ? got / tot : 0.0f;
}

void draw_steps(const Gfx& g, const setup::Snapshot& s, float x, float y, float w) {
    panel(g, x, y, w, 284.0f, "STEPS");
    for (int i = 0; i < setup::S_COUNT; ++i) {
        const float ry = y + 10.0f + 26.5f * i;
        const bool cur = i == s.stage && (s.status == setup::Status::Running || s.status == setup::Status::Cancelling);
        const bool stopped_here = i == s.stage && (s.status == setup::Status::Failed || s.status == setup::Status::Cancelled);
        ImU32 col = s.done[i] ? kText : cur ? kBright : stopped_here ? kWarn : kOff;
        if (cur)
            g.dl->AddRectFilledMultiColor(at(g, x + 1, ry), at(g, x + w - 1, ry + 24.0f), IM_COL32(240, 160, 32, 60),
                                          IM_COL32(240, 160, 32, 0), IM_COL32(240, 160, 32, 0), IM_COL32(240, 160, 32, 60));
        /* the mark: a tick when done, an arrow on the one running */
        const ImVec2 m = at(g, x + 16.0f, ry + 12.0f);
        if (s.done[i]) {
            g.dl->AddLine(ImVec2(m.x - 4 * g.s, m.y), ImVec2(m.x - 1 * g.s, m.y + 3 * g.s), kOn, 2.0f * g.s * 0.6f);
            g.dl->AddLine(ImVec2(m.x - 1 * g.s, m.y + 3 * g.s), ImVec2(m.x + 5 * g.s, m.y - 4 * g.s), kOn, 2.0f * g.s * 0.6f);
        } else if (cur) {
            g.dl->AddTriangleFilled(at(g, x + 12.0f, ry + 7.0f), at(g, x + 21.0f, ry + 12.0f), at(g, x + 12.0f, ry + 17.0f), kHi);
        } else if (stopped_here) {
            text(g, x + 12.0f, ry + 5.0f, 13.0f, kWarn, "!");
        }
        text(g, x + 30.0f, ry + 5.0f, 13.0f, col, upper(setup::stage_name(i)).c_str());
        std::string v;
        ImU32 vc = kDim;
        if (s.skipped[i]) v = "ALREADY DONE";
        else if (s.done[i]) v = "DONE";
        else if (cur && s.frac[i] > 0.0f) { char b[16]; std::snprintf(b, sizeof b, "%d%%", (int)(s.frac[i] * 100)); v = b; vc = kHi; }
        else if (stopped_here) { v = s.status == setup::Status::Failed ? "STOPPED" : "PAUSED"; vc = kWarn; }
        if (!v.empty()) text_right(g, x + w - 12.0f, ry + 7.0f, 10.0f, vc, v.c_str());
    }
}

void draw_prep(const Gfx& g, const Input& in, Requests& req) {
    setup::reap();
    const setup::Snapshot s = setup::snapshot();
    header(g, "PREPARING GAME", (clock_text(s.elapsed) + "  ELAPSED").c_str());
    if (S.show_log) draw_log(g, s, 24.0f, 76.0f, 592.0f, 284.0f);
    else draw_steps(g, s, 24.0f, 76.0f, 352.0f);
    if (!S.show_log) {
        panel(g, 392.0f, 76.0f, 224.0f, 284.0f, "NOW");
        const int st = std::clamp(s.stage, 0, setup::S_COUNT - 1);
        text(g, 406.0f, 92.0f, 11.0f, kHi, upper(setup::stage_name(st)).c_str(), 196.0f);
        if (!s.detail.empty()) text(g, 406.0f, 128.0f, 10.0f, kText, upper(s.detail).c_str(), 196.0f);
        if (s.frac[st] >= 0.0f) bar(g, 406.0f, 150.0f, 196.0f, 8.0f, s.frac[st], kHi);
        text(g, 406.0f, 182.0f, 10.0f, kDim, "ALL STEPS");
        bar(g, 406.0f, 198.0f, 196.0f, 8.0f, overall(s), kOn);
        text(g, 406.0f, 214.0f, 11.0f, kBright,
             s.status == setup::Status::Cancelling ? "STOPPING..." : left_text(s.eta).c_str());
        text(g, 406.0f, 256.0f, 10.0f, kDim,
             "You can leave this running. If it is stopped, it carries on from the step it was on.", 196.0f);
    }
    bar(g, 24.0f, 372.0f, 592.0f, 6.0f, overall(s), kOn);
    footer(g, S.show_log ? "B  STOP      Y  HIDE LOG" : "B  STOP      Y  SHOW LOG");

    if (s.status == setup::Status::Done) { go(P_DONE); return; }
    if (s.status == setup::Status::Failed) { go(P_FAILED); return; }
    if (s.status == setup::Status::Cancelled) { go(P_STOPPED); return; }

    if (S.ask_cancel) {
        panel(g, 150.0f, 160.0f, 340.0f, 120.0f, "STOP?");
        g.dl->AddRectFilled(at(g, 151.0f, 161.0f), at(g, 489.0f, 279.0f), IM_COL32(11, 18, 24, 250));
        text(g, 166.0f, 174.0f, 11.0f, kText, "Stop preparing the game? Next time it carries on from this step.", 310.0f);
        static const char* rows[] = { "KEEP GOING", "STOP" };
        Input in2 = in;
        const int a = menu_rows(g, 151.0f, 214.0f, 338.0f, rows, 2, &S.ask_cursor, in2);
        if (in.back || a == 0) S.ask_cancel = false;
        else if (a == 1) { S.ask_cancel = false; setup::cancel(); }
        return;
    }
    if (in.y) S.show_log = !S.show_log;
    if (in.back && s.status == setup::Status::Running) { S.ask_cancel = true; S.ask_cursor = 0; }
    (void)req;
}

void draw_stopped(const Gfx& g, const Input& in, Requests& req) {
    const setup::Snapshot s = setup::snapshot();
    header(g, "PREPARING GAME", "PAUSED");
    if (S.show_log) draw_log(g, s, 24.0f, 76.0f, 592.0f, 284.0f);
    else draw_steps(g, s, 24.0f, 76.0f, 352.0f);
    if (!S.show_log) {
        panel(g, 392.0f, 76.0f, 224.0f, 284.0f, "PAUSED");
        const std::string t = "Stopped at: " + std::string(setup::stage_name(std::clamp(s.stage, 0, setup::S_COUNT - 1))) +
                              ". CARRY ON starts again from there; what is done stays done.";
        text(g, 406.0f, 92.0f, 11.0f, kText, t.c_str(), 196.0f);
    }
    static const char* rows[] = { "CARRY ON", "BACK TO THE MENU" };
    footer(g, "A  SELECT      D-PAD  MOVE      Y  LOG");
    const int a = menu_rows(g, 392.0f, 230.0f, 224.0f, rows, 2, &S.cursor, in);
    if (S.test_exit && ImGui::GetTime() - S.page_since > 2.0) req.quit = true;
    if (in.y) S.show_log = !S.show_log;
    if (a == 0) start_build(S.disc_cue);
    else if (a == 1 || in.back) go(P_NONE);
}

void draw_failed(const Gfx& g, const Input& in, Requests& req) {
    const setup::Snapshot s = setup::snapshot();
    header(g, "PREPARING GAME", "STOPPED");
    if (S.show_log) {
        draw_log(g, s, 24.0f, 76.0f, 592.0f, 236.0f);
    } else {
        panel(g, 24.0f, 76.0f, 592.0f, 236.0f, "WHAT HAPPENED");
        text(g, 38.0f, 92.0f, 13.0f, kWarn, upper(s.error_title).c_str(), 0.0f, true);
        text(g, 38.0f, 116.0f, 11.0f, kText, s.error_text.c_str(), 564.0f);
        const std::string st = "STEP: " + upper(setup::stage_name(std::clamp(s.stage, 0, setup::S_COUNT - 1)));
        text(g, 38.0f, 288.0f, 10.0f, kDim, st.c_str());
    }
    panel(g, 24.0f, 326.0f, 300.0f, 92.0f, "SELECT");
    static const char* rows[] = { "RETRY", "SHOW LOG", "BACK TO THE MENU" };
    const char* vals[] = { "CARRIES ON", S.show_log ? "SHOWING" : nullptr, nullptr };
    const int a = menu_rows(g, 25.0f, 338.0f, 298.0f, rows, 3, &S.cursor, in, vals);
    panel(g, 340.0f, 326.0f, 276.0f, 92.0f, "LOG FILE");
    text(g, 354.0f, 340.0f, 10.0f, kText, fit(g, 10.0f, s.log_file, 250.0f).c_str());
    text(g, 354.0f, 358.0f, 10.0f, kDim, "Send this file when asking for help.", 250.0f);
    footer(g, "A  SELECT      D-PAD  MOVE      Y  LOG");
    if (S.test_exit && ImGui::GetTime() - S.page_since > 2.0) req.quit = true;
    if (in.y || a == 1) S.show_log = !S.show_log;
    if (a == 0) start_build(S.disc_cue);
    if (a == 2 || in.back) go(P_NONE);
}

void draw_done(const Gfx& g, const Input& in, Requests& req) {
    const setup::Snapshot s = setup::snapshot();
    header(g, "PREPARING GAME", "DONE");
    draw_steps(g, s, 24.0f, 76.0f, 352.0f);
    panel(g, 392.0f, 76.0f, 224.0f, 284.0f, "READY");
    text(g, 406.0f, 92.0f, 13.0f, kOn, "THE GAME IS READY", 0.0f, true);
    std::string info;
    setup::game_built(&info);
    const std::string t = "Built from your disc in " + clock_text(s.elapsed > 0 ? s.elapsed : 0) +
                          ".\n\nEverything it needs is in RecompData. START GAME starts it.";
    text(g, 406.0f, 116.0f, 11.0f, kText, t.c_str(), 196.0f);
    static const char* rows[] = { "START GAME", "BACK TO THE MENU" };
    const int a = menu_rows(g, 392.0f, 282.0f, 224.0f, rows, 2, &S.cursor, in);
    footer(g, "A  SELECT");
    if (S.test_exit && ImGui::GetTime() - S.page_since > 2.0) req.quit = true;
    if (a == 0) { S.start_now = true; go(P_NONE); return; }
    if (a == 1 || in.back) go(P_NONE);
}

} /* namespace */

void open() {
    const setup::Snapshot s = setup::snapshot();
    if (s.status == setup::Status::Running || s.status == setup::Status::Cancelling) { go(P_PREP); return; }
    go(P_WELCOME);
}

void find_disc() {
    S.find_only = true;
    S.disc_ready = false;
    disc::scan(P().root, last_disc());
    go(P_FIND);
}

bool take_disc_ready() {
    const bool r = S.disc_ready;
    S.disc_ready = false;
    return r;
}

bool take_start_now() {
    const bool r = S.start_now;
    S.start_now = false;
    return r;
}

fs::path chosen_disc() {
    const fs::path d = last_disc();
    std::error_code ec;
    return !d.empty() && fs::exists(d, ec) ? d : fs::path();
}

bool active() { return S.page != P_NONE; }
bool preparing() { return S.page == P_PREP; }

void draw(const Gfx& g, const Input& in, Requests& req) {
    switch (S.page) {
        case P_WELCOME:  draw_welcome(g, in, req); break;
        case P_FIND:     draw_find(g, in, req); break;
        case P_COPY_ASK: draw_copy_ask(g, in, req); break;
        case P_COPYING:  draw_copying(g, in, req); break;
        case P_PICKER:   draw_picker(g, in, req); break;
        case P_PREP:     draw_prep(g, in, req); break;
        case P_STOPPED:  draw_stopped(g, in, req); break;
        case P_FAILED:   draw_failed(g, in, req); break;
        case P_DONE:     draw_done(g, in, req); break;
        default: break;
    }
}

void set_test_auto(bool on, bool exit_when_done) { S.test_auto = on; S.test_exit = exit_when_done; }

std::string shot_key() {
    const setup::Snapshot s = setup::snapshot();
    char b[96];
    std::snprintf(b, sizeof b, "page%d-stage%d-status%d-log%d-cancel%d", (int)S.page,
                  S.page == P_PREP || S.page == P_DONE || S.page == P_FAILED || S.page == P_STOPPED ? s.stage : -1,
                  (int)s.status, (int)S.show_log, (int)S.ask_cancel);
    return b;
}

std::string disc_status(bool* ok) {
    const fs::path d = last_disc();
    std::error_code ec;
    if (!d.empty() && fs::exists(d, ec)) { if (ok) *ok = true; return upper(disc::display_name(d)); }
    if (ok) *ok = false;
    return "NOT CHOSEN YET";
}

} /* namespace firstrun */

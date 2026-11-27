#include "kvfile.h"

#include "platform.h"

#include <cctype>
#include <cstdio>

namespace kv {

namespace {

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

/* The value part of a key line, without a trailing comment. A '#' inside a
 * quoted TOML string is part of the string. */
std::string value_of(const std::string& text) {
    const size_t eq = text.find('=');
    if (eq == std::string::npos) return "";
    std::string v = text.substr(eq + 1);
    bool in_q = false;
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] == '"' && (i == 0 || v[i - 1] != '\\')) in_q = !in_q;
        else if (!in_q && (v[i] == '#' || v[i] == ';')) { v.resize(i); break; }
    }
    return trim(v);
}

} /* namespace */

namespace {
/* Split text into lines with their sections and keys. */
void parse(const std::string& all, std::vector<std::string>* out) {
    std::string text;
    for (char c : all) {
        if (c == '\n') { out->push_back(text); text.clear(); }
        else text += c;
    }
    if (!text.empty()) out->push_back(text);
}
} /* namespace */

void File::load_text(const std::string& all) {
    path_.clear();
    lines_.clear();
    existed_ = true;
    std::vector<std::string> raw;
    parse(all, &raw);
    std::string section;
    for (std::string text : raw) {
        if (!text.empty() && text.back() == '\r') text.pop_back();
        Line l;
        l.text = text;
        const std::string t = trim(text);
        if (t.size() >= 2 && t.front() == '[' && t.back() == ']' && t[1] != '[') {
            section = lower(trim(t.substr(1, t.size() - 2)));
        } else if (!t.empty() && t[0] != '#' && t[0] != ';') {
            const size_t eq = t.find('=');
            if (eq != std::string::npos) l.key = lower(trim(t.substr(0, eq)));
        }
        l.section = section;
        lines_.push_back(l);
    }
}

bool File::load(const std::filesystem::path& p) {
    lines_.clear();
    existed_ = false;
    path_ = p;
    FILE* f = plat::open_file(p, "rb");
    if (!f) return false;
    std::string all;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) all.append(buf, n);
    std::fclose(f);
    load_text(all);
    path_ = p;
    return true;
}

bool File::save() const {
    if (path_.empty()) return false;
    std::error_code ec;
    if (!path_.parent_path().empty()) std::filesystem::create_directories(path_.parent_path(), ec);
    const std::filesystem::path tmp = path_.string() + ".new";
    FILE* f = plat::open_file(tmp, "wb");
    if (!f) return false;
    for (const Line& l : lines_) std::fprintf(f, "%s\n", l.text.c_str());
    if (std::fclose(f) != 0) return false;
    std::filesystem::rename(tmp, path_, ec);
    if (ec) {   /* Windows: rename over an existing file can fail; replace it. */
        std::filesystem::remove(path_, ec);
        std::filesystem::rename(tmp, path_, ec);
    }
    return !ec;
}

int File::find(const std::string& section, const std::string& key) const {
    const std::string s = lower(section), k = lower(key);
    for (size_t i = 0; i < lines_.size(); ++i)
        if (lines_[i].section == s && lines_[i].key == k) return (int)i;
    return -1;
}

bool File::get(const std::string& section, const std::string& key, std::string* raw) const {
    const int i = find(section, key);
    if (i < 0) return false;
    if (raw) *raw = value_of(lines_[i].text);
    return true;
}

void File::set(const std::string& section, const std::string& key, const std::string& raw) {
    const std::string s = lower(section);
    const int i = find(s, key);
    Line l;
    l.text = key + " = " + raw;
    l.section = s;
    l.key = lower(key);
    if (i >= 0) { lines_[i] = l; return; }
    if (s.empty()) {
        /* Top-level: before the first section header (and its blank line). */
        size_t at = 0;
        while (at < lines_.size() && lines_[at].section.empty()) ++at;
        while (at > 0 && trim(lines_[at - 1].text).empty()) --at;
        lines_.insert(lines_.begin() + (long)at, l);
        return;
    }
    /* After the last key line of the section, or a new section at the end. */
    int last = -1, header = -1;
    for (size_t j = 0; j < lines_.size(); ++j) {
        if (lines_[j].section != s) continue;
        if (header < 0) header = (int)j;
        if (!lines_[j].key.empty()) last = (int)j;
    }
    if (header >= 0) {
        /* The header line is the section's first line (load() gives it the new section). */
        lines_.insert(lines_.begin() + (last >= 0 ? last + 1 : header + 1), l);
        return;
    }
    if (!lines_.empty() && !trim(lines_.back().text).empty()) lines_.push_back(Line{ "", lines_.back().section, "" });
    lines_.push_back(Line{ "[" + section + "]", s, "" });
    lines_.push_back(l);
}

void File::erase(const std::string& section, const std::string& key) {
    const int i = find(section, key);
    if (i >= 0) lines_.erase(lines_.begin() + i);
}

std::string unquote(const std::string& raw) {
    if (raw.size() >= 2 && ((raw.front() == '"' && raw.back() == '"') || (raw.front() == '\'' && raw.back() == '\'')))
        return raw.substr(1, raw.size() - 2);
    return raw;
}

std::string quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}

} /* namespace kv */

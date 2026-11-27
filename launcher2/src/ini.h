/* Tiny key=value files: RecompData/launcher/launcher.ini (window, disc choice)
 * and build-times.ini (how long each step took on this PC, for the estimate). */
#pragma once

#include "platform.h"

#include <map>
#include <string>

namespace ini {

inline std::map<std::string, std::string> load(const std::filesystem::path& p) {
    std::map<std::string, std::string> kv;
    FILE* f = plat::open_file(p, "rb");
    if (!f) return kv;
    std::string line;
    int c;
    auto take = [&] {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        const size_t eq = line.find('=');
        if (eq != std::string::npos && line[0] != '#') kv[line.substr(0, eq)] = line.substr(eq + 1);
        line.clear();
    };
    while ((c = std::fgetc(f)) != EOF) {
        if (c == '\n') take();
        else line += (char)c;
    }
    take();
    std::fclose(f);
    return kv;
}

inline bool save(const std::filesystem::path& p, const std::map<std::string, std::string>& kv) {
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    const std::filesystem::path tmp = p.string() + ".new";
    FILE* f = plat::open_file(tmp, "wb");
    if (!f) return false;
    for (const auto& e : kv) std::fprintf(f, "%s=%s\n", e.first.c_str(), e.second.c_str());
    const bool ok = std::fclose(f) == 0;
    if (!ok) return false;
    std::filesystem::rename(tmp, p, ec);
    return !ec;
}

/* Change one key, keep the rest. */
inline void set(const std::filesystem::path& p, const std::string& key, const std::string& value) {
    auto kv = load(p);
    if (value.empty()) kv.erase(key);
    else kv[key] = value;
    save(p, kv);
}

} /* namespace ini */

/* A line-keeping editor for the game's small setting files: settings.toml
 * (TOML), input.ini and keybinds.ini (INI). All three are "[section]" headers
 * and "key = value" lines, so one editor serves them.
 *
 * It changes only the lines it is asked to: every other line, comment, blank
 * and section stays as it was, so keys the launcher has no row for (paths the
 * game wrote, a player's hand edits) are never lost. Values are kept raw: the
 * caller writes TOML strings with their quotes (settings.h does that). */
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace kv {

class File {
public:
    /* Missing file: empty, saved as new on save(). */
    bool load(const std::filesystem::path& p);
    /* From text in memory (game.toml built into the launcher); save() does nothing. */
    void load_text(const std::string& text);
    bool save() const;           /* written to a .new file, then renamed */
    bool exists() const { return existed_; }

    /* The raw value of [section] key (section "" = above the first header). */
    bool get(const std::string& section, const std::string& key, std::string* raw) const;
    /* Change it in place, or add it at the end of its section (the section is
     * added at the end of the file when missing). */
    void set(const std::string& section, const std::string& key, const std::string& raw);
    void erase(const std::string& section, const std::string& key);
    /* Put a top-level line (above every section), e.g. settings_format = 2. */
    void set_top(const std::string& key, const std::string& raw) { set("", key, raw); }

private:
    struct Line { std::string text; std::string section; std::string key; };  /* key empty: not a key line */
    int find(const std::string& section, const std::string& key) const;
    std::filesystem::path path_;
    std::vector<Line> lines_;
    bool existed_ = false;
};

/* TOML value helpers. */
std::string unquote(const std::string& raw);   /* "abc" -> abc (or raw as it is) */
std::string quote(const std::string& s);       /* abc -> "abc" */

} /* namespace kv */

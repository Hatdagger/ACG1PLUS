/* A small JSON reader (0.2.4 UPDATES): enough for GitHub's list of releases and
 * our own manifest.json. Reads everything into a tree of Values; numbers are
 * kept as doubles (sizes up to 2^53 bytes are exact). No writer: the launcher
 * writes no JSON. */
#pragma once

#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace json {

struct Value {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool        b = false;
    double      n = 0.0;
    std::string s;
    std::vector<Value> items;                         /* Array */
    std::vector<std::pair<std::string, Value>> keys;  /* Object, in file order */

    const Value& operator[](const char* k) const {
        static const Value none;
        if (type != Object) return none;
        for (const auto& kv : keys) if (kv.first == k) return kv.second;
        return none;
    }
    const std::string& str() const { static const std::string e; return type == String ? s : e; }
    double num() const { return type == Number ? n : 0.0; }
    bool truthy() const { return type == Bool ? b : false; }
    bool is_object() const { return type == Object; }
    bool is_array() const { return type == Array; }
};

namespace detail {

struct Parser {
    const char* p;
    const char* end;
    bool ok = true;

    void ws() { while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p; }
    bool lit(const char* w) {
        const char* q = p;
        for (; *w; ++w, ++q) if (q >= end || *q != *w) return false;
        p = q;
        return true;
    }
    static void put_utf8(std::string& o, unsigned c) {
        if (c < 0x80) o += (char)c;
        else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) {
            o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F));
        } else {
            o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 0x3F));
            o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F));
        }
    }
    unsigned hex4() {
        if (end - p < 4) { ok = false; return 0; }
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = *p++;
            v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
            else { ok = false; return 0; }
        }
        return v;
    }
    std::string string() {
        std::string o;
        ++p;   /* the opening quote */
        while (p < end && *p != '"') {
            char c = *p++;
            if (c != '\\') { o += c; continue; }
            if (p >= end) { ok = false; break; }
            c = *p++;
            switch (c) {
                case 'n': o += '\n'; break;
                case 't': o += '\t'; break;
                case 'r': o += '\r'; break;
                case 'b': o += '\b'; break;
                case 'f': o += '\f'; break;
                case 'u': {
                    unsigned cp = hex4();
                    if (cp >= 0xD800 && cp < 0xDC00 && end - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                        p += 2;
                        const unsigned lo = hex4();
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    put_utf8(o, cp);
                    break;
                }
                default: o += c; break;   /* \" \\ \/ */
            }
        }
        if (p < end) ++p; else ok = false;
        return o;
    }
    Value value(int depth) {
        Value v;
        ws();
        if (p >= end || depth > 64) { ok = false; return v; }
        const char c = *p;
        if (c == '{') {
            v.type = Value::Object;
            ++p;
            ws();
            if (p < end && *p == '}') { ++p; return v; }
            while (ok) {
                ws();
                if (p >= end || *p != '"') { ok = false; break; }
                std::string k = string();
                ws();
                if (p >= end || *p != ':') { ok = false; break; }
                ++p;
                v.keys.emplace_back(std::move(k), value(depth + 1));
                ws();
                if (p < end && *p == ',') { ++p; continue; }
                if (p < end && *p == '}') { ++p; break; }
                ok = false;
            }
        } else if (c == '[') {
            v.type = Value::Array;
            ++p;
            ws();
            if (p < end && *p == ']') { ++p; return v; }
            while (ok) {
                v.items.push_back(value(depth + 1));
                ws();
                if (p < end && *p == ',') { ++p; continue; }
                if (p < end && *p == ']') { ++p; break; }
                ok = false;
            }
        } else if (c == '"') {
            v.type = Value::String;
            v.s = string();
        } else if (lit("true")) { v.type = Value::Bool; v.b = true; }
        else if (lit("false")) { v.type = Value::Bool; }
        else if (lit("null")) { }
        else if (c == '-' || (c >= '0' && c <= '9')) {
            const char* q = p;
            while (q < end && (std::strchr("+-.eE", *q) || (*q >= '0' && *q <= '9'))) ++q;
            v.type = Value::Number;
            v.n = std::strtod(std::string(p, q).c_str(), nullptr);
            p = q;
        } else {
            ok = false;
        }
        return v;
    }
};

} /* namespace detail */

/* False when the text isn't valid JSON (out is then whatever was read). */
inline bool parse(const std::string& text, Value* out) {
    detail::Parser ps{ text.data(), text.data() + text.size() };
    *out = ps.value(0);
    ps.ws();
    return ps.ok && ps.p == ps.end;
}

} /* namespace json */

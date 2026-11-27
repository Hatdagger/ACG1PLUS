#include "unzip.h"

#include "miniz.h"
#include "platform.h"

#include <cstdio>
#include <vector>

namespace unzip {

namespace fs = std::filesystem;

namespace {

struct Sink {
    FILE* f;
    uint64_t* done;
    uint64_t total;
    const std::function<bool(uint64_t, uint64_t)>* progress;
    uint64_t last_report;
    bool stop;
};

size_t write_cb(void* opaque, mz_uint64 /*ofs*/, const void* buf, size_t n) {
    Sink* s = (Sink*)opaque;
    if (s->stop) return 0;
    if (std::fwrite(buf, 1, n, s->f) != n) return 0;
    *s->done += n;
    if (*s->progress && *s->done - s->last_report > (8u << 20)) {
        s->last_report = *s->done;
        if (!(*s->progress)(*s->done, s->total)) { s->stop = true; return 0; }
    }
    return n;
}

bool safe_name(const std::string& n) {
    if (n.empty() || n[0] == '/' || n[0] == '\\' || (n.size() > 1 && n[1] == ':')) return false;
    size_t a = 0;
    while (a <= n.size()) {
        size_t b = n.find_first_of("/\\", a);
        if (b == std::string::npos) b = n.size();
        if (n.compare(a, b - a, "..") == 0 && b - a == 2) return false;
        a = b + 1;
    }
    return true;
}

} /* namespace */

bool extract_all(const fs::path& zip, const fs::path& dest,
                 const std::function<bool(uint64_t, uint64_t)>& progress, std::string* error) {
    FILE* zf = plat::open_file(zip, "rb");
    if (!zf) { if (error) *error = "could not open " + plat::u8str(zip); return false; }
    mz_zip_archive za = {};
    if (!mz_zip_reader_init_cfile(&za, zf, 0, 0)) {
        std::fclose(zf);
        if (error) *error = "not a readable zip: " + plat::u8str(zip);
        return false;
    }
    const mz_uint count = mz_zip_reader_get_num_files(&za);
    uint64_t total = 0, done = 0;
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st;
        if (mz_zip_reader_file_stat(&za, i, &st)) total += st.m_uncomp_size;
    }
    std::error_code ec;
    fs::create_directories(dest, ec);
    bool ok = true;
    for (mz_uint i = 0; i < count && ok; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&za, i, &st)) { ok = false; if (error) *error = "broken zip entry"; break; }
        const std::string name = st.m_filename;
        if (!safe_name(name)) { ok = false; if (error) *error = "refused a file name in the zip: " + name; break; }
        const fs::path out = dest / plat::u8path(name);
        if (st.m_is_directory) { fs::create_directories(out, ec); continue; }
        fs::create_directories(out.parent_path(), ec);
        FILE* f = plat::open_file(out, "wb");
        if (!f) { ok = false; if (error) *error = "could not write " + plat::u8str(out); break; }
        Sink s = { f, &done, total, &progress, done, false };
        const bool got = mz_zip_reader_extract_to_callback(&za, i, write_cb, &s, 0);
        const bool closed = std::fclose(f) == 0;
        if (s.stop) { ok = false; if (error) *error = "stopped"; break; }
        if (!got || !closed) {
            ok = false;
            if (error) *error = "could not unpack " + name + " (is the drive full?)";
            break;
        }
        /* Unix mode bits live in the top half of the external attributes. */
        const mz_uint32 mode = st.m_external_attr >> 16;
        if (mode & 0111) plat::make_executable(out);
    }
    mz_zip_reader_end(&za);
    std::fclose(zf);
    if (ok && progress) progress(total, total);
    return ok;
}

} /* namespace unzip */

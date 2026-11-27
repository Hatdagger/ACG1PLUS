/* SHA-256 (downloads) and MD5 (the disc check against game.toml's known_md5). */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace hash {

struct Sha256 {
    uint32_t h[8];
    uint64_t len = 0;
    unsigned char buf[64];
    size_t used = 0;
    Sha256();
    void update(const void* data, size_t n);
    std::string hex();   /* finishes; call once */
};

struct Md5 {
    uint32_t h[4];
    uint64_t len = 0;
    unsigned char buf[64];
    size_t used = 0;
    Md5();
    void update(const void* data, size_t n);
    std::string hex();
};

} /* namespace hash */

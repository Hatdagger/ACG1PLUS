/* Reads a PlayStation sound bank (VAB: a .VH header plus a .VB body of
 * PS-ADPCM samples) into plain 16-bit sound, one clip per tone.
 *
 * Used for the menu sounds, which come from the player's own disc
 * (GG/MS/MENU_VAB.T entries 0 + 1, see docs/research/sound-map.md section 5).
 * No SDL or disc code here, so it can be tested on its own.
 */
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace moa_vab {

struct Clip {
    std::vector<int16_t> pcm;   /* mono */
    int   rate = 44100;         /* samples a second when played at the asked note */
    float volume = 1.0f;        /* the tone's own volume, 0..1 */
};

static inline uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint32_t rd32(const uint8_t* p) { return rd16(p) | ((uint32_t)rd16(p + 2) << 16); }

/* PS-ADPCM: 16-byte blocks of 28 samples each. Stops at the block marked
 * as the end (flag bit 0), so a looping sample plays once. */
static inline std::vector<int16_t> decode_adpcm(const uint8_t* data, size_t size) {
    static const int f[5][2] = { {0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60} };
    std::vector<int16_t> out;
    out.reserve(size / 16 * 28);
    int h1 = 0, h2 = 0;
    for (size_t i = 0; i + 16 <= size; i += 16) {
        const uint8_t* b = data + i;
        const int shift = b[0] & 15;
        int filter = b[0] >> 4;
        if (filter > 4) filter = 0;
        for (int j = 0; j < 28; ++j) {
            int n = (b[2 + j / 2] >> ((j & 1) * 4)) & 15;
            if (n >= 8) n -= 16;
            int s = (n << 12) >> shift;
            s += (h1 * f[filter][0] + h2 * f[filter][1] + 32) >> 6;
            if (s > 32767) s = 32767;
            if (s < -32768) s = -32768;
            h2 = h1; h1 = s;
            out.push_back((int16_t)s);
        }
        if ((b[1] & 1) && i > 0) break;
    }
    return out;
}

/* Program `program` of the bank, played at MIDI note `note`: one clip per
 * tone, in tone order (the game picks sounds by tone number). Returns false
 * when the data isn't a VAB the menu can use. */
static inline bool read_bank(const uint8_t* vh, size_t vh_size, const uint8_t* vb,
                             size_t vb_size, int program, int note,
                             std::vector<Clip>& out) {
    out.clear();
    if (vh_size < 0x20 || std::memcmp(vh, "pBAV", 4) != 0) return false;
    const int programs = rd16(vh + 0x12);
    const int vags = rd16(vh + 0x16);
    if (programs <= 0 || programs > 128 || vags <= 0 || vags > 254 || program >= programs)
        return false;
    const size_t prog_at = 0x20;
    const size_t tone_at = prog_at + 128 * 16;
    const size_t sizes_at = tone_at + (size_t)programs * 16 * 32;
    if (vh_size < sizes_at + 256 * 2) return false;

    /* Sample n starts after the sizes of samples 0..n-1 (sizes are in 8-byte
     * units; entry 0 is always empty). */
    std::vector<size_t> start(vags + 2, 0), length(vags + 2, 0);
    size_t at = 0;
    for (int v = 0; v <= vags; ++v) {
        start[v] = at;
        length[v] = (size_t)rd16(vh + sizes_at + v * 2) * 8;
        at += length[v];
    }
    if (at > vb_size) return false;

    const int tones = vh[prog_at + program * 16];
    if (tones <= 0 || tones > 16) return false;
    /* Tones of program p are stored after those of every earlier program. */
    const uint8_t* t0 = vh + tone_at + (size_t)program * 16 * 32;
    for (int t = 0; t < tones; ++t) {
        const uint8_t* t_ = t0 + t * 32;
        const int vol = t_[2];
        const int center = t_[4];
        const int fine = t_[5];        /* 1/128ths of a semitone */
        const int vag = (int16_t)rd16(t_ + 0x16);
        Clip c;
        c.volume = vol / 127.0f;
        const double semis = (note - center) - fine / 128.0;
        c.rate = (int)std::lround(44100.0 * std::pow(2.0, semis / 12.0));
        if (vag > 0 && vag <= vags && length[vag] > 0)
            c.pcm = decode_adpcm(vb + start[vag], length[vag]);
        out.push_back(std::move(c));
    }
    return true;
}

} /* namespace moa_vab */

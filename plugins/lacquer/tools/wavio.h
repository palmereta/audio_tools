#pragma once
// Lectura y escritura de WAV sin dependencias (PCM 16/24/32 y float 32/64, mono o estéreo; WAVE_FORMAT_EXTENSIBLE
// incluido). Lo usan lacquerrender y mastertest.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace dh::lq {

struct Audio { double fs = 48000.0; int channels = 2, bits = 24; bool isFloat = false; std::vector<float> L, R; };

inline bool readWav (const std::string& path, Audio& a, std::string& err)
{
    FILE* f = std::fopen (path.c_str(), "rb");
    if (! f) { err = "cannot open " + path; return false; }
    std::vector<unsigned char> d;
    { unsigned char buf[65536]; size_t k; while ((k = std::fread (buf, 1, sizeof buf, f)) > 0) d.insert (d.end(), buf, buf + k); }
    std::fclose (f);
    auto u16 = [&] (size_t p) { return (uint32_t) d[p] | ((uint32_t) d[p + 1] << 8); };
    auto u32 = [&] (size_t p) { return u16 (p) | (u16 (p + 2) << 16); };
    if (d.size() < 12 || std::memcmp (d.data(), "RIFF", 4) || std::memcmp (d.data() + 8, "WAVE", 4)) { err = "not a WAV file"; return false; }
    size_t p = 12, dataPos = 0, dataLen = 0; int fmtTag = 0, ch = 0, bits = 0, align = 0; uint32_t rate = 0;
    while (p + 8 <= d.size())
    {
        const uint32_t len = u32 (p + 4);
        if (! std::memcmp (d.data() + p, "fmt ", 4) && p + 24 <= d.size())
        {
            fmtTag = (int) u16 (p + 8); ch = (int) u16 (p + 10); rate = u32 (p + 12); align = (int) u16 (p + 20); bits = (int) u16 (p + 22);
            if (fmtTag == 0xFFFE && len >= 26) fmtTag = (int) u16 (p + 8 + 24);
        }
        else if (! std::memcmp (d.data() + p, "data", 4)) { dataPos = p + 8; dataLen = std::min ((size_t) len, d.size() - dataPos); break; }
        p += 8 + len + (len & 1);
    }
    if (! dataPos || ch < 1 || align <= 0) { err = "WAV without fmt/data"; return false; }
    if (! ((fmtTag == 1 && (bits == 16 || bits == 24 || bits == 32)) || (fmtTag == 3 && (bits == 32 || bits == 64)))) { err = "unsupported WAV encoding"; return false; }
    const size_t frames = dataLen / (size_t) align;
    a.fs = rate; a.channels = std::min (ch, 2); a.bits = bits; a.isFloat = fmtTag == 3;
    a.L.resize (frames); a.R.resize (frames);
    const int bps = bits / 8;
    auto sample = [&] (size_t q) -> float {
        const unsigned char* s = d.data() + q;
        if (fmtTag == 3) { if (bits == 32) { float v; std::memcpy (&v, s, 4); return v; } double v; std::memcpy (&v, s, 8); return (float) v; }
        if (bits == 16) return (float) (int16_t) (s[0] | (s[1] << 8)) / 32768.0f;
        if (bits == 24) { int32_t v = (int32_t) ((uint32_t) s[0] << 8 | (uint32_t) s[1] << 16 | (uint32_t) s[2] << 24) >> 8; return (float) v / 8388608.0f; }
        int32_t v; std::memcpy (&v, s, 4); return (float) ((double) v / 2147483648.0);
    };
    for (size_t i = 0; i < frames; ++i)
    {
        const size_t q = dataPos + i * (size_t) align;
        a.L[i] = sample (q);
        a.R[i] = ch >= 2 ? sample (q + (size_t) bps) : a.L[i];
    }
    return true;
}

// bits 16 (con dither TPDF determinista), 24 (redondeo) o 32 (float).
inline bool writeWav (const std::string& path, const Audio& a, int bits, std::string& err)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) { err = "cannot write " + path; return false; }
    const size_t n = a.L.size();
    const int bps = bits / 8, ch = 2;
    const uint32_t dataLen = (uint32_t) (n * (size_t) (bps * ch));
    std::vector<unsigned char> h;
    auto put = [&] (uint32_t v, int k) { for (int i = 0; i < k; ++i) h.push_back ((unsigned char) (v >> (8 * i))); };
    h.insert (h.end(), { 'R', 'I', 'F', 'F' }); put (36 + dataLen, 4); h.insert (h.end(), { 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ' });
    put (16, 4); put (bits == 32 ? 3 : 1, 2); put ((uint32_t) ch, 2); put ((uint32_t) a.fs, 4); put ((uint32_t) (a.fs * bps * ch), 4);
    put ((uint32_t) (bps * ch), 2); put ((uint32_t) bits, 2); h.insert (h.end(), { 'd', 'a', 't', 'a' }); put (dataLen, 4);
    std::fwrite (h.data(), 1, h.size(), f);
    std::vector<unsigned char> out; out.reserve ((size_t) dataLen);
    uint32_t rng = 0x9E3779B9u;
    auto rnd = [&] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (double) rng / 4294967296.0; };
    for (size_t i = 0; i < n; ++i)
        for (int c = 0; c < ch; ++c)
        {
            const double x = (double) (c == 0 ? a.L[i] : a.R[i]);
            if (bits == 32) { float v = (float) x; unsigned char b[4]; std::memcpy (b, &v, 4); out.insert (out.end(), b, b + 4); }
            else if (bits == 24)
            {
                const int32_t v = (int32_t) std::lround (std::fmax (-8388608.0, std::fmin (8388607.0, x * 8388608.0)));
                out.push_back ((unsigned char) v); out.push_back ((unsigned char) (v >> 8)); out.push_back ((unsigned char) (v >> 16));
            }
            else
            {
                const double dith = rnd() - rnd();                                  // TPDF ±1 LSB
                const int32_t v = (int32_t) std::lround (std::fmax (-32768.0, std::fmin (32767.0, x * 32768.0 + dith)));
                out.push_back ((unsigned char) v); out.push_back ((unsigned char) (v >> 8));
            }
        }
    std::fwrite (out.data(), 1, out.size(), f);
    std::fclose (f);
    return true;
}

} // namespace dh::lq

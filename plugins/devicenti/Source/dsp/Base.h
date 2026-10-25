#pragma once
// DeviceNTI V: basic pieces shared by the voice, the buses and the effects. No JUCE; deterministic; no allocation
// after prepare. Sine by table (no std::sin per sample: the Windows CRT is several times slower), the TPT
// state-variable filter, one-poles, a Hermite delay line, a hash for decisions tied to position, and the 97-tap
// half-band decimator that brings the 2x voice down to the host rate with an exact 24-sample delay.

#include "Common.h"           // shared/dsp: clampf, fastTanh, fastExp2, fastTan, prewarp
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>
// The SIMD wrappers must inline (MSVC passes a struct of two __m128 through memory when a call is not inlined).
#if defined (_MSC_VER)
 #define DV_INLINE __forceinline
#else
 #define DV_INLINE inline __attribute__ ((always_inline))
#endif
// SIMD headers here, outside the namespace (V4 below picks the implementation).
#if defined (__ARM_NEON) || defined (__ARM_NEON__) || defined (_M_ARM64)
 #include <arm_neon.h>
 #define DV_NEON 1
#elif defined (__SSE2__) || defined (_M_X64) || (defined (_M_IX86_FP) && _M_IX86_FP >= 2)
 #include <emmintrin.h>
 #define DV_SSE 1
#endif

namespace dh::dv {

using dh::kPi; using dh::kTwoPi; using dh::clampf; using dh::fastExp; using dh::fastExp2; using dh::fastTan; using dh::fastTanh;

inline float dbToGain (float db) { return fastExp2 (db * 0.16609640474f); }
inline float gainToDb (float g) { return g > 1e-9f ? 20.0f * std::log10 (g) : -180.0f; }
inline float semis (float st) { return fastExp2 (st * (1.0f / 12.0f)); }
inline float mtof (float note) { return 440.0f * fastExp2 ((note - 69.0f) * (1.0f / 12.0f)); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }

// ── Sine and cosine by table: 4096 points with linear interpolation (error < 3e-7, −130 dB). Phase in turns. ──
struct SineTable
{
    static constexpr int N = 4096;
    float t[N + 1];
    SineTable() { for (int i = 0; i <= N; ++i) t[i] = (float) std::sin (2.0 * 3.14159265358979323846 * i / N); }
    static const SineTable& get() { static const SineTable s; return s; }
};
// x − ⌊x⌋ for x ≥ 0: exact in double, same result as std::fmod (x, 1.0), much cheaper.
inline double frac1 (double x) { return x - std::floor (x); }

inline float sinT (float turns)
{
    const auto& s = SineTable::get();
    float p = turns - std::floor (turns);
    const float x = p * (float) SineTable::N;
    int i = (int) x; if (i >= SineTable::N) i = SineTable::N - 1;
    const float f = x - (float) i;
    return s.t[i] + (s.t[i + 1] - s.t[i]) * f;
}
inline float cosT (float turns) { return sinT (turns + 0.25f); }

// ── Hash → [0, 1): probability and humanise decisions tied to (seed, position, track), never to block order. ──
inline uint64_t mix64 (uint64_t z)
{
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
inline double hashUni (uint64_t a, uint64_t b, uint64_t c) { return (double) (mix64 (a ^ mix64 (b ^ mix64 (c + 0x632BE59BD9B4E019ull))) >> 11) * (1.0 / 9007199254740992.0); }
inline float hashBi (uint64_t a, uint64_t b, uint64_t c) { return (float) (2.0 * hashUni (a, b, c) - 1.0); }

struct Noise
{
    uint32_t s = 0x9E3779B9u;
    void seed (uint32_t v) { s = v ? v : 1u; }
    inline float bi() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.0f / 2147483648.0f); }
    inline float uni() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s >> 8) * (1.0f / 16777216.0f); }
};

// ── TPT state-variable filter (Zavalishin): LP, BP (unity peak = bp·k), HP, notch, peak in one pass. ──
struct Svf
{
    float g = 0.1f, k = 1.4f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    float lp = 0, bp = 0, hp = 0;
    inline void setG (float gg, float kk) { g = gg; k = kk; a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2; }
    void set (float hz, float q, float fs) { setG (dh::prewarp (hz, fs), 1.0f / std::max (0.05f, q)); }
    inline void tick (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    inline float lowpass (float x) { tick (x); return lp; }
    inline float highpass (float x) { tick (x); return hp; }
    inline float bandpass (float x) { tick (x); return bp * k; }
    void reset() { ic1 = ic2 = lp = bp = hp = 0; }
};

// One-pole low-pass by coefficient (a = exp(−2π f / fs)); high-pass = x − lp.
struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setHz (float hz, float fs) { a = std::exp (-kTwoPi * clampf (hz, 0.1f, 0.49f * fs) / fs); }
    inline float lp (float x) { z = x + a * (z - x); return z; }
    inline float hp (float x) { return x - lp (x); }
    void reset (float v = 0.0f) { z = v; }
};

// Delay line, power-of-two size, Hermite read (d ≥ 1 samples back; d = 1 is the last written sample).
class Line
{
public:
    void prepare (int maxSamples) { int n = 1; while (n < maxSamples + 8) n <<= 1; b.assign ((size_t) n, 0.0f); mask = n - 1; w = 0; }
    void clear() { std::fill (b.begin(), b.end(), 0.0f); w = 0; }
    inline void push (float x) { b[(size_t) w] = x; w = (w + 1) & mask; }
    inline float at (int d) const { return b[(size_t) ((w - d) & mask)]; }
    inline float read (float d) const
    {
        d = clampf (d, 1.0f, (float) (mask - 4));
        const float fd = std::floor (d); const int i = (int) fd; const float t = d - fd;
        const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }
    int capacity() const { return mask - 4; }
private:
    std::vector<float> b; int mask = 0, w = 0;
};


// ── Four floats in one register: NEON on ARM, SSE2 on x86 (MSVC too), plain arrays otherwise. Only the operations the
// four-voice renderer needs; each is the same IEEE operation as the scalar code, so a lane computes what one voice
// alone computed. ──
struct V4
{
#if DV_NEON
    float32x4_t v;
    V4() : v (vdupq_n_f32 (0.0f)) {}
    V4 (float32x4_t x) : v (x) {}
    V4 (float x) : v (vdupq_n_f32 (x)) {}
    static DV_INLINE V4 load (const float* p) { return vld1q_f32 (p); }
    DV_INLINE void store (float* p) const { vst1q_f32 (p, v); }
    friend DV_INLINE V4 operator+ (V4 a, V4 b) { return vaddq_f32 (a.v, b.v); }
    friend DV_INLINE V4 operator- (V4 a, V4 b) { return vsubq_f32 (a.v, b.v); }
    friend DV_INLINE V4 operator* (V4 a, V4 b) { return vmulq_f32 (a.v, b.v); }
    friend DV_INLINE V4 operator/ (V4 a, V4 b) { return vdivq_f32 (a.v, b.v); }
    static DV_INLINE V4 floor (V4 a) { return vrndmq_f32 (a.v); }
    static DV_INLINE V4 min (V4 a, V4 b) { return vminq_f32 (a.v, b.v); }
    static DV_INLINE V4 max (V4 a, V4 b) { return vmaxq_f32 (a.v, b.v); }
    static DV_INLINE V4 abs (V4 a) { return vabsq_f32 (a.v); }
    // m ≠ 0 picks a, else b (m holds 0 or 1 per lane).
    static DV_INLINE V4 select (V4 m, V4 a, V4 b) { return vbslq_f32 (vcgtq_f32 (m.v, vdupq_n_f32 (0.0f)), a.v, b.v); }
    static DV_INLINE V4 lt (V4 a, V4 b) { return vbslq_f32 (vcltq_f32 (a.v, b.v), vdupq_n_f32 (1.0f), vdupq_n_f32 (0.0f)); }
    static DV_INLINE V4 eq (V4 a, V4 b) { return vbslq_f32 (vceqq_f32 (a.v, b.v), vdupq_n_f32 (1.0f), vdupq_n_f32 (0.0f)); }
    static DV_INLINE V4 copysign (V4 mag, V4 sgn) { return vbslq_f32 (vdupq_n_u32 (0x80000000u), sgn.v, mag.v); }
#elif DV_SSE
    __m128 v;
    V4() : v (_mm_setzero_ps()) {}
    V4 (__m128 x) : v (x) {}
    V4 (float x) : v (_mm_set1_ps (x)) {}
    static DV_INLINE V4 load (const float* p) { return _mm_loadu_ps (p); }
    DV_INLINE void store (float* p) const { _mm_storeu_ps (p, v); }
    friend DV_INLINE V4 operator+ (V4 a, V4 b) { return _mm_add_ps (a.v, b.v); }
    friend DV_INLINE V4 operator- (V4 a, V4 b) { return _mm_sub_ps (a.v, b.v); }
    friend DV_INLINE V4 operator* (V4 a, V4 b) { return _mm_mul_ps (a.v, b.v); }
    friend DV_INLINE V4 operator/ (V4 a, V4 b) { return _mm_div_ps (a.v, b.v); }
    static DV_INLINE V4 floor (V4 a)
    {
        // Truncate, then step down where truncation went up (negative values): exact for |x| < 2^31.
        const __m128 t = _mm_cvtepi32_ps (_mm_cvttps_epi32 (a.v));
        return _mm_sub_ps (t, _mm_and_ps (_mm_cmpgt_ps (t, a.v), _mm_set1_ps (1.0f)));
    }
    static DV_INLINE V4 min (V4 a, V4 b) { return _mm_min_ps (a.v, b.v); }
    static DV_INLINE V4 max (V4 a, V4 b) { return _mm_max_ps (a.v, b.v); }
    static DV_INLINE V4 abs (V4 a) { return _mm_andnot_ps (_mm_set1_ps (-0.0f), a.v); }
    static DV_INLINE V4 select (V4 m, V4 a, V4 b) { const __m128 k = _mm_cmpgt_ps (m.v, _mm_setzero_ps()); return _mm_or_ps (_mm_and_ps (k, a.v), _mm_andnot_ps (k, b.v)); }
    static DV_INLINE V4 lt (V4 a, V4 b) { return _mm_and_ps (_mm_cmplt_ps (a.v, b.v), _mm_set1_ps (1.0f)); }
    static DV_INLINE V4 eq (V4 a, V4 b) { return _mm_and_ps (_mm_cmpeq_ps (a.v, b.v), _mm_set1_ps (1.0f)); }
    static DV_INLINE V4 copysign (V4 mag, V4 sgn) { const __m128 m = _mm_set1_ps (-0.0f); return _mm_or_ps (_mm_and_ps (m, sgn.v), _mm_andnot_ps (m, mag.v)); }
#else
    float v[4];
    V4() : v { 0, 0, 0, 0 } {}
    V4 (float x) : v { x, x, x, x } {}
    static DV_INLINE V4 load (const float* p) { V4 r; for (int i = 0; i < 4; ++i) r.v[i] = p[i]; return r; }
    DV_INLINE void store (float* p) const { for (int i = 0; i < 4; ++i) p[i] = v[i]; }
    template <typename F> static V4 map2 (V4 a, V4 b, F f) { V4 r; for (int i = 0; i < 4; ++i) r.v[i] = f (a.v[i], b.v[i]); return r; }
    friend DV_INLINE V4 operator+ (V4 a, V4 b) { return map2 (a, b, [] (float x, float y) { return x + y; }); }
    friend DV_INLINE V4 operator- (V4 a, V4 b) { return map2 (a, b, [] (float x, float y) { return x - y; }); }
    friend DV_INLINE V4 operator* (V4 a, V4 b) { return map2 (a, b, [] (float x, float y) { return x * y; }); }
    friend DV_INLINE V4 operator/ (V4 a, V4 b) { return map2 (a, b, [] (float x, float y) { return x / y; }); }
    static DV_INLINE V4 floor (V4 a) { V4 r; for (int i = 0; i < 4; ++i) r.v[i] = std::floor (a.v[i]); return r; }
    static DV_INLINE V4 min (V4 a, V4 b) { return map2 (a, b, [] (float x, float y) { return std::min (x, y); }); }
    static DV_INLINE V4 max (V4 a, V4 b) { return map2 (a, b, [] (float x, float y) { return std::max (x, y); }); }
    static DV_INLINE V4 abs (V4 a) { V4 r; for (int i = 0; i < 4; ++i) r.v[i] = std::fabs (a.v[i]); return r; }
    static DV_INLINE V4 select (V4 m, V4 a, V4 b) { V4 r; for (int i = 0; i < 4; ++i) r.v[i] = m.v[i] > 0.0f ? a.v[i] : b.v[i]; return r; }
    static DV_INLINE V4 lt (V4 a, V4 b) { return map2 (a, b, [] (float x, float y) { return x < y ? 1.0f : 0.0f; }); }
    static DV_INLINE V4 eq (V4 a, V4 b) { return map2 (a, b, [] (float x, float y) { return x == y ? 1.0f : 0.0f; }); }
    static DV_INLINE V4 copysign (V4 mag, V4 sgn) { return map2 (mag, sgn, [] (float x, float y) { return std::copysign (x, y); }); }
#endif
};

// sinT for four phases: the same table and interpolation as the scalar one (gathered lane by lane).
DV_INLINE V4 sinT4 (V4 turns)
{
    const auto& s = SineTable::get();
    const V4 p = turns - V4::floor (turns);
    const V4 x = p * V4 ((float) SineTable::N);
    alignas (16) float xs[4], r[4];
    x.store (xs);
    for (int l = 0; l < 4; ++l)
    {
        int i = (int) xs[l]; if (i >= SineTable::N) i = SineTable::N - 1;
        const float f = xs[l] - (float) i;
        r[l] = s.t[i] + (s.t[i + 1] - s.t[i]) * f;
    }
    return V4::load (r);
}
// sin(2π·p) for four phases without a table: fold to [−π/2, π/2] and a degree-11 odd polynomial (error < 2e-7,
// below the table's). No gathers, so the four lanes stay in the register.
DV_INLINE V4 sin2pi4 (V4 p)
{
    p = p - V4::floor (p + V4 (0.5f));                                     // [−0.5, 0.5)
    V4 x = p * V4 (6.28318530717958647692f);
    const V4 hp (1.57079632679489661923f), pi (3.14159265358979323846f);
    x = V4::select (V4::lt (hp, x), pi - x, V4::select (V4::lt (x, V4 (0.0f) - hp), V4 (0.0f) - pi - x, x));
    const V4 x2 = x * x;
    return x * (V4 (1.0f) + x2 * (V4 (-1.0f / 6.0f) + x2 * (V4 (1.0f / 120.0f) + x2 * (V4 (-1.0f / 5040.0f) + x2 * (V4 (1.0f / 362880.0f) + x2 * V4 (-1.0f / 39916800.0f))))));
}
// fastTan for four values (same continued fraction as the scalar one).
DV_INLINE V4 fastTan4 (V4 x)
{
    const V4 x2 = x * x;
    return x * (V4 (135135.0f) + x2 * (V4 (-17325.0f) + x2 * (V4 (378.0f) - x2))) / (V4 (135135.0f) + x2 * (V4 (-62370.0f) + x2 * (V4 (3150.0f) - V4 (28.0f) * x2)));
}
// fastTanh for four values (same rational as the scalar one).
DV_INLINE V4 fastTanh4 (V4 x)
{
    x = V4::min (V4 (4.97f), V4::max (V4 (-4.97f), x));
    const V4 x2 = x * x;
    const V4 a = x * (V4 (135135.0f) + x2 * (V4 (17325.0f) + x2 * (V4 (378.0f) + x2)));
    const V4 b = V4 (135135.0f) + x2 * (V4 (62370.0f) + x2 * (V4 (3150.0f) + x2 * V4 (28.0f)));
    return a / b;
}

// Eight lanes as two independent V4 halves: the renderer runs two quads' worth of voices in one pass, so the long
// per-sample dependency chains (operator chain, ladder) of the two halves overlap in the pipeline.
struct V8
{
    V4 a, b;
    V8() = default;
    V8 (V4 x, V4 y) : a (x), b (y) {}
    V8 (float x) : a (x), b (x) {}
    static DV_INLINE V8 load (const float* p) { return { V4::load (p), V4::load (p + 4) }; }
    DV_INLINE void store (float* p) const { a.store (p); b.store (p + 4); }
    friend DV_INLINE V8 operator+ (V8 x, V8 y) { return { x.a + y.a, x.b + y.b }; }
    friend DV_INLINE V8 operator- (V8 x, V8 y) { return { x.a - y.a, x.b - y.b }; }
    friend DV_INLINE V8 operator* (V8 x, V8 y) { return { x.a * y.a, x.b * y.b }; }
    friend DV_INLINE V8 operator/ (V8 x, V8 y) { return { x.a / y.a, x.b / y.b }; }
    static DV_INLINE V8 floor (V8 x) { return { V4::floor (x.a), V4::floor (x.b) }; }
    static DV_INLINE V8 min (V8 x, V8 y) { return { V4::min (x.a, y.a), V4::min (x.b, y.b) }; }
    static DV_INLINE V8 max (V8 x, V8 y) { return { V4::max (x.a, y.a), V4::max (x.b, y.b) }; }
    static DV_INLINE V8 abs (V8 x) { return { V4::abs (x.a), V4::abs (x.b) }; }
    static DV_INLINE V8 select (V8 m, V8 x, V8 y) { return { V4::select (m.a, x.a, y.a), V4::select (m.b, x.b, y.b) }; }
    static DV_INLINE V8 lt (V8 x, V8 y) { return { V4::lt (x.a, y.a), V4::lt (x.b, y.b) }; }
    static DV_INLINE V8 eq (V8 x, V8 y) { return { V4::eq (x.a, y.a), V4::eq (x.b, y.b) }; }
    static DV_INLINE V8 copysign (V8 m, V8 s) { return { V4::copysign (m.a, s.a), V4::copysign (m.b, s.b) }; }
};
DV_INLINE V8 sinT4 (V8 x) { return { sinT4 (x.a), sinT4 (x.b) }; }
DV_INLINE V8 sin2pi4 (V8 x) { return { sin2pi4 (x.a), sin2pi4 (x.b) }; }
DV_INLINE V8 fastTan4 (V8 x) { return { fastTan4 (x.a), fastTan4 (x.b) }; }
DV_INLINE V8 fastTanh4 (V8 x) { return { fastTanh4 (x.a), fastTanh4 (x.b) }; }

// ── 2x → 1x half-band decimator, 97 taps (Kaiser β 10, ~100 dB from 28 kHz at 48 kHz base): the delay is exactly 48
// samples at 2x = 24 at the base rate. Odd taps are zero except the centre, so only 24 symmetric pairs are summed. ──
struct HalfbandCoefs
{
    static constexpr int kTaps = 97, kHalf = 48, kPairs = 24;
    float h[kPairs] {};
    HalfbandCoefs()
    {
        auto bessel = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 50; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; } return s; };
        const double beta = 10.0, ib = bessel (beta);
        for (int j = 0; j < kPairs; ++j)
        {
            const int t = 2 * j + 1;                                   // odd offset from the centre
            const double sinc = std::sin (3.14159265358979323846 * t * 0.5) / (3.14159265358979323846 * t);
            const double r = (double) t / kHalf;
            h[j] = (float) (sinc * bessel (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / ib);
        }
    }
    static const HalfbandCoefs& get() { static const HalfbandCoefs c; return c; }
};

class Decimator2
{
public:
    static constexpr int kLatency = 24;
    void reset() { std::fill (std::begin (buf), std::end (buf), 0.0f); pos = 0; }
    // Two 2x samples in, one base-rate sample out.
    inline float process (float x0, float x1)
    {
        put (x0); put (x1);
        // Newest sample is at offset 0; the centre (the output's time) is 49 back, taps at centre ± odd up to ±47.
        const auto& c = HalfbandCoefs::get();
        const float* p = buf + pos;          // p[k] = sample k back from the newest (mirror buffer)
        // Four partial sums: one accumulator would make a chain of 24 dependent multiply-adds (~100 cycles of
        // latency per output sample, the largest single cost of a voice before this).
        float a0 = 0.5f * p[49], a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        for (int j = 0; j < HalfbandCoefs::kPairs; j += 4)
        {
            a0 += c.h[j] * (p[48 - 2 * j] + p[50 + 2 * j]);
            a1 += c.h[j + 1] * (p[46 - 2 * j] + p[52 + 2 * j]);
            a2 += c.h[j + 2] * (p[44 - 2 * j] + p[54 + 2 * j]);
            a3 += c.h[j + 3] * (p[42 - 2 * j] + p[56 + 2 * j]);
        }
        return (a0 + a1) + (a2 + a3);
    }
private:
    static constexpr int kN = 128;
    float buf[2 * kN] {};
    int pos = 0;
    inline void put (float x) { pos = (pos == 0 ? kN : pos) - 1; buf[pos] = buf[pos + kN] = x; }
};

// ── 2x → 4x → 2x around a nonlinearity (the drive and fold stage): a 33-tap half-band (Kaiser β 8) up and down,
// 16 samples of delay at 2x in all (8 at the host rate, an integer: the voice latency stays exact). The interpolator's
// even outputs are the input itself (delayed); only 8 coefficient pairs are summed. ──
struct Hb33
{
    float c[8] {};
    Hb33()
    {
        auto bessel = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 50; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; } return s; };
        const double beta = 8.0, ib = bessel (beta);
        for (int j = 0; j < 8; ++j)
        {
            const int t = 2 * j + 1;
            const double sinc = std::sin (3.14159265358979323846 * t * 0.5) / (3.14159265358979323846 * t);
            const double r = (double) t / 16.0;
            c[j] = (float) (sinc * bessel (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / ib);
        }
    }
    static const Hb33& get() { static const Hb33 h; return h; }
};
class UpDown4
{
public:
    static constexpr int kDelay2x = 16;       // samples at 2x (8 up + 8 down) = 8 at the host rate
    void reset() { std::fill (std::begin (xh), std::end (xh), 0.0f); std::fill (std::begin (vh), std::end (vh), 0.0f); xp = vp = 0; std::fill (std::begin (dl), std::end (dl), 0.0f); dp = 0; stale = false; }
    // The histories as if the 32 given inputs (oldest first) had been pushed through the bypass.
    void rebuildFrom (const float* last32)
    {
        std::fill (std::begin (xh), std::end (xh), 0.0f); std::fill (std::begin (vh), std::end (vh), 0.0f); xp = vp = 0;
        for (int k = 0; k < 32; ++k) { const float x = last32[k]; xp = (xp == 0 ? 32 : xp) - 1; xh[xp] = xh[xp + 32] = x; push (x); push (x); dl[dp] = x; dp = (dp + 1) & 31; }
        stale = false;
    }
    template <typename F> inline float processPrimed (float x, F&& f) { stale = false; return process (x, f); }
    template <typename F> inline float process (float x, F&& f)
    {
        if (stale) rebuild();
        const auto& h = Hb33::get();
        dl[dp] = x; dp = (dp + 1) & 31;
        // Interpolate: x history (2x).
        xp = (xp == 0 ? 32 : xp) - 1; xh[xp] = xh[xp + 32] = x;
        const float* X = xh + xp;            // X[k] = x[p − k]
        const float y0 = X[8];
        float y1a = 0.0f, y1b = 0.0f;
        for (int j = 0; j < 8; j += 2) { y1a += h.c[j] * (X[8 + j] + X[7 - j]); y1b += h.c[j + 1] * (X[9 + j] + X[6 - j]); }
        const float y1 = 2.0f * (y1a + y1b);
        // Nonlinearity at 4x, then decimate: v history (4x).
        push (f (y0)); push (f (y1));
        const float* V = vh + vp;            // V[k] = v[2p+1 − k]: the newest is the odd sample
        float aa = 0.5f * V[17], ab = 0.0f;
        for (int j = 0; j < 8; j += 2) { aa += h.c[j] * (V[18 + 2 * j] + V[16 - 2 * j]); ab += h.c[j + 1] * (V[20 + 2 * j] + V[14 - 2 * j]); }
        return aa + ab;
    }
    // Bypass with the same delay (switching between the two paths does not move the signal in time). The filter
    // histories are rebuilt from the delay line when the filtered path starts again (what feeding them every sample
    // would have left there), so the bypass costs a ring write and a read.
    inline float delayOnly (float x)
    {
        dl[dp] = x; const float y = dl[(dp - kDelay2x) & 31]; dp = (dp + 1) & 31;
        stale = true;
        return y;
    }
private:
    float xh[64] {}, vh[128] {}, dl[32] {};
    int xp = 0, vp = 0, dp = 0; bool stale = false;
    void rebuild()
    {
        // The last 32 inputs (oldest first) as if each had been pushed: once into xh, twice into vh.
        for (int k = 32; k >= 1; --k)
        {
            const float x = dl[(dp - k) & 31];
            xp = (xp == 0 ? 32 : xp) - 1; xh[xp] = xh[xp + 32] = x;
            push (x); push (x);
        }
        stale = false;
    }
    inline void push (float v) { vp = (vp == 0 ? 64 : vp) - 1; vh[vp] = vh[vp + 64] = v; }
};

// First-order antiderivative anti-aliasing (ADAA) for the drive and the fold: y = (F(x) − F(x1)) / (x − x1), with the
// midpoint when the step is tiny. In double: the difference quotient needs the precision, the functions are cheap.
struct Adaa
{
    double x1 = 0.0, F1 = 0.0; bool primed = false;
    void reset() { x1 = 0.0; F1 = 0.0; primed = false; }
    template <typename Fn, typename AFn> inline double process (double x, Fn&& f, AFn&& F)
    {
        // The first sample after a reset has no previous point: F1 must be F of it, not 0 (F(0) is not 0 for the
        // folder: an unprimed state made a spike of thousands at every note start).
        if (! primed) { primed = true; x1 = x; F1 = F (x); return f (x); }
        const double Fx = F (x), d = x - x1;
        const double y = std::fabs (d) > 1e-5 ? (Fx - F1) / d : f (0.5 * (x + x1));
        x1 = x; F1 = Fx;
        return y;
    }
};

// Soft clipper with a polynomial knee (cubic): f(x) = x − x³/3 on |x| ≤ 1, ±2/3 outside; F its antiderivative.
inline double clipF (double x) { return x <= -1.0 ? -2.0 / 3.0 : (x >= 1.0 ? 2.0 / 3.0 : x - x * x * x / 3.0); }
inline double clipAF (double x)
{
    if (x <= -1.0) return -2.0 / 3.0 * x - 0.25;
    if (x >= 1.0) return 2.0 / 3.0 * x - 0.25;
    const double x2 = x * x;
    return 0.5 * x2 - x2 * x2 / 12.0;
}
// Triangle folder: maps x onto a triangle wave of period 4 (unity slope at 0); F its antiderivative (piecewise
// parabola). Folding several times is what makes the "fold" growl.
inline double foldF (double x)
{
    const double p = x + 1.0, m = p - 4.0 * std::floor (p * 0.25);    // m in [0, 4)
    return m < 2.0 ? m - 1.0 : 3.0 - m;
}
inline double foldAF (double x)
{
    const double p = x + 1.0, k = std::floor (p * 0.25), m = p - 4.0 * k;
    // ∫ over one period is 0, so F is periodic: F(m) on [0,2): (m−1)²/2 − 1/2; on [2,4): 1/2 − (3−m)²/2 ... continuous.
    return m < 2.0 ? 0.5 * (m - 1.0) * (m - 1.0) - 0.5 : 0.5 - 0.5 * (3.0 - m) * (3.0 - m);
}


// ── The drive and fold stage for the four lanes of a quad at once (float; the 4x histories are shared positions,
// one column per lane). The same filters as UpDown4; ADAA in float with a step threshold that keeps the difference
// quotient's rounding below −75 dB (the midpoint is used under it, where the quotient and the midpoint agree). ──
template <class V> DV_INLINE V clipF4 (V x) { const V c = V::min (V::max (x, V (-1.0f)), V (1.0f)); return c - c * c * c * V (1.0f / 3.0f); }
template <class V> DV_INLINE V clipAF4 (V x)
{
    const V c = V::min (V::max (x, V (-1.0f)), V (1.0f)), c2 = c * c;
    return V (0.5f) * c2 - c2 * c2 * V (1.0f / 12.0f) + V (2.0f / 3.0f) * (V::abs (x) - V::abs (c));
}
template <class V> DV_INLINE V foldM4 (V x) { const V p = x + V (1.0f); return p - V (4.0f) * V::floor (p * V (0.25f)); }
template <class V> DV_INLINE V foldF4 (V x) { const V m = foldM4 (x); return V::select (V::lt (m, V (2.0f)), m - V (1.0f), V (3.0f) - m); }
template <class V> DV_INLINE V foldAF4 (V x)
{
    const V m = foldM4 (x), a = m - V (1.0f), b = V (3.0f) - m;
    return V::select (V::lt (m, V (2.0f)), V (0.5f) * a * a - V (0.5f), V (0.5f) - V (0.5f) * b * b);
}
template <int L> struct AdaaQ
{
    alignas (16) float x1[L] {}, F1[L] {}, primed[L] {};
    template <class V, typename Fn, typename AFn> inline V process (V x, Fn&& f, AFn&& F)
    {
        const V X1 = V::load (x1), Fx = F (x), d = x - X1, pr = V::load (primed);
        const V big = V::lt (V (2.0e-3f), V::abs (d));
        const V q = (Fx - V::load (F1)) / V::select (big, d, V (1.0f));
        const V y = V::select (pr, V::select (big, q, f (V (0.5f) * (x + X1))), f (x));
        x.store (x1); Fx.store (F1); V (1.0f).store (primed);
        return y;
    }
    // Two consecutive samples: the second quotient needs only the first input, so both divisions run side by side.
    template <class V, typename Fn, typename AFn> inline void process2 (V& xa, V& xb, Fn&& f, AFn&& F)
    {
        const V X1 = V::load (x1), Fa = F (xa), Fb = F (xb), pr = V::load (primed);
        const V da = xa - X1, db = xb - xa;
        const V bigA = V::lt (V (2.0e-3f), V::abs (da)), bigB = V::lt (V (2.0e-3f), V::abs (db));
        const V qa = (Fa - V::load (F1)) / V::select (bigA, da, V (1.0f));
        const V qb = (Fb - Fa) / V::select (bigB, db, V (1.0f));
        const V ya = V::select (pr, V::select (bigA, qa, f (V (0.5f) * (xa + X1))), f (xa));
        const V yb = V::select (bigB, qb, f (V (0.5f) * (xb + xa)));
        xb.store (x1); Fb.store (F1); V (1.0f).store (primed);
        xa = ya; xb = yb;
    }
};
template <int L> struct UpDownQ
{
    alignas (16) float xh[64][L] {}, vh[128][L] {};
    int xp = 0, vp = 0;
    // Lane l's histories as if its last 32 inputs (oldest first) had gone through the bypass (UpDown4::rebuildFrom).
    void rebuildLane (int l, const float* last32)
    {
        for (int k = 0; k < 32; ++k) { const int i = (xp + k) & 31; xh[i][l] = xh[i + 32][l] = last32[31 - k]; }
        for (int k = 0; k < 64; ++k) { const int i = (vp + k) & 63; vh[i][l] = vh[i + 64][l] = last32[31 - k / 2]; }
    }
    template <class V, typename F> inline V process (V x, F&& f)
    {
        const auto& h = Hb33::get();
        xp = (xp == 0 ? 32 : xp) - 1; x.store (xh[xp]); x.store (xh[xp + 32]);
        const float (*X)[L] = xh + xp;
        const V y0 = V::load (X[8]);
        V y1a (0.0f), y1b (0.0f);
        for (int j = 0; j < 8; j += 2)
        {
            y1a = y1a + V (h.c[j]) * (V::load (X[8 + j]) + V::load (X[7 - j]));
            y1b = y1b + V (h.c[j + 1]) * (V::load (X[9 + j]) + V::load (X[6 - j]));
        }
        V a = y0, b = V (2.0f) * (y1a + y1b);
        f (a, b);                            // the nonlinearity on both 4x samples (in place)
        push (a); push (b);
        const float (*Vh)[L] = vh + vp;
        V aa = V (0.5f) * V::load (Vh[17]), ab (0.0f);
        for (int j = 0; j < 8; j += 2)
        {
            aa = aa + V (h.c[j]) * (V::load (Vh[18 + 2 * j]) + V::load (Vh[16 - 2 * j]));
            ab = ab + V (h.c[j + 1]) * (V::load (Vh[20 + 2 * j]) + V::load (Vh[14 - 2 * j]));
        }
        return aa + ab;
    }
    template <class V> inline void push (V v) { vp = (vp == 0 ? 64 : vp) - 1; v.store (vh[vp]); v.store (vh[vp + 64]); }
};

} // namespace dh::dv

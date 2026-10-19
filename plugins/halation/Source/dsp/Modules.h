#pragma once
// Módulos que rodean a la red: los que van dentro del lazo (shimmer, granos) y los que trabajan sobre la salida
// húmeda (reverse, espectral, resonador, gate, ducking, stutter). Todos deterministas y sin asignar memoria
// durante el proceso.

#include "Core.h"
#include <complex>

namespace dh::sp {

// ── Transpositor granular de dos cabezas (para el shimmer) ───────────────────────────────────────────────────
// Dos lecturas separadas medio grano recorren el búfer a `ratio` veces la velocidad de escritura; cada una con su
// ventana de Hann, que suman 1. Grano de 90 ms: suficiente para pads; en el lazo el defecto de fase se diluye.
class PitchShifter
{
public:
    void prepare (float sampleRate) { fs = sampleRate; buf.prepare ((int) (0.5f * fs)); grain = 0.09f * fs; reset(); }
    void reset() { buf.clear(); ph = 0.0f; }
    void setRatio (float r) { ratio = r; }
    inline float process (float x)
    {
        buf.push (x);
        // La fase avanza (1 − ratio) / grano por muestra: el retardo de cada cabeza recorre [grano, 0) o al revés.
        ph += (1.0f - ratio) / grain; ph -= std::floor (ph);
        float y = 0.0f;
        for (int k = 0; k < 2; ++k)
        {
            float p = ph + 0.5f * (float) k; p -= std::floor (p);
            const float w = 0.5f - 0.5f * std::cos (2.0f * kPi * p);
            y += w * buf.read (2.0f + p * grain);
        }
        return y;
    }
private:
    float fs = 48000.0f, grain = 4320.0f, ratio = 2.0f, ph = 0.0f;
    DelayLine buf;
};

// Intervalos del shimmer: hasta tres transposiciones sumadas (razones de frecuencia justas).
struct Interval { const char* name; int n; float ratio[3]; };
inline const std::array<Interval, 7>& intervals()
{
    static const std::array<Interval, 7> v {{
        { "Octave",        1, { 2.0f, 1.0f, 1.0f } },
        { "Fifth",         1, { 1.5f, 1.0f, 1.0f } },
        { "Octave + fifth",2, { 2.0f, 1.5f, 1.0f } },
        { "Fourth",        1, { 1.3333f, 1.0f, 1.0f } },
        { "Major chord",   3, { 1.25f, 1.5f, 2.0f } },
        { "Minor chord",   3, { 1.2f, 1.5f, 2.0f } },
        { "Octave down",   1, { 0.5f, 1.0f, 1.0f } },
    }};
    return v;
}

class Shimmer
{
public:
    void prepare (float fs) { for (auto& s : sh) s.prepare (fs); hp.setHz (180.0f, fs); }
    void reset() { for (auto& s : sh) s.reset(); hp.reset(); }
    void setInterval (int idx) { iv = intervals()[(size_t) std::clamp (idx, 0, (int) intervals().size() - 1)]; for (int k = 0; k < 3; ++k) sh[k].setRatio (iv.ratio[k]); }
    inline float process (float x)
    {
        x -= hp.lp (x);                                  // sin graves: el shimmer sube limpio
        float y = 0.0f;
        for (int k = 0; k < iv.n; ++k) y += sh[k].process (x);
        return y / std::sqrt ((float) iv.n);
    }
    int layers() const { return iv.n; }
private:
    PitchShifter sh[3];
    Interval iv = intervals()[0];
    OnePole hp;
};

// ── Granos en el lazo: un puñado de fragmentos de los últimos 2 s, en posiciones al azar ──────────────────────
class GrainCloud
{
public:
    static constexpr int kGrains = 12;
    void prepare (float sampleRate, uint32_t seed) { fs = sampleRate; buf.prepare ((int) (2.2f * fs)); seed0 = seed; reset(); }
    void reset() { buf.clear(); for (auto& g : gr) g.left = 0; next = 0; rng = Rand (seed0); }
    inline float process (float x, float density, float sizeMs)
    {
        buf.push (x);
        if (--next <= 0)
        {
            next = (int) (fs / std::max (1.0f, density * 40.0f) * (0.5f + rng.uni()));
            for (auto& g : gr) if (g.left <= 0)
            {
                g.len = std::max (32, (int) (sizeMs * 1e-3f * fs * (0.6f + 0.8f * rng.uni())));
                g.left = g.len; g.pos = 0.05f * fs + rng.uni() * 1.9f * fs; g.step = rng.uni() < 0.3f ? -1.0f : 1.0f;   // un tercio al revés
                break;
            }
        }
        float y = 0.0f;
        for (auto& g : gr) if (g.left > 0)
        {
            const float t = 1.0f - (float) g.left / (float) g.len;
            y += (0.5f - 0.5f * std::cos (2.0f * kPi * t)) * buf.read (g.pos);
            g.pos += 1.0f - g.step;  // retardo creciente = lectura hacia atrás en el tiempo… el retardo sube 2 por muestra si va al revés
            --g.left;
        }
        return y * 0.5f;
    }
    int active() const { int n = 0; for (auto& g : gr) n += g.left > 0; return n; }
private:
    struct G { int len = 0, left = 0; float pos = 0, step = 1; };
    std::array<G, kGrains> gr;
    float fs = 48000.0f; int next = 0; uint32_t seed0 = 1;
    DelayLine buf; Rand rng;
};

// ── Reverse: la salida húmeda en trozos tocados al revés (cada trozo crece hasta su final) ───────────────────
class Reverser
{
public:
    void prepare (float sampleRate) { fs = sampleRate; buf.prepare ((int) (4.2f * fs)); reset(); }
    void reset() { buf.clear(); pos = 0; }
    void setChunk (float ms) { chunk = std::max (0.05f * fs, std::min (4.0f * fs, ms * 1e-3f * fs)); }
    inline float process (float x)
    {
        buf.push (x);
        // Lectura: dentro de cada trozo, el retardo va de 0 a 2·trozo (lo más viejo se escucha último → al revés).
        const float t = (float) pos / chunk;
        const float d = 1.0f + 2.0f * (float) pos;
        const float w = std::min (1.0f, std::min (t, 1.0f - t) * 8.0f);  // fundidos en los bordes del trozo
        if (++pos >= (int) chunk) pos = 0;
        return w * buf.read (d);
    }
private:
    float fs = 48000.0f, chunk = 24000.0f; int pos = 0;
    DelayLine buf;
};

// ── Espectral: difuminado (las magnitudes se arrastran en el tiempo) y congelado (se sostienen), con fase al azar ─
class Spectral
{
public:
    static constexpr int kN = 2048, kHop = 512;
    void prepare (float sampleRate, uint32_t seed)
    {
        fs = sampleRate; seed0 = seed;
        win.resize (kN); for (int i = 0; i < kN; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * kPi * (float) i / kN);
        reset();
    }
    void reset()
    {
        inBuf.assign (kN, 0.0f); outBuf.assign (kN + kHop, 0.0f); mag.assign (kN / 2 + 1, 0.0f);
        fill = 0; outPos = 0; rng = Rand (seed0);
    }
    // smear 0..1 (cuánto se arrastran las magnitudes); freezeMask: bandas sostenidas (1 graves < 250 Hz,
    // 2 medios, 4 agudos > 2.5 kHz; 7 = todo).
    // live = true: los bins no congelados pasan tal cual (fase incluida), para congelar sólo una banda.
    inline float process (float x, float smear, int freezeMask, bool live = false)
    {
        inBuf[(size_t) (kN - kHop + fill)] = x;
        const float y = outBuf[(size_t) outPos];
        if (++outPos >= kHop) outPos = 0;
        if (++fill >= kHop) { fill = 0; frame (smear, freezeMask, live); }
        return y;
    }
    static constexpr int latency() { return kN; }
    // Inactivo: sólo guarda la entrada (así, al activarse, la primera ventana ya tiene historia).
    inline void feed (float x)
    {
        inBuf[(size_t) (kN - kHop + fill)] = x;
        if (++fill >= kHop) { fill = 0; for (int i = 0; i < kN - kHop; ++i) inBuf[(size_t) i] = inBuf[(size_t) (i + kHop)]; prevMask = 0; }
    }
private:
    int prevMask = 0;
    std::vector<float> win, inBuf, outBuf, mag;
    std::vector<std::complex<float>> spec = std::vector<std::complex<float>> (kN);
    int fill = 0, outPos = 0; Rand rng; float fs = 48000.0f; uint32_t seed0 = 1;
    void fft (std::vector<std::complex<float>>& a, bool inverse)
    {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
        for (size_t len = 2; len <= n; len <<= 1)
        {
            const float ang = (inverse ? 2.0f : -2.0f) * kPi / (float) len;
            const std::complex<float> wl (std::cos (ang), std::sin (ang));
            for (size_t i = 0; i < n; i += len)
            {
                std::complex<float> w (1.0f, 0.0f);
                for (size_t j = 0; j < len / 2; ++j) { const auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; }
            }
        }
    }
    void frame (float smear, int freezeMask, bool live)
    {
        for (int i = 0; i < kN; ++i) spec[(size_t) i] = { inBuf[(size_t) i] * win[(size_t) i], 0.0f };
        fft (spec, false);
        const float keep = 0.6f + 0.395f * clampf (smear, 0.0f, 1.0f);
        for (int k = 0; k <= kN / 2; ++k)
        {
            const float hz = (float) k * fs / kN;
            const int band = hz < 250.0f ? 1 : hz < 2500.0f ? 2 : 4;
            const bool freezeNow = (freezeMask & band) != 0;
            const float m = std::abs (spec[(size_t) k]);
            const float ph = 2.0f * kPi * rng.uni();
            if (freezeNow && ! (prevMask & band)) mag[(size_t) k] = m;               // recién congelada: captura
            if (live && ! freezeNow) { mag[(size_t) k] = m; }                         // pasa intacto
            else
            {
                mag[(size_t) k] = freezeNow ? mag[(size_t) k] : std::max (m, keep * mag[(size_t) k] + (1.0f - keep) * m);
                spec[(size_t) k] = std::polar (mag[(size_t) k], ph);
            }
            if (k > 0 && k < kN / 2) spec[(size_t) (kN - k)] = std::conj (spec[(size_t) k]);
        }
        prevMask = freezeMask;
        fft (spec, true);
        // Solapamiento-suma con ventana de síntesis (Hann² con salto N/4 suma 1.5 → normalización 2/3).
        for (int i = 0; i < kN - kHop; ++i) outBuf[(size_t) i] = outBuf[(size_t) (i + kHop)];
        for (int i = kN - kHop; i < kN; ++i) outBuf[(size_t) i] = 0.0f;
        for (int i = 0; i < kN; ++i) outBuf[(size_t) i] += spec[(size_t) i].real() / (float) kN * win[(size_t) i] * (2.0f / 3.0f);
        for (int i = 0; i < kN - kHop; ++i) inBuf[(size_t) i] = inBuf[(size_t) (i + kHop)];
    }
};

// ── Resonador: un peine realimentado por nota (hasta 4), afinado por MIDI o por la perilla ───────────────────
class Resonator
{
public:
    static constexpr int kMax = 4;
    void prepare (float sampleRate) { fs = sampleRate; for (auto& d : comb) d.prepare ((int) (fs / 20.0f) + 8); for (auto& l : lp) l.setHz (6000.0f, fs); reset(); }
    void reset() { for (auto& d : comb) d.clear(); for (auto& l : lp) l.reset(); }
    void setNotes (const float* midi, int n, float feedback)
    {
        count = std::clamp (n, 0, kMax); fb = clampf (feedback, 0.0f, 0.995f);
        for (int k = 0; k < count; ++k) period[k] = fs / (440.0f * std::exp2 ((midi[k] - 69.0f) / 12.0f));
    }
    inline float process (float x)
    {
        if (count == 0) return 0.0f;
        float y = 0.0f;
        for (int k = 0; k < count; ++k)
        {
            const float v = x + fb * lp[k].lp (comb[k].read (period[k]));
            comb[k].push (std::tanh (v));
            y += v;
        }
        return y * (1.0f - fb) * 2.0f / std::sqrt ((float) count);
    }
private:
    float fs = 48000.0f, fb = 0.9f, period[kMax] {};
    int count = 0;
    DelayLine comb[kMax]; OnePole lp[kMax];
};

// ── Seguidor de envolvente (para ducking, gate y modulación por envolvente) ─────────────────────────────────
struct Envelope
{
    float a = 0.0f, r = 0.0f, e = 0.0f;
    void set (float attackMs, float releaseMs, float fs) { a = std::exp (-1.0f / (attackMs * 1e-3f * fs)); r = std::exp (-1.0f / (releaseMs * 1e-3f * fs)); }
    inline float process (float x) { const float v = std::fabs (x); e = v > e ? a * e + (1.0f - a) * v : r * e + (1.0f - r) * v; return e; }
};

// ── Stutter: captura el último trozo de la salida y lo repite, a tempo y con probabilidad ───────────────────
// Durante la repetición k (0, 1, …) el retardo es trozo·(k + 1): siempre se lee el mismo trozo capturado.
class Stutter
{
public:
    void prepare (float sampleRate, uint32_t seed) { fs = sampleRate; bufL.prepare ((int) (4.2f * fs)); bufR.prepare ((int) (4.2f * fs)); seed0 = seed; reset(); }
    void reset() { bufL.clear(); bufR.clear(); active = false; pos = 0; rep = 0; beatPos = 0; rng = Rand (seed0); }
    // slice: largo del trozo (muestras); prob: chance de repetir en cada trozo; repeats: cuántas veces suena.
    // beatStart: true en la muestra donde empieza un trozo de la grilla del host (si no hay grilla, se cuenta solo).
    inline void process (float& l, float& r, int slice, float prob, int repeats, bool beatStart)
    {
        bufL.push (l); bufR.push (r);
        slice = std::clamp (slice, 64, (int) (0.5f * fs));
        repeats = std::clamp (repeats, 2, 8);
        bool start = beatStart;
        if (! start && ++beatPos >= slice) { beatPos = 0; start = true; }
        if (start && ! active && rng.uni() < prob) { active = true; len = slice; reps = repeats; rep = 0; pos = 0; }
        if (! active) return;
        const float d = (float) (len * (rep + 1));
        const float t = (float) pos / (float) len, w = std::min (1.0f, std::min (t, 1.0f - t) * 40.0f);
        l = w * bufL.read (d); r = w * bufR.read (d);
        if (++pos >= len) { pos = 0; if (++rep >= reps) active = false; }
    }
    bool isActive() const { return active; }
private:
    float fs = 48000.0f; bool active = false; int len = 1, reps = 2, rep = 0, pos = 0, beatPos = 0; uint32_t seed0 = 1;
    DelayLine bufL, bufR; Rand rng;
};

} // namespace dh::sp

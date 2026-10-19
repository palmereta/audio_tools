#pragma once
// Formant Sequence (Fseq): cuadros de ~7,9 ms (126 por segundo a velocidad 100 %) con la altura
// fundamental y la frecuencia y el nivel de 8 formantes con voz y 8 sin voz. Los operadores con el
// Fseq encendido toman la frecuencia y el nivel del cuadro en lugar de los suyos.
//
// El FS1R no podía crear ni editar Fseqs; el ATFS1 los genera de dos maneras:
//   • vowelSequence(): vocales con las frecuencias de formantes medidas por Peterson y Barney (1952);
//   • analyze(): cualquier grabación → altura (autocorrelación), formantes (LPC de orden 16 a 16 kHz,
//     picos de la envolvente) y ruido (energía aperiódica en 8 bandas).

#include "Common.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <string>
#include <vector>

namespace dh::fs {

constexpr float kFseqFps = 126.0f;

struct FseqFrame
{
    float pitch = 110.0f;                    // Hz
    std::array<float, 8> vFreq {}, nFreq {}; // Hz
    std::array<uint8_t, 8> vLevel {}, nLevel {};   // 0..127 (127 = 0 dB, 0,75 dB por paso)
};

struct FseqData
{
    std::string name = "Empty";
    std::vector<FseqFrame> frames;
    int loopStart = 0, loopEnd = 0, loopMode = 0;   // 0 one-way, 1 round (ida y vuelta)
    int noteAssign = 60;                             // nota que reproduce la altura grabada
    bool empty() const { return frames.empty(); }
};

inline float fseqLevelGain (uint8_t l) { return l == 0 ? 0.0f : fastExp2 (((float) l - 127.0f) * 0.75f * 0.16609640474f); }
inline uint8_t fseqLevelFromDb (float db) { return (uint8_t) clampf (std::round (127.0f + db / 0.75f), 0.0f, 127.0f); }

// ───────── reproducción ─────────
struct FseqPlayer
{
    float pos = 0.0f; int dir = 1; bool active = false, held = false;
    void start (int startFrame = 0) { pos = (float) startFrame; dir = 1; active = true; held = true; }
    void release() { held = false; }
    // Avanza dt segundos a la velocidad dada (1 = 100 %); devuelve el cuadro actual (sin interpolar, como el
    // original) o, con smooth, interpolado entre cuadros (extensión).
    // Lazo entre ls y le mientras la nota está apretada (mode 0 one-way, 1 round).
    void advance (const FseqData& d, int ls, int le, int mode, float dt, float speed)
    {
        if (! active || d.empty()) return;
        const int n = (int) d.frames.size();
        ls = std::clamp (ls, 0, n - 1); le = std::clamp (le, 0, n - 1);
        pos += (float) dir * dt * kFseqFps * speed;
        if (held && le > ls)
        {
            if (mode == 1) { if (pos >= (float) le) { pos = (float) le; dir = -1; } else if (pos <= (float) ls && dir < 0) { pos = (float) ls; dir = 1; } }
            else if (pos >= (float) le) pos = (float) ls + (pos - (float) le);
        }
        else if (dir < 0) dir = 1;
        if (pos >= (float) (n - 1)) { pos = (float) (n - 1); active = held && le > ls; }
    }
    FseqFrame frame (const FseqData& d, bool smooth) const
    {
        if (d.empty()) return {};
        const int n = (int) d.frames.size();
        const int i = std::clamp ((int) pos, 0, n - 1);
        if (! smooth || i >= n - 1) return d.frames[(size_t) i];
        const float t = pos - (float) i;
        const auto& a = d.frames[(size_t) i]; const auto& b = d.frames[(size_t) i + 1];
        FseqFrame f;
        auto lerpHz = [t] (float x, float y) { return x + (y - x) * t; };     // entre cuadros vecinos, lineal alcanza (y es barato)
        f.pitch = lerpHz (a.pitch, b.pitch);
        for (int k = 0; k < 8; ++k)
        {
            f.vFreq[(size_t) k] = lerpHz (a.vFreq[(size_t) k], b.vFreq[(size_t) k]);
            f.nFreq[(size_t) k] = lerpHz (a.nFreq[(size_t) k], b.nFreq[(size_t) k]);
            f.vLevel[(size_t) k] = (uint8_t) std::lround ((float) a.vLevel[(size_t) k] + ((float) b.vLevel[(size_t) k] - (float) a.vLevel[(size_t) k]) * t);
            f.nLevel[(size_t) k] = (uint8_t) std::lround ((float) a.nLevel[(size_t) k] + ((float) b.nLevel[(size_t) k] - (float) a.nLevel[(size_t) k]) * t);
        }
        return f;
    }
};

// ───────── vocales sintéticas ─────────
// Formantes F1..F5 (Hz) y niveles relativos (dB) de vocales de una voz masculina adulta; la voz femenina
// multiplica las frecuencias por 1,17. Fuente: Peterson y Barney, JASA 24 (1952), promedios; F4 y F5 estimados.
struct Vowel { char name; std::array<float, 5> f; std::array<float, 5> db; };
inline const std::array<Vowel, 7>& vowels()
{
    static const std::array<Vowel, 7> v { {
        { 'a', { 730, 1090, 2440, 3400, 4100 }, { 0, -5, -22, -28, -34 } },
        { 'e', { 530, 1840, 2480, 3500, 4200 }, { 0, -12, -18, -26, -32 } },
        { 'i', { 270, 2290, 3010, 3700, 4300 }, { 0, -18, -20, -26, -32 } },
        { 'o', { 570, 840, 2410, 3400, 4100 }, { 0, -2, -26, -30, -36 } },
        { 'u', { 300, 870, 2240, 3300, 4000 }, { 0, -12, -32, -36, -40 } },
        { 'y', { 280, 1650, 2190, 3300, 4100 }, { 0, -14, -22, -28, -34 } },     // /y/ (ü)
        { 'm', { 250, 1200, 2200, 3200, 4000 }, { -6, -30, -36, -40, -44 } },    // nasal cerrada, para "hum"
    } };
    return v;
}
inline const Vowel& vowel (char c) { for (auto& v : vowels()) if (v.name == c) return v; return vowels()[0]; }

// text: secuencia de vocales y silencios ("aeiou", "a-o-u", "m"); secsPer: duración de cada una;
// glide: fracción de transición. pitchHz: altura de partida; vibrato en semitonos.
inline FseqData vowelSequence (const std::string& name, const std::string& text, float secsPer, float glide, float pitchHz,
                               float female = 1.0f, float vibratoSemis = 0.0f, float breath = 0.0f)
{
    FseqData d; d.name = name;
    const int per = std::max (2, (int) std::lround (secsPer * kFseqFps));
    const int n = (int) text.size() * per;
    d.frames.resize ((size_t) n);
    auto frameFor = [&] (char c, float level) {
        FseqFrame f;
        if (c == '-') { for (int k = 0; k < 8; ++k) { f.vLevel[(size_t) k] = 0; f.nLevel[(size_t) k] = 0; f.vFreq[(size_t) k] = 500.0f * (float) (k + 1); f.nFreq[(size_t) k] = 2500.0f + 900.0f * (float) k; } return f; }
        const auto& v = vowel (c);
        for (int k = 0; k < 8; ++k)
        {
            const float fr = k < 5 ? v.f[(size_t) k] * female : (4600.0f + 700.0f * (float) (k - 5)) * female;
            const float db = (k < 5 ? v.db[(size_t) k] : -42.0f - 4.0f * (float) (k - 5)) + level;
            f.vFreq[(size_t) k] = fr; f.vLevel[(size_t) k] = fseqLevelFromDb (db);
            f.nFreq[(size_t) k] = 1800.0f + 1100.0f * (float) k; f.nLevel[(size_t) k] = fseqLevelFromDb (breath > 0 ? (-30.0f + 20.0f * std::log10 (breath + 1e-3f)) - 3.0f * (float) k : -96.0f);
        }
        return f;
    };
    for (int i = 0; i < n; ++i)
    {
        const int seg = i / per;
        const float t = (float) (i % per) / (float) per;
        FseqFrame a = frameFor (text[(size_t) seg], 0.0f);
        if (seg + 1 < (int) text.size() && t > 1.0f - glide)
        {
            const FseqFrame b = frameFor (text[(size_t) seg + 1], 0.0f);
            const float u = (t - (1.0f - glide)) / glide;
            const float s = u * u * (3.0f - 2.0f * u);
            for (int k = 0; k < 8; ++k)
            {
                a.vFreq[(size_t) k] = a.vFreq[(size_t) k] * std::pow (b.vFreq[(size_t) k] / a.vFreq[(size_t) k], s);
                a.vLevel[(size_t) k] = (uint8_t) std::lround ((float) a.vLevel[(size_t) k] * (1.0f - s) + (float) b.vLevel[(size_t) k] * s);
                a.nLevel[(size_t) k] = (uint8_t) std::lround ((float) a.nLevel[(size_t) k] * (1.0f - s) + (float) b.nLevel[(size_t) k] * s);
            }
        }
        const float tt = (float) i / kFseqFps;
        a.pitch = pitchHz * std::pow (2.0f, vibratoSemis * std::sin (kTwoPi * 5.2f * tt) * std::min (1.0f, tt * 1.5f) / 12.0f);
        d.frames[(size_t) i] = a;
    }
    d.loopStart = std::max (0, n - per); d.loopEnd = n - 1; d.loopMode = 1;
    d.noteAssign = (int) std::lround (69.0f + 12.0f * std::log2 (pitchHz / 440.0f));
    return d;
}

// ───────── análisis de una grabación ─────────
namespace detail {
inline void fft (std::vector<std::complex<float>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const float ang = -2.0f * 3.14159265f / (float) len;
        const std::complex<float> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<float> w (1.0f, 0.0f);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl;
            }
        }
    }
}
// Levinson-Durbin: coeficientes LPC a partir de la autocorrelación.
inline std::vector<float> lpc (const std::vector<float>& x, int order)
{
    std::vector<double> r ((size_t) order + 1, 0.0);
    for (int k = 0; k <= order; ++k) for (size_t i = (size_t) k; i < x.size(); ++i) r[(size_t) k] += (double) x[i] * x[i - (size_t) k];
    r[0] *= 1.0 + 1e-9; r[0] += 1e-12;
    std::vector<double> a ((size_t) order + 1, 0.0), tmp ((size_t) order + 1, 0.0);
    a[0] = 1.0; double e = r[0];
    for (int i = 1; i <= order; ++i)
    {
        double acc = r[(size_t) i];
        for (int j = 1; j < i; ++j) acc += a[(size_t) j] * r[(size_t) (i - j)];
        const double k = -acc / e;
        tmp = a;
        for (int j = 1; j < i; ++j) a[(size_t) j] = tmp[(size_t) j] + k * tmp[(size_t) (i - j)];
        a[(size_t) i] = k;
        e *= (1.0 - k * k);
        if (e <= 0) break;
    }
    std::vector<float> out ((size_t) order + 1);
    for (int i = 0; i <= order; ++i) out[(size_t) i] = (float) a[(size_t) i];
    return out;
}
// Raíces de z^n + a1·z^(n−1) + … + an (a[0] = 1) por Durand-Kerner.
inline std::vector<std::complex<double>> polyRoots (const std::vector<float>& a)
{
    const int n = (int) a.size() - 1;
    std::vector<std::complex<double>> z ((size_t) n);
    const std::complex<double> seed (0.4, 0.9);
    for (int i = 0; i < n; ++i) z[(size_t) i] = std::pow (seed, i) * 0.9;
    auto p = [&] (std::complex<double> x) { std::complex<double> y (1.0, 0.0); for (int k = 1; k <= n; ++k) y = y * x + (double) a[(size_t) k]; return y; };
    for (int it = 0; it < 300; ++it)
    {
        double moved = 0.0;
        for (int i = 0; i < n; ++i)
        {
            std::complex<double> d (1.0, 0.0);
            for (int j = 0; j < n; ++j) if (j != i) d *= z[(size_t) i] - z[(size_t) j];
            if (std::abs (d) < 1e-300) d = 1e-12;
            const auto step = p (z[(size_t) i]) / d;
            z[(size_t) i] -= step;
            moved = std::max (moved, std::abs (step));
        }
        if (moved < 1e-10) break;
    }
    return z;
}
} // namespace detail

// mono: muestras a fs. Devuelve un Fseq de hasta maxFrames cuadros.
inline FseqData analyze (const std::vector<float>& mono, float fs, const std::string& name, int maxFrames = 512)
{
    using namespace detail;
    FseqData d; d.name = name;
    if (mono.size() < 2048) return d;
    // Decimación a ~16 kHz con un FIR de ventana (sólo para la altura y los formantes).
    const int dec = std::max (1, (int) std::lround (fs / 16000.0f));
    const float fs16 = fs / (float) dec;
    std::vector<float> h (63);
    for (int i = 0; i < 63; ++i)
    {
        const float t = (float) (i - 31);
        const float sinc = i == 31 ? 1.0f : std::sin (3.14159265f * t / (float) dec) / (3.14159265f * t / (float) dec);
        h[(size_t) i] = sinc * (0.54f - 0.46f * std::cos (2.0f * 3.14159265f * (float) i / 62.0f)) / (float) dec;
    }
    std::vector<float> x16 (mono.size() / (size_t) dec);
    for (size_t i = 0; i < x16.size(); ++i)
    {
        float s = 0.0f;
        for (int k = 0; k < 63; ++k) { const long j = (long) (i * (size_t) dec) + k - 31; if (j >= 0 && j < (long) mono.size()) s += h[(size_t) k] * mono[(size_t) j]; }
        x16[i] = s;
    }
    const float hop = fs16 / kFseqFps;
    const int win = (int) (0.032f * fs16);
    if (x16.size() <= (size_t) win + 2) return d;     // grabación más corta que una ventana (la resta de abajo daba la vuelta)
    const int frames = std::min (maxFrames, (int) ((float) (x16.size() - (size_t) win) / hop));
    if (frames <= 2) return d;
    d.frames.resize ((size_t) frames);
    float peakDb = -200.0f;
    std::vector<std::array<float, 8>> vDb ((size_t) frames), nDb ((size_t) frames);
    float lastPitch = 120.0f;
    const int nfft = 2048;
    for (int fi = 0; fi < frames; ++fi)
    {
        const size_t s0 = (size_t) (fi * hop);
        std::vector<float> fr ((size_t) win);
        double energy = 0.0;
        for (int i = 0; i < win; ++i)
        {
            const float w = 0.54f - 0.46f * std::cos (2.0f * 3.14159265f * (float) i / (float) (win - 1));
            fr[(size_t) i] = x16[s0 + (size_t) i] * w;
            energy += (double) fr[(size_t) i] * fr[(size_t) i];
        }
        const float rms = (float) std::sqrt (energy / win) + 1e-9f;
        // Altura: autocorrelación normalizada entre 60 y 500 Hz.
        const int lagMin = (int) (fs16 / 500.0f), lagMax = std::min (win - 2, (int) (fs16 / 60.0f));
        float best = 0.0f; int bestLag = 0;
        for (int lag = lagMin; lag <= lagMax; ++lag)
        {
            double num = 0, e1 = 0, e2 = 0;
            for (int i = 0; i + lag < win; ++i) { num += (double) fr[(size_t) i] * fr[(size_t) (i + lag)]; e1 += (double) fr[(size_t) i] * fr[(size_t) i]; e2 += (double) fr[(size_t) (i + lag)] * fr[(size_t) (i + lag)]; }
            const float c = (float) (num / std::sqrt (e1 * e2 + 1e-20));
            if (c > best) { best = c; bestLag = lag; }
        }
        const float voicing = clampf ((best - 0.35f) / 0.4f, 0.0f, 1.0f);
        if (voicing > 0.3f && bestLag > 0) lastPitch = fs16 / (float) bestLag;
        auto& F = d.frames[(size_t) fi];
        F.pitch = lastPitch;
        // Formantes: raíces del polinomio LPC (con preénfasis 0,94 para que el modelo vea los formantes altos).
        // Cada polo complejo da frecuencia y ancho de banda; quedan los angostos (< 500 Hz). El nivel de cada uno
        // es la envolvente LPC en su frecuencia sin el realce de la preénfasis, relativo al más fuerte del cuadro.
        const int order = std::clamp ((int) std::lround (fs16 / 1000.0f) + 4, 12, 28);
        std::vector<float> pe ((size_t) win);
        pe[0] = fr[0];
        for (int i = 1; i < win; ++i) pe[(size_t) i] = fr[(size_t) i] - 0.94f * fr[(size_t) i - 1];
        const auto a = lpc (pe, order);
        auto envDb = [&] (float hz) {
            const double w = 2.0 * 3.14159265358979 * hz / fs16;
            std::complex<double> z (0.0, 0.0);
            for (int k = 0; k <= order; ++k) z += (double) a[(size_t) k] * std::polar (1.0, -w * k);
            const double pre = std::abs (1.0 - 0.94 * std::polar (1.0, -w));
            return (float) (-20.0 * std::log10 (std::abs (z) + 1e-12) - 20.0 * std::log10 (pre + 1e-9));
        };
        std::vector<std::pair<float, float>> fm;          // Hz, nivel de la envolvente
        for (const auto& r : polyRoots (a))
        {
            if (r.imag() <= 0.0) continue;
            const float f = (float) (std::arg (r) * fs16 / (2.0 * 3.14159265358979));
            const float bw = (float) (-std::log (std::abs (r)) * fs16 / 3.14159265358979);
            if (f > 90.0f && f < fs16 * 0.5f - 150.0f && bw < 500.0f) fm.push_back ({ f, 0.0f });
        }
        std::sort (fm.begin(), fm.end());
        float strongest = -1e9f;
        for (auto& [f, l] : fm) { l = envDb (f); strongest = std::max (strongest, l); }
        const float levDb = 20.0f * std::log10 (rms * voicing + 1e-9f);
        for (int k = 0; k < 8; ++k)
        {
            if (k < (int) fm.size())
            {
                F.vFreq[(size_t) k] = fm[(size_t) k].first;
                vDb[(size_t) fi][(size_t) k] = levDb + fm[(size_t) k].second - strongest;
            }
            else { F.vFreq[(size_t) k] = k > 0 ? std::min (fs16 * 0.48f, F.vFreq[(size_t) k - 1] * 1.25f) : 500.0f; vDb[(size_t) fi][(size_t) k] = -200.0f; }
        }
        // Ruido: energía en 8 bandas del espectro a la frecuencia original, pesada por la aperiodicidad.
        std::vector<std::complex<float>> spec ((size_t) nfft);
        const size_t s48 = (size_t) ((float) s0 * (float) dec);
        const int win48 = std::min (nfft, win * dec);
        for (int i = 0; i < win48 && s48 + (size_t) i < mono.size(); ++i)
            spec[(size_t) i] = mono[s48 + (size_t) i] * (0.5f - 0.5f * std::cos (2.0f * 3.14159265f * (float) i / (float) (win48 - 1)));
        fft (spec);
        static const float BANDS[9] = { 1500, 2200, 3000, 4000, 5200, 6600, 8200, 10500, 14000 };
        for (int k = 0; k < 8; ++k)
        {
            const int b0 = (int) (BANDS[k] / fs * nfft), b1 = std::min (nfft / 2, (int) (BANDS[k + 1] / fs * nfft));
            double e = 0.0;
            for (int b = b0; b < b1; ++b) e += (double) std::norm (spec[(size_t) b]);
            const float bandDb = 10.0f * std::log10 ((float) (e / std::max (1, b1 - b0)) + 1e-12f) - 24.0f;
            F.nFreq[(size_t) k] = std::sqrt (BANDS[k] * BANDS[k + 1]);
            nDb[(size_t) fi][(size_t) k] = bandDb + 20.0f * std::log10 (std::max (0.02f, 1.0f - voicing)) - 6.0f;   // en lo sonoro, las bandas son armónicos: casi nada de ruido
        }
        for (int k = 0; k < 8; ++k) peakDb = std::max ({ peakDb, vDb[(size_t) fi][(size_t) k], nDb[(size_t) fi][(size_t) k] });
    }
    // Niveles relativos al pico de la grabación.
    for (int fi = 0; fi < frames; ++fi)
        for (int k = 0; k < 8; ++k)
        {
            d.frames[(size_t) fi].vLevel[(size_t) k] = fseqLevelFromDb (vDb[(size_t) fi][(size_t) k] - peakDb);
            d.frames[(size_t) fi].nLevel[(size_t) k] = fseqLevelFromDb (nDb[(size_t) fi][(size_t) k] - peakDb - 6.0f);
        }
    float pitchSum = 0.0f; int pitchN = 0;
    for (auto& f : d.frames) if (f.vLevel[0] > 40) { pitchSum += std::log2 (f.pitch); ++pitchN; }
    const float meanPitch = pitchN > 0 ? std::exp2 (pitchSum / (float) pitchN) : 120.0f;
    d.noteAssign = (int) std::lround (69.0f + 12.0f * std::log2 (meanPitch / 440.0f));
    d.loopStart = d.loopEnd = 0; d.loopMode = 0;
    return d;
}

} // namespace dh::fs

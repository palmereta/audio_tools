#pragma once
// Efector doble del AT42: un motor por canal (L y R) que carga un programa de un cartucho.
// 13 cartuchos × 3 programas con X, Y, Z compartidos (perilla + CV). Los algoritmos son propios
// (el original usa un DSP de cartuchos; ver docs/hardware-spec.md §3.10 y §8.5).
//
// Carácter del motor original: memoria de delay limitada (~1,3 s), un pasabajos de reconstrucción
// cerca de 14 kHz y una pizca de ruido digital (opcional).

#include "Common.h"
#include <vector>

namespace dh {

struct DelayLine42
{
    std::vector<float> buf;
    int w = 0;
    void prepare (int n) { buf.assign ((size_t) std::max (16, n), 0.0f); w = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) % (int) buf.size(); }
    inline float tap (float d) const
    {
        const int n = (int) buf.size();
        d = clampf (d, 1.0f, (float) (n - 2));
        float rp = (float) w - d;
        while (rp < 0.0f) rp += (float) n;
        const int i0 = (int) rp; const float fr = rp - (float) i0;
        const float a = buf[(size_t) (i0 % n)], b = buf[(size_t) ((i0 + 1) % n)];
        return a + (b - a) * fr;
    }
};

// Pitch shifter de dos lecturas que barren la memoria con ventana cruzada.
struct Shifter42
{
    DelayLine42 dl;
    float ph = 0.0f, win = 2400.0f;
    void prepare (float fs) { dl.prepare ((int) (fs * 0.2f)); win = fs * 0.045f; }
    inline float process (float x, float ratio)
    {
        dl.push (x);
        ph += (1.0f - ratio) / win;
        ph -= std::floor (ph);
        const float p2 = ph + 0.5f - std::floor (ph + 0.5f);
        const float a = dl.tap (4.0f + ph * win), b = dl.tap (4.0f + p2 * win);
        const float ga = std::sin (3.14159265f * ph), gb = std::sin (3.14159265f * p2);
        return a * ga + b * gb;
    }
};

// Reverb mono: difusión de entrada + red de 4 líneas con matriz de Householder.
struct Verb42
{
    DelayLine42 ap[3], line[4];
    float lp[4] {}, apLen[3] {}, len[4] {};
    float fs = 48000.0f, modPh = 0.0f;
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        const float k = fs / 48000.0f;
        const float a[3] = { 142, 379, 277 }, l[4] = { 1557, 1617, 1491, 1422 };
        for (int i = 0; i < 3; ++i) { apLen[i] = a[i] * k; ap[i].prepare ((int) apLen[i] + 4); }
        for (int i = 0; i < 4; ++i) { len[i] = l[i] * k * 2.2f; line[i].prepare ((int) (len[i] * 1.6f) + 64); }
    }
    void clear() { for (auto& d : ap) d.clear(); for (auto& d : line) d.clear(); for (auto& v : lp) v = 0.0f; }
    // size 0..1 (largo), decay 0..1 (cola), damp 0..1.
    inline float process (float x, float size, float decay, float damp)
    {
        for (int i = 0; i < 3; ++i)
        {
            const float d = ap[i].tap (apLen[i]);
            const float v = x + 0.6f * d;
            ap[i].push (v);
            x = d - 0.6f * v;
        }
        modPh += 0.7f / fs; if (modPh >= 1.0f) modPh -= 1.0f;
        const float sc = 0.45f + 0.55f * size;
        float o[4];
        for (int i = 0; i < 4; ++i)
            o[i] = line[i].tap (len[i] * sc + (i == 0 ? 6.0f * std::sin (6.2831853f * modPh) : 0.0f));
        const float g = 0.55f + 0.44f * clampf (decay, 0.0f, 1.0f);
        const float s = 0.5f * (o[0] + o[1] + o[2] + o[3]);
        const float dc = 0.15f + 0.8f * damp;
        for (int i = 0; i < 4; ++i)
        {
            float f = (o[i] - s) * g;
            lp[i] += (f - lp[i]) * (1.0f - dc);
            line[i].push (x + lp[i]);
        }
        return 0.5f * (o[0] - o[1] + o[2] - o[3]);
    }
};

struct Svf42
{
    float s1 = 0, s2 = 0;
    // Devuelve lp; bp y hp por referencia.
    inline float process (float x, float fc, float q, float fs, float& bp, float& hp)
    {
        const float g = fastTan (3.14159265f * clampf (fc, 10.0f, 0.45f * fs) / fs), k = 1.0f / std::max (0.3f, q);
        hp = (x - (k + g) * s1 - s2) / (1.0f + g * (k + g));
        const float v1 = g * hp; bp = v1 + s1; s1 = v1 + bp;
        const float v2 = g * bp; const float lp = v2 + s2; s2 = v2 + lp;
        return lp;
    }
};

class EffectEngine42
{
public:
    enum Cart { Cathedral = 0, Magic, Time, Vibrotrem, Filter, Vibe, PitchShifter, Infinity, StringRinger, Syntex, Digital, Generator, Ochre };
    static constexpr int kCarts = 13;

    void setMismatch (float m) { mis = m; }
    void prepare (float sampleRate, float mismatch)
    {
        fs = sampleRate; mis = mismatch;
        dl.prepare ((int) (fs * 1.4f)); dl2.prepare ((int) (fs * 1.4f));
        rev.prepare (fs); sh1.prepare (fs); sh2.prepare (fs);
        mod.prepare ((int) (fs * 0.05f));
        rec.prepare ((int) (fs * 4.0f));      // la reversa lee hasta 2 × 1,9 s atrás
        reset();
    }

    void reset()
    {
        dl.clear(); dl2.clear(); rev.clear(); mod.clear(); rec.clear();
        for (auto& a : aps) a = 0.0f;
        fb1 = fb2 = env = held = lfo = ph1 = ph2 = 0.0f; revPh = 0.0f; revCount = 0; crushCount = 0; crushHold = 0.0f;
        sv1 = sv2 = {};
    }

    void load (int cart, int prog)
    {
        if (cart != curCart || prog != curProg) { curCart = cart; curProg = prog; reset(); }
    }

    // x, y, z en 0..1 (perilla + CV); devuelve la señal húmeda.
    inline float process (float in, float x, float y, float z)
    {
        x = clampf (x * mis, 0.0f, 1.0f);
        float out = 0.0f;
        switch (curCart * 3 + curProg)
        {
            // ── CATHEDRAL ──
            case 0: { // Shimmer: octava arriba, octava abajo, decaimiento
                const float wet = rev.process (in + fb1, 0.9f, 0.5f + 0.42f * z, 0.35f);
                // La octava vuelve a la reverb con ganancia < 1 y saturada: brilla sin desbocarse.
                fb1 = softClip ((sh1.process (wet, 2.0f) * x + sh2.process (wet, 0.5f) * y) * 0.32f);
                out = wet * 1.3f; break; }
            case 1: { // Delay con octava arriba en la realimentación, más reverb
                const float d = dl.tap (ms (40.0f + 900.0f * y));
                dl.push (in + softClip (sh1.process (d, 2.0f) * x * 0.95f));
                out = d + rev.process (d, 0.8f, 0.7f, 0.4f) * z; break; }
            case 2: { // Space reverb: delay → reverb grande
                const float d = dl.tap (ms (20.0f + 700.0f * y));
                dl.push (in + d * x * 0.9f);
                out = 0.6f * d + rev.process (in * 0.5f + d * 0.5f, 1.0f, 0.6f + 0.39f * z, 0.3f) * 1.2f; break; }
            // ── MAGIC ──
            case 3: case 5: { // Pitch delay / Bell pitch delay
                const float ratio = fastExp2 ((z - 0.5f) * 2.0f);
                const float d = dl.tap (ms (30.0f + 950.0f * y));
                float f = sh1.process (d, ratio);
                if (curProg == 2) f = allpassChain (f, 0.7f);
                dl.push (in + softClip (f * x * 0.97f));
                out = d; break; }
            case 4: { // Reverse pitch delay
                const float len = fs * (0.08f + 0.9f * y);
                rec.push (in + fb1);
                revPh += 1.0f; if (revPh >= len) revPh = 0.0f;
                const float g = std::sin (3.14159265f * revPh / len);
                const float r = rec.tap (2.0f * revPh + 2.0f) * g;
                fb1 = softClip (sh1.process (r, fastExp2 ((z - 0.5f) * 2.0f)) * x * 0.85f);
                out = r; break; }
            // ── TIME ──
            case 6: { // Delay + reverb
                const float d = dl.tap (ms (20.0f + 1000.0f * y));
                dl.push (in + d * x * 0.95f);
                out = d + rev.process (d + in * 0.3f, 0.75f, 0.6f, 0.45f) * z; break; }
            case 7: case 8: { // Delay chorus / delay vibrato
                lfo = sine (curProg == 1 ? 0.6f : 0.5f + 7.0f * y);
                const float dly = curProg == 1 ? ms (30.0f + 800.0f * y) : ms (240.0f);
                const float d = dl.tap (dly + z * ms (curProg == 1 ? 6.0f : 4.0f) * (1.0f + lfo));
                dl.push (in + d * x * 0.92f);
                out = d; break; }
            // ── VIBROTREM ──
            case 9: { const float t = 1.0f - x * 0.5f * (1.0f + sine (0.5f + 12.0f * y));
                      out = in * t + rev.process (in * t, 0.6f, 0.5f, 0.5f) * z; break; }
            case 10: { mod.push (in); out = mod.tap (ms (5.0f) + ms (4.0f) * x * (1.0f + sine (0.3f + 9.0f * y)));
                       out += rev.process (out, 0.6f, 0.5f, 0.5f) * z; break; }
            case 11: { mod.push (in); out = 0.6f * in + 0.7f * mod.tap (ms (12.0f) + ms (7.0f) * x * (1.0f + sine (0.1f + 3.0f * y)));
                       out += rev.process (out, 0.6f, 0.5f, 0.5f) * z; break; }
            // ── FILTER ──
            case 12: { // Auto-wah: el filtro sigue la envolvente
                env += (std::fabs (in) - env) * (std::fabs (in) > env ? 0.01f : 0.0008f);
                float bp, hp; sv1.process (in, 200.0f * fastExp2 (x * 2.0f + y * 6.0f * clampf (env * 4.0f, 0.0f, 1.0f)), 5.0f, fs, bp, hp);
                out = bp * 1.6f + rev.process (bp, 0.6f, 0.5f, 0.5f) * z; break; }
            case 13: { float bp, hp, bp2, hp2;
                       sv1.process (in, 20.0f * std::pow (400.0f, x), 0.6f + 6.0f * z, fs, bp, hp);
                       out = sv2.process (hp, 200.0f * std::pow (100.0f, y), 0.6f + 6.0f * z, fs, bp2, hp2); break; }
            case 14: { float bp, hp, bp2, hp2;
                       const float a = sv1.process (in, 60.0f * std::pow (200.0f, x), 0.4f + 4.0f * z, fs, bp, hp);
                       const float n1 = a + hp;
                       const float b = sv2.process (n1, 60.0f * std::pow (200.0f, y), 0.4f + 4.0f * z, fs, bp2, hp2);
                       out = b + hp2; break; }
            // ── VIBE ──
            case 15: { // Phaser de 6 etapas
                const float c = 0.2f + 0.75f * (0.5f + 0.5f * sine (0.05f + 4.0f * y)) * x;
                float v = in + fb1 * 0.6f;
                for (int i = 0; i < 6; ++i) { const float o = aps[i] - c * v; aps[i] = v + c * o; v = o; }
                fb1 = v; out = 0.5f * (in + v); out += rev.process (out, 0.6f, 0.5f, 0.5f) * z; break; }
            case 16: case 17: { // Flanger / flanger resonante
                mod.push (in + fb1);
                const float depth = curProg == 1 ? x : z;
                const float d = mod.tap (ms (0.4f) + ms (4.5f) * depth * (0.5f + 0.5f * sine (0.05f + 3.0f * y)));
                fb1 = d * (curProg == 2 ? 0.95f * x : 0.5f);
                out = 0.5f * (in + d);
                if (curProg == 1) out += rev.process (out, 0.6f, 0.5f, 0.5f) * z;
                break; }
            // ── PITCH SHIFTER ──
            case 18: case 19: { // SynthTaver / Octaver
                const float dn = sh1.process (in, 0.5f), up = sh2.process (in, 2.0f);
                float sub = dn;
                if (curProg == 0) { float bp, hp; sub = sv1.process (squareOf (dn), 300.0f, 1.2f, fs, bp, hp) * 0.4f; }
                out = sub * x * 1.4f + up * y + in * z; break; }
            case 20: { // Harmonizador de dos voces
                const float r1 = fastExp2 (std::round ((x - 0.5f) * 24.0f) / 12.0f), r2 = fastExp2 (std::round ((y - 0.5f) * 24.0f) / 12.0f);
                out = in * (1.0f - z) + (sh1.process (in, r1) + sh2.process (in, r2)) * z * 0.8f; break; }
            // ── INFINITY ──
            case 21: { // Reverb resonante con predelay modulado
                lfo = sine (0.2f + 2.0f * y);
                const float d = dl.tap (ms (5.0f + 400.0f * x) * (1.0f + 0.05f * y * lfo));
                dl.push (in);
                out = rev.process (d, 1.0f, 0.75f + 0.245f * z, 0.2f) * 1.5f; break; }
            case 22: { // O.D.D.: delay que oscila y se ensucia
                const float d = dl.tap (ms (15.0f + 900.0f * y));
                const float f = sh1.process (d, fastExp2 ((z - 0.5f) * 2.0f));
                dl.push (fastTanh ((in + f * (0.3f + 0.9f * x)) * 1.4f));
                out = d; break; }
            case 23: { // Delay resonante (peine corto afinado)
                const float hz = 40.0f * fastExp2 (z * 6.0f);
                const float d = dl.tap (std::max (2.0f, fs / hz) * (0.25f + 0.75f * y));
                dl.push (in + softClip (d * (0.6f + 0.39f * x)));
                out = d; break; }
            // ── STRING RINGER ──
            case 24: { // Anillo sintético: ring + peine resonante + sub
                const float car = sine2 (50.0f * fastExp2 (x * 6.0f));
                const float r = in * car;
                const float d = dl.tap (fs / (60.0f * fastExp2 (x * 5.0f)));
                dl.push (r + d * 0.9f * y);
                out = d + squareOf (sh1.process (in, 0.5f)) * z * 0.3f; break; }
            case 25: { const float hz = 30.0f * fastExp2 (x * 7.0f) * (1.0f + 0.3f * sine (0.1f + 6.0f * y));
                       out = in * sine2 (hz); out += rev.process (out, 0.6f, 0.5f, 0.5f) * z; break; }
            case 26: { // Ring con sample & hold de la frecuencia
                lfo += (0.2f + 20.0f * y) / fs;
                if (lfo >= 1.0f) { lfo -= 1.0f; held = rng.unipolar(); }
                const float target = 50.0f * fastExp2 (held * 6.0f * z + x * 2.0f);
                fb2 += (target - fb2) * (0.0005f + x * 0.02f);
                out = in * sine2 (fb2); break; }
            // ── SYNTEX-1 (sinte de bajo desde la entrada) ──
            case 27: case 28: case 29: {
                const float sq = squareOf (in);
                const float subv = squareOf (sh1.process (sq, 0.5f));
                float bp, hp;
                float cutoff = 300.0f;
                if (curProg == 0) cutoff = 200.0f * fastExp2 (2.0f + 1.5f * sine (0.2f + 7.0f * x));
                else if (curProg == 1) cutoff = 1200.0f;
                else { env += (std::fabs (in) - env) * (std::fabs (in) > env ? 0.02f : 0.0006f); cutoff = 80.0f * fastExp2 (x * 6.0f + 3.0f * y * env); }
                float o = sv1.process (sq, cutoff, 0.7f + 8.0f * (curProg == 2 ? y : y), fs, bp, hp);
                if (curProg == 1) o *= 0.5f + 0.5f * (sine (0.3f + 10.0f * x) > 0.0f ? 1.0f : 0.0f);
                envGate += (std::fabs (in) - envGate) * 0.002f;
                out = (o * 0.6f + subv * z * 0.5f) * clampf (envGate * 8.0f, 0.0f, 1.0f); break; }
            // ── DIGITAL ──
            case 30: case 31: case 32: {
                float rate = 400.0f * std::pow (60.0f, 1.0f - x);
                if (curProg == 1) rate *= 1.0f + 0.8f * z * sine (0.1f + 8.0f * y);
                if (curProg == 2) { env += (std::fabs (in) - env) * 0.01f; rate *= 1.0f - 0.9f * y * clampf (env * 5.0f, 0.0f, 1.0f); }
                const int hold = std::max (1, (int) (fs / std::max (100.0f, rate)));
                const float g = curProg == 1 ? 1.0f : 1.0f + (curProg == 0 ? z : z) * 6.0f;
                if (++crushCount >= hold) { crushCount = 0; crushHold = std::round (softClip (in * g) * 24.0f) / 24.0f; }
                float bp, hp;
                out = curProg == 0 ? sv1.process (crushHold, 200.0f * std::pow (80.0f, y), 0.8f, fs, bp, hp) : crushHold; break; }
            // ── GENERATOR (genera sonido; la entrada lo dispara) ──
            case 33: case 34: case 35: {
                envGate += (std::fabs (in) - envGate) * (std::fabs (in) > envGate ? 0.01f : 0.0003f);
                const float amp = clampf (envGate * 6.0f, 0.0f, 1.0f);
                float o;
                if (curProg == 0) { const float m = sineAt (ph2, 40.0f * fastExp2 (y * 7.0f)); o = sineAt (ph1, 40.0f * fastExp2 (x * 7.0f) * (1.0f + z * 3.0f * m)); }
                else if (curProg == 1) { lfo += (0.1f + 12.0f * x) / fs; if (lfo >= 1.0f) lfo -= 1.0f;
                                          o = sawAt (ph1, 60.0f * fastExp2 (y * 6.0f + (z - 0.5f) * 4.0f * lfo)); }
                else { float bp, hp; const float n = rng.bipolar() * 0.5f + sawAt (ph1, 50.0f * fastExp2 (y * 5.0f)) * 0.5f;
                       const float lpv = sv1.process (n, 100.0f * std::pow (60.0f, x), 4.0f, fs, bp, hp); o = z > 0.5f ? hp : lpv; }
                out = o * amp * 0.7f; break; }
            // ── OCHRE (delay invertido) ──
            default: {
                const bool loop = curProg == 2;
                const float len = fs * (curProg == 0 ? 0.3f + 1.6f * x : (curProg == 1 ? 0.05f + 0.4f * x : 0.1f + 1.8f * x));
                rec.push (in + fb1);
                if (! loop)
                {
                    // Un disparo: arranca cuando la entrada supera el umbral.
                    if (revCount <= 0 && std::fabs (in) > 0.02f + 0.5f * z * z) revCount = (int) len;
                    if (revCount > 0) --revCount;
                }
                revPh += 1.0f; if (revPh >= len) revPh = 0.0f;
                const float g = std::sin (3.14159265f * revPh / len);
                float r = rec.tap (2.0f * revPh + 2.0f + (loop ? z * ms (8.0f) * (1.0f + sine (0.3f)) : 0.0f)) * g;
                if (! loop && revCount <= 0) r *= 0.0f;
                fb1 = softClip (r * y * 0.9f);
                out = r; break; }
        }
        // Reconstrucción ~14 kHz del motor original.
        recon += (out - recon) * reconK;
        return recon;
    }

    void setReconstruction (float fc) { reconK = 1.0f - std::exp (-6.2831853f * fc / fs); }

private:
    float fs = 48000.0f, mis = 1.0f;
    int curCart = -1, curProg = -1;
    DelayLine42 dl, dl2, mod, rec;
    Verb42 rev;
    Shifter42 sh1, sh2;
    Svf42 sv1, sv2;
    float aps[6] {};
    float fb1 = 0, fb2 = 0, env = 0, envGate = 0, held = 0, lfo = 0, ph1 = 0, ph2 = 0, revPh = 0, lfoPh = 0, crushHold = 0;
    float recon = 0.0f, reconK = 0.8f, apz[4] {};
    int revCount = 0, crushCount = 0;
    Rng rng { 99 };

    inline float ms (float m) const { return m * 0.001f * fs; }
    inline float sine (float hz) { lfoPh += hz / fs; if (lfoPh >= 1.0f) lfoPh -= 1.0f; return std::sin (6.2831853f * lfoPh); }
    inline float sine2 (float hz) { ph2 += hz / fs; if (ph2 >= 1.0f) ph2 -= 1.0f; return std::sin (6.2831853f * ph2); }
    inline float sineAt (float& ph, float hz) { ph += hz / fs; if (ph >= 1.0f) ph -= 1.0f; return std::sin (6.2831853f * ph); }
    inline float sawAt (float& ph, float hz) { ph += hz / fs; if (ph >= 1.0f) ph -= 1.0f; return 2.0f * ph - 1.0f; }
    static inline float softClip (float v) { return fastTanh (v); }
    static inline float squareOf (float v) { return v > 0.0f ? 0.5f : -0.5f; }
    inline float allpassChain (float v, float c)
    {
        for (int i = 0; i < 4; ++i) { const float o = apz[i] - c * v; apz[i] = v + c * o; v = o; }
        return v;
    }
};

} // namespace dh

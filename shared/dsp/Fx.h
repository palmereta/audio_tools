#pragma once
// Etapas posteriores a la voz (no son del MS-20): vocoder de bandas, delay ping-pong y reverb de
// placa. Corren a la frecuencia del host, después del decimador.

#include "Common.h"
#include <array>
#include <vector>

namespace dh {

// Filtro de estado variable TPT (Zavalishin), salida pasa banda normalizada (ganancia 1 en fc).
struct SvfBand
{
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    void set (float fc, float q, float fs)
    {
        g = prewarp (fc, fs); k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    inline float process (float v0)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        return v1 * k;
    }
    void reset() { ic1 = ic2 = 0; }
};

// Vocoder de canales (estilo VC-10): el modulador (voz por la entrada de audio) se analiza en
// bandas logarítmicas; cada envolvente controla la misma banda del portador (el sintetizador).
class Vocoder
{
public:
    static constexpr int kMaxBands = 20;

    void prepare (float sampleRate) { fs = sampleRate; configured = false; hissHp.setCutoff (4500.0f, fs); sibHp.setCutoff (5000.0f, fs); }

    void configure (int nBands, float shiftOct, float qAmount)
    {
        nBands = std::max (4, std::min (kMaxBands, nBands));
        if (configured && nBands == bands && std::fabs (shiftOct - shift) < 1e-3f && std::fabs (qAmount - qa) < 1e-3f) return;
        bands = nBands; shift = shiftOct; qa = qAmount; configured = true;
        const float lo = 120.0f, hi = 7500.0f;
        // Ancho relativo: las bandas se tocan a −3 dB; qAmount angosta (más "robot") o ensancha.
        const float ratio = std::pow (hi / lo, 1.0f / (float) (bands - 1));
        const float baseQ = std::sqrt (ratio) / (ratio - 1.0f);
        const float q = baseQ * (0.6f + 1.6f * qAmount);
        for (int b = 0; b < bands; ++b)
        {
            const float fc = lo * std::pow (ratio, (float) b);
            auto& B = band[(size_t) b];
            B.m1.set (fc, q, fs); B.m2.set (fc, q, fs);
            const float fcc = clampf (fc * std::exp2 (shift), 40.0f, 0.42f * fs);
            B.c1.set (fcc, q, fs); B.c2.set (fcc, q, fs);
        }
        // Compensación de nivel: más bandas angostas = menos energía por banda.
        makeup = 2.2f * std::sqrt (20.0f / (float) bands) * (1.0f + 0.6f * qAmount);
    }

    // carrier/mod de entrada, out de salida (puede ser el mismo buffer que carrier).
    void process (const float* carrier, const float* mod, float* out, int n, float attackS, float releaseS, float hissAmt)
    {
        const float aC = 1.0f - std::exp (-1.0f / (std::max (0.0005f, attackS) * fs));
        const float rC = 1.0f - std::exp (-1.0f / (std::max (0.005f, releaseS) * fs));
        for (int i = 0; i < n; ++i)
        {
            const float m = mod[i], c = carrier[i];
            float y = 0.0f;
            for (int b = 0; b < bands; ++b)
            {
                auto& B = band[(size_t) b];
                const float mb = std::fabs (B.m2.process (B.m1.process (m)));
                B.env += (mb - B.env) * (mb > B.env ? aC : rC);
                y += B.c2.process (B.c1.process (c)) * B.env;
            }
            // Sibilancia: las consonantes (energía arriba de 5 kHz en la voz) abren ruido filtrado.
            const float hs = std::fabs (m - sibHp.process (m));   // parte aguda de la voz
            sibEnv += (hs - sibEnv) * (hs > sibEnv ? aC : rC);
            const float nz = rng.bipolar();
            const float hiss = nz - hissHp.process (nz);
            out[i] = y * makeup * 2.0f + hiss * sibEnv * hissAmt * 2.0f;
        }
    }

    void reset() { for (auto& b : band) { b.m1.reset(); b.m2.reset(); b.c1.reset(); b.c2.reset(); b.env = 0; } sibEnv = 0; }

private:
    struct Band { SvfBand m1, m2, c1, c2; float env = 0.0f; };
    std::array<Band, kMaxBands> band {};
    OnePoleLP hissHp, sibHp;
    Rng rng { 0xC0FFEEu };
    float fs = 48000.0f, shift = 0.0f, qa = 0.5f, makeup = 1.0f, sibEnv = 0.0f;
    int bands = 16;
    bool configured = false;
};

// Línea de retardo con lectura interpolada.
struct DelayLine
{
    std::vector<float> buf; int w = 0;
    void prepare (int maxSamples) { buf.assign ((size_t) maxSamples + 4, 0.0f); w = 0; }
    inline void push (float x) { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }
    inline float tap (float d) const   // d en muestras (≥ 1)
    {
        const int N = (int) buf.size();
        float r = (float) w - d;
        while (r < 0.0f) r += (float) N;
        // Si w − d es un negativo diminuto, r + N redondea a N en float: sin esta vuelta se leía una muestra
        // fuera del buffer (un clic aislado cada tanto en delays y coros modulados).
        int i0 = (int) r;
        if (i0 >= N) { i0 -= N; r -= (float) N; }
        const float fr = std::max (0.0f, r - (float) i0);
        const int i1 = i0 + 1 >= N ? 0 : i0 + 1;
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * fr;
    }
    inline float tapInt (int d) const
    {
        const int N = (int) buf.size();
        int r = (w - d) % N; if (r < 0) r += N;
        return buf[(size_t) r];
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void restart() { clear(); w = 0; }       // como recién preparada (tap() con otra w redondea distinto)
};

// Delay estéreo ping-pong con tono (pasa bajos en la realimentación) y tiempo suavizado.
class StereoDelay
{
public:
    void prepare (float sampleRate) { fs = sampleRate; l.prepare ((int) (fs * 4.1f)); r.prepare ((int) (fs * 4.1f)); time.reset (fs * 0.375f); time.setTime (0.08f, fs); }
    void process (float* L, float* R, int n, float seconds, float feedback, float tone, float mix)
    {
        lpL.setCutoff (1500.0f + 14000.0f * tone * tone, fs); lpR.setCutoff (1500.0f + 14000.0f * tone * tone, fs);
        hpL.setCutoff (90.0f, fs); hpR.setCutoff (90.0f, fs);
        const float target = clampf (seconds * fs, 4.0f, fs * 4.0f);
        for (int i = 0; i < n; ++i)
        {
            const float d = time.process (target);
            const float yl = l.tap (d), yr = r.tap (d);
            const float in = 0.5f * (L[i] + R[i]);
            // Ping-pong: la entrada va a la izquierda; cada repetición cruza de lado.
            float fl = lpL.process (in + yr * feedback); fl -= hpL.process (fl);
            float fr = lpR.process (yl * feedback); fr -= hpR.process (fr);
            l.push (fastTanh (fl * 0.8f) * 1.25f); r.push (fastTanh (fr * 0.8f) * 1.25f);
            L[i] += yl * mix; R[i] += yr * mix;
        }
    }
    void reset() { l.clear(); r.clear(); }
    // Como recién preparado: líneas, filtros y tiempo suavizado (un render tiene que sonar igual que una instancia nueva).
    void restart() { l.restart(); r.restart(); lpL.reset(); lpR.reset(); hpL.reset(); hpR.reset(); time.reset (fs * 0.375f); }
private:
    DelayLine l, r; OnePoleLP lpL, lpR, hpL, hpR; Smoother time;
    float fs = 48000.0f;
};

// Reverb de placa de Dattorro ("Effect Design Part 1", 1997), tiempos escalados desde 29761 Hz.
class PlateReverb
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        const float s = fs / 29761.0f;
        auto L = [s] (int n) { return std::max (1, (int) std::lround (n * s)); };
        const int in[4] = { L (142), L (107), L (379), L (277) };
        for (int i = 0; i < 4; ++i) { inAp[i].prepare (in[i] + 2); inLen[i] = in[i]; }
        pre.prepare ((int) (fs * 0.1f));
        apL1.prepare (L (672) + L (32) + 4); apL1Len = L (672);
        dL1.prepare (L (4453) + 2); dL1Len = L (4453);
        // La tabla de salidas de Dattorro lee 1913 muestras de este allpass de 1800: la línea
        // tiene que ser más larga que su retardo (si no, la lectura cae fuera del buffer).
        apL2.prepare (L (2000) + 2); apL2Len = L (1800);
        dL2.prepare (L (3720) + 2); dL2Len = L (3720);
        apR1.prepare (L (908) + L (32) + 4); apR1Len = L (908);
        dR1.prepare (L (4217) + 2); dR1Len = L (4217);
        apR2.prepare (L (2656) + 2); apR2Len = L (2656);
        dR2.prepare (L (3163) + 2); dR2Len = L (3163);
        excursion = (float) L (16);
        auto T = [s] (int n) { return std::max (1, (int) std::lround (n * s)); };
        tl = { T (266), T (2974), T (1913), T (1996), T (1990), T (187), T (1066) };
        tr = { T (353), T (3627), T (1228), T (2673), T (2111), T (335), T (121) };
        lfoInc = 1.0f / fs;   // ~1 Hz
    }

    void process (float* L, float* R, int n, float size, float damp, float mix)
    {
        const float decay = 0.45f + 0.5f * clampf (size, 0.0f, 1.0f);
        const float damping = 0.05f + 0.75f * clampf (damp, 0.0f, 1.0f);
        const float bw = 0.9995f;
        for (int i = 0; i < n; ++i)
        {
            const float x = 0.5f * (L[i] + R[i]);
            pre.push (x);
            float v = pre.tapInt ((int) (fs * 0.012f));
            bwState += bw * (v - bwState); v = bwState;
            const float coef[4] = { 0.75f, 0.75f, 0.625f, 0.625f };
            for (int k = 0; k < 4; ++k) v = allpass (inAp[k], inLen[k], v, coef[k]);

            lfoPhase += lfoInc; if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
            const float mod = std::sin (kTwoPi * lfoPhase) * excursion;

            // Rama izquierda.
            float a = v + lastR * decay;
            a = allpassMod (apL1, (float) apL1Len + mod, a, -0.70f);
            dL1.push (a); a = dL1.tapInt (dL1Len);
            dampL += (1.0f - damping) * (a - dampL); a = dampL * decay;
            a = allpass (apL2, apL2Len, a, 0.5f);
            dL2.push (a); lastL = dL2.tapInt (dL2Len);
            // Rama derecha.
            float b = v + lastL * decay;
            b = allpassMod (apR1, (float) apR1Len - mod, b, -0.70f);
            dR1.push (b); b = dR1.tapInt (dR1Len);
            dampR += (1.0f - damping) * (b - dampR); b = dampR * decay;
            b = allpass (apR2, apR2Len, b, 0.5f);
            dR2.push (b); lastR = dR2.tapInt (dR2Len);

            const float yl = dL1.tapInt (tl[0]) + dL1.tapInt (tl[1]) - apL2.tapInt (tl[2]) + dL2.tapInt (tl[3])
                           - dR1.tapInt (tl[4]) - apR2.tapInt (tl[5]) - dR2.tapInt (tl[6]);
            const float yr = dR1.tapInt (tr[0]) + dR1.tapInt (tr[1]) - apR2.tapInt (tr[2]) + dR2.tapInt (tr[3])
                           - dL1.tapInt (tr[4]) - apL2.tapInt (tr[5]) - dL2.tapInt (tr[6]);
            L[i] += yl * 0.42f * mix;
            R[i] += yr * 0.42f * mix;
        }
    }

    void reset()
    {
        for (auto* d : { &pre, &apL1, &dL1, &apL2, &dL2, &apR1, &dR1, &apR2, &dR2 }) d->clear();
        for (auto& d : inAp) d.clear();
        lastL = lastR = dampL = dampR = bwState = 0;
    }
    // Como recién preparada: también la fase del LFO que modula los allpass (reset() la deja donde estaba).
    void restart()
    {
        for (auto* d : { &pre, &apL1, &dL1, &apL2, &dL2, &apR1, &dR1, &apR2, &dR2 }) d->restart();
        for (auto& d : inAp) d.restart();
        lastL = lastR = dampL = dampR = bwState = 0; lfoPhase = 0.0f;
    }

private:
    static inline float allpass (DelayLine& d, int len, float x, float g)
    {
        const float delayed = d.tapInt (len);
        const float v = x + g * delayed;   // forma de Dattorro: v entra a la línea
        d.push (v);
        return delayed - g * v;
    }
    static inline float allpassMod (DelayLine& d, float len, float x, float g)
    {
        const float delayed = d.tap (len);
        const float v = x + g * delayed;
        d.push (v);
        return delayed - g * v;
    }
    DelayLine inAp[4], pre, apL1, dL1, apL2, dL2, apR1, dR1, apR2, dR2;
    int inLen[4] {}, apL1Len = 1, dL1Len = 1, apL2Len = 1, dL2Len = 1, apR1Len = 1, dR1Len = 1, apR2Len = 1, dR2Len = 1;
    std::array<int, 7> tl {}, tr {};
    float fs = 48000.0f, excursion = 16.0f, lfoPhase = 0.0f, lfoInc = 0.0f;
    float lastL = 0, lastR = 0, dampL = 0, dampR = 0, bwState = 0;
};

} // namespace dh

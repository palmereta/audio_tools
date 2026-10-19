#pragma once
// Módulos de control del SH-101: envolvente ADSR discreta (curvas RC), modulador (LFO/reloj) y el
// chorus estéreo de la página de extensiones.

#include "Common.h"
#include <vector>

namespace dh {

inline float rcCoef (float seconds, float fs) { return 1.0f - fastExp (-1.0f / std::max (1.0e-5f, seconds * fs)); }

// Potenciómetro logarítmico (curva A): 10 % de la resistencia a mitad de recorrido.
inline float taperA (float x) { return (std::pow (10.0f, 2.0f * clampf (x, 0.0f, 1.0f)) - 1.0f) / 99.0f; }

namespace cal101 {
    // Constantes de tiempo del ENV GEN (C59 = 4,7 µF; 270/220/150 Ω en serie con 1 MΩ curva A).
    inline float attackTau (float x)  { return (270.0f + 1.0e6f * taperA (x)) * 4.7e-6f * 0.82f; }
    inline float decayTau (float x)   { return (220.0f + 1.0e6f * taperA (x)) * 4.7e-6f; }
    inline float releaseTau (float x) { return (150.0f + 1.0e6f * taperA (x)) * 4.7e-6f; }
    // Tiempos "a la vista" (lo que muestra el globo): ataque hasta el pico, D/R hasta −40 dB aprox.
    inline float attackTime (float x) { return attackTau (x) * 1.0986f; }
    inline float decayTime (float x)  { return decayTau (x) * 2.2f; }
    inline float releaseTime (float x){ return releaseTau (x) * 2.2f; }
    inline float lfoHz (float x)      { return 0.1f * std::pow (300.0f, clampf (x, 0.0f, 1.0f)); }
    inline float cutoffHz (float x)   { return 10.0f * std::pow (2000.0f, clampf (x, 0.0f, 1.0f)); }
    inline float portaTau (float x)   { return 1.7f * taperA (x); }
    inline float vcoModOct (float x)  { return (std::exp (4.0f * x) - 1.0f) / (std::exp (4.0f) - 1.0f); }
    inline float resK (float x)       { return 4.55f * x; }
}

// ADSR del 101: el ataque carga hacia 1,5 y un comparador corta en el pico; decaimiento y relajación
// son exponenciales hacia S y hacia 0. Un redisparo arranca el ataque desde el nivel actual.
struct Adsr
{
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Idle;
    float y = 0.0f;

    void gateOn (bool retrigger)
    {
        if (retrigger || stage == Idle || stage == Release) stage = Attack;
    }
    void gateOff() { if (stage != Idle) stage = Release; }
    void reset() { stage = Idle; y = 0.0f; }

    // a, d, r: constantes de tiempo en segundos; s: 0..1.
    inline float process (float aCoef, float dCoef, float s, float rCoef)
    {
        switch (stage)
        {
            case Idle: break;
            case Attack:
                y += (1.5f - y) * aCoef;
                if (y >= 1.0f) { y = 1.0f; stage = Decay; }
                break;
            case Decay:
            case Sustain:
                y += (s - y) * dCoef;          // sigue al slider de S aunque se mueva
                break;
            case Release:
                y += (0.0f - y) * rCoef;
                if (y < 1e-5f) { y = 0.0f; stage = Idle; }
                break;
        }
        return y;
    }
};

// Modulador del SH-101: integrador + comparador (triángulo y cuadrada de la misma fase).
// El flanco de subida de la cuadrada es el reloj interno del arpegiador y del secuenciador.
struct Lfo101
{
    enum Wave { Triangle = 0, Square, Random, Noise };
    float phase = 0.0f, tri = 0.0f, sq = 1.0f;
    bool rise = false;
    OnePoleLP noiseLp;
    Rng rng { 0x5eedu };
    float noise = 0.0f;

    void prepare (float fs) { noiseLp.setCutoff (60.0f, fs); }
    void reset() { phase = 0.0f; }

    inline void process (float hz, float fs)
    {
        phase += hz / fs;
        rise = false;
        if (phase >= 1.0f) { phase -= std::floor (phase); rise = true; }
        // Fase 0 = arranque del integrador: el triángulo sube desde 0 y la cuadrada está arriba.
        tri = phase < 0.25f ? 4.0f * phase : (phase < 0.75f ? 2.0f - 4.0f * phase : 4.0f * phase - 4.0f);
        sq = phase < 0.5f ? 1.0f : -1.0f;
        noise = clampf (noiseLp.process (rng.bipolar()) * 6.0f, -1.0f, 1.0f);
    }
};

// Chorus estéreo de dos líneas BBD (modo I lento y suave, II más rápido y profundo, I+II ambos).
class Chorus101
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        buf.assign ((size_t) (fs * 0.05f) + 4, 0.0f);
        pos = 0; ph = 0.0f;
        lp.setCutoff (9000.0f, fs);
        hp.setCutoff (90.0f, fs);
    }

    void process (float* L, float* R, int n, int mode, float mix)
    {
        if (mode <= 0 || buf.empty()) return;
        const float rate = mode == 1 ? 0.5f : (mode == 2 ? 0.83f : 1.0f);
        const float depth = (mode == 1 ? 1.6f : (mode == 2 ? 2.4f : 1.2f)) * 0.001f * fs;
        const float centre = 3.6f * 0.001f * fs;
        const int size = (int) buf.size();
        for (int i = 0; i < n; ++i)
        {
            float in = 0.5f * (L[i] + R[i]);
            in = lp.process (in - hp.process (in));
            buf[(size_t) pos] = in;
            ph += rate / fs; if (ph >= 1.0f) ph -= 1.0f;
            float tri = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
            if (mode == 3) tri = 0.6f * tri + 0.4f * std::sin (6.2831853f * ph * 7.0f);
            const float dl = centre + depth * tri, dr = centre - depth * tri;
            const float wl = tap (dl, size), wr = tap (dr, size);
            L[i] = L[i] * (1.0f - 0.5f * mix) + wl * mix;
            R[i] = R[i] * (1.0f - 0.5f * mix) + wr * mix;
            pos = (pos + 1) % size;
        }
    }

private:
    inline float tap (float d, int size) const
    {
        float rp = (float) pos - d;
        while (rp < 0.0f) rp += (float) size;
        const int i0 = (int) rp; const float fr = rp - (float) i0;
        const float a = buf[(size_t) (i0 % size)], b = buf[(size_t) ((i0 + 1) % size)];
        return a + (b - a) * fr;
    }
    std::vector<float> buf;
    int pos = 0;
    float fs = 48000.0f, ph = 0.0f;
    OnePoleLP lp, hp;
};

} // namespace dh

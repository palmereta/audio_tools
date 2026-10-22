#pragma once
// VCO A/B del AT42 (núcleo AS3340): seis formas de onda (seno, triángulo, sierra, pulso y dos
// morphing: sierra ↔ sierra invertida pasando por triángulo, y seno ↔ triángulo), FM lineal
// (pasa por cero), sync duro, octava de 8 posiciones y afinación de una octava.
// Envolvente ADSR con HOLD (VCA abierto) y LOOP (se redispara sola: un LFO).
// Filtro Polivoks: SVF de 12 dB con integradores de slew limitado (el "sucio" del UD12), salida
// LP o BP, y la distorsión estéreo del 42F/N después del filtro.

#include "Blep.h"
#include "Common.h"
#include "Drone42.h"

namespace dh {

class Vco42
{
public:
    static constexpr float kOctBase[8] = { 2.044f, 16.35f, 32.70f, 65.41f, 130.81f, 261.63f, 523.25f, 1046.5f };
    float out = 0.0f;
    bool wrapped = false;

    // voct: voltios (1 V/oct); fmV: entrada FM en voltios; fmAmt 0..1; pw 0..1; syncIn en voltios.
    inline float process (int octave, float tune, float voct, float fmV, float fmAmt, int wave, float pw, float syncIn, float fs)
    {
        float hz = kOctBase[std::clamp (octave, 0, 7)] * fastExp2 (voct + (tune - 0.5f));
        hz *= 1.0f + fmAmt * fmV * 0.4f;                     // lineal: puede cruzar cero
        hz = clampf (hz, -0.45f * fs, 0.45f * fs);
        const float dt = hz / fs, adt = std::fabs (dt);

        const bool syncHigh = syncIn > 0.0f;
        if (syncHigh && ! lastSync) phase = 0.0f;
        lastSync = syncHigh;

        const float w = clampf (pw, 0.03f, 0.97f);
        float y;
        switch (wave)
        {
            case 0: y = std::sin (6.2831853f * phase); break;
            case 1: y = phase < 0.5f ? 4.0f * phase - 1.0f : 3.0f - 4.0f * phase; break;
            case 2: y = 2.0f * phase - 1.0f - polyBlep (phase, adt); break;
            case 3:
            {
                y = phase < w ? 1.0f : -1.0f;
                y += polyBlep (phase, adt);
                float t = phase - w; if (t < 0.0f) t += 1.0f;
                y -= polyBlep (t, adt);
                break;
            }
            case 4:
            {
                // Rampa de pendiente variable: pw 1 = sierra, 0.5 = triángulo, 0 = sierra invertida.
                const float k = clampf (pw, 0.02f, 0.98f);
                y = phase < k ? -1.0f + 2.0f * phase / k : 1.0f - 2.0f * (phase - k) / (1.0f - k);
                break;
            }
            default:
            {
                const float s = std::sin (6.2831853f * phase);
                const float tri = phase < 0.25f ? 4.0f * phase : (phase < 0.75f ? 2.0f - 4.0f * phase : 4.0f * phase - 4.0f);
                y = s + (tri - s) * clampf (pw, 0.0f, 1.0f);
                break;
            }
        }
        phase += dt;
        wrapped = false;
        if (phase >= 1.0f) { phase -= 1.0f; wrapped = true; }
        else if (phase < 0.0f) phase += 1.0f;
        out = y;
        return y;
    }

private:
    float phase = 0.0f;
    bool lastSync = false;
};

struct Adsr42
{
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Idle;
    float y = 0.0f;
    bool lastGate = false;
    float ca = 0, cd = 0, cr = 0, la = -1, ld = -1, lr = -1;
    inline void coefs (float a, float d, float r, float fs)
    {
        if (a != la) { la = a; ca = coefFor (expMap (a, 0.001f, 8.0f) * 0.55f, fs); }
        if (d != ld) { ld = d; cd = coefFor (expMap (d, 0.002f, 10.0f) * 0.3f, fs); }
        if (r != lr) { lr = r; cr = coefFor (expMap (r, 0.002f, 10.0f) * 0.3f, fs); }
    }

    inline float process (bool gate, bool loop, float a, float d, float s, float r, float fs)
    {
        if (gate && ! lastGate) stage = Attack;
        // Sin gate y sin LOOP la envolvente suelta, también si el LOOP se apagó con el gate ya abajo (quedaba sostenida
        // para siempre: athosttest, parámetros al azar con LOOP y bypass).
        if (! gate && ! loop && (stage == Attack || stage == Decay || stage == Sustain)) stage = Release;
        lastGate = gate;
        if (loop && stage == Idle) stage = Attack;
        coefs (a, d, r, fs);
        switch (stage)
        {
            case Idle: break;
            case Attack:
                y += (1.2f - y) * ca;
                if (y >= 1.0f) { y = 1.0f; stage = Decay; }
                break;
            case Decay:
            {
                const float target = loop ? 0.0f : s;
                y += (target - y) * cd;
                if (loop && y < 0.01f) stage = Attack;
                else if (! loop && std::fabs (y - s) < 1e-4f) stage = Sustain;
                break;
            }
            case Sustain: y += (s - y) * 0.001f; break;
            case Release:
                y += (0.0f - y) * cr;
                if (y < 1e-6f) { y = 0.0f; stage = Idle; }
                break;
        }
        return y;
    }
};

class Polivoks
{
public:
    void reset() { s1 = s2 = 0.0f; }

    // fc en Hz, res 0..1, bp: salida pasa banda; mismatch: factor de componentes del canal.
    inline float process (float x, float fc, float res, bool bp, float fs)
    {
        const float g = fastTan (3.14159265f * clampf (fc, 15.0f, 0.42f * fs) / fs);
        const float k = 2.0f - 1.97f * clampf (res, 0.0f, 1.0f);
        const float hp = (x - (k + g) * s1 - s2) / (1.0f + g * (k + g));
        // Integradores con slew limitado: el incremento por muestra satura (los UD12 en el original).
        const float v1 = 1.6f * g * fastTanh (hp * 0.625f);
        const float bpo = v1 + s1;
        s1 = v1 + bpo;
        const float v2 = 1.6f * g * fastTanh (bpo * 0.625f);
        const float lpo = v2 + s2;
        s2 = v2 + lpo;
        if (! std::isfinite (s1) || ! std::isfinite (s2)) reset();
        return bp ? bpo * (0.6f + 0.6f * k) : lpo;
    }

private:
    float s1 = 0.0f, s2 = 0.0f;
};

// Distorsión del 42F/N: recorte asimétrico; DIST mezcla seco/saturado, GAIN es la ganancia.
inline float distort42 (float x, float dist, float gain)
{
    const float d = x * (1.0f + gain * 24.0f);
    const float y = d > 0.0f ? fastTanh (d) : 0.8f * fastTanh (d * 1.25f);
    return x + (y * 0.9f - x) * dist;
}

} // namespace dh

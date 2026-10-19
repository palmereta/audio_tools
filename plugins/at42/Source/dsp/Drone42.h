#pragma once
// Voces drone del AT42.
//
// Clásica (drones 1, 2, 4, 5): cinco osciladores de sierra de un solo transistor (negistor). El
// capacitor carga por el pote de TUNE hacia la fuente Vs y se descarga de golpe al llegar a la
// tensión de ruptura: la rampa es un tramo de exponencial, v(u) = (1 − r^u)/(1 − r) con
// r = (Vs − Vbr)/(Vs − Vlow). VOLT baja la fuente: el período se estira (todo transpone hacia
// abajo) y la forma se vuelve más curva. Pasada la mitad, la fuente "cede" con cada descarga
// y los cinco osciladores se modulan entre sí (el FM que describe el manual).
// MOD pone el fotosensor (LDR) en paralelo con el pote: más luz, más agudo. La luz es la del
// ambiente (menos la sombra de la mano) más el LED rojo interno que maneja la entrada CV.
//
// Papa Srapa (drones 3, 6): un modulador de onda cuadrada lento y un oscilador de tono, con FM,
// AM (cortes), ruido blanco y un sample & hold.

#include "Blep.h"
#include "Common.h"
#include <array>

namespace dh {

inline float expMap (float x, float lo, float hi) { return lo * fastExp2 (std::log2 (hi / lo) * clampf (x, 0.0f, 1.0f)); }

// Envolvente AR analógica (curvas RC) de las voces drone.
struct Ar42
{
    float y = 0.0f;
    bool attacking = false;
    inline float process (bool gate, float aCoef, float rCoef)
    {
        if (gate)
        {
            if (! attacking && y < 0.999f) attacking = true;
            if (attacking) { y += (1.25f - y) * aCoef; if (y >= 1.0f) { y = 1.0f; attacking = false; } }
        }
        else { attacking = false; y += (0.0f - y) * rCoef; if (y < 1e-6f) y = 0.0f; }
        return y;
    }
};

// 1 − e^(−1/(τ·fs)) con expm1: con fastExp la resta de dos números casi iguales perdía precisión y las
// envolventes largas salían más cortas a frecuencias altas (30 s → 13 s a 192 kHz × 4).
inline float coefFor (float seconds, float fs) { return (float) -std::expm1 (-1.0 / std::max (1.0e-5, (double) seconds * fs)); }

class NegistorOsc
{
public:
    float phase = 0.0f;
    bool fired = false;

    // hz: frecuencia instantánea; lr = ln r (forma de la rampa, 0 < r < 1); invNorm = 1/(1 − r).
    inline float process (float hz, float lr, float invNorm, float fs)
    {
        const float dt = clampf (hz / fs, 0.0f, 0.45f);
        const float v = (1.0f - fastExp (phase * lr)) * invNorm;
        float out = 2.0f * v - 1.0f;
        out -= polyBlep (phase, dt) * 1.0f;          // la descarga casi instantánea: salto de 2
        phase += dt;
        fired = false;
        if (phase >= 1.0f) { phase -= 1.0f; fired = true; }
        return out;
    }
};

class ClassicVoice
{
public:
    // Rangos de cada oscilador según el manual (Hz).
    static constexpr float kLo[5] = { 16.35f, 61.74f, 146.83f, 329.63f, 783.99f };
    static constexpr float kHi[5] = { 98.0f, 196.0f, 493.88f, 1046.5f, 2349.3f };

    // Controles (0..1 o booleanos).
    std::array<float, 5> tune { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    std::array<bool, 5> mute {}, mod {};
    float volt = 0.15f, att = 0.25f, rls = 0.5f, cvAmt = 0.5f;
    bool hold = false;
    // Ajustes.
    bool chromatic = false;
    float drift = 0.3f, longRelease = 0.0f;

    // Estado visible.
    float env = 0.0f, light = 0.0f, ledLevel = 0.0f;

    void prepare (float sampleRate, uint32_t seed)
    {
        fs = sampleRate;
        rng = Rng (seed);
        for (int i = 0; i < 5; ++i) { osc[(size_t) i].phase = rng.unipolar(); driftState[(size_t) i] = 0.0f; }
        lightLp = 0.0f; sag = 0.0f;
        sagDecay = std::exp (-1.0f / (0.002f * fs));
    }

    // Frecuencia nominal del pote de TUNE: el período crece con la resistencia (f ∝ 1/R), así que
    // el último tramo del pote es mucho más sensible, como en el original.
    static float tuneHz (int i, float x)
    {
        const float a = 1.0f / kHi[i], b = 1.0f / kLo[i] - a;
        return 1.0f / (a + b * (1.0f - clampf (x, 0.0f, 1.0f)));
    }

    // gateIn en voltios (≥ 1 V = alto); cvIn en voltios; ambient 0..1; shade 0..1 (mano sobre el orbe).
    inline float process (float gateIn, float cvIn, float ambient, float shade)
    {
        // Fotosensor: luz ambiente + LED rojo interno (sólo enciende con tensión positiva).
        ledLevel = clampf ((cvIn * cvAmt * 2.0f - 1.6f) / 6.0f, 0.0f, 1.5f);
        const float target = ambient * (1.0f - shade) + ledLevel;
        lightLp += (target - lightLp) * (target > lightLp ? 0.0045f : 0.00016f) * (48000.0f / fs);
        light = lightLp;
        const float ldrMul = 1.0f + 0.65f * fastExp2 (0.8f * std::log2 (std::max (1.0e-6f, lightLp)));

        // Fuente: VOLT la baja; pasada la mitad, cada descarga la hace ceder (FM cruzado).
        const float vs0 = 14.0f - 4.4f * volt;
        const float couple = std::max (0.0f, volt - 0.5f) * 2.0f;
        const float vs = vs0 - couple * 2.6f * sag;
        const float r = clampf ((vs - 9.0f) / (vs - 1.5f), 0.02f, 0.9f);
        const float lr = std::log (r), invNorm = 1.0f / (1.0f - r);
        constexpr float lnNom = 0.91629073f;                 // ln (1 / 0.4): la forma con VOLT en cero
        const float pitchScale = lnNom / -lr;

        float sum = 0.0f;
        int fired = 0;
        for (int i = 0; i < 5; ++i)
        {
            // Deriva lenta (paseo aleatorio, unos centésimos).
            if ((++driftTick & 511) == (unsigned) i * 97u) driftTarget[(size_t) i] = rng.bipolar() * drift * 0.006f;
            driftState[(size_t) i] += (driftTarget[(size_t) i] - driftState[(size_t) i]) * 0.00002f;
            float hz = baseHz[(size_t) i];
            hz *= pitchScale * (1.0f + driftState[(size_t) i]);
            if (mod[(size_t) i]) hz *= ldrMul;
            const float s = osc[(size_t) i].process (hz, lr, invNorm, fs);
            if (osc[(size_t) i].fired) ++fired;
            if (! mute[(size_t) i]) sum += s;
        }
        sag += (float) fired * 0.35f;
        sag *= sagDecay;

        const bool gate = gateIn > 1.0f || hold;
        env = ar.process (gate, aCoef, rCoef);
        const float vca = hold ? 1.0f : env;
        return sum * 0.3f * vca;
    }

    // Una vez por bloque: afinación de cada oscilador y coeficientes de la envolvente.
    void updateBlock()
    {
        for (int i = 0; i < 5; ++i)
        {
            float hz = tuneHz (i, tune[(size_t) i]);
            if (chromatic) hz = 440.0f * std::exp2 (std::round (12.0f * std::log2 (hz / 440.0f)) / 12.0f);
            baseHz[(size_t) i] = hz;
        }
        const float relMax = longRelease > 0.5f ? 30.0f : 8.0f;
        aCoef = coefFor (expMap (att, 0.001f, 5.0f), fs);
        rCoef = coefFor (expMap (rls, 0.005f, relMax) * 0.4f, fs);
    }

    bool oscOn (int i) const { return ! mute[(size_t) i]; }

private:
    std::array<NegistorOsc, 5> osc {};
    std::array<float, 5> driftState {}, driftTarget {}, baseHz { 100, 100, 100, 100, 100 };
    float aCoef = 0.01f, rCoef = 0.001f;
    unsigned driftTick = 0;
    Ar42 ar;
    Rng rng { 1 };
    float fs = 96000.0f, lightLp = 0.0f, sag = 0.0f, sagDecay = 0.99f;
};

class PapaVoice
{
public:
    float rate = 0.4f, fmAmt = 0.35f, divider = 0.3f, pitch = 0.5f, noise = 0.0f, att = 0.25f, rls = 0.5f;
    bool x10 = false, fmOn = true, amOn = false, hi = false, hold = false;
    float longRelease = 0.0f;

    // Salidas de CV y estado.
    float modOut = 0.0f, env = 0.0f, shOut = 0.0f, noiseOut = 0.0f;
    bool shFlash = false;

    void prepare (float sampleRate, uint32_t seed) { fs = sampleRate; rng = Rng (seed); }

    // fmIn en voltios; shIn: señal a muestrear (normal: el ruido propio); shClk en voltios.
    inline float process (float gateIn, float fmIn, float shIn, float shClk)
    {
        // Modulador: cuadrada de Schmitt, 0.1–30 Hz (x10: hasta 300 Hz, FM a ritmo de audio).
        const float mhz = modHz;
        mPhase += mhz / fs;
        if (mPhase >= 1.0f) mPhase -= std::floor (mPhase);
        const float sq = mPhase < 0.5f ? 1.0f : 0.0f;
        modOut = sq * 12.0f;
        // DIVIDER suaviza la cuadrada antes de modular (de trino a sirena).
        const float slew = slewCoef;
        modSmooth += (sq - modSmooth) * slew;

        // Tono: oscilador de Schmitt (cuadrada), con FM del modulador y de la entrada CV.
        const float base = baseHz;
        float oct = fmIn * 0.2f;
        if (fmOn) oct += fmAmt * 4.0f * (modSmooth * 2.0f - 1.0f);
        const float hz = clampf (base * fastExp2 (oct), 1.0f, 0.45f * fs);
        const float dt = hz / fs;
        float tone = tPhase < 0.5f ? 1.0f : -1.0f;
        tone += polyBlep (tPhase, dt);
        float t2 = tPhase - 0.5f; if (t2 < 0.0f) t2 += 1.0f;
        tone -= polyBlep (t2, dt);
        tPhase += dt; if (tPhase >= 1.0f) tPhase -= 1.0f;
        if (amOn) { amLp += (sq - amLp) * 0.02f; tone *= amLp; }

        noiseOut = rng.bipolar();
        float sig = tone * (1.0f - 0.6f * noise) + noiseOut * noise * 1.2f;

        // Sample & hold: toma la entrada en cada flanco de subida del reloj.
        const bool clk = shClk > 1.5f;
        shFlash = false;
        if (clk && ! lastClk) { shOut = clampf (shIn, -5.0f, 5.0f); shFlash = true; }
        lastClk = clk;

        const bool gate = gateIn > 1.0f || hold;
        env = ar.process (gate, aCoef, rCoef);
        return sig * 0.45f * (hold ? 1.0f : env);
    }

    void updateBlock()
    {
        modHz = expMap (rate, 0.1f, 30.0f) * (x10 ? 10.0f : 1.0f);
        slewCoef = coefFor (0.0004f + divider * divider * 0.35f, fs);
        baseHz = hi ? expMap (pitch, 130.81f, 2637.0f) : expMap (pitch, 16.35f, 1046.5f);
        const float relMax = longRelease > 0.5f ? 30.0f : 8.0f;
        aCoef = coefFor (expMap (att, 0.001f, 5.0f), fs);
        rCoef = coefFor (expMap (rls, 0.005f, relMax) * 0.4f, fs);
    }

private:
    Ar42 ar;
    Rng rng { 7 };
    float fs = 96000.0f, mPhase = 0.0f, tPhase = 0.0f, modSmooth = 0.0f, amLp = 0.0f;
    float modHz = 1.0f, slewCoef = 1.0f, baseHz = 100.0f, aCoef = 0.01f, rCoef = 0.001f;
    bool lastClk = false;
};

} // namespace dh

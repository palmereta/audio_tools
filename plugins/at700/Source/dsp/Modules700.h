#pragma once
// Módulos del AT700 (System-700, 1976), en voltios como el hardware: audio de 10 V pico a pico,
// CV de 0 a ±10 V, compuertas de +15 V, 1 V/octava en todo el sistema. Cada clase corre por muestra
// a la frecuencia sobremuestreada; los coeficientes que dependen de perillas se recalculan por bloque.
//
// Fuentes: manual de instrucciones de 1976 y redibujos de los esquemas (docs/hardware-spec.md).

#include "Blep.h"
#include "Common.h"
#include <array>
#include <vector>

namespace dh {

// sin(πx/2) para x en [−1, 1] (error < 2e-4): la CRT de Windows hace std::sin varias veces más lento.
inline float sinHalfPi (float x) { const float x2 = x * x; return x * (1.5707963f - x2 * (0.6459637f - 0.0796926f * x2)); }

// Mapa exponencial de una perilla 0..1 a [lo, hi].
inline float expMap700 (float v, float lo, float hi) { return lo * fastExp2 (v * std::log2 (hi / lo)); }
inline float coef700 (float seconds, float fs) { return 1.0f - fastExp (-1.0f / std::max (1.0f, seconds * fs)); }
inline float softClip700 (float x, float lim) { return lim * fastTanh (x / lim); }

// ───────── 702 VCO ─────────
// Núcleo de diente de sierra (integrador con reset por JFET). Las cuatro salidas salen del mismo
// núcleo: sierra 0..+10 V, pulso 0..+10 V (comparador, 10–90 %), triángulo ±5 V y seno ±5 V (el
// triángulo por un conformador de diodos). SYNC fuerte resetea el integrador; el débil inyecta un
// pulso por un capacitor: adelanta la fase y engancha también en relaciones no armónicas.
struct Vco702
{
    float phase = 0.0f, dt = 0.0f, lastSyncIn = 0.0f, syncJump = 0.0f;
    float saw = 0.0f, pulse = 0.0f, tri = 0.0f, sine = 0.0f;

    void seed (float ph) { phase = ph - std::floor (ph); }

    inline void process (float hz, float pw, float syncIn, bool syncPatched, bool strong, float fs)
    {
        dt = clampf (hz / fs, 1.0e-7f, 0.45f);
        phase += dt;
        bool synced = false;
        if (syncPatched)
        {
            if (lastSyncIn < 5.0f && syncIn >= 5.0f)
            {
                if (strong)
                {
                    // Fracción de la muestra transcurrida desde el cruce de +5 V.
                    const float f = clampf ((syncIn - 5.0f) / std::max (1.0e-6f, syncIn - lastSyncIn), 0.0f, 1.0f);
                    float atEdge = phase - dt * f;
                    if (atEdge >= 1.0f) atEdge -= 1.0f;
                    syncJump = clampf (atEdge, 0.0f, 1.0f);
                    phase = dt * f;
                    synced = true;
                }
                else phase += 0.14f;                                 // pulso por capacitor (sync débil)
            }
            lastSyncIn = syncIn;
        }
        if (phase >= 1.0f) { phase -= std::floor (phase); syncJump = 0.0f; }
        const float t = phase;
        // Sierra 0..10 V con BLEP en el reset (natural o por sync, de la altura alcanzada).
        float s = t - 0.5f * polyBlep (t, dt);
        if (syncJump > 0.0f)
        {
            if (t < dt) s = t - 0.5f * syncJump * polyBlep (t, dt);     // salto de la altura alcanzada, no de 1
            if (! synced && t >= dt) syncJump = 0.0f;
        }
        saw = 10.0f * s;
        // Pulso (comparador sobre la sierra), ancho 10–90 %.
        const float w = clampf (pw, 0.1f, 0.9f);
        float pz = t < w ? 1.0f : 0.0f;
        pz += 0.5f * polyBlep (t, dt);
        float t2 = t - w; if (t2 < 0.0f) t2 += 1.0f;
        pz -= 0.5f * polyBlep (t2, dt);
        pulse = 10.0f * pz;
        // Triángulo desde el núcleo, con BLAMP en las puntas.
        float tr = t < 0.5f ? 4.0f * t - 1.0f : 3.0f - 4.0f * t;
        tr += 4.0f * dt * (polyBlamp (t, dt) - polyBlamp (t < 0.5f ? t + 0.5f : t - 0.5f, dt));
        tri = 5.0f * tr;
        // Seno por diodos: casi puro, con algo del triángulo.
        sine = 5.0f * (0.97f * sinHalfPi (tr) + 0.03f * tr);
    }
};

// ───────── 703A/B/C: VCF de 12 dB de estado variable (dos integradores OTA) ─────────
// La resonancia pasa por un par de diodos en antiparalelo: el lazo se recorta suave y la
// autooscilación (desde RES ≈ 7–8) queda acotada y afinada a 1 V/oct.
struct Vcf703Svf
{
    float s1 = 0.0f, s2 = 0.0f;
    void reset() { s1 = s2 = 0.0f; }

    // mode: 0 HP, 1 BP, 2 LP.
    inline float process (float x, float fc, float res, int mode, float fs)
    {
        const float g = prewarp (fc, fs);
        const float R = 1.0f - 1.32f * res;                         // R < 0 → autooscila
        x = softClip700 (x, 9.0f);                                  // sumador de entrada (LM741)
        const float hp = (x - (2.0f * R + g) * s1 - s2) / (1.0f + 2.0f * R * g + g * g);
        const float v1 = g * hp;
        const float bp = v1 + s1;
        s1 = softClip700 (bp + v1, 6.5f);                          // diodos del lazo de resonancia
        const float v2 = g * bp;
        const float lp = v2 + s2;
        s2 = lp + v2;
        if (! std::isfinite (s1) || ! std::isfinite (s2)) { reset(); return 0.0f; }
        return mode == 0 ? hp : (mode == 1 ? bp * 1.2f : lp);
    }
};

// ───────── 703E/F/G: VCF de 24 dB, cuatro etapas OTA (CA3080) con realimentación global ─────────
// Igual resolución que el IR3109 del AT101V (cada etapa linealizada alrededor de la muestra anterior,
// lazo resuelto en forma cerrada). Sin compensación: al subir la resonancia baja la banda pasante,
// el rasgo que el manual describe como "difícil de usar".
struct Vcf703Ota
{
    void reset() { for (auto& v : s) v = 0.0f; for (auto& v : tin) v = 1.0f; for (auto& v : tout) v = 1.0f; tfb = 1.0f; }

    inline float process (float in, float fc, float k, float fs)
    {
        const float g = prewarp (fc, fs);
        float A = 1.0f, B = 0.0f, a[4], b[4];
        for (int i = 0; i < 4; ++i)
        {
            const float den = 1.0f / (1.0f + g * tout[i]);
            a[i] = g * tin[i] * den;
            b[i] = s[i] * den;
            A *= a[i];
            B = B * a[i] + b[i];
        }
        const float u = in * 0.12f;                                 // 10 V p-p → ±0,6 (zona de trabajo del OTA)
        const float kc = k * tfb;
        const float x0 = (u - kc * B) / (1.0f + kc * A);
        float x = x0, y[4];
        for (int i = 0; i < 4; ++i) { y[i] = a[i] * x + b[i]; x = y[i]; }
        float xin = x0;
        for (int i = 0; i < 4; ++i)
        {
            s[i] = 2.0f * y[i] - s[i];
            tin[i] = tau (clampf (xin, -8.0f, 8.0f));
            tout[i] = tau (clampf (y[i], -8.0f, 8.0f));
            xin = y[i];
        }
        if (! std::isfinite (y[3])) { reset(); return 0.0f; }
        const float v = clampf (y[3], -8.0f, 8.0f) * (1.0f / 3.4f);
        tfb = std::fabs (v) < 1e-4f ? 1.0f : fastTanh (v) / v;
        return y[3] * 8.3f;
    }

private:
    static inline float tau (float v) { return std::fabs (v) < 1e-4f ? 1.0f : fastTanh (v) / v; }
    float s[4] { 0, 0, 0, 0 }, tin[4] { 1, 1, 1, 1 }, tout[4] { 1, 1, 1, 1 }, tfb = 1.0f;
};

// ───────── 704 VCA (CA3080): lineal 10 %/V o exponencial 10 dB/V ─────────
inline float vcaGain704 (float vctl, bool expo)
{
    if (expo) return vctl <= 0.0f ? 0.0f : std::min (2.0f, fastExp2 ((vctl - 10.0f) * 1.660964f));   // 10 dB/V, 10 V = 0 dB
    return clampf (vctl * 0.1f, 0.0f, 2.0f);
}

// ───────── 705A: ADSR (segmentos RC; el ataque carga hacia ~13 V y el PUT corta a 10 V) ─────────
struct Adsr705
{
    enum Stage { Idle, Attack, Decay, Release };
    Stage stage = Idle;
    float y = 0.0f;
    bool lastGate = false;

    void trigger() { stage = Attack; }

    // gate y trig ya decididos por el modo; coeficientes por bloque.
    inline float process (bool gate, bool trig, float aC, float dC, float sV, float rC)
    {
        if (gate && (! lastGate || trig)) stage = Attack;
        if (! gate && lastGate) stage = Release;
        lastGate = gate;
        switch (stage)
        {
            case Attack:
                y += (13.0f - y) * aC;
                if (y >= 10.0f) { y = 10.0f; stage = Decay; }
                break;
            case Decay: y += (sV - y) * dC; break;
            case Release:
                y += (0.0f - y) * rC;
                if (y < 1.0e-4f) { y = 0.0f; stage = Idle; }
                break;
            case Idle: break;
        }
        return y;
    }
};

// ───────── 706 LFO ─────────
struct Lfo706
{
    float phase = 0.0f, delayEnv = 1.0f;
    float out = 0.0f;
    // wave: 0 cuadrada/pulso (0..10 V), 1 sierra descendente (10..0 V), 2 triángulo ±5 V, 3 seno ±5 V.
    inline float process (float hz, int wave, float pw, float fs)
    {
        phase += hz / fs;
        if (phase >= 1.0f) phase -= std::floor (phase);
        const float t = phase;
        switch (wave)
        {
            case 0: out = t < pw ? 10.0f : 0.0f; break;
            case 1: out = 10.0f * (1.0f - t); break;
            case 2: out = 5.0f * (t < 0.5f ? 4.0f * t - 1.0f : 3.0f - 4.0f * t); break;
            default: out = 5.0f * sinHalfPi (t < 0.5f ? 4.0f * t - 1.0f : 3.0f - 4.0f * t); break;
        }
        return out;
    }
    // KEY TRIG: seno y triángulo arrancan a 90°, sierra y cuadrada a 0°.
    void keyTrig (int wave) { phase = (wave >= 2) ? (wave == 3 ? 0.25f : 0.5f) : 0.0f; }
};

// ───────── 709 S&H ─────────
struct Sh709
{
    float held = 0.0f, target = 0.0f, out = 0.0f, clockPhase = 0.0f, lastExt = 0.0f;
    bool clockHigh = false;
    // Devuelve true en el flanco de muestreo.
    inline bool clock (bool external, float extIn, float rateHz, float fs)
    {
        bool edge = false;
        if (external)
        {
            edge = lastExt < 5.0f && extIn >= 5.0f;
            clockHigh = extIn >= 5.0f;
            lastExt = extIn;
        }
        else
        {
            clockPhase += rateHz / fs;
            if (clockPhase >= 1.0f) { clockPhase -= std::floor (clockPhase); edge = true; }
            clockHigh = clockPhase < 0.5f;
        }
        return edge;
    }
    inline float process (bool edge, float in, float slewC)
    {
        if (edge) target = clampf (in, -10.0f, 10.0f);
        out += (target - out) * slewC;                              // tiempo de muestreo 0,3 ms
        return out;
    }
};

// ───────── 708A: ruido blanco y rosa (−3 dB/oct) ─────────
struct Noise708
{
    Rng rng { 0x7008u };
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    float white = 0.0f, pink = 0.0f;
    inline void process()
    {
        const float w = 0.5f * (rng.bipolar() + rng.bipolar());     // casi gaussiano
        white = w * 7.5f * 1.4f;
        // Filtro de Paul Kellet.
        b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f;
        b2 = 0.96900f * b2 + w * 0.1538520f; b3 = 0.86650f * b3 + w * 0.3104856f;
        b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
        const float p = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
        b6 = w * 0.115926f;
        pink = p * 0.11f * 7.5f * 1.4f;
    }
};

// ───────── 707A: amplificador, seguidor de envolvente e integrador ─────────
struct EnvFollower707
{
    float env = 0.0f;
    inline float process (float x, float atkC, float relC)
    {
        const float r = std::fabs (x) * 2.0f;                       // 1 V continua por cada 1 V p-p
        env += (r - env) * (r > env ? atkC : relC);
        return std::min (env, 10.0f);
    }
};

// ───────── 711A: reverb de resorte (dos resortes) ─────────
// Cadena dispersiva de pasa-todos dentro de un lazo con retardo: el "chirrido" y el goteo del
// resorte. Tiempo máximo de reverberación ~2 s; el original suena oscuro y metálico.
class Spring711
{
public:
    void prepare (float fs_)
    {
        fs = fs_;
        for (int s = 0; s < 2; ++s)
        {
            Spring& sp = springs[(size_t) s];
            sp.len = (int) (fs * (s == 0 ? 0.0383f : 0.0461f));
            sp.buf.assign ((size_t) sp.len + 8, 0.0f);
            sp.pos = 0;
            for (auto& a : sp.ap) a = 0.0f;
            for (auto& a : sp.apx) a = 0.0f;
            sp.lp = 0.0f; sp.hp = 0.0f; sp.hpx = 0.0f;
        }
        lpC = 1.0f - fastExp (-kTwoPi * 3800.0f / fs);
        hpR = fastExp (-kTwoPi * 90.0f / fs);
    }

    inline float process (float x)
    {
        float out = 0.0f;
        for (int s = 0; s < 2; ++s)
        {
            Spring& sp = springs[(size_t) s];
            const float fb = sp.buf[(size_t) sp.pos];
            float v = x * 0.5f + fb * 0.76f;
            // Dispersión: pasa-todos de primer orden en cascada (coeficiente alto → chirrido).
            const float a = s == 0 ? 0.62f : 0.58f;
            for (int i = 0; i < kAp; ++i)
            {
                const float y = -a * v + sp.apx[(size_t) i] + a * sp.ap[(size_t) i];
                sp.apx[(size_t) i] = v; sp.ap[(size_t) i] = y; v = y;
            }
            sp.lp += (v - sp.lp) * lpC;
            const float hpo = sp.lp - sp.hpx + hpR * sp.hp;
            sp.hpx = sp.lp; sp.hp = hpo;
            sp.buf[(size_t) sp.pos] = softClip700 (hpo, 12.0f);
            if (++sp.pos >= sp.len) sp.pos = 0;
            out += hpo;
        }
        return out * 0.9f;
    }

private:
    static constexpr int kAp = 14;
    struct Spring { std::vector<float> buf; int len = 1, pos = 0; std::array<float, kAp> ap {}, apx {}; float lp = 0, hp = 0, hpx = 0; };
    std::array<Spring, 2> springs;
    float fs = 48000.0f, lpC = 0.3f, hpR = 0.99f;
};

// ───────── 711A: phase shifter (8 pasa-todos con JFET, 1440°) ─────────
struct Phaser711
{
    std::array<float, 8> z {};
    float fb = 0.0f, lfo = 0.0f;
    // sweep 0..1 mueve la muesca central de 100 Hz a 10 kHz; res 0..1 realimenta.
    inline float process (float x, float sweep, float res, float fs)
    {
        const float fc = 100.0f * fastExp2 (clampf (sweep, 0.0f, 1.0f) * 6.643856f);
        const float t = prewarp (fc, fs);
        const float a = (t - 1.0f) / (t + 1.0f);
        float v = x + fb * res * 0.85f;
        for (int i = 0; i < 8; ++i)
        {
            const float y = a * v + z[(size_t) i];
            z[(size_t) i] = v - a * y;
            v = y;
        }
        fb = softClip700 (v, 8.0f);
        return 0.5f * (x + v);
    }
};

} // namespace dh

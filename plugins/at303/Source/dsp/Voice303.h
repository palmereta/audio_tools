#pragma once
// La voz del AT303, del manual de servicio de 1982 y de los análisis publicados (docs/research.md §1, §4).
//
//   oscilador: rampa descendente exponencial 1 V/oct (A = 110 Hz en 2,75 V); la cuadrada no sale de un comparador
//     sino de un transistor PNP (Q8) que mira la rampa a través de una red RC y se autopolariza por el emisor: por
//     eso no es simétrica, cae en los tramos "planos" y cambia con la altura. Acá: un pasa bajos de la rampa
//     (R35·C10, τ ≈ 1 ms), la polarización lenta del emisor (R45·C11, τ ≈ 22 ms) y la llave del transistor.
//   filtro: escalera de diodos de cuatro polos con el capacitor de abajo casi a la mitad (0,018 contra 0,033 µF).
//     Es de 24 dB/oct pero con un codo tan blando que entre 2 y 4 veces la resonancia cae ~19 dB/oct (el famoso
//     "18 dB"); autooscila recién con k = 17 (4 en la escalera de Moog), así que la resonancia se come ~25 dB de
//     graves. Ecuaciones de Zavalishin (VAFD §6.9) con la proporción real de capacitores, resueltas sin retardo
//     (TPT) con las tanh linealizadas alrededor del estado anterior, como LadderMd del ATMD.
//   envolventes: la MEG (filtro: Decay de 200 ms a 2,5 s hasta el 10 %, forzada al mínimo en los acentos) y la
//     VEG (VCA: retardo de 4 ms, ataque de 3 ms, caída de ~1,5 s; al soltar, 8 ms a nivel y 8 ms lineales).
//   acento: la MEG entra al VCA por un RC de 1,6 ms y al corte por el circuito de barrido (diodo, 47 kΩ y un
//     capacitor de 1 µF en la segunda sección del potenciómetro de Resonance). El capacitor no se descarga entre
//     notas: los acentos seguidos suben cada vez más. Ese es "el wow", y está resuelto como circuito.
//   slide: RC de 100 kΩ y 0,22 µF sobre el CV en volts (τ = 22 ms): el mismo tiempo para cualquier intervalo.
//
// Todo en float salvo la fase; la voz corre a la frecuencia sobremuestreada (×4 por defecto).

#include "Common.h"

namespace dh::tb {

using dh::kPi;
using dh::clampf;

struct VoiceParams
{
    float tuneSemis = 0.0f;              // ±7 semitonos (Tuning)
    float cutoff = 0.5f, resonance = 0.5f, envMod = 0.5f, decay = 0.5f, accent = 0.5f;   // 0..1, perillas
    bool square = false;
    // Página extendida (los valores de fábrica reproducen el original).
    float overdrive = 1.0f;              // ×1 .. ×66 a la entrada del filtro
    float normalDecayMs = -1.0f, accentDecayMs = 200.0f;   // −1: la de la perilla Decay
    float softAttackMs = 3.0f;           // ataque de las notas sin acento
    bool noteDelay = true;               // los 4 ms de retardo antes del ataque
    float slideTauMs = 22.0f;
    float sweepSpeed = 1.0f;             // 0,33 rápido, 1 normal, 2,2 lento (capacitor del barrido)
    bool sweepOn = true;
    float tracking = 0.0f;               // octavas de corte por volt de CV (0 = el original)
    float filterFm = 0.0f;               // salida del VCA al corte
    float muffler = 0.0f;                // recorte suave después del VCA (0, 0,5, 1)
    bool highResonance = false;          // k × 2: autooscila
    float envModRange = 1.0f;            // 1 = original; 3 = el triple
    bool envModZero = false;             // el original no llega a cero
    bool bassFix = false;                // pasa altos de salida en 4,7 Hz en vez de 47
    float cutoffTopHz = 2400.0f;         // tope del rango (5000 con la extensión)
    float vcaClick = 1.0f;               // golpe de control del VCA (0..2 × nominal)
};

class Voice
{
public:
    void prepare (float baseRate, int oversample)
    {
        fsBase = baseRate; os = std::max (1, oversample); fs = baseRate * (float) os; reset();
    }
    // Cambio de calidad tocando: la frecuencia interna cambia, las envolventes y la nota siguen.
    void changeOversample (int oversample) { os = std::max (1, oversample); fs = fsBase * (float) os; }
    void reset()
    {
        phase = 0.0; y1 = y2 = y3 = y4 = 0; s1 = s2 = s3 = s4 = 0; hpFb.reset(); hpIn.reset(); hpOut.reset();
        sqLp = 0; sqBias = 0.45f; meg = 0; megTarget = 0; veg = 0; vegPhase = kIdle; vegT = 0; accRc = 0;
        vC13 = 0; vW = 0; cv = cvTarget = 2.0f; gate = false; accentOn = false; slideOn = false; noiseRng.s = 0x1234567u;
        lastY = 0; uPrev = 0;
    }

    void set (const VoiceParams& p) { P = p; }

    // Eventos del secuenciador o del MIDI (a la frecuencia base, exactos a la muestra).
    // gateOn: flanco de compuerta (dispara las envolventes); legato = sin flanco (la compuerta sigue alta).
    void noteOn (float cvVolts, bool accent, bool legato)
    {
        cvTarget = cvVolts;
        accentOn = accent;
        // Una nota ligada sólo sigue sin flanco si la compuerta está alta: si estaba cerrada (el slide del final del
        // patrón hacia el primer paso al dar Play, un patrón editado que cerró la compuerta) se dispara como nueva.
        if (! legato || ! gate)
        {
            cv = cvVolts;                // sin slide la CV salta (el op-amp sigue al DAC)
            gate = true;
            megCharge = true; megChargeT = 0;
            vegPhase = P.noteDelay && ! accent ? kDelay : kAttack; vegT = 0;
        }
    }
    void setSlide (bool on) { slideOn = on; }
    void gateOff() { if (gate) { gate = false; vegPhase = kHold; vegT = 0; } }
    bool gateHigh() const { return gate; }

    // Una muestra de la frecuencia base: actualiza envolventes, barrido y slide; después corre `os` submuestras.
    // `out` recibe las `os` submuestras (para el diezmador).
    inline void processBase (float* out)
    {
        const float dt = 1.0f / fsBase;
        // ── Slide: RC sobre el CV en volts, conmutado ──
        if (slideOn) cv += (cvTarget - cv) * (1.0f - std::exp (-dt / (P.slideTauMs * 0.001f)));
        else cv += (cvTarget - cv) * 0.9f;
        // ── MEG: carga de ~1 ms al pico y caída RC; τ = (68 kΩ + Decay)·1 µF, o 68 ms en los acentos ──
        if (megCharge)
        {
            megChargeT += dt;
            meg += (1.0f - meg) * (1.0f - std::exp (-dt / 0.0003f));
            if (megChargeT >= 0.001f) megCharge = false;
        }
        else
        {
            const float tau = accentOn ? accentTau() : decayTau();
            meg *= std::exp (-dt / tau);
        }
        // ── VEG ──
        stepVeg (dt);
        // ── Acento al VCA: MEG × Accent por un RC de 47 kΩ / 33 nF (1,6 ms), sólo con acento ──
        const float accSrc = accentOn ? P.accent * meg : 0.0f;
        accRc += (accSrc - accRc) * (1.0f - std::exp (-dt / 0.00155f));
        // ── Circuito de barrido del acento (D24, R46 47 kΩ, potenciómetro de Resonance, C13 1 µF) ──
        sweepStep (dt);
        // ── Exponente del corte (en octavas sobre 300 Hz) ──
        const float u = clampf (P.cutoff, 0.0f, 1.0f);
        const float topOct = std::log2 (P.cutoffTopHz / 300.0f);
        float oct = topOct * u * u;                                         // 300 Hz · (tope/300)^(u²): 500 Hz al medio
        const float em = P.envModZero ? P.envMod : 0.08f + 0.92f * P.envMod;   // el original no baja a cero
        const float emT = em * em * P.envModRange;
        oct += emT * (4.2f * meg - 1.6f);                                   // más Env Mod: más barrido y base más baja [SN p8]
        if (P.sweepOn) oct += 0.32f * vW;                                   // corriente del barrido al sumador
        oct += P.tracking * (cv - 2.1f);
        cutoffOct = oct;
        const float fRes = 300.0f * std::exp2 (oct);                       // frecuencia de resonancia
        fcBase = fRes * 1.41421356f;                                       // ω0 = ωc/√2 en este modelo
        // ── Resonancia: k hasta 16,5 (por debajo del umbral 17); con la extensión, × 2 ──
        const float r = clampf (P.resonance, 0.0f, 1.0f);
        k = 16.5f * r * (P.highResonance ? 2.0f : 1.0f);
        // ── VCA ──
        vcaCtl = clampf (veg + 1.6f * accRc, 0.0f, 3.0f);
        // ── Frecuencia del oscilador ──
        const float f0 = 65.406f * std::exp2 (cv - 2.0f + P.tuneSemis / 12.0f);
        const double inc = (double) f0 / (double) fs;
        // Submuestras.
        for (int i = 0; i < os; ++i) out[i] = sub (inc);
    }

    // Lo que dibuja la vista "Inspect".
    float c13() const { return vC13; }
    float megLevel() const { return meg; }
    float vegLevel() const { return veg; }
    float cutoffHz() const { return fcBase / 1.41421356f; }
    float cvVolts() const { return cv; }
    float kNow() const { return k; }
    float ladderState (int i) const { return i == 0 ? y1 : i == 1 ? y2 : i == 2 ? y3 : y4; }
    float vcaControl() const { return vcaCtl; }

private:
    enum VegPhase { kIdle, kDelay, kAttack, kDecay, kHold, kRelease };
    VoiceParams P;
    float fsBase = 48000, fs = 192000; int os = 4;
    double phase = 0;
    float y1 = 0, y2 = 0, y3 = 0, y4 = 0, s1 = 0, s2 = 0, s3 = 0, s4 = 0, uPrev = 0;
    float sqLp = 0, sqBias = 0.45f;
    float meg = 0, megTarget = 0, megChargeT = 0; bool megCharge = false;
    float veg = 0, vegT = 0, vegStart = 0; VegPhase vegPhase = kIdle;
    float accRc = 0, vC13 = 0, vW = 0;
    float cv = 2, cvTarget = 2; bool gate = false, accentOn = false, slideOn = false;
    float cutoffOct = 0, fcBase = 700, k = 0, vcaCtl = 0, lastY = 0;
    dh::Rng noiseRng;

    struct Hp     // pasa altos de un polo, TPT: afín en su entrada (así el lazo se resuelve en forma cerrada)
    {
        float G = 0, s = 0;
        void setHz (float hz, float fsr) { const float g = dh::fastTan (kPi * hz / fsr); G = g / (1.0f + g); }
        // hp = a·x + b
        inline float a() const { return 1.0f - G; }
        inline float b() const { return -(1.0f - G) * s; }
        inline float tick (float x) { const float v = (x - s) * G; const float lp = v + s; s = lp + v; return x - lp; }
        void reset() { s = 0; }
    } hpFb, hpIn, hpOut;

    float decayTau() const
    {
        if (P.normalDecayMs > 0) return P.normalDecayMs * 0.001f / 2.302585f;      // extensión: ms hasta el 10 %
        const float x = clampf (P.decay, 0.0f, 1.0f);
        const float rDecay = 1.0e6f * (std::pow (10.0f, 2.0f * x) - 1.0f) / 99.0f;  // potenciómetro de 1 MΩ, curva A
        return (68.0e3f + rDecay) * 1.0e-6f;                                         // 68 ms .. 1,07 s
    }
    float accentTau() const { return clampf (P.accentDecayMs, 30.0f, 3000.0f) * 0.001f / 2.302585f; }

    inline void stepVeg (float dt)
    {
        vegT += dt;
        switch (vegPhase)
        {
            case kIdle: veg = 0; break;
            case kDelay: if (vegT >= 0.004f) { vegPhase = kAttack; vegT = 0; } break;
            case kAttack:
            {
                const float at = accentOn ? 0.0005f : std::max (0.0003f, P.softAttackMs * 0.001f);
                veg += (1.0f - veg) * std::min (1.0f, dt / at * 1.5f);
                if (vegT >= at) { vegPhase = gate ? kDecay : kHold; vegT = 0; }
                break;
            }
            case kDecay: veg *= std::exp (-dt / 1.5f); break;                    // R123·C42
            case kHold: if (vegT >= 0.008f) { vegPhase = kRelease; vegT = 0; vegStart = veg; } break;
            case kRelease: veg = std::max (0.0f, vegStart * (1.0f - vegT / 0.008f)); if (veg <= 0) vegPhase = kIdle; break;
        }
    }

    // Barrido del acento como circuito: fuente v_a = MEG × Accent × 5 V (sólo en acentos), diodo (0,6 V), 47 kΩ,
    // el potenciómetro de 50 kΩ partido por la posición de Resonance ρ (R_a de la fuente al cursor, R_b del cursor a
    // C13) y del cursor 100 kΩ a tierra virtual. Euler implícito: C13 no explota cuando R_b → 0.
    inline void sweepStep (float dt)
    {
        const float rho = clampf (P.resonance, 0.0f, 1.0f);
        const float Ra = 47.0e3f + 50.0e3f * rho, Rb = std::max (200.0f, 50.0e3f * (1.0f - rho)), R3 = 100.0e3f;
        const float C = 1.0e-6f * P.sweepSpeed;
        const float va = accentOn ? 5.0f * P.accent * meg : 0.0f;
        const float a = C / dt;
        // V_c' = α V_c + β V_w (de la ecuación del capacitor)
        const float alpha = a / (a + 1.0f / Rb), beta = (1.0f / Rb) / (a + 1.0f / Rb);
        // KCL en el cursor: i_fuente = (V_w − V_c')/R_b + V_w/R3
        auto solve = [&] (bool diode) {
            const float gw = (1.0f - beta) / Rb + 1.0f / R3 + (diode ? 1.0f / Ra : 0.0f);
            const float rhs = alpha * vC13 / Rb + (diode ? (va - 0.6f) / Ra : 0.0f);
            return rhs / gw;
        };
        float w = solve (true);
        if (va - 0.6f - w <= 0.0f) w = solve (false);
        vW = w;
        vC13 = alpha * vC13 + beta * w;
    }

    // Una submuestra: oscilador, escalera, VCA.
    inline float sub (double inc)
    {
        // Rampa descendente con PolyBLEP en el reinicio.
        phase += inc;
        if (phase >= 1.0) phase -= 1.0;
        const float t = (float) phase, dtp = (float) inc;
        float saw = 1.0f - 2.0f * t;                                       // de +1 a −1
        saw += polyBlep (t, dtp);                                          // el salto del reinicio es hacia arriba
        float x;
        if (! P.square) x = saw;
        else
        {
            // Q8: la rampa por R35·C10 (pasa bajos, τ ≈ 1 ms), la base del PNP contra el emisor autopolarizado
            // (R45·C11, τ ≈ 22 ms); la llave conduce cuando la base cae bajo el emisor.
            sqLp += (saw - sqLp) * (1.0f - std::exp (-1.0f / (0.001f * fs)));
            sqBias += (sqLp - sqBias) * (1.0f / (0.022f * fs));
            x = -std::tanh (9.0f * (sqLp - sqBias + 0.06f));
        }
        // Piso de ruido térmico (−90 dB): algo para que la resonancia crezca.
        x += 3.0e-5f * noiseRng.bipolar();
        const float in = x * 1.25f * P.overdrive;
        const float y = ladder (in);
        // VCA de transconductancia: la señal pasa por una tanh y la escala la corriente de control; el control
        // se filtra un poco hacia la salida (el golpe que los clones limpios no tienen).
        float v = std::tanh (y * 0.55f) / 0.55f * vcaCtl * 0.45f + P.vcaClick * 0.004f * vcaCtl;
        if (P.muffler > 0) v = (1.0f - P.muffler) * v + P.muffler * std::tanh (v * 2.2f) / 2.2f;
        if (P.filterFm > 0) lastY = v;
        return hpOut.tick (v);
    }

    // Escalera de diodos, TPT con tanh linealizadas (tanh(v) ≈ v·T(v_anterior)). Estados en unidades de 2V_T.
    inline float ladder (float x)
    {
        float fc = fcBase;
        if (P.filterFm > 0) fc *= std::exp2 (P.filterFm * 4.0f * lastY);
        const float g = dh::fastTan (kPi * clampf (fc, 8.0f, 0.45f * fs) / fs);
        const float r = 0.545f;                                            // C18 / C = 0,018 / 0,033
        const float G1 = g / (2.0f * r), G = g * 0.5f;
        hpFb.setHz (9.0f, fs);
        // Ganancias no lineales evaluadas en el estado anterior.
        const float tu = tdx (uPrev), t12 = tdx (y1 - y2), t23 = tdx (y2 - y3), t34 = tdx (y3 - y4), t4 = tdx (y4);
        // Sistema: tridiagonal en y1..y4 con la realimentación u = x − k·hp(y4) (hp afín: a·y4 + b).
        const float ha = hpFb.a(), hb = hpFb.b();
        const float d1 = 1.0f + G1 * t12, e1 = -G1 * t12, f1 = G1 * tu * k * ha, r1 = s1 + G1 * tu * (x - k * hb);
        const float c2 = -G * t12, d2 = 1.0f + G * (t12 + t23), e2 = -G * t23, r2 = s2;
        const float c3 = -G * t23, d3 = 1.0f + G * (t23 + t34), e3 = -G * t34, r3 = s3;
        const float c4 = -G * t34, d4 = 1.0f + G * (t34 + t4), r4 = s4;
        // Eliminación hacia adelante (todos los pivotes ≥ 1: bien condicionado).
        const float m2 = c2 / d1;
        const float D2 = d2 - m2 * e1, F2 = -m2 * f1, R2 = r2 - m2 * r1;
        const float m3 = c3 / D2;
        const float D3 = d3 - m3 * e2, F3 = e3 - m3 * F2, R3 = r3 - m3 * R2;
        const float m4 = c4 / D3;
        const float D4 = d4 - m4 * F3, R4 = r4 - m4 * R3;
        y4 = R4 / D4;
        y3 = (R3 - F3 * y4) / D3;
        y2 = (R2 - e2 * y3 - F2 * y4) / D2;
        y1 = (r1 - e1 * y2 - f1 * y4) / d1;
        const float fb = hpFb.tick (y4);
        const float u = x - k * fb;
        uPrev = u;
        // Estados TPT: s ← 2y − s.
        s1 = 2.0f * y1 - s1; s2 = 2.0f * y2 - s2; s3 = 2.0f * y3 - s3; s4 = 2.0f * y4 - s4;
        if (! dh::isFiniteF (s4) || ! dh::isFiniteF (s1)) { s1 = s2 = s3 = s4 = y1 = y2 = y3 = y4 = 0; hpFb.reset(); }
        return y4 * 2.0f;
    }

    // tanh(x)/x por aproximante de Padé (exacta en 0).
    static inline float tdx (float x)
    {
        const float a = x * x;
        return ((a + 105.0f) * a + 945.0f) / ((15.0f * a + 420.0f) * a + 945.0f);
    }
    static inline float polyBlep (float t, float dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }
};

} // namespace dh::tb

#pragma once
// Módulos de control del MS-20: EG1, EG2, MG, S/H, VCA de modulación y ESP.
// Los tiempos llegan ya convertidos a segundos/Hz desde Params.h (curvas de las perillas).

#include "Common.h"
#include <array>

namespace dh {

// Etapa RC: se acerca exponencialmente a un objetivo. El ataque apunta por encima de 1 y corta
// al llegar a 1 (como el comparador del circuito), así la curva de ataque es la cóncava típica.
struct RcStage
{
    static inline float coef (float seconds, float fs)
    {
        return 1.0f - std::exp (-1.0f / std::max (1.0e-5f, seconds * fs));
    }
};

// EG1: DELAY TIME → ATTACK → (sostiene a máximo mientras hay gate) → RELEASE.
struct Eg1
{
    enum Stage { Idle, Delay, Attack, Hold, Release };
    Stage stage = Idle;
    float y = 0.0f, delayLeft = 0.0f;
    bool gate = false;

    inline void trigger (float delaySec)
    {
        gate = true;
        delayLeft = delaySec;
        stage = delaySec > 1e-4f ? Delay : Attack;
    }
    inline void release() { gate = false; if (stage != Idle) stage = Release; }

    inline float process (float attackSec, float releaseSec, float fs)
    {
        switch (stage)
        {
            case Idle: break;
            case Delay:
                delayLeft -= 1.0f / fs;
                if (delayLeft <= 0.0f) stage = Attack;
                break;
            case Attack:
                y += (1.25f - y) * RcStage::coef (attackSec * 0.72f, fs);
                if (y >= 1.0f) { y = 1.0f; stage = Hold; }
                break;
            case Hold: y = 1.0f; break;
            case Release:
                y += (0.0f - y) * RcStage::coef (releaseSec * 0.25f, fs);
                if (y < 1e-5f) { y = 0.0f; stage = Idle; }
                break;
        }
        return y;
    }
};

// EG2: HOLD, ATTACK, DECAY, SUSTAIN, RELEASE. HOLD alarga el gate: aunque se suelte la tecla,
// la envolvente sigue en ataque/decaimiento/sostén hasta cumplir el tiempo de hold.
struct Eg2
{
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Idle;
    float y = 0.0f, holdSec = 0.0f, holdElapsed = 1.0e9f;
    bool keyDown = false;

    inline void trigger (float hold)
    {
        keyDown = true;
        holdSec = hold; holdElapsed = 0.0f;
        stage = Attack;
    }
    inline void release() { keyDown = false; }
    // HOLD es un temporizador desde el disparo que sigue a la perilla en vivo (como el RC del original): si se
    // baja mientras corre, se acorta.
    inline void setHold (float hold) { holdSec = hold; }

    inline float process (float attackSec, float decaySec, float sustain, float releaseSec, float fs)
    {
        if (holdElapsed < holdSec) holdElapsed += 1.0f / fs;
        const bool gateOn = keyDown || holdElapsed < holdSec;
        if (! gateOn && stage != Idle && stage != Release) stage = Release;

        switch (stage)
        {
            case Idle: break;
            case Attack:
                y += (1.25f - y) * RcStage::coef (attackSec * 0.72f, fs);
                if (y >= 1.0f) { y = 1.0f; stage = Decay; }
                break;
            case Decay:
                y += (sustain - y) * RcStage::coef (decaySec * 0.25f, fs);
                if (std::fabs (y - sustain) < 1e-4f) stage = Sustain;
                break;
            case Sustain: y += (sustain - y) * 0.001f; break;
            case Release:
                y += (0.0f - y) * RcStage::coef (releaseSec * 0.25f, fs);
                if (y < 1e-5f) { y = 0.0f; stage = Idle; }
                break;
        }
        return y;
    }
};

// MG: generador de modulación con simetría variable. WAVE FORM en el centro = triángulo; hacia
// un extremo sierra descendente, hacia el otro ascendente. La salida cuadrada es un pulso cuyo
// ancho sigue a la simetría (sale del mismo comparador).
struct Mg
{
    float phase = 0.0f;
    float tri = 0.0f, pulse = 0.0f;
    bool risingEdge = false;

    inline void process (float hz, float symmetry01, float fs)
    {
        // Asimetría hasta ~1:80 en los extremos (manual de servicio).
        const float w = 1.0f / 81.0f + clampf (symmetry01, 0.0f, 1.0f) * (79.0f / 81.0f);
        phase += hz / fs;
        risingEdge = false;
        if (phase >= 1.0f) { phase -= std::floor (phase); risingEdge = true; }
        tri = phase < w ? (2.0f * phase / w - 1.0f) : (1.0f - 2.0f * (phase - w) / (1.0f - w));
        pulse = phase < w ? 1.0f : -1.0f;
    }
};

// Entrada S-trig del MS-20: se activa cuando la tensión cae a masa (≈ 0 V) y se libera al
// volver arriba. Con histéresis: activa por debajo de 1,5 V, libre por encima de 2,5 V.
struct STrigInput
{
    bool active = false;
    inline bool update (float v)
    {
        if (! active && v < 0.3f) active = true;
        else if (active && v > 0.5f) active = false;
        return active;
    }
};

// S/H: muestrea cuando el reloj S-trig se va a masa (flanco descendente de un 0/+5 V).
struct SampleHold
{
    float held = 0.0f;
    STrigInput clock;
    inline float process (float in, float clk)
    {
        const bool was = clock.active;
        if (clock.update (clk) && ! was) held = in;
        return held;
    }
};

// ESP (External Signal Processor): preamplificador con "auto pad", pasa banda (LOW CUT 50–2500 Hz,
// HIGH CUT 100–5000 Hz, 12 dB/oct cada lado), convertidor F-V (Hz/V, como el CV del teclado),
// seguidor de envolvente (ataque ~1 ms, caída ~50 ms) y disparador Schmitt con salida S-trig.
struct Esp
{
    OnePoleLP hpA, hpB, lpA, lpB;
    float env = 0.0f;
    static constexpr int kRing = 1024, kWin = 512, kHop = 192;
    std::array<float, kRing> ring {};
    int ringPos = 0, decCount = 0, decFactor = 16, sinceAnalysis = 0;
    float decAcc = 0.0f;
    bool trackPitch = false;
    std::array<float, kRing> lin {}, dbuf {};

    // YIN (de Cheveigné & Kawahara 2002) sobre los últimos kRing puntos del anillo.
    void analyse (float rate)
    {
        for (int i = 0; i < kRing; ++i) lin[(size_t) i] = ring[(size_t) ((ringPos + i) % kRing)];
        const int maxLag = std::min (kRing - kWin - 1, (int) (rate / 40.0f));
        const int minLag = std::max (2, (int) (rate / 1200.0f));
        float running = 0.0f;
        dbuf[0] = 1.0f;
        int best = -1;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            float d = 0.0f;
            const float* a = lin.data() + (kRing - kWin - maxLag);
            for (int j = 0; j < kWin; ++j) { const float e = a[j] - a[j + tau]; d += e * e; }
            running += d;
            dbuf[(size_t) tau] = running > 0.0f ? d * (float) tau / running : 1.0f;
            if (best < 0 && tau > minLag + 1 && dbuf[(size_t) tau - 1] < 0.15f && dbuf[(size_t) tau - 1] <= dbuf[(size_t) tau])
                best = tau - 1;
        }
        if (best < 0) return;   // sin periodicidad clara: se sostiene el último valor
        const float y0 = dbuf[(size_t) best - 1], y1 = dbuf[(size_t) best], y2 = dbuf[(size_t) best + 1];
        const float den = y0 - 2.0f * y1 + y2;
        const float shift = std::fabs (den) > 1e-9f ? 0.5f * (y0 - y2) / den : 0.0f;
        hz = rate / ((float) best + clampf (shift, -0.5f, 0.5f));
    }
    float hz = 0.0f;
    OnePoleLP fvSmooth;
    bool trigHigh = false;
    float attackCoef = 0.0f, releaseCoef = 0.0f;

    float ampOut = 0, bpfOut = 0, envOut = 0, fvOut = 0, trigOut = 1.0f;
    float peak = 0.0f;

    void prepare (float fs)
    {
        fvSmooth.setCutoff (25.0f, fs);
        decFactor = std::max (1, (int) std::lround (fs / 12000.0f));
        attackCoef = 1.0f - std::exp (-1.0f / (0.001f * fs));
        releaseCoef = 1.0f - std::exp (-1.0f / (0.050f * fs));
    }

    inline void process (float in, float levelGain, float lowCutHz, float highCutHz,
                         float cvAdjust01, float threshold01, float fs)
    {
        const float pre = in * levelGain;
        peak = std::max (peak * 0.9999f, std::fabs (pre));
        ampOut = fastTanh (pre * 0.5f) * 2.0f;          // el preamp recorta suave a ±2 (±10 V)

        hpA.setCutoff (lowCutHz, fs); hpB.setCutoff (lowCutHz, fs);
        lpA.setCutoff (highCutHz, fs); lpB.setCutoff (highCutHz, fs);
        float x = ampOut - hpA.process (ampOut);
        x = x - hpB.process (x);
        x = lpA.process (x);
        bpfOut = lpB.process (x);

        // El seguidor toma la salida del preamp (en paralelo al pasa banda).
        const float r = std::fabs (ampOut);
        env += (r - env) * (r > env ? attackCoef : releaseCoef);
        envOut = clampf (env * 1.4f, 0.0f, 1.0f);

        // F-V: detector de período YIN sobre el pasa banda decimado a ~12 kHz (sólo si la salida
        // F∝V está patcheada). Un cruce por cero no alcanza con señales reales: los armónicos
        // de una cuerda suelen superar a la fundamental. La salida conserva el retardo del original.
        if (trackPitch)
        {
            decAcc += bpfOut;
            if (++decCount >= decFactor)
            {
                ring[(size_t) ringPos] = decAcc / (float) decFactor;
                ringPos = (ringPos + 1) % kRing;
                decAcc = 0.0f; decCount = 0;
                if (++sinceAnalysis >= kHop) { sinceAnalysis = 0; if (env > 0.01f) analyse (fs / (float) decFactor); }
            }
        }
        // CV ADJUST: unidad en 10 (las cartas de fábrica lo usan ahí); en Hz/V escalar transpone.
        fvOut = fvSmooth.process (hz / (32.7032f * 5.0f) * (0.25f + 0.75f * cvAdjust01));

        // Schmitt sobre la envolvente; salida S-trig (+5 V en reposo, masa al disparar).
        const float trigTh = 0.03f + threshold01 * 0.85f;
        if (! trigHigh && envOut > trigTh) trigHigh = true;
        else if (trigHigh && envOut < trigTh * 0.85f) trigHigh = false;
        trigOut = trigHigh ? 0.0f : 1.0f;
    }
};

} // namespace dh

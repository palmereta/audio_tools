#pragma once
// 717A ANALOG SEQUENCER: 3 canales × 12 pasos (36 en serie), reloj de 2–20 pasos/minuto (×1/×10)
// calibrado con DIVIDE y FINE en "1"; cada paso dura DIVIDE (1/32…1) × FINE (1…2) de un "entero".
// HOLD repite el registro anterior y calla el CLOCK OUT de ese paso (silencio); el GATE OUT del paso
// sale igual. Con el secuenciador parado, el GATE OUT del paso encendido queda en +15 V. ANALOG IN
// convierte los registros del paso en controles de nivel de esa señal (llave electrónica).
//
// Reloj: interno (en tiempo), externo (cada pulso es 1/32 de entero, el divisor del original) o el
// del host (extensión: 1 entero = una redonda; el paso cae en la grilla del DAW).

#include "Common.h"
#include <array>

namespace dh {

struct Seq717
{
    // Panel (por bloque).
    std::array<std::array<float, 12>, 3> reg {};       // 0..1
    std::array<float, 12> divide {}, fine {};          // fracción de entero, 1..2
    std::array<bool, 12> hold {};
    std::array<float, 3> range { 10, 10, 10 };
    float rate = 6.0f, pw = 0.5f;                      // pasos/minuto (ya con ×1/×10)
    int serMode = 0, steps = 12;                       // 0 A, 1 A-B, 2 A-B-C, 3 MANUAL
    bool repeat = true;

    // Estado.
    bool running = false;
    int step = 0, chan = 0, manualChan = 0;
    double pos = 0.0;                                  // fracción del paso transcurrida (reloj interno/host)
    int ticks = 0;                                     // pulsos externos en el paso
    std::array<float, 3> held {};                      // última tensión de cada canal (para HOLD)
    float startPulse = 0.0f;                           // segundos restantes del pulso de START
    float lastExt = 0.0f;
    std::array<float, 4> lastCp {};

    // Salidas (V).
    float clockOut = 0.0f, startOut = 0.0f, endOut = 15.0f, serOut = 0.0f;
    std::array<float, 3> par {};
    std::array<float, 12> gateOut {};

    double stepLenWholes (int s) const { return (double) divide[(size_t) s] * (double) fine[(size_t) s]; }

    void start() { step = 0; chan = 0; pos = 0.0; ticks = 0; running = true; startPulse = 0.005f; }
    void cont() { running = true; }
    void stop() { running = false; }
    void end() { running = false; step = 0; chan = 0; pos = 0.0; ticks = 0; }
    void manual (int s) { step = juce_clampi (s, 0, 11); pos = 0.0; ticks = 0; }
    static int juce_clampi (int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

    int channels() const { return serMode == 0 ? 1 : serMode == 1 ? 2 : serMode == 2 ? 3 : 1; }

    // Avanza conservando lo que sobró del paso (si no, cada paso se alargaba una muestra y el secuenciador se
    // atrasaba respecto del DAW: ~100 ms cada 10 minutos).
    void advanceKeeping (double len)
    {
        const double overWholes = (pos - 1.0) * len;
        advance();
        pos = std::max (0.0, overWholes / std::max (1.0e-4, stepLenWholes (step)));
    }

    void advance()
    {
        pos = 0.0; ticks = 0;
        if (++step >= steps)
        {
            step = 0;
            if (serMode == 3) return;
            if (++chan >= channels())
            {
                chan = 0;
                if (! repeat) { running = false; step = steps - 1; chan = channels() - 1; }
            }
        }
    }

    // Una vez por muestra base. ext: valor del EXT CLOCK si está patcheado (NAN si no);
    // cv: CONTROL VOLTAGE IN; cps: los 4 CONTROL PULSE IN; hostPpqDelta ≥ 0 si sigue al host.
    inline void process (float ext, bool extPatched, float cv, const std::array<float, 4>& cps, double hostPpqDelta, bool hostRun, float dt)
    {
        // Pulsos de control (+7 V): START, CONTINUE, STOP, END.
        for (int i = 0; i < 4; ++i)
        {
            if (lastCp[(size_t) i] < 7.0f && cps[(size_t) i] >= 7.0f) { if (i == 0) start(); else if (i == 1) cont(); else if (i == 2) stop(); else end(); }
            lastCp[(size_t) i] = cps[(size_t) i];
        }
        const double len = std::max (1.0e-4, stepLenWholes (step));
        if (running)
        {
            if (hostPpqDelta >= 0.0)
            {
                if (hostRun) pos += hostPpqDelta / (4.0 * len);
                if (pos >= 1.0) advanceKeeping (len);
            }
            else if (extPatched)
            {
                if (lastExt < 5.0f && ext >= 5.0f && ++ticks >= std::max (1, (int) std::lround (32.0 * len))) advance();
                else if (ticks > 0) pos = (double) ticks / std::max (1.0, 32.0 * len);
            }
            else
            {
                const double whole = 60.0 / std::max (0.01, (double) rate * (double) fastExp2 (clampf (cv, -5.0f, 5.0f)));
                pos += (double) dt / (whole * len);
                if (pos >= 1.0) advanceKeeping (len);
            }
        }
        lastExt = ext;

        // Registros y salidas.
        const int s = step;
        const bool isHold = hold[(size_t) s];
        for (int c = 0; c < 3; ++c)
        {
            if (! isHold) held[(size_t) c] = reg[(size_t) c][(size_t) s];
            par[(size_t) c] = held[(size_t) c] * range[(size_t) c];
        }
        const int sc = serMode == 3 ? manualChan : chan;
        serOut = par[(size_t) sc];
        // Con reloj externo la salida sigue además al pulso que entra: con un pulso por paso (DIVIDE 1/32) pos vuelve
        // a 0 en cada pulso y CLOCK OUT quedaba siempre alto.
        const bool extClock = extPatched && hostPpqDelta < 0.0;
        const bool high = pos < (double) pw && (! extClock || ext >= 5.0f);
        clockOut = running && ! isHold && high ? 15.0f : 0.0f;
        for (int i = 0; i < 12; ++i) gateOut[(size_t) i] = (i == s && (! running || high)) ? 15.0f : 0.0f;
        if (startPulse > 0.0f) { startPulse -= dt; startOut = 15.0f; } else startOut = 0.0f;
        endOut = running ? 0.0f : 15.0f;
    }
};

// Pulse shaper: comparador con umbral y monoestable de 0–3 s.
struct PulseShaper717
{
    float last = 0.0f, remain = 0.0f, out = 0.0f;
    inline float process (float in, float threshold, float onTime, float dt)
    {
        if (last < threshold && in >= threshold) remain = std::max (0.004f, onTime);
        last = in;
        if (remain > 0.0f) { remain -= dt; out = 15.0f; } else out = 0.0f;
        return out;
    }
};

} // namespace dh

#pragma once
// VCO del SH-101: un núcleo de sierra; el pulso sale de un comparador contra el PW y el
// sub-oscilador de dos flip-flops que cuentan los resets de la sierra (−1 oct, −2 oct, y el
// pulso de 25 % a −2 oct como AND de ambos). Todas las formas comparten el instante de reset,
// así que cada una se limita en banda con PolyBLEP sobre su propia fase.

#include "Blep.h"
#include "Common.h"

namespace dh {

class Vco101
{
public:
    enum SubWave { SubOct1Square = 0, SubOct2Square, SubOct2Pulse };

    float saw = 0, pulse = 0, sub = 0, noise = 0;

    void setFreq (float hz, float fs) { dt = clampf (hz / fs, 0.0f, 0.45f); }
    void seed (uint32_t s, float startPhase) { rng = Rng (s); phase = startPhase; }

    // width: ancho del pulso (0.5 = cuadrada). subWave: SubWave.
    inline void process (float width, int subWave)
    {
        // Sierra ascendente con el leve redondeo del reset del integrador.
        saw = 2.0f * phase - 1.0f - polyBlep (phase, dt);

        const float w = clampf (width, 2.0f * dt, 0.5f);
        float p = phase < w ? 1.0f : -1.0f;
        p += polyBlep (phase, dt);
        float t2 = phase - w; if (t2 < 0.0f) t2 += 1.0f;
        p -= polyBlep (t2, dt);
        pulse = p - (2.0f * w - 1.0f);        // acople AC

        // Sub: fase de −1 oct = (c&1 + fase)/2; de −2 oct = (c + fase)/4.
        if (subWave == SubOct1Square)
        {
            const float ph = ((float) (count & 1) + phase) * 0.5f;
            sub = square (ph, dt * 0.5f, 0.5f);
        }
        else
        {
            const float ph = ((float) count + phase) * 0.25f;
            sub = square (ph, dt * 0.25f, subWave == SubOct2Square ? 0.5f : 0.25f);
        }

        noise = rng.bipolar();

        phase += dt;
        if (phase >= 1.0f) { phase -= 1.0f; count = (count + 1) & 3; }
    }

private:
    static inline float square (float ph, float d, float w)
    {
        float v = ph < w ? 1.0f : -1.0f;
        v += polyBlep (ph, d);
        float t = ph - w; if (t < 0.0f) t += 1.0f;
        v -= polyBlep (t, d);
        return v - (2.0f * w - 1.0f);
    }

    float phase = 0.0f, dt = 0.0f;
    int count = 0;
    Rng rng { 0x101u };
};

} // namespace dh

#pragma once
// VCO del MS-20. El núcleo de cada VCO es una rampa (integrador + reset), de la que salen el
// triángulo (rectificación) y los pulsos (comparador). Se generan con PolyBLEP/PolyBLAMP y el
// motor corre sobremuestreado, así que el aliasing queda por debajo del piso de ruido.

#include "Common.h"
#include "Blep.h"

namespace dh {

struct RampCore
{
    float phase = 0.0f;   // [0,1)
    float dt = 0.0f;      // incremento por muestra
    bool wrapped = false;

    inline void setFreq (float hz, float fs) { dt = clampf (hz / fs, 0.0f, 0.45f); }
    inline void advance()
    {
        phase += dt;
        wrapped = phase >= 1.0f;
        if (wrapped) phase -= 1.0f;
    }

    // Diente de sierra descendente (la serigrafía del MS-20 dibuja subida vertical y caída en
    // rampa), con la curvatura leve del integrador real (~2 %) y el salto suavizado por BLEP.
    inline float saw (float curve = 0.02f) const
    {
        float s = 1.0f - 2.0f * phase;
        s += curve * (1.0f - s * s);
        return s + polyBlep (phase, dt);
    }

    // Pulso ±1 sin corregir el nivel medio (para el XOR del ring).
    inline float pulseRaw (float width) const
    {
        width = clampf (width, 2.0f * dt, 1.0f - 2.0f * dt);
        float p = phase < width ? 1.0f : -1.0f;
        p += polyBlep (phase, dt);
        float t2 = phase - width; if (t2 < 0.0f) t2 += 1.0f;
        p -= polyBlep (t2, dt);
        return p;
    }

    // Nivel medio cero, como sale del acople AC.
    inline float pulse (float width) const
    {
        const float w = clampf (width, 2.0f * dt, 1.0f - 2.0f * dt);
        return pulseRaw (w) - (2.0f * w - 1.0f);
    }

    inline float triangle() const
    {
        float t = 1.0f - 4.0f * std::fabs (phase - 0.5f);   // -1 en 0, +1 en 0.5
        t = -t;                                             // arranca en +1, baja a -1 en 0.5
        // Vértice superior en fase 0 (la pendiente cae 8·dt por muestra) e inferior en 0.5.
        t -= 4.0f * dt * polyBlamp (phase, dt);
        float t2 = phase + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f;
        t += 4.0f * dt * polyBlamp (t2, dt);
        return t;
    }
};

// VCO1: triángulo, sierra, pulso de ancho variable, ruido blanco.
struct Vco1
{
    enum Wave { Triangle = 0, Saw, Pulse, Noise };
    RampCore core;
    Rng rng { 0x1234567u };

    // Devuelve la forma seleccionada y deja el pulso con PW (para el ring) en pulseOut.
    float pulseOut = 0.0f;

    inline float process (int wave, float pwKnob01)
    {
        // PW: en 0 cuadrada (1:1); hacia 10 el pulso se angosta hasta desaparecer (1:∞).
        const float w = 0.5f * (1.0f - clampf (pwKnob01, 0.0f, 1.0f));
        const float minW = 2.0f * core.dt;
        const float fade = w < minW ? w / minW : 1.0f;
        pulseOut = core.pulseRaw (std::max (w, minW)) * fade + (fade - 1.0f);
        float out;
        switch (wave)
        {
            case Triangle: out = core.triangle(); break;
            case Saw:      out = core.saw(); break;
            case Pulse:    out = core.pulse (std::max (w, minW)) * fade; break;
            default:       out = rng.bipolar() * 0.85f; break;
        }
        core.advance();
        return out;
    }
};

// VCO2: sierra, cuadrada, pulso angosto fijo y modulador de anillo (VCO1 x VCO2).
struct Vco2
{
    enum Wave { Saw = 0, Square, NarrowPulse, Ring };
    RampCore core;

    inline float process (int wave, float vco1Pulse)
    {
        float out;
        switch (wave)
        {
            case Saw:         out = core.saw(); break;
            case Square:      out = core.pulse (0.5f); break;
            case NarrowPulse: out = core.pulse (0.12f); break;
            default:
                // RING del MS-20: XOR del pulso de VCO1 (con su PW) y la cuadrada de VCO2.
                // Con señales ±1, XOR = −(a·b); el producto de dos pulsos con BLEP queda limitado
                // en banda en cada flanco de cualquiera de los dos.
                out = -(vco1Pulse * core.pulseRaw (0.5f));
                break;
        }
        core.advance();
        return out;
    }
};

// Ruido blanco y rosa (filtro de Paul Kellet, -3 dB/oct).
struct NoiseGen
{
    Rng rng { 0xBEEFu };
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    float white = 0.0f, pink = 0.0f;

    inline void process()
    {
        white = rng.bipolar();
        b0 = 0.99886f * b0 + white * 0.0555179f;
        b1 = 0.99332f * b1 + white * 0.0750759f;
        b2 = 0.96900f * b2 + white * 0.1538520f;
        b3 = 0.86650f * b3 + white * 0.3104856f;
        b4 = 0.55000f * b4 + white * 0.5329522f;
        b5 = -0.7616f * b5 - white * 0.0168980f;
        pink = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f) * 0.18f;
        b6 = white * 0.115926f;
    }
};

} // namespace dh

#pragma once
// Generador de contorno del Model D: ATTACK, DECAY y SUSTAIN. No tiene release propio: al soltar la tecla,
// con el interruptor DECAY encendido el nivel cae con el tiempo de DECAY; apagado, cae en unos milisegundos.
// Es un capacitor que se carga y descarga por resistencias: el ataque apunta a 9.3 V y se corta en 5 V (sube
// casi recto y se corta en el tope), el decay y la caída son exponenciales. El ataque arranca del nivel en que
// esté (no vuelve a cero).

#include "Common.h"

namespace dh::md {

// Potenciómetro logarítmico ("audio"): fracción de la resistencia total para la perilla 0..10, con el 10 %
// a mitad de recorrido como los de carbón de la época.
inline float audioTaper (float knob)
{
    const float d = clampf (knob, 0.0f, 10.0f) / 10.0f;
    constexpr float k = 4.6f;                 // e^(k/2) ≈ 10 → 10 % en la mitad
    return (std::exp (k * d) - 1.0f) / (std::exp (k) - 1.0f);
}

// Constante de tiempo de un segmento: (100 Ω + pote de 1 MΩ) × 10 µF [spec §5] → 1 ms .. 10 s.
inline float contourTau (float knob) { return 0.001f + 10.0f * audioTaper (knob); }

class Contour
{
public:
    enum Stage { Idle, Attack, Decay, Release };

    void reset() { y = 0.0f; stage = Idle; }

    // Constantes de tiempo (s) de ataque y decay; sustain 0..1; decayOn = interruptor DECAY.
    void set (float tauAttack, float tauDecay, float sustain, bool decayOn, float fs)
    {
        aCoef = 1.0f - std::exp (-1.0f / std::max (1.0f, tauAttack * fs));
        dCoef = 1.0f - std::exp (-1.0f / std::max (1.0f, tauDecay * fs));
        // Sin DECAY la nota cae con τ = 15 ms, o con la del decay si es más corta [spec §5].
        rCoef = decayOn ? dCoef : 1.0f - std::exp (-1.0f / std::max (1.0f, std::min (kShortRelease, tauDecay) * fs));
        sus = clampf (sustain, 0.0f, 1.0f);
    }

    void gateOn (bool retrigger) { if (retrigger || stage == Idle || stage == Release) stage = Attack; }
    void gateOff() { if (stage != Idle) stage = Release; }
    bool active() const { return stage != Idle; }
    Stage now() const { return stage; }
    float value() const { return y; }

    inline float process()
    {
        switch (stage)
        {
            case Attack:
                y += (kAttackTarget - y) * aCoef;
                if (y >= 1.0f) { y = 1.0f; stage = Decay; }
                break;
            case Decay:   y += (sus - y) * dCoef; break;
            case Release:
                y += (0.0f - y) * rCoef;
                if (y < 1.0e-5f) { y = 0.0f; stage = Idle; }
                break;
            case Idle: break;
        }
        return y;
    }

private:
    static constexpr float kAttackTarget = 9.3f / 5.0f;
    static constexpr float kShortRelease = 0.015f;   // τ con DECAY apagado
    Stage stage = Idle;
    float y = 0.0f, aCoef = 0.01f, dCoef = 0.001f, rCoef = 0.01f, sus = 1.0f;
};

} // namespace dh::md

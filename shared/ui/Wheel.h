#pragma once
// Rueda del mouse y del trackpad para los controles. Un desplazamiento sólo horizontal (deltaY = 0) no mueve nada
// (antes bajaba el valor), el trackpad manda muchos deltas chicos y una muesca de rueda uno grande: los controles
// continuos avanzan en proporción al delta y los que tienen pasos acumulan hasta completar uno.

#include <JuceHeader.h>

namespace dh {

inline float wheelDelta (const juce::MouseWheelDetails& w) { return (w.isReversed ? -1.0f : 1.0f) * w.deltaY; }

// Continuo: el cambio normalizado (una muesca típica, deltaY ≈ 0,1, mueve ~2,5 %).
inline float wheelAmount (const juce::MouseWheelDetails& w) { return 0.25f * wheelDelta (w); }

// Con pasos: cuántos pasos mover en este evento (±), acumulando lo que sobra.
struct WheelSteps
{
    float acc = 0.0f;
    int take (const juce::MouseWheelDetails& w, float perStep = 0.1f)
    {
        acc += wheelDelta (w);
        int s = 0;
        while (acc >= perStep) { ++s; acc -= perStep; }
        while (acc <= -perStep) { --s; acc += perStep; }
        return s;
    }
};

} // namespace dh

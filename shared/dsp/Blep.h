#pragma once
// PolyBLEP / PolyBLAMP de 2 muestras: corrigen los saltos de valor y de pendiente de las formas de
// onda ingenuas. Con el motor sobremuestreado, el aliasing queda por debajo del piso de ruido.

#include "Common.h"

namespace dh {

// Corrección de discontinuidad de escalón (PolyBLEP de 2 muestras).
inline float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

// Corrección de discontinuidad de pendiente (PolyBLAMP de 2 muestras), integrado de polyBlep.
inline float polyBlamp (float t, float dt)
{
    if (t < dt) { t = t / dt - 1.0f; return -(1.0f / 3.0f) * t * t * t; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt + 1.0f; return (1.0f / 3.0f) * t * t * t; }
    return 0.0f;
}

} // namespace dh

#pragma once
// El filtro escalera de transistores del Model D: cuatro polos de 6 dB/oct en cascada con realimentación
// negativa del último al primero; cada etapa es un par diferencial (una tanh).
//
// Resuelto sin retardo en el lazo (topología TPT de Zavalishin, "The Art of VA Filter Design"): cada
// integrador trapezoidal, la realimentación resuelta en forma cerrada y las tanh linealizadas alrededor del
// estado actual (tanh(x)/x evaluado en la muestra, el método de "cheap non-linear zero-delay filters" de
// Teemu Voipio). Así el corte queda afinado por el prewarp en todo el rango, la autooscilación aparece en
// k = 4 a cualquier frecuencia y la saturación de cada etapa está en el lazo, como en el circuito.
//
// Lo que sale del modelo sin agregarlo aparte: la pérdida de graves al subir la resonancia (1/(1+k) en
// continua), la senoidal casi pura de la autooscilación y la compresión de la resonancia con señal fuerte.

#include "Common.h"

namespace dh::md {

class Ladder
{
public:
    void reset() { s0 = s1 = s2 = s3 = 0.0f; }

    // fc en Hz, res 0..1.1 (1 = umbral de autooscilación), fs la frecuencia de trabajo.
    inline void set (float fc, float res, float fs)
    {
        f = fastTan (kPi * clampf (fc, 5.0f, 0.46f * fs) / fs);     // fastTan: la tan de la CRT de Windows es varias veces más lenta
        k = 4.0f * clampf (res, 0.0f, 1.15f);
    }

    // x: entrada en el nivel nominal del mezclador (±1). Devuelve la salida de 24 dB/oct.
    inline float process (float x)
    {
        const float in = x * kDrive;
        // Ganancias no lineales alrededor del estado actual.
        const float t0 = tanhXdX (in - k * s3);
        const float t1 = tanhXdX (s0), t2 = tanhXdX (s1), t3 = tanhXdX (s2), t4 = tanhXdX (s3);
        const float g0 = 1.0f / (1.0f + f * t1), g1 = 1.0f / (1.0f + f * t2), g2 = 1.0f / (1.0f + f * t3), g3 = 1.0f / (1.0f + f * t4);
        const float f3 = f * t3 * g3, f2 = f * t2 * g2 * f3, f1 = f * t1 * g1 * f2, f0 = f * t0 * g0 * f1;
        // La salida resuelve el lazo de realimentación en forma cerrada.
        const float y3 = (g3 * s3 + f3 * g2 * s2 + f2 * g1 * s1 + f1 * g0 * s0 + f0 * in) / (1.0f + k * f0);
        const float xx = t0 * (in - k * y3);
        const float y0 = t1 * g0 * (s0 + f * xx);
        const float y1 = t2 * g1 * (s1 + f * y0);
        const float y2 = t3 * g2 * (s2 + f * y1);
        s0 += 2.0f * f * (xx - y0);
        s1 += 2.0f * f * (y0 - y1);
        s2 += 2.0f * f * (y1 - y2);
        s3 += 2.0f * f * (y2 - t4 * y3);
        if (! isFiniteF (s3) || ! isFiniteF (s0)) reset();
        return y3 * (1.0f / kDrive);
    }

    // Nivel interno: el nominal del mezclador entra apenas en la zona curva de los pares diferenciales.
    static constexpr float kDrive = 1.6f;

private:
    float f = 0.1f, k = 0.0f;
    float s0 = 0, s1 = 0, s2 = 0, s3 = 0;

    // tanh(x)/x por aproximante de Padé (exacta en 0, cae como 1/|x| lejos).
    static inline float tanhXdX (float x)
    {
        const float a = x * x;
        return ((a + 105.0f) * a + 945.0f) / ((15.0f * a + 420.0f) * a + 945.0f);
    }
};

} // namespace dh::md

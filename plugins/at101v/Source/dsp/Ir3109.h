#pragma once
// Filtro del SH-101: IR3109, cuatro etapas OTA en cascada (24 dB/oct) con realimentación global.
//
// Cada etapa es un integrador OTA: dy/dt = ωc·(tanh(x) − tanh(y)). En forma TPT (trapezoidal) con
// la tanh linealizada alrededor de la muestra anterior (τ(a) = tanh(a)/a, método "cheap ZDF" de
// Mystran), cada etapa queda afín en su entrada y la realimentación se resuelve en forma cerrada:
// sin retardo de una muestra en el lazo, así la resonancia afina y autooscila donde corresponde.
//
// La realimentación del IR3109 resta la salida a la entrada: con resonancia alta la banda pasante
// pierde graves (ganancia 1/(1+k) en continua), uno de los rasgos del sonido del 101. En el lazo hay
// dos diodos en antiparalelo (D30/D31) que recortan la realimentación: limitan la amplitud de la
// autooscilación y le dan la distorsión suave. Se linealizan igual que las etapas (τ de la muestra
// anterior). El 101 compensa la pérdida subiendo la salida, no la entrada (bassComp ≈ 0,5).

#include "Common.h"

namespace dh {

class Ir3109
{
public:
    void reset() { for (auto& v : s) v = 0.0f; for (auto& v : tin) v = 1.0f; for (auto& v : tout) v = 1.0f; y4 = 0.0f; tfb = 1.0f; }

    // fc en Hz, k de 0 a ~4.4 (autooscila un poco arriba de 4), fs sobremuestreada.
    // bassComp 0..1 devuelve parte del grave perdido (el circuito lo compensa sólo en parte).
    inline float process (float in, float fc, float k, float fs, float drive = 1.0f, float bassComp = 0.5f)
    {
        const float g = prewarp (fc, fs);
        // Etapa i: y = (s + g·τx·x)/(1 + g·τy) = a·x + b
        float A = 1.0f, B = 0.0f, a[4], b[4];
        for (int i = 0; i < 4; ++i)
        {
            const float den = 1.0f / (1.0f + g * tout[i]);
            a[i] = g * tin[i] * den;
            b[i] = s[i] * den;
            A *= a[i];
            B = B * a[i] + b[i];
        }
        // Entrada con realimentación saturada suavemente (el sumador también es un OTA).
        const float u = in * drive;
        const float kc = k * tfb;                     // diodos del lazo, linealizados
        const float x0 = (u - kc * B) / (1.0f + kc * A);
        float x = x0;
        float y[4];
        for (int i = 0; i < 4; ++i)
        {
            y[i] = a[i] * x + b[i];
            x = y[i];
        }
        // Actualización de estados trapezoidales y de las pendientes para la próxima muestra.
        float xin = x0;
        for (int i = 0; i < 4; ++i)
        {
            const float xi = clampf (xin, -8.0f, 8.0f), yi = clampf (y[i], -8.0f, 8.0f);
            s[i] = 2.0f * y[i] - s[i];
            tin[i] = tau (xi);
            tout[i] = tau (yi);
            xin = y[i];
        }
        if (! std::isfinite (y[3])) { reset(); return 0.0f; }
        y4 = y[3];
        {
            const float v = clampf (y4, -8.0f, 8.0f) * (1.0f / kVd);
            tfb = std::fabs (v) < 1e-4f ? 1.0f : fastTanh (v) / v;
        }
        return y4 * (1.0f + bassComp * k) / drive;
    }

private:
    static inline float tau (float v)
    {
        if (std::fabs (v) < 1e-4f) return 1.0f;
        return fastTanh (v) / v;
    }
    float s[4] { 0, 0, 0, 0 }, tin[4] { 1, 1, 1, 1 }, tout[4] { 1, 1, 1, 1 };
    static constexpr float kVd = 1.15f;
    float y4 = 0.0f, tfb = 1.0f;
};

} // namespace dh

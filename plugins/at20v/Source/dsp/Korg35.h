#pragma once
// Filtros del MS-20 como filtros de retardo cero (TPT) con la no linealidad resuelta implícita.
//
// Estructura (Stinchcombe, "A study of the Korg MS10 & MS20 filters"): Sallen-Key de dos polos
// en el que la "masa" del primer capacitor la maneja la realimentación b:
//     V2 = LP1(a − b) + b,    c = LP2(V2),    b = k · N(c)
// Transferencia lineal: ω² / (s² + (2 − K)·ω·s + ω²) → autooscila en K = 2.
//
// HPF: Korg puso a masa la entrada del LP e inyectó la señal en la base de C2:
//     V2 = LP1(−b) + b,       c = LP2(V2 − x) + x
// Transferencia: (s² + ω·s) / (s² + (2 − K)·ω·s + ω²) → 6 dB/oct por debajo del corte (como el
// hardware; no es un HP de 12 dB).
//
// Revisiones:
//   Rev1 (Korg35): par de diodos sobre la etapa de salida ×58 → la distorsión crece con el nivel
//     de entrada en todo el espectro. La corriente de señal modula los transistores que hacen de
//     resistencias: el semiciclo negativo resuena más agudo (~1.7 vs 2.0 kHz).
//   Rev2 (LM13600): diodos en el lazo de realimentación (×4) → sólo se limita el pico resonante.
//
// Unidades: 1.0 = 5 V. Por muestra se resuelve F(c) = c − G·β·k·N(c) − C0 = 0 con Newton.

#include "Common.h"

namespace dh {

class Ms20Filter
{
public:
    enum Mode { Lowpass, Highpass };

    Mode mode = Lowpass;
    int revision = 1;

    void reset() { s1 = s2 = 0.0f; cPrev = 0.0f; outPrev = 0.0f; }

    // fc en Hz, peak = K del lazo lineal (0..~2.3; autooscila arriba de 2), fs sobremuestreada.
    inline float process (float x, float fc, float K, float fs)
    {
        // Rev1: la señal corre el corte (semiciclo negativo más agudo).
        if (revision == 1)
            fc *= 1.0f - kAsym * clampf (outPrev, -1.5f, 1.5f);

        const float g = prewarp (fc, fs);
        const float G = g / (1.0f + g);
        const float b_ = 1.0f - G;      // β

        float a, xin;
        if (revision == 1) { a = x * kInGain1; xin = a; }
        else               { a = -x * kInGain2; xin = a; }

        // Ganancia de realimentación: K = k1·k2 en rev1 (k1 = 58), K = 4·kp en rev2.
        const float k = revision == 1 ? K / kK1 : K / 4.0f;

        // C0: todo lo que no depende de c.
        const float C0 = mode == Lowpass ? (G * G * a + G * b_ * s1 + b_ * s2)
                                         : (G * b_ * s1 + b_ * (xin + s2));

        // Newton, arrancando de la solución con la pendiente de la muestra anterior.
        float c = cPrev;
        for (int it = 0; it < 6; ++it)
        {
            float d;
            const float n = nl (c, d);
            const float F = c - G * b_ * k * n - C0;
            const float dF = 1.0f - G * b_ * k * d;
            const float step = F / (dF > 0.02f ? dF : 0.02f);
            c -= step;
            if (std::fabs (step) < 1e-7f) break;
        }
        if (! std::isfinite (c)) { reset(); c = 0.0f; }

        float d;
        const float n = nl (c, d);
        const float fb = k * n;

        // Estados.
        if (mode == Lowpass)
        {
            const float v1 = (a - fb - s1) * G; const float lp1 = v1 + s1; s1 = lp1 + v1;
            const float V2 = lp1 + fb;
            const float v2 = (V2 - s2) * G; s2 = (v2 + s2) + v2;
        }
        else
        {
            const float v1 = (-fb - s1) * G; const float lp1 = v1 + s1; s1 = lp1 + v1;
            const float V2 = lp1 + fb;
            const float v2 = (V2 - xin - s2) * G; s2 = (v2 + s2) + v2;
        }
        cPrev = c;

        // Salida normalizada a ganancia ~1 en la banda pasante (pequeña señal).
        const float out = revision == 1 ? n * kOutGain1 : c * kOutGain2;
        outPrev = out;
        return out;
    }

    // Etapa no lineal. Rev1: diodos sobre el ×58 → Dfwd(c) = c + Vd·tanh((k1−1)c/Vd); es la salida
    // y también lo que vuelve por la realimentación. Rev2: Dfb(c) = c + Vd·tanh(3c/Vd) sólo en el lazo.
    inline float nl (float c, float& deriv) const
    {
        if (revision == 1)
        {
            const float t = fastTanh ((kK1 - 1.0f) * c / kVd1);
            deriv = 1.0f + (kK1 - 1.0f) * tanhDeriv (t);
            return c + kVd1 * t;
        }
        const float t = fastTanh (3.0f * c / kVd2);
        deriv = 1.0f + 3.0f * tanhDeriv (t);
        return c + kVd2 * t;
    }

    // Constantes de calibración (ver docs/hardware-spec.md §3.5 y §4.2).
    static constexpr float kK1 = 58.0f;          // ganancia del operacional de salida (rev1)
    static constexpr float kVd1 = 0.1f;          // caída de diodo 0,5 V en unidades de 5 V
    static constexpr float kVd2 = 0.42f;         // cadena de 3 diodos ≈ 2,1 V (rev2)
    static constexpr float kAsym = 0.075f;       // corrimiento de corte por la señal (rev1)
    // Rev1: la señal llega al núcleo a nivel de mV; con una VCO al máximo (x = 1) el ×58 entra
    // apenas en el codo de los diodos, con las dos se ensucia en serio.
    static constexpr float kInGain1 = 0.0016f;
    static constexpr float kOutGain1 = 1.0f / (kK1 * kInGain1);
    static constexpr float kInGain2 = 1.0f;
    static constexpr float kOutGain2 = -1.0f;    // rev2 invierte (entrada inversora)

private:
    float s1 = 0.0f, s2 = 0.0f, cPrev = 0.0f, outPrev = 0.0f;
};

} // namespace dh

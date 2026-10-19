#pragma once
// Utilidades comunes del motor AT20V. Sin dependencias de JUCE: el mismo código lo usan el
// plugin y la herramienta de render por línea de comandos (tools/render).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace dh {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

inline float clampf (float x, float lo, float hi) { return std::min (hi, std::max (lo, x)); }

// ¿Es finito? Por bits (exponente distinto de todos unos): con /fp:fast o -ffast-math el compilador puede
// suponer que no hay NaN y borrar un `x != x` o un std::isfinite.
inline bool isFiniteF (float x) { uint32_t u; std::memcpy (&u, &x, 4); return (u & 0x7f800000u) != 0x7f800000u; }

// Un cable de patchera: rieles de ±15 V y NaN/Inf → 0.
inline float railV (float v) { return isFiniteF (v) ? clampf (v, -15.0f, 15.0f) : 0.0f; }

// tanh racional (error < 1e-4 en todo el rango, saturación exacta a ±1).
inline float fastTanh (float x)
{
    x = clampf (x, -4.97f, 4.97f);
    const float x2 = x * x;
    const float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return a / b;
}

// Derivada de tanh a partir de su valor: 1 - tanh².
inline float tanhDeriv (float t) { return 1.0f - t * t; }

// 2^x rápido: la parte entera va directo al exponente del float y la fraccionaria por un polinomio
// de grado 5 (error relativo < 2,3e-7, 0,0004 cent). Las std::exp2/exp/tan de la CRT de Windows son
// varias veces más lentas que las de macOS, y estas funciones corren por muestra y por voz.
inline float fastExp2 (float x)
{
    x = clampf (x, -126.0f, 126.0f);
    int i = (int) x;
    if (x < (float) i) --i;
    const float f = x - (float) i;
    const float p = ((((0.00189439f * f + 0.00894063f) * f + 0.05587644f) * f + 0.24013177f) * f + 0.69315676f) * f + 0.99999977f;
    const uint32_t bits = (uint32_t) (i + 127) << 23;
    float scale;
    std::memcpy (&scale, &bits, sizeof (scale));
    return p * scale;
}

inline float fastExp (float x) { return fastExp2 (x * 1.4426950409f); }

// tan(x) para 0 ≤ x < 1,45 por fracción continua de Lambert (error relativo < 4e-9 hasta 0,45·fs).
inline float fastTan (float x)
{
    const float x2 = x * x;
    return x * (135135.0f + x2 * (-17325.0f + x2 * (378.0f - x2))) / (135135.0f + x2 * (-62370.0f + x2 * (3150.0f - 28.0f * x2)));
}

// tan(pi*f/fs) con prewarping; f ya acotado por debajo de Nyquist.
inline float prewarp (float fc, float fs)
{
    return fastTan (kPi * clampf (fc, 1.0f, 0.45f * fs) / fs);
}

// Conversión CV (octavas, 0 = C0 = 16.3516 Hz) a Hz.
inline float octToHz (float oct) { return 16.351597831287414f * std::exp2 (oct); }

// Generador xorshift32: rápido, determinista por semilla, suficiente para ruido de audio.
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    explicit Rng (uint32_t seed = 0x9E3779B9u) : s (seed ? seed : 1u) {}
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float bipolar() { return (float) (int32_t) next() * (1.0f / 2147483648.0f); }
    inline float unipolar() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
};

// Suavizado exponencial de un parámetro (evita zipper noise al girar perillas).
struct Smoother
{
    float y = 0.0f, a = 0.0f;
    void setTime (float seconds, float fs) { a = std::exp (-1.0f / std::max (1.0f, seconds * fs)); }
    void reset (float v) { y = v; }
    inline float process (float target) { y = target + a * (y - target); return y; }
};

// Pasa altos de un polo (acople AC: los capacitores de acople del circuito).
struct DcBlocker
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.9995f;
    void setCutoff (float hz, float fs) { r = std::exp (-kTwoPi * hz / fs); }
    inline float process (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0.0f; }
};

// Pasa bajos de un polo TPT, para suavizar CV y limitar el ancho de banda de las etapas.
struct OnePoleLP
{
    float s = 0.0f, G = 0.5f;
    void setCutoff (float hz, float fs) { const float g = prewarp (hz, fs); G = g / (1.0f + g); }
    inline float process (float x) { const float v = (x - s) * G; const float y = v + s; s = y + v; return y; }
    void reset (float v = 0.0f) { s = v; }
};

} // namespace dh

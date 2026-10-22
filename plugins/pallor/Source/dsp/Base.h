#pragma once
// Piezas básicas de Pallor (docs/research.md §7): seno rápido, resonador modal (fasor complejo que decae, el
// "puente en T" pateado por el pulso de disparo), osciladores de fase con envolvente de altura, el banco de seis
// cuadradas metálicas con PolyBLEP, el filtro de estado variable con salidas LP/BP/HP, envolventes exponenciales y el
// convertidor companding de 8 bits de las máquinas de 1980-85 (la ley del DAC-76). Sin JUCE.
// Todo corre por muestra y es determinista: el mismo disparo da las mismas muestras con cualquier tamaño de bloque.

#include "Common.h"
#include "Blep.h"
#include <array>
#include <vector>

namespace dh::pal {

using dh::kPi; using dh::kTwoPi; using dh::clampf; using dh::fastExp; using dh::fastExp2; using dh::fastTan; using dh::fastTanh;

constexpr double kPiD = 3.14159265358979323846;

inline float dbToGain (float db) { return fastExp2 (db * 0.16609640474f); }          // 10^(db/20)
inline float semis (float st) { return fastExp2 (st * (1.0f / 12.0f)); }

// sin(2π·p) para cualquier p (vuelta = 1). Polinomio de Taylor de grado 9 sobre [−π/2, π/2] con el plegado de la
// simetría: error < 4e-6 (−108 dB). Sin std::sin por muestra (en la CRT de Windows pesa varias veces más).
inline float sin2pi (float p)
{
    p -= std::floor (p + 0.5f);                   // [−0.5, 0.5)
    float x = kTwoPi * p;                         // [−π, π)
    if (x > 0.5f * kPi) x = kPi - x;
    else if (x < -0.5f * kPi) x = -kPi - x;
    const float x2 = x * x;
    return x * (1.0f + x2 * (-1.0f / 6.0f + x2 * (1.0f / 120.0f + x2 * (-1.0f / 5040.0f + x2 * (1.0f / 362880.0f)))));
}
inline float cos2pi (float p) { return sin2pi (p + 0.25f); }

// Coeficiente de un decaimiento exponencial con un T60 dado (segundos): la amplitud cae 60 dB en t60.
inline float t60Coef (float t60, float fs) { return fastExp (-6.9077553f / std::max (1e-4f, t60 * fs)); }
// Coeficiente de un pasa bajos de un polo con constante de tiempo tau (segundos).
inline float tauCoef (float tau, float fs) { return fastExp (-1.0f / std::max (1e-6f, tau * fs)); }
// Mapeo exponencial de 0..1 a lo..hi.
inline float expMap (float u, float lo, float hi) { return lo * fastExp2 (clampf (u, 0.0f, 1.0f) * std::log2 (hi / lo)); }

// Ruido blanco determinista (xorshift32).
struct Noise
{
    uint32_t s = 0x9E3779B9u;
    void seed (uint32_t v) { s = v ? v : 1u; }
    inline float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.0f / 2147483648.0f); }
    inline float uni() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s >> 8) * (1.0f / 16777216.0f); }
};

// Hash entero → [0, 1): decisiones de probabilidad atadas a la posición (no al orden de proceso).
inline uint64_t mix64 (uint64_t z)
{
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
inline double hashUni (uint64_t a, uint64_t b, uint64_t c) { return (double) (mix64 (a ^ mix64 (b ^ mix64 (c + 0x632BE59BD9B4E019ull))) >> 11) * (1.0 / 9007199254740992.0); }

// Resonador modal: un fasor complejo z que gira ω por muestra y decae r por muestra. La salida es Im(z): una
// sinusoide amortiguada que arranca en cero. Un impulso de área A deja una amplitud A. Es el puente en T de las
// cajas analógicas (un pasa banda de dos polos) visto en su forma modal: se puede barrer ω en cada muestra sin
// inestabilidad (|r| < 1 siempre) y la amplitud no depende de la frecuencia.
struct Mode
{
    float re = 0, im = 0, c = 1, s = 0, r = 0.999f;
    void setFreq (float hz, float fs) { const float p = clampf (hz, 0.0f, 0.49f * fs) / fs; c = cos2pi (p); s = sin2pi (p); }
    void setDecay (float t60, float fs) { r = t60Coef (t60, fs); }
    void reset() { re = im = 0; }
    inline void strike (float a) { re += a; }                         // impulso: amplitud a, fase de seno
    inline float process (float x = 0.0f)
    {
        const float nr = r * (re * c - im * s) + x, ni = r * (re * s + im * c);
        re = nr; im = ni;
        return im;
    }
    float amp() const { return std::sqrt (re * re + im * im); }
};

// Oscilador de fase (seno o triángulo) con la frecuencia que el modelo le pase en cada muestra.
struct Phasor
{
    float p = 0;
    inline float tick (float hz, float fs) { p += hz / fs; p -= std::floor (p); return p; }
};
inline float triFromPhase (float p) { return p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p; }

// Seis cuadradas con PolyBLEP: el generador metálico de las cajas de 1980 (osciladores Schmitt de frecuencias sin
// relación armónica; docs/research.md §3.2). Las frecuencias se escalan juntas (tune) y se separan con `spread`.
struct Metal6
{
    std::array<float, 6> ph {}, inc {};
    void set (const float* hz, int n, float tuneRatio, float spread, float fs)
    {
        // spread 1 = las relaciones originales; < 1 las junta hacia su media geométrica, > 1 las separa.
        float lg = 0; for (int i = 0; i < n; ++i) lg += std::log2 (hz[i]); lg /= (float) n;
        for (int i = 0; i < 6; ++i)
        {
            if (i >= n) { inc[(size_t) i] = 0; continue; }
            const float f = std::exp2 (lg + (std::log2 (hz[i]) - lg) * spread) * tuneRatio;
            inc[(size_t) i] = clampf (f / fs, 0.0f, 0.45f);
        }
    }
    void resetPhases (uint32_t seed)
    {
        // Los osciladores corren libres en el original: cada golpe los encuentra en otra fase. Fases de un hash.
        for (int i = 0; i < 6; ++i) ph[(size_t) i] = (float) hashUni (seed, (uint64_t) i, 77);
    }
    inline float process()
    {
        float sum = 0;
        for (int i = 0; i < 6; ++i)
        {
            const float dt = inc[(size_t) i];
            if (dt <= 0) continue;
            float& p = ph[(size_t) i];
            p += dt; if (p >= 1.0f) p -= 1.0f;
            float sq = p < 0.5f ? 1.0f : -1.0f;
            sq += polyBlep (p, dt);
            float p2 = p + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
            sq -= polyBlep (p2, dt);
            sum += sq;
        }
        return sum * (1.0f / 6.0f);
    }
};

// Filtro de estado variable TPT (Zavalishin/Simper) con las tres salidas.
struct Svf
{
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
    float lp = 0, bp = 0, hp = 0;
    void set (float hz, float q, float fs)
    {
        const float g = prewarp (hz, fs);
        k = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }
    inline void process (float v0)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
    inline float lowpass (float x) { process (x); return lp; }
    inline float bandpass (float x) { process (x); return bp * k; }    // ganancia 1 en el centro
    inline float highpass (float x) { process (x); return hp; }
};

// Pasa bajos / pasa altos de un polo (coeficiente directo).
struct Lp1
{
    float a = 0, z = 0;
    void set (float hz, float fs) { a = fastExp (-kTwoPi * clampf (hz, 1.0f, 0.49f * fs) / fs); }
    inline float process (float x) { z = x + a * (z - x); return z; }
    void reset (float v = 0) { z = v; }
};
struct Hp1
{
    float r = 0.999f, x1 = 0, y1 = 0;
    void set (float hz, float fs) { r = fastExp (-kTwoPi * clampf (hz, 0.1f, 0.49f * fs) / fs); }
    inline float process (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0; }
};

// Envolvente de decaimiento exponencial: salta a `level` y cae con su coeficiente.
struct Decay
{
    float v = 0, c = 0.999f;
    void set (float t60, float fs) { c = t60Coef (t60, fs); }
    void setTau (float tau, float fs) { c = tauCoef (tau, fs); }
    inline void fire (float level = 1.0f) { v = level; }
    inline float tick() { const float o = v; v *= c; return o; }
    bool done() const { return v < 1e-5f; }
};

// El convertidor companding de 8 bits de las cajas de 1980-85 (DAC-76 y su reemplazo AM6070; docs/research.md §2.1):
// signo, 3 bits de cuerda y 4 de paso. Cada cuerda empieza en {0, 33, 99, 231, 495, 1023, 2079, 4191} y su paso vale
// 2^(cuerda+1); el fondo de escala es 8031 (72 dB, 255 niveles). Es la ley μ-255 por tramos: el ruido de cuantización
// sigue a la señal (unos 35-40 dB por debajo) en vez de quedar fijo como en 8 bits lineales.
namespace dac76 {
inline const std::array<float, 128>& magnitudes()
{
    static const std::array<float, 128> m = [] {
        std::array<float, 128> a {};
        static const int base[8] = { 0, 33, 99, 231, 495, 1023, 2079, 4191 };
        for (int c = 0; c < 8; ++c) for (int st = 0; st < 16; ++st) a[(size_t) (c * 16 + st)] = (float) (base[c] + (2 << c) * st) / 8031.0f;
        return a;
    }();
    return m;
}
// El código más cercano (bit 7 signo, 6-4 cuerda, 3-0 paso).
inline uint8_t encode (float x)
{
    const auto& m = magnitudes();
    const float a = std::min (1.0f, std::fabs (x));
    int lo = 0, hi = 127;
    while (hi - lo > 1) { const int mid = (lo + hi) >> 1; if (m[(size_t) mid] <= a) lo = mid; else hi = mid; }
    const int k = (a - m[(size_t) lo]) <= (m[(size_t) hi] - a) ? lo : hi;
    return (uint8_t) ((x >= 0.0f ? 0x80 : 0) | k);
}
inline float decode (uint8_t c) { const float v = magnitudes()[(size_t) (c & 0x7F)]; return (c & 0x80) ? v : -v; }
inline float quantize (float x) { return decode (encode (x)); }
} // namespace dac76

// Cuantización lineal a `bits` (12 bits en los muestreadores de 1987-88).
inline float quantLinear (float x, float levels) { return std::round (clampf (x, -1.0f, 1.0f) * levels) / levels; }

// Recorte suave con ganancia de entrada g y salida normalizada para señal chica (tanh(g·x)/g).
inline float softClip (float x, float g) { return fastTanh (g * x) / g; }

} // namespace dh::pal

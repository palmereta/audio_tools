#pragma once
// Piezas básicas de Thrum (docs/research.md): hash determinista, filtros (SVF TPT, un polo, biquad por transformada
// bilineal de un prototipo analógico), líneas de retardo con lectura de Lagrange de tercer orden, sobremuestreo con
// latencia entera, convolución directa (gabinete) y no linealidades con antiderivada (ADAA de primer orden).
// Sin JUCE: el mismo código lo usan el plugin, thrumtest y thrumrender.
// Las funciones rápidas (fastExp2, fastTanh, fastTan) son las de shared/dsp/Common.h: nada de std::exp/std::tan
// por muestra (en la CRT de Windows son varias veces más lentas).

#include "Common.h"
#include "Oversampling.h"
#include <array>
#include <bit>
#include <complex>
#include <vector>

namespace dh::gt {

using dh::kPi; using dh::kTwoPi; using dh::clampf; using dh::fastExp; using dh::fastExp2; using dh::fastTan; using dh::fastTanh;

constexpr double kPiD = 3.14159265358979323846;

// Bits en uno (portable: std::popcount de C++20, no la intrínseca de GCC/Clang que MSVC no tiene).
inline int popc (uint32_t x) { return std::popcount (x); }

inline float dbToGain (float db) { return fastExp2 (db * 0.16609640474f); }          // 10^(db/20)
inline float gainToDb (float g) { return g > 1e-9f ? 20.0f * std::log10 (g) : -180.0f; }
inline float semis (float st) { return fastExp2 (st * (1.0f / 12.0f)); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
// Mapeo exponencial de 0..1 a lo..hi.
inline float expMap (float u, float lo, float hi) { return lo * fastExp2 (clampf (u, 0.0f, 1.0f) * std::log2 (hi / lo)); }
// Coeficiente de un decaimiento con un T60 dado (s) y de un pasa bajos de un polo con constante de tiempo tau (s).
inline float t60Coef (float t60, float fs) { return fastExp (-6.9077553f / std::max (1e-4f, t60 * fs)); }
inline float tauCoef (float tau, float fs) { return fastExp (-1.0f / std::max (1e-6f, tau * fs)); }

// sin(2π·p) para cualquier p (Taylor de grado 9 con plegado; error < 4e-6).
inline float sin2pi (float p)
{
    p -= std::floor (p + 0.5f);
    float x = kTwoPi * p;
    if (x > 0.5f * kPi) x = kPi - x;
    else if (x < -0.5f * kPi) x = -kPi - x;
    const float x2 = x * x;
    return x * (1.0f + x2 * (-1.0f / 6.0f + x2 * (1.0f / 120.0f + x2 * (-1.0f / 5040.0f + x2 * (1.0f / 362880.0f)))));
}
inline float cos2pi (float p) { return sin2pi (p + 0.25f); }
inline float triFromPhase (float p) { p -= std::floor (p); return p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p; }

// ── Azar determinista ──
// Todo lo "humano" (afinación, tiempo, fuerza, púa) sale de un hash del número de evento y de la semilla, no de un
// generador que avanza con el proceso: el mismo MIDI da las mismas muestras con cualquier tamaño de bloque.
inline uint64_t mix64 (uint64_t z)
{
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
inline double hashUni (uint64_t a, uint64_t b, uint64_t c) { return (double) (mix64 (a ^ mix64 (b ^ mix64 (c + 0x632BE59BD9B4E019ull))) >> 11) * (1.0 / 9007199254740992.0); }
inline float hashBi (uint64_t a, uint64_t b, uint64_t c) { return (float) (2.0 * hashUni (a, b, c) - 1.0); }
// Aproximadamente normal (suma de tres uniformes, desvío 1).
inline float hashGauss (uint64_t a, uint64_t b, uint64_t c) { return (hashBi (a, b, c) + hashBi (a, b, c + 101) + hashBi (a, b, c + 202)) * 1.0f; }

struct Noise
{
    uint32_t s = 0x9E3779B9u;
    void seed (uint32_t v) { s = v ? v : 1u; }
    inline float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.0f / 2147483648.0f); }
};

// ── Filtros ──
// Variable de estado TPT (Zavalishin/Simper) con las tres salidas; se puede barrer por muestra (wah).
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
    void reset() { ic1 = ic2 = 0; lp = bp = hp = 0; }
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

// Biquad en forma directa traspuesta II, en double (los estantes y los resonadores de baja frecuencia a 192 kHz
// pierden precisión en float).
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline float process (float xf)
    {
        const double x = xf;
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
    // Prototipo analógico H(s) = (B0 + B1 s + B2 s²) / (A0 + A1 s + A2 s²), bilineal con prewarp en fw (Hz).
    void fromAnalog (double B0, double B1, double B2, double A0, double A1, double A2, double fs, double fw)
    {
        const double w = 2.0 * kPiD * std::min (fw, 0.45 * fs);
        const double c = w / std::tan (w / (2.0 * fs));                 // s = c·(1 − z⁻¹)/(1 + z⁻¹)
        const double c2 = c * c;
        const double d0 = A0 + A1 * c + A2 * c2;
        b0 = (B0 + B1 * c + B2 * c2) / d0;
        b1 = (2.0 * B0 - 2.0 * B2 * c2) / d0;
        b2 = (B0 - B1 * c + B2 * c2) / d0;
        a1 = (2.0 * A0 - 2.0 * A2 * c2) / d0;
        a2 = (A0 - A1 * c + A2 * c2) / d0;
    }
    // Recetas del "Audio EQ Cookbook" (R. Bristow-Johnson), por la bilineal del prototipo.
    void peak (double f, double q, double db, double fs)
    {
        const double A = std::pow (10.0, db / 40.0), w = 2.0 * kPiD * f;
        fromAnalog (w * w, w * A / q, 1.0, w * w, w / (A * q), 1.0, fs, f);
    }
    void lowShelf (double f, double q, double db, double fs)
    {
        const double A = std::pow (10.0, db / 40.0), w = 2.0 * kPiD * f, sA = std::sqrt (A);
        // H(s) = A·(s² + √A/Q·w s + A w²)/(A s² + √A/Q·w s + w²)
        const double nb0 = A * (A * w * w), nb1 = A * (sA / q * w), nb2 = A, na0 = w * w, na1 = sA / q * w, na2 = A;
        fromAnalog (nb0, nb1, nb2, na0, na1, na2, fs, f);
    }
    void highShelf (double f, double q, double db, double fs)
    {
        const double A = std::pow (10.0, db / 40.0), w = 2.0 * kPiD * f, sA = std::sqrt (A);
        // H(s) = A·(A s² + √A/Q·w s + w²)/(s² + √A/Q·w s + A w²)
        fromAnalog (A * w * w, A * sA / q * w, A * A, A * w * w, sA / q * w, 1.0, fs, f);
    }
    void lowpass (double f, double q, double fs) { const double w = 2.0 * kPiD * f; fromAnalog (w * w, 0, 0, w * w, w / q, 1.0, fs, f); }
    void highpass (double f, double q, double fs) { const double w = 2.0 * kPiD * f; fromAnalog (0, 0, 1.0, w * w, w / q, 1.0, fs, f); }
    void bandpass (double f, double q, double fs) { const double w = 2.0 * kPiD * f; fromAnalog (0, w / q, 0, w * w, w / q, 1.0, fs, f); }
    std::complex<double> response (double f, double fs) const
    {
        const std::complex<double> z1c = std::polar (1.0, -2.0 * kPiD * f / fs), z2c = z1c * z1c;
        return (b0 + b1 * z1c + b2 * z2c) / (1.0 + a1 * z1c + a2 * z2c);
    }
};

// Sistema analógico de tercer orden H(s) = (b0 + b1 s + b2 s² + b3 s³)/(a0 + a1 s + a2 s² + a3 s³) llevado a digital
// por la bilineal (con prewarp en fw) y corrido en forma directa traspuesta II en double. Lo usan la red pasiva de la
// pastilla (bobina, capacidad, potes) y el tonestack del amplificador.
struct Iir3
{
    double b[4] {1, 0, 0, 0}, a[4] {1, 0, 0, 0}, z[3] {};
    void reset() { z[0] = z[1] = z[2] = 0; }
    void fromAnalog (const double* B, const double* A, double fs, double fw)
    {
        const double w = 2.0 * kPiD * std::min (fw, 0.45 * fs);
        const double c = w / std::tan (w / (2.0 * fs)), c2 = c * c, c3 = c2 * c;
        // (1 + z⁻¹)³, (1 + z⁻¹)²(1 − z⁻¹), (1 + z⁻¹)(1 − z⁻¹)², (1 − z⁻¹)³
        static const double P[4][4] = { { 1, 3, 3, 1 }, { 1, 1, -1, -1 }, { 1, -1, -1, 1 }, { 1, -3, 3, -1 } };
        const double kc[4] = { 1, c, c2, c3 };
        double nb[4] {}, na[4] {};
        for (int k = 0; k < 4; ++k)
            for (int i = 0; i < 4; ++i) { nb[i] += B[k] * kc[k] * P[k][i]; na[i] += A[k] * kc[k] * P[k][i]; }
        const double d = na[0];
        for (int i = 0; i < 4; ++i) { b[i] = nb[i] / d; a[i] = na[i] / d; }
    }
    inline float process (float xf)
    {
        const double x = xf;
        const double y = b[0] * x + z[0];
        z[0] = b[1] * x - a[1] * y + z[1];
        z[1] = b[2] * x - a[2] * y + z[2];
        z[2] = b[3] * x - a[3] * y;
        return (float) y;
    }
    std::complex<double> response (double f, double fs) const
    {
        const std::complex<double> zi = std::polar (1.0, -2.0 * kPiD * f / fs);
        std::complex<double> n = b[3], d = a[3];
        for (int i = 2; i >= 0; --i) { n = n * zi + b[i]; d = d * zi + a[i]; }
        return n / d;
    }
};

// ── Retardos ──
// Línea circular (tamaño potencia de 2) con lectura de Lagrange de tercer orden: la de las cuerdas, que se modulan
// (bends, slides, vibrato, palanca) sin los transitorios de un pasa todos.
struct Line
{
    std::vector<float> b; int mask = 0, w = 0;
    void prepare (int maxSamples) { int n = 1; while (n < maxSamples + 8) n <<= 1; b.assign ((size_t) n, 0.0f); mask = n - 1; w = 0; }
    void clear() { std::fill (b.begin(), b.end(), 0.0f); w = 0; }
    int capacity() const { return mask - 4; }
    inline void push (float x) { b[(size_t) w] = x; w = (w + 1) & mask; }
    inline float at (int d) const { return b[(size_t) ((w - d) & mask)]; }           // d = 1: la última escrita
    inline float lagrange (float d) const
    {
        d = clampf (d, 1.0f, (float) (mask - 4));
        const float fd = std::floor (d);
        const int i = (int) fd; const float t = d - fd;
        const float y0 = at (i - 1 < 1 ? 1 : i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        // Interpolador de Lagrange de 4 puntos en x = t + 1 sobre los nodos 0..3 (retardo i−1 … i+2).
        const float x = t + 1.0f;
        const float c0 = -(x - 1.0f) * (x - 2.0f) * (x - 3.0f) * (1.0f / 6.0f);
        const float c1 = x * (x - 2.0f) * (x - 3.0f) * 0.5f;
        const float c2 = -x * (x - 1.0f) * (x - 3.0f) * 0.5f;
        const float c3 = x * (x - 1.0f) * (x - 2.0f) * (1.0f / 6.0f);
        return c0 * y0 + c1 * y1 + c2 * y2 + c3 * y3;
    }
    // Lagrange de quinto orden (6 puntos): la pérdida de agudos por vuelta es ~8 veces menor que con 4 puntos (a 6 kHz,
    // 0,009 dB contra 0,074 con el fraccional en 0,5), así una nota aguda no se apaga antes según dónde cae su retardo.
    inline float lagrange5 (float d) const
    {
        d = clampf (d, 3.0f, (float) (mask - 6));
        const float fd = std::floor (d);
        const int i = (int) fd; const float x = d - fd + 2.0f;
        const float y0 = at (i - 2), y1 = at (i - 1), y2 = at (i), y3 = at (i + 1), y4 = at (i + 2), y5 = at (i + 3);
        const float a0 = x, a1 = x - 1.0f, a2 = x - 2.0f, a3 = x - 3.0f, a4 = x - 4.0f, a5 = x - 5.0f;
        const float c0 = a1 * a2 * a3 * a4 * a5 * (-1.0f / 120.0f);
        const float c1 = a0 * a2 * a3 * a4 * a5 * (1.0f / 24.0f);
        const float c2 = a0 * a1 * a3 * a4 * a5 * (-1.0f / 12.0f);
        const float c3 = a0 * a1 * a2 * a4 * a5 * (1.0f / 12.0f);
        const float c4 = a0 * a1 * a2 * a3 * a5 * (-1.0f / 24.0f);
        const float c5 = a0 * a1 * a2 * a3 * a4 * (1.0f / 120.0f);
        return c0 * y0 + c1 * y1 + c2 * y2 + c3 * y3 + c4 * y4 + c5 * y5;
    }
    inline float linear (float d) const
    {
        d = clampf (d, 1.0f, (float) (mask - 2));
        const int i = (int) d; const float t = d - (float) i;
        return at (i) + (at (i + 1) - at (i)) * t;
    }
    // Hermite (los efectos de modulación: más barata, suficiente para coros y ecos).
    inline float hermite (float d) const
    {
        d = clampf (d, 1.0f, (float) (mask - 4));
        const float fd = std::floor (d); const int i = (int) fd; const float t = d - fd;
        const float y0 = at (i - 1 < 1 ? 1 : i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }
};

// Retardo entero fijo (alinear la salida DI con la procesada, compensar latencias).
struct FixedDelay
{
    std::vector<float> b; int n = 0, w = 0;
    void prepare (int samples) { n = std::max (0, samples); b.assign ((size_t) n + 1, 0.0f); w = 0; }
    void clear() { std::fill (b.begin(), b.end(), 0.0f); w = 0; }
    inline float process (float x)
    {
        if (n == 0) return x;
        const float y = b[(size_t) w]; b[(size_t) w] = x; if (++w > n - 1) w = 0;
        return y;
    }
};

// ── Sobremuestreo ──
// Medias bandas FIR de fase lineal (las de shared/dsp/Oversampling.h) con largos elegidos para que la ida y vuelta
// tenga un retardo entero a la frecuencia base: 2x = 95 coeficientes (23,5 + 23,5 = 47 muestras), 4x agrega una de
// 33 a 4x (4 + 4: 55 muestras), 8x una de 17 a 8x (1 + 1: 57). Así la latencia declarada es exacta.
class Oversampler
{
public:
    static int latencyFor (int factor) { return factor >= 8 ? 57 : factor >= 4 ? 55 : factor >= 2 ? 47 : 0; }
    void prepare (int f, int maxBlock)
    {
        factor = f >= 8 ? 8 : f >= 4 ? 4 : f >= 2 ? 2 : 1;
        stages = factor == 8 ? 3 : factor == 4 ? 2 : factor == 2 ? 1 : 0;
        static const int taps[3] = { 95, 33, 17 };       // etapa 0 = la más cercana a la frecuencia base
        static const double beta[3] = { 9.0, 7.0, 6.0 };
        for (int s = 0; s < stages; ++s) { up[s].design (taps[s], beta[s]); dn[s].design (taps[s], beta[s]); }
        bufA.assign ((size_t) (maxBlock * 8 + 16), 0.0f); bufB.assign (bufA.size(), 0.0f); dA.assign (bufA.size(), 0.0f); dB.assign (bufA.size(), 0.0f);
    }
    int getFactor() const { return factor; }
    void reset() { for (int s = 0; s < 3; ++s) { up[s].reset(); dn[s].reset(); } }
    // in: n muestras → out (puntero interno): n·factor.
    float* upsample (const float* in, int n)
    {
        if (stages == 0) { std::copy (in, in + n, bufA.data()); return bufA.data(); }
        const float* src = in; int len = n;
        for (int s = 0; s < stages; ++s)
        {
            float* dst = (s % 2 == 0) ? bufA.data() : bufB.data();
            for (int i = 0; i < len; ++i)
            {
                up[s].push (src[i] * 2.0f); dst[2 * i] = up[s].output();
                up[s].push (0.0f);          dst[2 * i + 1] = up[s].output();
            }
            src = dst; len *= 2;
        }
        return (stages % 2 == 1) ? bufA.data() : bufB.data();
    }
    // data: n·factor muestras (puede ser el puntero de upsample) → out: n. La salida de cada etapa se toma después
    // del primer empuje (fase 0), así el retardo de la bajada es igual al de la subida.
    void downsample (const float* data, float* out, int n)
    {
        if (stages == 0) { std::copy (data, data + n, out); return; }
        int len = n * factor;
        const float* src = data;
        for (int s = stages - 1; s >= 0; --s)
        {
            const int outLen = len / 2;
            float* dst = s == 0 ? out : ((s % 2) ? dA.data() : dB.data());
            for (int i = 0; i < outLen; ++i)
            {
                dn[s].push (src[2 * i]);
                const float y = dn[s].output();
                dn[s].push (src[2 * i + 1]);
                dst[i] = y;
            }
            src = dst; len = outLen;
        }
    }
private:
    HalfbandFir up[3], dn[3];
    std::vector<float> bufA, bufB, dA, dB;
    int factor = 1, stages = 0;
};

// ── ADAA de primer orden (Parker, Zavalishin y Le Bivic, DAFx 2016) ──
// y[n] = (F(x[n]) − F(x[n−1])) / (x[n] − x[n−1]), con F la antiderivada; si los dos valores están muy cerca, la
// no linealidad en el punto medio. Reduce el aliasing de los pedales de saturación sin sobremuestrear (latencia 0;
// el retardo de grupo es media muestra).
template <typename Fn, typename AntiFn>
struct Adaa1
{
    float x1 = 0, F1 = 0;
    void reset() { x1 = 0; F1 = AntiFn() (0.0f); }
    inline float process (float x)
    {
        const float F = AntiFn() (x);
        const float d = x - x1;
        const float y = std::fabs (d) > 1e-4f ? (F - F1) / d : Fn() (0.5f * (x + x1));
        x1 = x; F1 = F;
        return y;
    }
};
// Recorte cúbico suave: f(x) = 1,5·(x − x³/3) en |x| ≤ 1 y ±1 afuera (pendiente 1,5 en el origen, 0 en el codo);
// antiderivada polinómica, sin log ni exp por muestra.
struct CubicFn { inline float operator() (float x) const { const float a = clampf (x, -1.0f, 1.0f); return 1.5f * (a - a * a * a * (1.0f / 3.0f)); } };
struct CubicAnti { inline float operator() (float x) const { const float a = std::fabs (x); return a <= 1.0f ? 1.5f * (0.5f * x * x - x * x * x * x * (1.0f / 12.0f)) : a - 0.375f; } };
// Recorte duro en ±1 y su antiderivada.
struct HardFn { inline float operator() (float x) const { return clampf (x, -1.0f, 1.0f); } };
struct HardAnti { inline float operator() (float x) const { const float a = std::fabs (x); return a <= 1.0f ? 0.5f * x * x : a - 0.5f; } };

} // namespace dh::gt

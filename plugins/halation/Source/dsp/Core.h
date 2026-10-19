#pragma once
// Piezas básicas del motor de espacio: línea de retardo con lectura fraccionaria modulable, filtros de un polo,
// la matriz de realimentación ortogonal por etapas mariposa (de identidad a Hadamard con un solo ángulo) y un
// generador de ruido determinista. Todo sin JUCE, en float con acumuladores en double donde importa.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace dh::sp {

constexpr float kPi = 3.14159265358979323846f;
constexpr double kPiD = 3.14159265358979323846;

inline float clampf (float x, float a, float b) { return std::min (b, std::max (a, x)); }
inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }
inline float gainToDb (float g) { return g > 1e-9f ? 20.0f * std::log10 (g) : -180.0f; }

// xorshift32: el mismo ruido en todas las plataformas y en cada corrida.
struct Rand
{
    uint32_t s = 0x9E3779B9u;
    explicit Rand (uint32_t seed = 0x9E3779B9u) : s (seed ? seed : 1u) {}
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }       // [0, 1)
    inline float bi() { return 2.0f * uni() - 1.0f; }                                  // [−1, 1)
};

// Línea de retardo circular con lectura por interpolación cúbica (Hermite). El tamaño es potencia de 2.
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        int n = 1; while (n < maxSamples + 8) n <<= 1;
        buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // Lectura `d` muestras atrás (d ≥ 1; la última escrita es d = 1).
    inline float read (float d) const
    {
        d = clampf (d, 1.0f, (float) (mask - 4));
        const float fd = std::floor (d);
        const int i = (int) fd; const float t = d - fd;
        const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }
    inline float at (int d) const { return buf[(size_t) ((w - d) & mask)]; }
    int capacity() const { return mask - 4; }
private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};

struct OnePole       // pasa bajos de un polo (coeficiente directo por frecuencia)
{
    float a = 0.0f, z = 0.0f;
    void setHz (float hz, float fs) { a = std::exp (-2.0f * kPi * clampf (hz, 1.0f, 0.49f * fs) / fs); }
    inline float lp (float x) { z = x + a * (z - x); return z; }
    void reset() { z = 0.0f; }
};

struct DcBlock       // pasa altos de un polo para la realimentación
{
    float r = 0.999f, x1 = 0.0f, y1 = 0.0f;
    void setHz (float hz, float fs) { r = std::exp (-2.0f * kPi * hz / fs); }
    inline float process (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0.0f; }
};

// Matriz de realimentación de N = 2^k líneas: k etapas de rotaciones de Givens entre los pares (i, i ^ 2^s).
// Con θ = 0 es la identidad (cada línea se realimenta sola: ecos separados, un delay); con θ = π/4 es la
// Hadamard normalizada (todas las líneas se mezclan con el mismo peso: densidad de reverb). Siempre ortogonal,
// así la realimentación conserva la energía para cualquier θ (la estabilidad la deciden sólo las ganancias).
template <int N>
struct ButterflyMatrix
{
    static_assert ((N & (N - 1)) == 0 && N >= 2, "N potencia de 2");
    float c = 1.0f, s = 0.0f;
    void setAngle (float theta) { c = std::cos (theta); s = std::sin (theta); }
    inline void apply (std::array<float, N>& x) const
    {
        for (int step = 1; step < N; step <<= 1)
            for (int i = 0; i < N; ++i)
                if ((i & step) == 0)
                {
                    const int j = i | step;
                    const float a = x[(size_t) i], b = x[(size_t) j];
                    x[(size_t) i] = c * a - s * b;
                    x[(size_t) j] = s * a + c * b;
                }
    }
};

// Difusor de entrada: cadena de pasa todos de Schroeder (densidad sin cambiar el color).
class Diffuser
{
public:
    static constexpr int kStages = 4;
    void prepare (float fs)
    {
        const float ms[kStages] = { 4.77f, 3.59f, 12.73f, 9.31f };      // los del difusor de entrada de Dattorro (a 29.8 kHz), en ms
        for (int k = 0; k < kStages; ++k) { len[k] = std::max (2.0f, ms[k] * 1e-3f * fs); d[k].prepare ((int) len[k] + 4); }
    }
    void clear() { for (auto& x : d) x.clear(); }
    inline float process (float x, float g)
    {
        for (int k = 0; k < kStages; ++k)
        {
            const float delayed = d[k].at ((int) len[k]);
            const float v = x + g * delayed;
            d[k].push (v);
            x = delayed - g * v;
        }
        return x;
    }
private:
    DelayLine d[kStages];
    float len[kStages] {};
};

} // namespace dh::sp
